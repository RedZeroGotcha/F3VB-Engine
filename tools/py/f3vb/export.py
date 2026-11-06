"""Exportador B3D (+ esqueleto/animações GR2 + texturas) -> GLB.

Convenções (decisões do exportador; ver docs/EXPORTADOR_GLB.md):
 - O jogo é mão-esquerda, Y para cima (confirmado: `coordinate_system LEFT_HANDED`, cabeça em y ~ 5). O glTF é
   mão-direita, Y para cima: invertemos o eixo Z (posição, normal; quaternion (x,y,z,w) -> (-x,-y,z,w)).
 - O sentido dos triângulos é decidido POR DADOS: comparamos a normal geométrica com as normais dos vértices.
 - Unidade: 1 unidade do jogo = `unit_scale` (padrão 1.0). Sem referência oficial de escala.
 - Rotações de osso/nó do B3D são eixo-ângulo em GRAUS (observado: (0, .71, -.70, 179.99)).
 - Matrizes inversas de bind vêm do GR2 (InverseWorld4x4); no glTF o mesmo vetor de 16 floats serve (Granny usa a
   convenção linha-vetor; ler linha a linha equivale à coluna-major do glTF).
"""
from __future__ import annotations

from dataclasses import replace

import math
import re

from .b3d import Model, read_b3d
from .gr2export import Clip, Joint
from .glb import FLOAT, Glb, UINT, USHORT, ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER, quat_normalize
from .imagecodec import ImageError, image_to_png

# Escala da geometria do B3D em relação ao esqueleto do GR2 (MEDIDO em CR_Dog e CR_FloatingEyeBot: a melhor correspondência
# entre vértices de peso ~1 e o osso dono deles dá k = 10,2 a 10,4; 10,24 = coordinate_scale (1024) / 100 é HIPÓTESE).
# Sem isto, a pose de bind parece certa (W * IBM = identidade), mas qualquer animação gira os vértices em torno de pivôs errados.
DEFAULT_MESH_SCALE = 100.0 / 1024.0

ALPHA_MODES = {"OPAQUE": "OPAQUE", "ALPHABLEND": "BLEND", "ADD": "BLEND"}


def _axis_angle_to_quat(v):
    x, y, z, deg = v
    n = math.sqrt(x * x + y * y + z * z)
    if n < 1e-9:
        return (0.0, 0.0, 0.0, 1.0)
    s = math.sin(math.radians(deg) / 2) / n
    return quat_normalize((x * s, y * s, z * s, math.cos(math.radians(deg) / 2)))


def _quat_to_mat3(q):
    x, y, z, w = q
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1.0
    x, y, z, w = x / n, y / n, z / n, w / n
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]


def _mat4_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def _joint_local(j):
    r = _quat_to_mat3(j.orientation)
    s = j.scale
    return [[r[i][c] * s[c] for c in range(3)] + [j.position[i]] for i in range(3)] + [[0, 0, 0, 1]]


def detect_initial_placement(joints, tol=1e-3):
    """Deslocamento constante entre o esqueleto (TRS locais) e o InverseWorld4x4 das juntas.

    Mede P_i = mundo_i * IBM_i (forma coluna) para cada junta. Se TODAS as juntas dão a MESMA translação pura T0
    (sem rotação), o IBM foi calculado com um 'posicionamento inicial' do modelo (Granny: InitialPlacement) que a
    hierarquia de TRS locais não inclui. Para a pose de bind reproduzir a malha, a raiz precisa de T = -T0 aplicado
    à esquerda. Devolve (-T0x, -T0y, -T0z) ou None se não há deslocamento (ex.: CR_Dog) ou se não é constante.
    Medido em CR_FloatingEyeBot: T0 = (0.0117, 0, 0.0087) em todas as 8 juntas.
    """
    if not joints:
        return None
    world = []
    for j in joints:
        loc = _joint_local(j)
        world.append(loc if j.parent < 0 else _mat4_mul(world[j.parent], loc))
    t0 = None
    for w, j in zip(world, joints):
        ibm = [[j.inverse_world[r * 4 + c] for c in range(4)] for r in range(4)]
        ibm_col = [[ibm[c][r] for c in range(4)] for r in range(4)]
        p = _mat4_mul(w, ibm_col)
        rot_dev = max(abs(p[r][c] - (1.0 if r == c else 0.0)) for r in range(3) for c in range(3))
        if rot_dev > tol:
            return None
        t = (p[0][3], p[1][3], p[2][3])
        if t0 is None:
            t0 = t
        elif max(abs(t[i] - t0[i]) for i in range(3)) > tol:
            return None
    if t0 is None or max(abs(v) for v in t0) < 1e-5:
        return None
    return (-t0[0], -t0[1], -t0[2])


