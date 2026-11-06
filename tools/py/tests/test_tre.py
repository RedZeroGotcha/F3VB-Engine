"""v15/v16: leitor de .8tre (geometria 3D em octree, lotes de indices) e exportacao .f3map (v1 e v2). Dados SINTETICOS."""
import contextlib
import io
import struct
import tempfile
import unittest
from pathlib import Path

from f3vb.mapexport import F3MapError, build_f3map, grid_origin, read_f3map, world_to_cell
from f3vb.mapfile import read_map
from f3vb.mapreport import analyze_map
from f3vb.rle import read_rle
from f3vb.tre import TreError, _Lim, read_tre, split_batches
from tests.test_maps import make_map, make_rle


def chunk(tag: str, payload: bytes) -> bytes:
    return tag.encode() + struct.pack("<I", len(payload)) + payload


def stream(tok: str, data: bytes) -> bytes:
    return tok.encode() + struct.pack("<I", len(data)) + data


VERTS = [(0.0, 0.0, 0.0), (2.0, 0.0, 0.0), (0.0, 0.0, 2.0), (2.0, 0.0, 2.0)]


def make_tre(flag_index=True, with_lights=True, idx=None, ntris=2, nmat=1, textures=("a.tga", "lm_0.ctx"), head16=None) -> bytes:
    head = chunk("HEAD", struct.pack("<4I", 2, 12, 4, 5))
    lgtr = struct.pack("<2I3f3f3f2f3f", 1, 0, 1, .5, 1, -6, 20, -7, .58, -.57, .58, 2, 10, 1, .4, .5)
    lgtd = chunk("LGTD", chunk("INFO", struct.pack("<I", 1)) + chunk("LGTR", lgtr)) if with_lights else b""
    lvld = chunk("LVLD", chunk("INFO", struct.pack("<2I2f", 84, 84, .5, .5)) + chunk("LVL0", bytes(16)))
    matr = struct.pack("<I", 28) + b"SK_BASE".ljust(80, b"\0") + bytes([127, 127, 127, 255]) + b"OPAQUE".ljust(16, b"\0") + b"METAL".ljust(16, b"\0")
    matr += bytes(196 - len(matr) - 4) + struct.pack("<I", 1)
    matrs = b"".join(chunk("MATR", matr.replace(b"SK_BASE", b"SK_MAT%d" % i) if i else matr) for i in range(nmat))
    matd = chunk("MATD", chunk("INFO", struct.pack("<I", nmat)) + matrs)
    txtd = chunk("TXTD", struct.pack("<II", len(textures), 3) + b"".join(chunk("TXTR", x.encode() + b"\0") for x in textures))
    vtxb = struct.pack("<I", 4)
    vtxb += stream("XYZ ", b"".join(struct.pack("<3f", *v) for v in VERTS))
    vtxb += stream("NRML", b"".join(struct.pack("<3f", 0, 1, 0) for _ in VERTS))
    vtxb += stream("DIFU", bytes([127, 127, 127, 255]) * 4)
    vtxb += stream("TXUV", b"".join(struct.pack("<2f", u, v) for u, v in ((0, 0), (1, 0), (0, 1), (1, 1))))
    vtxb += stream("LMUV", bytes(32))
    vtxd = chunk("VTXD", struct.pack("<I", 1) + chunk("VTXB", vtxb))
    node = chunk("NODE", chunk("INFO", struct.pack("<6f", 0, -1, 0, 2, 1, 2) + struct.pack("<2I", 0, 2) + struct.pack("<4I", 0, 0, 0, 0) + bytes(16)))
    if idx is None:
        idx = [0, 1, 2, 2, 1, 3 | (0x8000 if flag_index else 0)]
    hdr = struct.pack("<6H", *head16) if head16 is not None else struct.pack("<3I", 1, 2, 3)       # v19: head16 = cabecalho real do 1o lote
    idxs = hdr + struct.pack("<%dH" % len(idx), *idx)
    idxd = chunk("IDXD", chunk("IDXS", idxs) + chunk("FLGS", bytes([1] * ntris)))
    leaf_body = chunk("INFO", struct.pack("<6f2I", 0, -1, 0, 2, 1, 2, 0, 1)) + idxd
    leaf = b"LEAF" + struct.pack("<I", len(leaf_body) - 4) + leaf_body          # tamanho declarado: 4 a menos (medido)
    real = node + leaf
    tree = b"TREE" + struct.pack("<I", len(real) - 4) + real
    body = head + lgtd + lvld + matd + txtd + vtxd + tree
    return b"8TRE" + struct.pack("<I", len(body) - 4) + body


