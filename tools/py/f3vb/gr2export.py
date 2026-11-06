"""Esqueletos e clipes de animação de arquivos GR2 (via parser vendorizado), prontos para o exportador GLB."""
from __future__ import annotations

from dataclasses import dataclass, field

from .gr2tool import _api

IGNORED_GROUPS = {"light", "camera"}


@dataclass
class Joint:
    name: str
    parent: int                       # -1 = raiz
    position: tuple
    orientation: tuple                # x, y, z, w
    scale: tuple
    inverse_world: tuple              # 16 floats, como no GR2 (row-vector, translação na última linha)


@dataclass
class Clip:
    name: str
    duration: float
    # nome do osso -> {"translation": (times, values), "rotation": (...), "scale": (...)}
    tracks: dict = field(default_factory=dict)
    warnings: list = field(default_factory=list)


def read_skeleton(data: bytes, prefer: str | None = None):
    """Devolve (lista de Joint, nome) do primeiro esqueleto "de verdade" (ignora Light/Camera)."""
    file, fixup, _geom, skeleton, _anim = _api()
    skels = skeleton.extract_skeletons(fixup.load_sections(file.read_gr2_bytes(data)))
    cands = [s for s in skels if s.name.lower() not in IGNORED_GROUPS and len(s.bones) > 1]
    if prefer:
        cands = sorted(cands, key=lambda s: s.name != prefer)
    if not cands:
        return [], None
    s = cands[0]
    joints = []
    for b in s.bones:
        ss = tuple(b.transform.scale_shear)
        scale = (ss[0], ss[4], ss[8]) if len(ss) >= 9 else (1.0, 1.0, 1.0)
        parent = b.parent_index if b.parent_index not in (b.index,) and b.parent_index >= 0 else -1
        joints.append(Joint(b.name, parent, tuple(b.transform.position), tuple(b.transform.orientation), scale,
                            tuple(b.inverse_world_transform)))
    return joints, s.name


def _curve_keys(curve, duration: float, step: float, dim: int):
    """(times, values) amostrados; curva constante vira 1 chave em t=0."""
    _api_mod = _api()[4]
    if curve is None:
        return None
    if curve.control_values:
        ts, samples = _api_mod.sample_curve_values(curve, duration, step)
        if samples:
            times = [min(duration, i * ts) for i in range(len(samples))]
            vals = [tuple(s[:dim]) if len(s) >= dim else None for s in samples]
            if all(v is not None for v in vals):
                return times, vals
    if curve.sample_value and len(curve.sample_value) >= dim:
        return [0.0], [tuple(curve.sample_value[:dim])]
    return None


def read_clips(data: bytes, bone_names: set[str], clip_name: str | None = None) -> list[Clip]:
    file, fixup, _geom, _skel, animation = _api()
    aset = animation.extract_animation_set(fixup.load_sections(file.read_gr2_bytes(data)))
    groups = {g.name: g for g in aset.track_groups}
    clips = []
    for an in aset.animations:
        clip = Clip(name=clip_name or str(an.name).replace("\\", "/").split("/")[-1].rsplit(".", 1)[0],
                    duration=float(an.duration))
        step = float(an.time_step) if an.time_step and an.time_step > 0 else 1 / 30
        names = [n for n in an.track_group_names if n.lower() not in IGNORED_GROUPS] or \
                [n for n in groups if n.lower() not in IGNORED_GROUPS]
        for gname in names:
            g = groups.get(gname)
            if g is None:
                continue
            for tr in g.transform_tracks:
                if tr.name not in bone_names:
                    clip.warnings.append(f"trilha '{tr.name}' sem osso correspondente")
                    continue
                entry = {}
                try:
                    q = _curve_keys(tr.orientation, an.duration, step, 4)
                    p = _curve_keys(tr.position, an.duration, step, 3)
                    sc = None
                    if tr.scale_shear is not None:
                        dim = tr.scale_shear.dimension
                        raw = _curve_keys(tr.scale_shear, an.duration, step, dim if dim in (3, 9) else 3)
                        if raw:
                            times, vals = raw
                            sc = (times, [(v[0], v[4], v[8]) if len(v) == 9 else tuple(v[:3]) for v in vals])
                    if q:
                        entry["rotation"] = q
                    if p:
                        entry["translation"] = p
                    if sc:
                        entry["scale"] = sc
                except Exception as exc:                      # uma trilha ruim não derruba o clipe
                    clip.warnings.append(f"trilha '{tr.name}': {type(exc).__name__}: {exc}")
                if entry:
                    clip.tracks[tr.name] = entry
        clips.append(clip)
    return clips