def apply_initial_placement(joints, clips):
    """Aplica o posicionamento inicial às raízes (e às chaves de translação delas). Devolve (juntas, clipes, T|None)."""
    place = detect_initial_placement(joints)
    if place is None:
        return joints, clips, None
    new_joints = [replace(j, position=tuple(j.position[i] + place[i] for i in range(3))) if j.parent < 0 else j
                  for j in joints]
    roots = {j.name for j in joints if j.parent < 0}
    new_clips = []
    for c in clips or []:
        tracks = {}
        for bone, tr in c.tracks.items():
            if bone in roots and "translation" in tr:
                t, v = tr["translation"]
                tr = dict(tr)
                tr["translation"] = (t, [tuple(x[i] + place[i] for i in range(3)) for x in v])
            tracks[bone] = tr
        new_clips.append(replace(c, tracks=tracks))
    return new_joints, new_clips, place


def resolve_bone_aliases(bone_names, joints):
    """Casa os nomes de osso do B3D com as juntas do esqueleto do GR2 quando os nomes diferem só por sufixo.

    Medido em HumFSkl/HumMNor: o B3D diz "L Clavicle", "L Fingers", "R Handle" e o GR2 tem "L Clavicle_1", "L Fingers_1",
    "R Handle_1" (o exportador de origem acrescentou "_1" para desfazer nomes repetidos). Sem o casamento, os vértices
    presos a esses ossos ficam sem junta e a malha desmonta (braços viram "lâminas" esticadas).
    Ordem: nome exato, depois nome+"_1".."_9", depois sem o sufixo "_N", depois sem diferenciar maiúsculas.
    Devolve (lista de nomes já resolvidos, {nome do B3D: nome da junta} só dos que foram trocados).
    """
    if not joints:
        return list(bone_names), {}
    jn = {j.name for j in joints}
    lower = {}
    for n in sorted(jn):
        lower.setdefault(n.lower(), n)
    out, aliased = [], {}
    for b in bone_names:
        pick = b if b in jn else None
        if pick is None:
            for k in range(1, 10):
                if f"{b}_{k}" in jn:
                    pick = f"{b}_{k}"
                    break
        if pick is None:
            m = re.match(r"^(.*)_(\d+)$", b)
            if m and m.group(1) in jn:
                pick = m.group(1)
        if pick is None and b.lower() in lower:
            pick = lower[b.lower()]
        if pick is not None and pick != b:
            aliased[b] = pick
        out.append(pick if pick is not None else b)
    return out, aliased