class TreTests(unittest.TestCase):
    def test_reads_every_section(self):
        t = read_tre(make_tre())
        self.assertEqual(t.head, (2, 12, 4, 5))
        self.assertEqual(t.level_info, (84, 84, 0.5, 0.5))
        self.assertEqual(len(t.lights), 1)
        self.assertEqual(t.lights[0]["type"], 1)
        self.assertAlmostEqual(t.lights[0]["pos"][1], 20.0)
        self.assertEqual(t.materials[0]["name"], "SK_BASE")
        self.assertEqual((t.materials[0]["blend"], t.materials[0]["surface"], t.materials[0]["color"]), ("OPAQUE", "METAL", [127, 127, 127, 255]))
        self.assertEqual(t.textures, ["a.tga", "lm_0.ctx"])
        self.assertEqual(len(t.blocks), 1)
        b = t.blocks[0]
        self.assertEqual((b.count, len(b.xyz), len(b.normals), len(b.colors), len(b.uv), len(b.lightmap_uv)), (4, 12, 12, 16, 8, 8))
        self.assertEqual([n.kind for n in t.nodes], ["NODE", "LEAF"])
        self.assertEqual(t.nodes[1].block, 0)
        self.assertEqual(t.nodes[1].bbox, (0.0, -1.0, 0.0, 2.0, 1.0, 2.0))
        self.assertEqual(t.nodes[1].flags, bytes([1, 1]))
        self.assertEqual(t.bounds(), (0.0, 0.0, 0.0, 2.0, 0.0, 2.0))

    def test_triangles_and_flag_bits(self):
        tris, st = read_tre(make_tre()).triangles()
        self.assertEqual(tris, [(0, (0, 1, 2)), (0, (2, 1, 3))])           # 3 | 0x8000 e corrigido para 3 pela mascara
        self.assertEqual(st["masked"], 1)
        self.assertEqual(st["bad"], 0)
        tris2, st2 = read_tre(make_tre(flag_index=False)).triangles()
        self.assertEqual(tris2, tris)
        self.assertEqual(st2["masked"], 0)

    def test_without_lights_and_errors(self):
        self.assertEqual(read_tre(make_tre(with_lights=False)).lights, [])
        with self.assertRaises(TreError):
            read_tre(b"XXXX" + bytes(40))
        good = make_tre()
        with self.assertRaises(TreError):
            read_tre(good[:-10])                                           # arvore cortada
        with self.assertRaises(TreError):
            read_tre(good + b"\x00\x00\x00\x00")                           # sobra no fim
        bad = bytearray(good)
        struct.pack_into("<I", bad, 4, len(good) * 2)                      # tamanho da raiz absurdo
        with self.assertRaises(TreError):
            read_tre(bytes(bad))

    def test_leaf_declared_size_is_four_short(self):
        """Duas folhas em sequencia: so lemos certo se a proxima ficha comeca em inicio + 8 + declarado + 4."""
        base = make_tre()
        n = struct.unpack_from("<I", base, 4)[0]
        body = base[8:]
        tree_at = body.index(b"TREE")
        tree_real = body[tree_at + 8:]
        leaf_at = tree_real.index(b"LEAF")
        leaf = tree_real[leaf_at:]
        new_tree_real = tree_real + leaf
        new_body = body[:tree_at] + b"TREE" + struct.pack("<I", len(new_tree_real) - 8) + new_tree_real
        data = b"8TRE" + struct.pack("<I", len(new_body) - 8) + new_body
        t = read_tre(data)
        self.assertEqual([x.kind for x in t.nodes], ["NODE", "LEAF", "LEAF"])
        self.assertEqual(len(t.triangles()[0]), 4)


# ------------------------------------------------------------------------------------------------ lotes de indices (v16)
def sub(k, idx):
    """sublote = [k][C] indices[C] (+ enchimento se C impar)."""
    return [k, len(idx)] + list(idx) + ([0] if len(idx) & 1 else [])


def page(p, *subs):
    body = [x for s_ in subs for x in s_]
    return [0x4000 | p, len(body)] + body


def group(mat, *pages):
    body = [x for p_ in pages for x in p_]
    return ([0x8000 | mat, len(body)] if mat is not None else []) + body


def batches_of(u, ntris, mats=3, ctx=3, tga=2):
    out, ok = split_batches(tuple(u), ntris, _Lim(mats, ctx, tga))
    return [(b.material, b.page, b.texture, tuple(b.indices)) for b in out], ok


class TreBatchTests(unittest.TestCase):
    def test_full_grammar(self):
        """Lote inicial + material + paginas + sublotes (inclui C impar com enchimento), grupo sem token de material e sublote nu."""
        u = [0, 1, 2, 2, 1, 3]
        u += group(2, page(1, sub(1, [0, 1, 2]), sub(2, [1, 2, 3, 3, 2, 1])), page(2, sub(0, [0, 2, 3])))
        u += group(None, page(3, sub(1, [3, 2, 1])))
        u += sub(2, [0, 1, 2])
        got, ok = batches_of(u, 8)
        self.assertTrue(ok)
        self.assertEqual(got, [(0, 0, 0, (0, 1, 2, 2, 1, 3)),
                               (2, 1, 1, (0, 1, 2)), (2, 1, 2, (1, 2, 3, 3, 2, 1)), (2, 2, 0, (0, 2, 3)),
                               (2, 3, 1, (3, 2, 1)),                      # sem token de material: continua o anterior (2)
                               (2, 3, 2, (0, 1, 2))])                     # sublote nu: herda material e pagina

    def test_odd_initial_batch_has_padding(self):
        u = [0, 1, 2, 0]                                                   # L0 = 3 indices + 1 u16 de enchimento
        u += group(1, page(2, sub(1, [2, 1, 0])))
        got, ok = batches_of(u, 2)
        self.assertTrue(ok)
        self.assertEqual(got, [(0, 0, 0, (0, 1, 2)), (1, 2, 1, (2, 1, 0))])

    def test_header_bits_in_big_blocks_are_indices(self):
        """Blocos com mais de 16383 vertices: 0x4000/0x8000 em indices NAO podem virar cabecalho."""
        u = [16385, 16386, 16387, 16387, 16386, 16388]
        got, ok = batches_of(u, 2)
        self.assertTrue(ok)
        self.assertEqual(got, [(0, 0, 0, tuple(u))])
        u2 = [0x8001, 0x8002, 0x8003, 0x4001, 0x4002, 0x4003]
        got2, ok2 = batches_of(u2, 2, mats=1, ctx=1, tga=0)               # material 1 nao existe (mats=1): continua sendo indice
        self.assertTrue(ok2)
        self.assertEqual(got2, [(0, 0, 0, tuple(u2))])

    def test_indices_mixed_with_real_headers(self):
        u = [0x4001, 0x8001, 0x4002]                                       # lote inicial cheio de bits altos
        u += [0] if len(u) & 1 else []
        u += group(1, page(1, sub(1, [0x4003, 5, 0x8004])))
        got, ok = batches_of(u, 2)
        self.assertTrue(ok)
        self.assertEqual(got[0], (0, 0, 0, (0x4001, 0x8001, 0x4002)))
        self.assertEqual(got[1], (1, 1, 1, (0x4003, 5, 0x8004)))

    def test_unclosed_stream_is_reported(self):
        u = [0, 1, 2, 2, 1, 3, 0x8001, 100, 0x4001, 3, 1, 3, 0, 1, 2]       # B/A prometem mais u16 do que ha
        got, ok = batches_of(u, 2)
        self.assertFalse(ok)
        got2, ok2 = batches_of([0, 1, 2, 2, 1, 3], 5)                       # total de FLGS (5 triangulos) nao bate: nao fecha
        self.assertFalse(ok2)

    def test_total_must_match_flags(self):
        u = [0, 1, 2, 0] + group(1, page(1, sub(1, [0, 1, 2])))          # L0 = 3 (impar): 1 u16 de enchimento
        self.assertTrue(batches_of(u, 2)[1])
        self.assertFalse(batches_of(u, 1)[1])                              # FLGS diz 1 triangulo, mas o fluxo declara 3 + 3: nao fecha

    def test_read_tre_end_to_end(self):
        u = [0, 1, 2, 2, 1, 3] + group(1, page(2, sub(2, [0, 1, 3])))
        data = make_tre(idx=u, ntris=3, nmat=3, textures=("a.tga", "b.tga", "lm_0.ctx", "lm_1.ctx"))
        t = read_tre(data)
        self.assertEqual([m["name"] for m in t.materials], ["SK_BASE", "SK_MAT1", "SK_MAT2"])
        leaf = t.nodes[1]
        self.assertTrue(leaf.parsed)
        self.assertEqual([(b.material, b.page, b.texture, len(b.indices)) for b in leaf.batches], [(0, 0, 0, 6), (1, 2, 2, 3)])
        tris, _ = t.triangles_ex()
        self.assertEqual(tris, [(0, (0, 1, 2), 0, 0, 0), (0, (2, 1, 3), 0, 0, 0), (0, (0, 1, 3), 1, 2, 2)])
        self.assertEqual(t.triangles()[0], [(0, (0, 1, 2)), (0, (2, 1, 3)), (0, (0, 1, 3))])
        st = t.batch_stats()
        self.assertEqual((st["materials"], st["pages"], st["textures"], st["nodes"], st["unparsed"]), ({0: 2, 1: 1}, {0: 2, 2: 1}, {0: 2, 2: 1}, 1, 0))

    def test_read_tre_unparsed_node_keeps_valid_part(self):
        u = [0, 1, 2, 2, 1, 3, 0x8001, 100]
        t = read_tre(make_tre(idx=u, ntris=2, nmat=3))
        leaf = t.nodes[1]
        self.assertFalse(leaf.parsed)
        self.assertEqual(t.batch_stats()["unparsed"], 1)
        self.assertEqual(len(t.triangles_ex()[0]), 2)                      # o lote inicial ainda desenha


    def test_tre_check_command_on_a_folder(self):
        from f3vb.cli import main
        u = [0, 1, 2, 2, 1, 3] + group(1, page(2, sub(2, [0, 1, 3])))
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "a.8tre").write_bytes(make_tre(idx=u, ntris=3, nmat=3, textures=("a.tga", "b.tga", "lm_0.ctx", "lm_1.ctx")))
            (Path(d) / "b.8tre").write_bytes(make_tre())
            (Path(d) / "ruim.8tre").write_bytes(b"XXXX" + bytes(40))
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                main(["tre-check", d])
        out = buf.getvalue()
        self.assertIn("'arquivos': 2", out)
        self.assertIn("'triangulos': 5", out)
        self.assertIn("'erros': 1", out)
        self.assertIn("nao_fechados': 0", out)