class Exporter:
    def __init__(self, unit_scale: float = 1.0, convert: bool = True, mesh_scale: float = DEFAULT_MESH_SCALE):
        self.k = unit_scale
        self.initial_placement = None
        self.ms = mesh_scale          # geometria do B3D -> unidades do esqueleto do GR2 (ver nota no topo do arquivo)
        self.convert = convert
        self.g = Glb()
        self.warnings: list[str] = []
        self.stats = {"surfaces": 0, "vertices": 0, "triangles": 0, "textures": 0, "joints": 0, "animations": 0}

    # ---- conversões de espaço ------------------------------------------------------------------------
    def p(self, v):
        return (v[0] * self.k, v[1] * self.k, (-v[2] if self.convert else v[2]) * self.k)

    def pm(self, v):
        """Posição de GEOMETRIA do B3D (vértices, transformações de nós): aplica a escala malha->esqueleto."""
        k = self.k * self.ms
        return (v[0] * k, v[1] * k, (-v[2] if self.convert else v[2]) * k)

    def n(self, v):
        return (v[0], v[1], -v[2] if self.convert else v[2])

    def q(self, q):
        q = quat_normalize(q)
        return (-q[0], -q[1], q[2], q[3]) if self.convert else q

    def m4(self, m):
        """16 floats do GR2 (linha-vetor) -> glTF coluna-major no espaço convertido (M' = S M S, translação escalada)."""
        if len(m) != 16:
            return tuple(float(i == j) for i in range(4) for j in range(4))
        a = [list(m[r * 4:(r + 1) * 4]) for r in range(4)]          # a[r][c] = M[c][r] (transposta implícita)
        out = [[a[r][c] for c in range(4)] for r in range(4)]
        if self.convert:
            s = (1, 1, -1, 1)
            out = [[out[r][c] * s[r] * s[c] for c in range(4)] for r in range(4)]
        for r in range(3):
            out[3][r] *= self.k                                      # translação está na última "linha" do vetor-linha
        return tuple(x for row in out for x in row)

    # ---- texturas / materiais -------------------------------------------------------------------------
    def _texture(self, file_name: str, loader):
        key = file_name.lower()
        if key in self.g._img_cache:
            return self.g._img_cache[key]
        stem = file_name.rsplit(".", 1)[0]
        raw = loader(stem) if loader else None
        if raw is None:
            self.warnings.append(f"textura '{file_name}' não encontrada")
            self.g._img_cache[key] = None
            return None
        try:
            _w, _h, png = image_to_png(raw)
        except (ImageError, ValueError) as exc:
            self.warnings.append(f"textura '{file_name}': {exc}")
            self.g._img_cache[key] = None
            return None
        self.stats["textures"] += 1
        return self.g.texture(png, key=key, name=stem)

    def _material(self, mat, loader):
        f = mat.floats if len(mat.floats) >= 4 else [1, 1, 1, 1]
        tex = self._texture(mat.textures[0], loader) if mat.textures else None
        mode = ALPHA_MODES.get(mat.blend, "OPAQUE")
        extras = {"f3vb_blend": mat.blend, "f3vb_surface_type": mat.type, "f3vb_material_id": mat.id,
                  "f3vb_d3d_material": [round(x, 5) for x in mat.floats]}
        return self.g.material(mat.name or "material", base_color=(f[0], f[1], f[2], f[3] if mode == "BLEND" else 1.0),
                               texture=tex, alpha_mode=mode, double_sided=False, extras=extras)

    # ---- geometria -------------------------------------------------------------------------------------
    def _prep_surface(self, name: str, surf, joint_by_name: dict | None, bone_names: list) -> dict:
        """Dados da superfície já no espaço de saída (convertido/escalado): usados pelo GLB e pelo F3M."""
        pos = [self.pm(v) for v in surf.positions]
        nor = [self.n(v) for v in surf.normals]
        out = {"pos": pos, "nor": nor, "uv0": list(surf.uv0), "uv1": list(surf.uv1), "col": list(surf.colors),
               "joints": None, "weights": None, "prims": [], "bone_refs": None}
        if surf.influences:
            j_all, w_all, missing = [], [], 0
            for infl in surf.influences:
                js, ws = [], []
                for bi, bw in infl:
                    if joint_by_name is not None:                      # GLB: índice de junta do esqueleto
                        nm = bone_names[bi] if 0 <= bi < len(bone_names) else None
                        j = joint_by_name.get(nm) if nm is not None else None
                        if j is None:
                            missing += 1
                            continue
                    else:                                              # F3M: índice na lista de nomes de ossos do B3D
                        j = bi if 0 <= bi < len(bone_names) else None
                        if j is None:
                            missing += 1
                            continue
                    js.append(j); ws.append(bw)
                js, ws = (js + [0] * 4)[:4], (ws + [0.0] * 4)[:4]
                tot = sum(ws)
                ws = [w / tot for w in ws] if tot > 0 else [1.0, 0.0, 0.0, 0.0]
                j_all.append(tuple(js)); w_all.append(tuple(ws))
            if missing:
                self.warnings.append(f"{name}: {missing} influências de osso sem correspondência")
            out["joints"], out["weights"] = j_all, w_all
        for pr in surf.primitives:
            idx = list(pr["indices"])
            if idx:
                out["prims"].append((pr["material"], self._fix_winding(idx, pos, nor)))
        return out

    def _surface_mesh(self, name: str, surf, loader, joint_by_name: dict | None, bone_names: list):
        g = self.g
        d = self._prep_surface(name, surf, joint_by_name, bone_names)
        attrs = {"POSITION": g.accessor(d["pos"], FLOAT, "VEC3", ARRAY_BUFFER, minmax=True),
                 "NORMAL": g.accessor(d["nor"], FLOAT, "VEC3", ARRAY_BUFFER),
                 "TEXCOORD_0": g.accessor(d["uv0"], FLOAT, "VEC2", ARRAY_BUFFER)}
        if d["uv1"]:
            attrs["TEXCOORD_1"] = g.accessor(d["uv1"], FLOAT, "VEC2", ARRAY_BUFFER)
        if d["col"]:
            attrs["COLOR_0"] = g.accessor(d["col"], FLOAT, "VEC4", ARRAY_BUFFER)
        skinned = bool(joint_by_name) and d["joints"] is not None
        if skinned:
            attrs["JOINTS_0"] = g.accessor(d["joints"], USHORT, "VEC4", ARRAY_BUFFER)
            attrs["WEIGHTS_0"] = g.accessor(d["weights"], FLOAT, "VEC4", ARRAY_BUFFER)
        prims = []
        for mat_i, idx in d["prims"]:
            ctype = USHORT if max(idx) < 65535 else UINT
            mat = surf.materials[mat_i] if mat_i < len(surf.materials) else None
            prim = {"attributes": dict(attrs), "indices": g.accessor(idx, ctype, "SCALAR", ELEMENT_ARRAY_BUFFER), "mode": 4}
            if mat is not None:
                prim["material"] = self._material(mat, loader)
            prims.append(prim)
            self.stats["triangles"] += len(idx) // 3
        self.stats["surfaces"] += 1
        self.stats["vertices"] += len(d["pos"])
        return g.mesh(name, prims, extras={"f3vb_lightmap": surf.lightmap} if surf.lightmap else None), skinned

    @staticmethod
    def _fix_winding(idx, pos, nor):
        """glTF: face frontal = anti-horário. Decide pelos dados: a normal geométrica deve concordar com as normais."""
        agree = 0
        total = 0
        for i in range(0, len(idx) - 2, 3):
            a, b, c = pos[idx[i]], pos[idx[i + 1]], pos[idx[i + 2]]
            u = (b[0] - a[0], b[1] - a[1], b[2] - a[2]); v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            cr = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
            nn = [sum(nor[j][k] for j in (idx[i], idx[i + 1], idx[i + 2])) for k in range(3)]
            d = cr[0] * nn[0] + cr[1] * nn[1] + cr[2] * nn[2]
            if abs(d) > 1e-12:
                total += 1
                agree += d > 0
        if total and agree < total / 2:                       # maioria discorda -> inverte a ordem
            out = list(idx)
            for i in range(0, len(out) - 2, 3):
                out[i + 1], out[i + 2] = out[i + 2], out[i + 1]
            return out
        return idx

    # ---- cena ---------------------------------------------------------------------------------------------
    def build(self, model: Model, name: str, texture_loader=None, joints: list[Joint] | None = None,
              clips: list[Clip] | None = None) -> bytes:
        g = self.g
        joints, clips, self.initial_placement = apply_initial_placement(joints, clips) if joints else (joints, clips, None)
        joint_nodes, joint_by_name = [], {}
        root_children = []
        if joints:
            for jt in joints:
                tr = self.p(jt.position)
                node = g.node(jt.name, translation=tr, rotation=self.q(jt.orientation), scale=jt.scale,
                              extras={"f3vb_bone": True})
                joint_nodes.append(node)
                joint_by_name.setdefault(jt.name, len(joint_nodes) - 1)
            for i, jt in enumerate(joints):
                if jt.parent >= 0:
                    g.j["nodes"][joint_nodes[jt.parent]].setdefault("children", []).append(joint_nodes[i])
                else:
                    root_children.append(joint_nodes[i])
            self.stats["joints"] = len(joints)
        bone_names, aliased = resolve_bone_aliases([b.name for b in model.bones], joints)
        self.stats["bone_aliases"] = aliased
        if aliased:
            self.warnings.append(f"{name}: {len(aliased)} ossos casados por sufixo ({', '.join(f'{a}->{b}' for a, b in list(aliased.items())[:4])})")
        skin_idx = None
        if joints:
            skin_idx = g.skin(name + "_skin", joint_nodes, [self.m4(j.inverse_world) for j in joints],
                              skeleton=root_children[0] if root_children else None)

        def convert_node(bn, parent_name=""):
            kids = []
            for s_node, surf in [(bn, s) for s in bn.surfaces]:
                mesh_name = bn.name or name
                mi, skinned = self._surface_mesh(mesh_name, surf, texture_loader, joint_by_name if joints else None, bone_names)
                kids.append(g.node(mesh_name, mesh=mi, skin=skin_idx if skinned else None))
            for c in bn.children:
                kids.append(convert_node(c, bn.name))
            tr = bn.transform or {}
            return g.node(bn.name or "node", children=kids or None,
                          translation=self.pm(tr["translation"]) if "translation" in tr else None,
                          rotation=self.q(_axis_angle_to_quat(tr["rotation"])) if "rotation" in tr else None,
                          scale=tr.get("scale"))

        for r in model.roots:
            root_children.append(convert_node(r))
        g.node(name, children=root_children, root=True,
               extras={"f3vb_source": "B3D", "f3vb_coordinate_system": model.globals.get("coordinate_system"),
                       "f3vb_coordinate_scale": model.globals.get("coordinate_scale"), "f3vb_unit_scale": self.k,
                       "f3vb_converted_lh_to_rh": self.convert})

        for clip in clips or []:
            chans = []
            for bone, tracks in clip.tracks.items():
                j = joint_by_name.get(bone)
                if j is None:
                    continue
                node = joint_nodes[j]
                if "translation" in tracks:
                    t, v = tracks["translation"]; chans.append((node, "translation", t, [self.p(x) for x in v]))
                if "rotation" in tracks:
                    t, v = tracks["rotation"]; chans.append((node, "rotation", t, [self.q(x) for x in v]))
                if "scale" in tracks:
                    t, v = tracks["scale"]; chans.append((node, "scale", t, list(v)))
            if chans:
                g.animation(clip.name, chans)
                self.stats["animations"] += 1
            else:
                self.warnings.append(f"clipe '{clip.name}' sem trilhas aproveitáveis")
            self.warnings += [f"{clip.name}: {w}" for w in clip.warnings[:3]]
        return g.to_bytes()


    # ---- cena neutra (usada pelo formato F3M do visualizador) ----------------------------------------------
    def scene(self, model: Model, name: str, texture_loader=None, joints=None, clips=None) -> dict:
        joints, clips, self.initial_placement = apply_initial_placement(joints, clips) if joints else (joints, clips, None)
        from .imagecodec import decode_bmp, decode_tga
        bone_names, aliased = resolve_bone_aliases([b.name for b in model.bones], joints)
        self.stats["bone_aliases"] = aliased
        if aliased:
            self.warnings.append(f"{name}: {len(aliased)} ossos casados por sufixo ({', '.join(f'{a}->{b}' for a, b in list(aliased.items())[:4])})")
        sc = {"name": name, "textures": [], "materials": [], "meshes": [], "joints": [], "clips": [],
              "bone_names": bone_names}
        tex_idx: dict = {}

        def texture(file_name):
            key = file_name.lower()
            if key in tex_idx:
                return tex_idx[key]
            raw = texture_loader(file_name.rsplit(".", 1)[0]) if texture_loader else None
            idx = None
            if raw is None:
                self.warnings.append(f"textura '{file_name}' não encontrada")
            else:
                try:
                    w, h, rgba = decode_bmp(raw) if raw[:2] == b"BM" else decode_tga(raw)
                    sc["textures"].append({"name": file_name.rsplit(".", 1)[0], "w": w, "h": h, "rgba": rgba})
                    idx = len(sc["textures"]) - 1
                    self.stats["textures"] += 1
                except (ImageError, ValueError) as exc:
                    self.warnings.append(f"textura '{file_name}': {exc}")
            tex_idx[key] = idx
            return idx

        mat_idx: dict = {}

        def material(m):
            key = (m.name, m.blend, tuple(m.textures[:1]), tuple(m.floats[:4]))
            if key in mat_idx:
                return mat_idx[key]
            f = m.floats if len(m.floats) >= 4 else [1, 1, 1, 1]
            mode = {"OPAQUE": 0, "ALPHABLEND": 1, "ADD": 2}.get(m.blend, 0)
            sc["materials"].append({"name": m.name or "material", "texture": texture(m.textures[0]) if m.textures else None,
                                    "color": tuple(float(x) for x in f[:4]), "alpha_mode": mode, "double_sided": 0})
            mat_idx[key] = len(sc["materials"]) - 1
            return mat_idx[key]

        bone_names = sc["bone_names"]
        for node, surf in model.surfaces():
            d = self._prep_surface(node.name or name, surf, None, bone_names)
            for mat_i, idx in d["prims"]:
                mat = surf.materials[mat_i] if mat_i < len(surf.materials) else None
                sc["meshes"].append({"name": node.name or name, "material": material(mat) if mat else None,
                                     "pos": d["pos"], "nor": d["nor"], "uv": d["uv0"], "col": d["col"] or [(1, 1, 1, 1)] * len(d["pos"]),
                                     "joints": d["joints"], "weights": d["weights"], "idx": idx})
                self.stats["triangles"] += len(idx) // 3
            self.stats["surfaces"] += 1
            self.stats["vertices"] += len(d["pos"])
        for jt in joints or []:
            sc["joints"].append({"name": jt.name, "parent": jt.parent, "t": self.p(jt.position), "q": self.q(jt.orientation),
                                 "s": tuple(jt.scale), "ibm": self.m4(jt.inverse_world)})
        self.stats["joints"] = len(sc["joints"])
        for clip in clips or []:
            chans = []
            for bone, tracks in clip.tracks.items():
                if "translation" in tracks:
                    t, v = tracks["translation"]; chans.append((bone, 0, list(t), [self.p(x) for x in v]))
                if "rotation" in tracks:
                    t, v = tracks["rotation"]; chans.append((bone, 1, list(t), [self.q(x) for x in v]))
                if "scale" in tracks:
                    t, v = tracks["scale"]; chans.append((bone, 2, list(t), [tuple(x) for x in v]))
            if chans:
                sc["clips"].append({"name": clip.name, "duration": float(clip.duration), "channels": chans})
                self.stats["animations"] += 1
        return sc


def export_b3d_to_glb(b3d_bytes: bytes, name: str, *, texture_loader=None, joints=None, clips=None,
                      unit_scale: float = 1.0, convert: bool = True, mesh_scale: float = DEFAULT_MESH_SCALE) -> tuple[bytes, dict]:
    model = read_b3d(b3d_bytes)
    ex = Exporter(unit_scale=unit_scale, convert=convert, mesh_scale=mesh_scale)
    data = ex.build(model, name, texture_loader=texture_loader, joints=joints, clips=clips)
    return data, {"stats": ex.stats, "warnings": ex.warnings}