class FirstHeaderTests(unittest.TestCase):
    """v19: os 12 bytes iniciais do IDXS sao o cabecalho do 1o lote ([0x8000|mat][A][0x4000|pag][B][k][C]); antes eram ignorados."""
    TEX = ("a.tga", "b.tga", "lm_0.ctx", "lm_1.ctx")

    def test_initial_batch_gets_material_page_texture(self):
        # 1o lote: material 2, pagina 1, textura 2, 6 indices; depois um grupo com material 1, pagina 2, textura 1, 3 indices
        head16 = (0x8000 | 2, 2 + 2 + 6, 0x4000 | 1, 2 + 6, 2, 6)
        u = [0, 1, 2, 2, 1, 3] + group(1, page(2, sub(1, [0, 1, 3])))
        t = read_tre(make_tre(idx=u, ntris=3, nmat=3, textures=self.TEX, head16=head16))
        leaf = t.nodes[1]
        self.assertTrue(leaf.full_grammar)
        self.assertEqual([(b.material, b.page, b.texture, len(b.indices)) for b in leaf.batches], [(2, 1, 2, 6), (1, 2, 1, 3)])
        tris, _ = t.triangles_ex()
        self.assertEqual(tris[0], (0, (0, 1, 2), 2, 1, 2))
        self.assertEqual(sum(1 for tr in tris if tr[2:] == (0, 0, 0)), 0)          # nenhum triangulo "sem estado"
        self.assertEqual(t.batch_stats()["full_grammar"], 1)

    def test_page_with_several_subbatches_in_header(self):
        # B cobre 2 sublotes: [k=1][C=3] (+1 enchimento) e [k=2][C=3] (+1): o cabecalho so mostra o primeiro, o resto vem no fluxo
        head16 = (0x8000 | 0, 2 + 12, 0x4000 | 2, 12, 1, 3)
        u = [0, 1, 2, 0, 2, 3, 1, 2, 3, 0]
        t = read_tre(make_tre(idx=u, ntris=2, nmat=1, textures=self.TEX, head16=head16))
        leaf = t.nodes[1]
        self.assertTrue(leaf.full_grammar)
        self.assertEqual([(b.material, b.page, b.texture, b.indices) for b in leaf.batches], [(0, 2, 1, (0, 1, 2)), (0, 2, 2, (1, 2, 3))])

    def test_garbage_header_falls_back_to_v16_reader(self):
        t = read_tre(make_tre())                                                  # cabecalho (1, 2, 3) do gerador antigo: nao e 0x8000
        self.assertFalse(t.nodes[1].full_grammar)
        self.assertTrue(t.nodes[1].parsed)
        self.assertEqual(len(t.triangles_ex()[0]), 2)

    def test_header_with_wrong_count_falls_back(self):
        head16 = (0x8000, 2 + 2 + 9, 0x4000 | 1, 2 + 9, 1, 9)                    # C=9 mas so ha 6 indices + FLGS=2: nao fecha pela gramatica
        t = read_tre(make_tre(idx=[0, 1, 2, 2, 1, 3], ntris=2, nmat=1, textures=self.TEX, head16=head16))
        self.assertFalse(t.nodes[1].full_grammar)
        self.assertEqual(len(t.triangles_ex()[0]), 2)


class F3MapTests(unittest.TestCase):
    def test_origin_and_cell_rule(self):
        t = read_tre(make_tre())
        self.assertEqual(grid_origin(t), (0.0, 0.0))
        self.assertEqual(grid_origin(None), (0.0, 0.0))
        self.assertEqual(world_to_cell((10.0, 0.0, 6.0), (4.0, 2.0)), (12.0, 8.0))        # (10-4)/0.5, (6-2)/0.5

    def test_build_and_read_back(self):
        m = read_map(make_map())
        g = read_rle(make_rle())
        t = read_tre(make_tre())
        d = read_f3map(build_f3map("Teste", m, g, t))
        self.assertEqual(d["info"]["name"], "Teste")
        self.assertEqual(d["info"]["cell"], 0.5)
        self.assertEqual(d["info"]["origin"], [0.0, 0.0])
        self.assertEqual((d["grid"]["width"], d["grid"]["rows"], len(d["grid"]["cells"])), (30, 26, 30 * 26))
        self.assertEqual(len(d["geom"]["blocks"]), 1)
        self.assertEqual(d["geom"]["blocks"][0]["n"], 4)
        self.assertEqual(d["geom"]["lists"][0]["indices"], (0, 1, 2, 2, 1, 3))
        self.assertEqual(d["ents"][0]["prototype"], "Critters_Npc.CRT")
        self.assertEqual(d["ents"][0]["script"], "Npc.amx")
        self.assertEqual(d["ents"][0]["pos"], [10.0, 0.0, 20.0])
        self.assertEqual(d["nav"][0]["links"], [1])
        self.assertEqual(len(d["entry"][0]["positions"]), 6)
        self.assertEqual([x["kind"] for x in d["triggers"]], [2, 1])
        self.assertEqual(d["triggers"][0]["target"], "Other_Map")
        self.assertEqual(d["triggers"][0]["entry"], 2)
        self.assertEqual(len(d["triggers"][0]["points"]), 4)
        self.assertEqual(d["paths"][0]["name"], "Waypoint01")
        self.assertEqual(d["sounds"][0]["sound"], "AMB_Wind.psf")
        self.assertEqual(d["markers"][0]["text_id"], 1906)
        self.assertEqual(d["effects"][0]["effect"], "FX_Dust.veg")
        self.assertEqual(d["water"][0]["texture"], "W_0.TGA")
        self.assertEqual(len(d["lights"]), 1)

    def test_v2_lists_materials_and_textures(self):
        u = [0, 1, 2, 2, 1, 3] + group(1, page(2, sub(2, [0, 1, 3])))
        tre = read_tre(make_tre(idx=u, ntris=3, nmat=3, textures=("a.tga", "b.tga", "lm_0.ctx", "lm_1.ctx")))
        d = read_f3map(build_f3map("Lotes", read_map(make_map()), None, tre, version=2))
        self.assertEqual(d["version"], 2)
        self.assertNotIn("uv", d["geom"]["blocks"][0])
        self.assertEqual([(x["block"], x["material"], x["page"], x["texture"], len(x["indices"])) for x in d["geom"]["lists"]], [(0, 0, 0, 0, 6), (0, 1, 2, 2, 3)])
        self.assertEqual([m["name"] for m in d["materials"]], ["SK_BASE", "SK_MAT1", "SK_MAT2"])
        self.assertEqual((d["materials"][0]["blend"], d["materials"][0]["surface"], d["materials"][0]["color"]), ("OPAQUE", "METAL", [127, 127, 127, 255]))
        self.assertEqual(d["textures"], ["a.tga", "b.tga", "lm_0.ctx", "lm_1.ctx"])

    def test_v3_has_uvs_per_vertex(self):
        tre = read_tre(make_tre())
        d = read_f3map(build_f3map("V3", read_map(make_map()), None, tre))               # v3 e o padrao
        self.assertEqual(d["version"], 3)
        b = d["geom"]["blocks"][0]
        self.assertEqual(len(b["uv"]), 2 * b["n"])
        self.assertEqual(len(b["lmuv"]), 2 * b["n"])
        self.assertEqual(b["uv"], tuple(tre.blocks[0].uv) if tre.blocks[0].uv else (0.0,) * (2 * b["n"]))
        self.assertEqual(b["lmuv"], tuple(tre.blocks[0].lightmap_uv) if tre.blocks[0].lightmap_uv else (0.0,) * (2 * b["n"]))
        self.assertEqual(d["textures"], ["a.tga", "lm_0.ctx"])

    def test_v1_is_still_written_and_read(self):
        u = [0, 1, 2, 2, 1, 3] + group(1, page(2, sub(2, [0, 1, 3])))
        tre = read_tre(make_tre(idx=u, ntris=3, nmat=3, textures=("a.tga", "b.tga", "lm_0.ctx", "lm_1.ctx")))
        d = read_f3map(build_f3map("V1", read_map(make_map()), None, tre, version=1))
        self.assertEqual(d["version"], 1)
        self.assertEqual(len(d["geom"]["lists"]), 1)                       # v1 agrupa so por bloco
        self.assertEqual(len(d["geom"]["lists"][0]["indices"]), 9)
        self.assertEqual(d["geom"]["lists"][0]["material"], 0)
        self.assertNotIn("materials", d)
        with self.assertRaises(F3MapError):
            build_f3map("X", read_map(make_map()), None, None, version=4)

    def test_without_tre_or_grid(self):
        d = read_f3map(build_f3map("So_mapa", read_map(make_map()), None, None))
        self.assertNotIn("grid", d)
        self.assertNotIn("geom", d)
        self.assertEqual(len(d["ents"]), 1)

    def test_reader_rejects_garbage(self):
        with self.assertRaises(F3MapError):
            read_f3map(b"XXXX" + bytes(20))
        good = build_f3map("X", read_map(make_map()), read_rle(make_rle()), read_tre(make_tre()))
        with self.assertRaises(F3MapError):
            read_f3map(good[:-9])

    def test_analyze_map_with_tre(self):
        m, g, info, txt, js, png, f3 = analyze_map("Teste", make_map(), make_rle(), make_tre())
        self.assertIn("[GEOMETRIA .8tre]", txt)
        self.assertIn("celula = (posicao - origem) / 0,5", txt)
        self.assertEqual(info[0], 2.0)
        self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(f3[:4], b"F3MP")


if __name__ == "__main__":
    unittest.main()
