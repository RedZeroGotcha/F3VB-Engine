"""Extrator: grava TODOS os recursos dos .grp em original_assets/ (ver layout.py) e converte para formatos abertos.

  original_assets/RETAIL/<Pack>/<Tipo>/<Nome>.<ext>   originais, byte a byte (ex.: Critters/G3D_Model/CR_Ant.b3d)
  original_assets/CONVERTED/png/<Pack>/<Nome>.png      texturas
  original_assets/CONVERTED/glb/<Pack>/<Nome>.glb      modelos para o Blender
  original_assets/CONVERTED/f3m/models/<Nome>.f3m      modelos para o visualizador F3VB
  original_assets/CONVERTED/f3m/skeletons/<Nome>.f3m   esqueleto + animacoes ("Adicionar animacoes" no visualizador)
  original_assets/index.csv, EXTRACAO_RELATORIO.txt, LEIA-ME.txt, .gitignore
As conversoes leem de RETAIL (nao dos .grp): refazer so CONVERTED = convert_retail().
Nada disso deve ser publicado: e conteudo do jogo.
"""
from __future__ import annotations

import csv
import re
import struct
import shutil
import threading
import time
import queue
from dataclasses import dataclass, field
from pathlib import Path

from .b3d import B3DError, read_b3d
from .classify import classify
from .export import Exporter
from .f3m import write_f3m
from .gr2export import read_clips, read_skeleton
from .imagecodec import ImageError, image_to_png
from .index import ResourceIndex, RetailIndex
from .layout import CONVERTER_VERSION, AssetsLayout
from .knowledge import RESOURCE_EXT

_BAD = re.compile(r'[<>:"/\\|?*\x00-\x1f]')
_RESERVED = {"con", "prn", "aux", "nul", *(f"com{i}" for i in range(1, 10)), *(f"lpt{i}" for i in range(1, 10))}
EXT_OVERRIDE = {300: "amx", 400: "8tre", 3700: "tok"}


@dataclass
class ExtractOptions:
    raw: bool = True
    png: bool = False
    glb: bool = False
    f3m: bool = False
    maps: bool = False       # v14: .map + .rle -> CONVERTED/maps (txt, json, png)
    anims_per_model: int = 8          # 0 = todas
    skeleton_packs: bool = True       # com f3m: um arquivo esqueleto+animações por esqueleto


def safe_name(name: str, fallback: str = "sem_nome") -> str:
    s = _BAD.sub("_", name).strip(" .")
    if not s:
        s = fallback
    if s.lower().split(".")[0] in _RESERVED:
        s = "_" + s
    return s[:120]


def pick_ext(type_id: int, head: bytes, length: int) -> str:
    if type_id == 200:
        return "bmp" if classify(head, length).startswith("bmp") else "tga"
    return EXT_OVERRIDE.get(type_id) or RESOURCE_EXT.get(type_id) or "bin"


def plan(game_dir) -> dict:
    """Quantos arquivos e bytes serão gravados (sem gravar nada)."""
    with ResourceIndex(game_dir) as ix:
        total, count = 0, 0
        by_type: dict = {}
        for e in ix.rht.entries:
            pack = ix.rht.pack_label(e.pack)
            g = ix._grp(pack) if pack.lower() in ix.grps else None
            if g is None or not 0 <= e.number < g.info.n_files:
                continue
            ln = g.info.lumps[e.number].length
            total += ln
            count += 1
            t = by_type.setdefault(e.type_name, [0, 0])
            t[0] += 1
            t[1] += ln
        return {"files": count, "bytes": total, "by_type": by_type,
                "packs": sorted({ix.rht.pack_label(e.pack) for e in ix.rht.entries})}


def fmt_bytes(n: float) -> str:
    for u in ("B", "KB", "MB", "GB"):
        if n < 1024 or u == "GB":
            return f"{n:.1f} {u}" if u != "B" else f"{int(n)} B"
        n /= 1024
    return str(n)


class _Steps:
    """Contador de progresso compartilhado entre as fases (extracao -> PNG -> modelos -> esqueletos)."""

    def __init__(self, tick, total):
        self.tick, self.total, self.n = tick, total, 0

    def __call__(self, msg):
        self.n += 1
        if self.tick:
            self.tick(self.n, self.total, msg)


def anim_prefixes(name: str) -> list:
    """Prefixos de busca das animacoes de um modelo/esqueleto: o proprio nome e, para humanos, a familia (HumM_ / HumF_).
    Medido na demo: HumMNor/HumMFat/HumMWir nao tem "HumMNor_*"; as animacoes se chamam "HumM_Com_Una_Walk_01" etc."""
    out = [name + "_"]
    m = re.match(r"^(Hum[MF])", name)
    if m and m.group(1) + "_" not in out:
        out.append(m.group(1) + "_")
    return out


def _new_log() -> dict:
    return {"written": 0, "bytes": 0, "skipped": [], "failures": [], "png": 0, "glb": 0, "f3m_models": 0,
            "f3m_skeletons": 0, "maps": 0, "scripts": 0, "textures": 0, "textures_missing": [], "cancelled": False}


def _convert_steps(rows, opts) -> int:
    n = 0
    if getattr(opts, "maps", False):
        n += sum(1 for r in rows if r[2] == "MAP_Map")
    if opts.png:
        n += sum(1 for r in rows if r[2] == "TGA_Image")
    if opts.glb or opts.f3m:
        n += sum(1 for r in rows if r[2] == "G3D_Model")
        if opts.f3m and opts.skeleton_packs:
            n += sum(1 for r in rows if r[2] == "SKEL_Skeleton")
    return n


def _convert(layout: AssetsLayout, rows, opts: ExtractOptions, log: dict, step, cancelled) -> None:
    """Converte a partir de RETAIL (nao precisa dos .grp): PNG, GLB e F3M em CONVERTED."""
    conv = layout.converted
    ix = RetailIndex(layout.retail, rows)

    def texture_loader(stem):
        return ix.read_named(stem, "TGA_Image")

    if opts.png and not log["cancelled"]:
        for pack, number, type_name, name, _rel, _size in rows:
            if type_name != "TGA_Image":
                continue
            if cancelled():
                break
            step(f"PNG {name}")
            try:
                _w, _h, png = image_to_png(ix.read(pack, number))
                dest = conv / "png" / safe_name(pack) / f"{safe_name(name)}.png"
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(png)
                log["png"] += 1
            except (ImageError, ValueError, KeyError, OSError) as exc:
                log["failures"].append(f"PNG {pack}/{name}: {exc}")

    if (opts.glb or opts.f3m) and not log["cancelled"]:
        skeleton_cache: dict = {}

        def get_skeleton(name):
            if name in skeleton_cache:
                return skeleton_cache[name]
            raw = ix.read_named(name, "SKEL_Skeleton")
            res = (None, None)
            if raw is not None:
                try:
                    res = (read_skeleton(raw)[0], None)
                except Exception as exc:
                    res = (None, str(exc))
            skeleton_cache[name] = res
            return res

        def get_clips(name, joints, limit="default"):
            """Animacoes de um modelo/esqueleto. Cada animal tem as suas ("CR_Dog_*"); os HUMANOS compartilham as da familia
            ("HumM_*" para HumMNor/HumMFat/HumMWir..., "HumF_*" para HumFStr/HumFSkl...). limit: "default" = anims_per_model;
            None = todas (usado no F3M de esqueleto, que serve para "Adicionar animacoes" no visualizador)."""
            lim = opts.anims_per_model if limit == "default" else limit
            rows_ = []
            for pre in anim_prefixes(name):
                rows_ += [r for r in ix.search("GR2_Animation", pre) if r not in rows_]
            if lim and len(rows_) > lim:
                rows_ = sorted(rows_, key=lambda r: (0 if any(k in r[0] for k in ("Idle_01", "Walk_01", "Run_01")) else 1, r[0]))[:lim]
            clips = []
            names = {j.name for j in joints}
            for nm, pk, num in rows_:
                try:
                    clips += read_clips(ix.read(pk, num), names, clip_name=nm)
                except Exception as exc:
                    log["failures"].append(f"animação {nm}: {type(exc).__name__}: {exc}")
            return clips

        for pack, number, type_name, name, _rel, _size in rows:
            if type_name != "G3D_Model":
                continue
            if cancelled():
                break
            step(f"modelo {name}")
            try:
                model = read_b3d(ix.read(pack, number))
            except B3DError as exc:
                log["failures"].append(f"B3D {pack}/{name}: {exc}")
                continue
            joints, _err = get_skeleton(name)
            clips = get_clips(name, joints) if joints else []
            try:
                if opts.glb:
                    glb = Exporter().build(model, name, texture_loader=texture_loader, joints=joints, clips=clips)
                    dest = conv / "glb" / safe_name(pack) / f"{safe_name(name)}.glb"
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(glb)
                    log["glb"] += 1
                if opts.f3m:
                    sc = Exporter().scene(model, name, texture_loader=texture_loader, joints=joints, clips=clips)
                    dest = conv / "f3m" / "models" / f"{safe_name(name)}.f3m"
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(write_f3m(sc))
                    log["f3m_models"] += 1
            except Exception as exc:
                log["failures"].append(f"exportar {pack}/{name}: {type(exc).__name__}: {exc}")

        if opts.f3m and opts.skeleton_packs and not log["cancelled"]:
            from .b3d import Model
            for pack, number, type_name, name, _rel, _size in rows:
                if type_name != "SKEL_Skeleton":
                    continue
                if cancelled():
                    break
                step(f"esqueleto {name}")
                joints, err = get_skeleton(name)
                if not joints:
                    if err:
                        log["failures"].append(f"esqueleto {name}: {err}")
                    continue
                try:
                    clips = get_clips(name, joints, limit=None)
                    sc = Exporter().scene(Model(), name, joints=joints, clips=clips)
                    dest = conv / "f3m" / "skeletons" / f"{safe_name(name)}.f3m"
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(write_f3m(sc))
                    log["f3m_skeletons"] += 1
                except Exception as exc:
                    log["failures"].append(f"esqueleto {name}: {type(exc).__name__}: {exc}")

    if getattr(opts, "maps", False) and not log["cancelled"]:
        from .mapreport import analyze_map
        from .mapfile import read_map
        mdir = conv / "maps"
        summary = ["RESUMO DOS MAPAS (v14)"]
        for pack, number, type_name, name, _rel, _size in rows:
            if type_name != "MAP_Map":
                continue
            if cancelled():
                break
            step(f"mapa {name}")
            try:
                data = ix.read(pack, number)
                rle_stem = read_map(data).header.get("rle", "").rsplit(".", 1)[0]
                rle = ix.read_named(rle_stem, "RLE") if rle_stem else None
                tre_stem = read_map(data).header.get("tre", "").rsplit(".", 1)[0]
                tre_raw = ix.read_named(tre_stem, "TRE_Octree") if tre_stem else None
                m, grid, info, txt, js, png, f3 = analyze_map(name, data, rle, tre_raw)
                mdir.mkdir(parents=True, exist_ok=True)
                base = safe_name(name)
                (mdir / f"{base}.txt").write_text(txt, encoding="utf-8")
                (mdir / f"{base}.json").write_text(js, encoding="utf-8")
                if png:
                    (mdir / f"{base}.png").write_bytes(png)
                if f3:
                    (mdir / f"{base}.f3map").write_bytes(f3)
                log["maps"] += 1
                summary.append(f"{name:44s} chunks {len(m.chunks):3d}  entidades {len(m.entities):3d}  nos de navegacao {len(m.nav_nodes):3d}  "
                               f"gatilhos {len(m.triggers):2d}  grade {('%dx%d' % (grid.width, grid.rows)) if grid else '-':>9s}  "
                               f"escala x{info[0]:g}  nao explicados {m.unexplained_bytes}")
            except Exception as exc:
                log["failures"].append(f"mapa {pack}/{name}: {type(exc).__name__}: {exc}")
        if log["maps"]:
            (mdir / "maps_summary.txt").write_text("\n".join(summary) + "\n", encoding="utf-8")
        if not log["cancelled"]:
            _write_scripts(ix, rows, conv, log)                       # v17: scripts ao lado dos mapas (o viewer liga entidade -> script pelo nome)
            _write_textures(ix, conv, log)                            # v18: texturas citadas pelos mapas (difusas e lightmaps) em CONVERTED/textures


SCRIPT_TYPES = ("AMX", "SMA_Script")


def _write_scripts(ix, rows, conv: Path, log: dict) -> int:
    """v17: copia os scripts (.amx, tipos AMX e SMA_Script) para CONVERTED/scripts/<nome>.amx; o F3VBMapViewer os acha pelo nome citado nas entidades do mapa."""
    sdir = conv / "scripts"
    used: set = set()
    n = 0
    for pack, number, type_name, name, _rel, _size in rows:
        if type_name not in SCRIPT_TYPES:
            continue
        try:
            base = safe_name(name)
            if base.lower() in used:                                   # mesmo nome nos dois tipos: nao sobrescrever
                base = f"{base}_{number}"
            used.add(base.lower())
            sdir.mkdir(parents=True, exist_ok=True)
            (sdir / f"{base}.amx").write_bytes(ix.read(pack, number))
            n += 1
        except Exception as exc:                                       # noqa: BLE001
            log["failures"].append(f"script {pack}/{name}: {type(exc).__name__}: {exc}")
    log["scripts"] = log.get("scripts", 0) + n
    return n


F3TEX_MAGIC = b"F3TX"


def encode_f3tex(w: int, h: int, rgba: bytes) -> bytes:
    """Textura crua para o F3VBMapViewer: "F3TX" + u32 largura + u32 altura + RGBA8 (linhas de CIMA para BAIXO, como no TGA da demo)."""
    if len(rgba) != w * h * 4:
        raise ValueError("RGBA com tamanho errado")
    return F3TEX_MAGIC + struct.pack("<II", w, h) + rgba


def _texture_stems_from_f3maps(maps_dir: Path) -> dict:
    """Nomes de textura (sem extensao) citados pelos .f3map ja convertidos: {minusculo: (nome, 'difusa'|'lightmap')}."""
    from .mapexport import read_f3map
    out: dict = {}
    for f in sorted(maps_dir.glob("*.f3map")):
        try:
            d = read_f3map(f.read_bytes())
        except Exception:                                                  # noqa: BLE001
            continue
        for name in d.get("textures", []):
            low = name.lower()
            if low.endswith(".tga"):
                out.setdefault(low[:-4], (name[:-4], "difusa"))
            elif low.endswith(".ctx"):
                out.setdefault(low[:-4], (name[:-4], "lightmap"))
    return out


def _write_textures(ix, conv: Path, log: dict, overwrite: bool = False) -> int:
    """v18: converte as texturas citadas pelos mapas (CONVERTED/textures/<nome>.f3tex). Tenta o recurso TGA_Image pelo nome sem extensao e com '.tga'."""
    from .imagecodec import ImageError, decode_tga
    stems = _texture_stems_from_f3maps(conv / "maps")
    tdir = conv / "textures"
    n = 0
    missing: list = []
    for low, (name, kind) in sorted(stems.items()):
        dest = tdir / f"{safe_name(name)}.f3tex"
        if dest.exists() and not overwrite:
            n += 1
            continue
        data = ix.read_named(name, "TGA_Image") or ix.read_named(name + ".tga", "TGA_Image")
        if data is None:
            missing.append(f"{name} ({kind})")
            continue
        try:
            w, h, rgba = decode_tga(data)
            tdir.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(encode_f3tex(w, h, rgba))
            n += 1
        except (ImageError, ValueError, struct.error) as exc:
            log["failures"].append(f"textura {name}: {type(exc).__name__}: {exc}")
            missing.append(f"{name} ({kind}; nao decodificou)")
    log["textures"] = log.get("textures", 0) + n
    log.setdefault("textures_missing", []).extend(missing)
    if stems:
        tdir.mkdir(parents=True, exist_ok=True)
        lines = [f"TEXTURAS CITADAS PELOS MAPAS: {len(stems)}; convertidas (ou ja existiam): {n}; nao encontradas/falharam: {len(missing)}", ""] + [f"  faltando: {m}" for m in missing]
        (tdir / "textures_summary.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return n


def export_textures(out_dir=None, overwrite: bool = False) -> dict:
    """So as texturas (usa os .f3map ja convertidos para saber quais): py -3 -m f3vb export-textures"""
    layout = AssetsLayout(out_dir).ensure()
    if not layout.index_csv.exists():
        raise FileNotFoundError(f"{layout.index_csv} nao existe: rode extract-all antes (precisa da pasta do jogo uma vez).")
    if not (layout.converted / "maps").exists():
        raise FileNotFoundError("CONVERTED/maps nao existe: rode antes  py -3 -m f3vb convert-retail --maps")
    log = _new_log()
    ix = RetailIndex.from_csv(layout.root)
    _write_textures(ix, layout.converted, log, overwrite)
    return log


def export_scripts(out_dir=None) -> dict:
    """So os scripts (rapido), sem refazer mapas/PNG/F3M: py -3 -m f3vb export-scripts"""
    layout = AssetsLayout(out_dir).ensure()
    if not layout.index_csv.exists():
        raise FileNotFoundError(f"{layout.index_csv} nao existe: rode extract-all antes (precisa da pasta do jogo uma vez).")
    log = _new_log()
    ix = RetailIndex.from_csv(layout.root)
    _write_scripts(ix, ix.rows, layout.converted, log)
    return log


def failure_summary(failures: list) -> list:
    """Agrupa as falhas por CAUSA (mensagem sem os numeros) e por pack, para dar para ler 2.400 falhas de uma vez."""
    cause: dict = {}
    pack: dict = {}
    for f in failures:
        m = re.match(r"(\S+) ([^/:]+)/[^:]*: (.*)", f)
        kind, pk, msg = (m.group(1), m.group(2), m.group(3)) if m else ("?", "?", f)
        key = (kind, re.sub(r"0x[0-9a-fA-F]+|-?\d+", lambda mm: "0xN" if mm.group(0).startswith("0x") else "N", msg)[:100])
        cause[key] = cause.get(key, 0) + 1
        pack[(kind, pk)] = pack.get((kind, pk), 0) + 1
    out = ["RESUMO DAS FALHAS POR CAUSA (numeros trocados por N):"]
    out += [f"  {n:5d}  {k}: {msg}" for (k, msg), n in sorted(cause.items(), key=lambda x: -x[1])[:25]]
    out += ["", "RESUMO DAS FALHAS POR PACK:"]
    out += [f"  {n:5d}  {k} em {pk}" for (k, pk), n in sorted(pack.items(), key=lambda x: -x[1])[:25]]
    return out


def _write_report(layout: AssetsLayout, log: dict, mode: str) -> None:
    lines = ["RELATORIO DA EXTRACAO F3VB", f"modo: {mode}", f"pasta: {layout.root}",
             f"  RETAIL (originais):     {layout.retail}", f"  CONVERTED (convertidos): {layout.converted}",
             f"arquivos originais gravados: {log['written']}  ({fmt_bytes(log['bytes'])})",
             f"PNG: {log['png']}  GLB: {log['glb']}  F3M modelos: {log['f3m_models']}  F3M esqueletos: {log['f3m_skeletons']}  mapas: {log['maps']}  scripts: {log.get('scripts', 0)}  texturas: {log.get('textures', 0)}",
             f"versao do conversor: {CONVERTER_VERSION}",
             f"cancelado: {log['cancelled']}  tempo: {log['seconds']} s", f"falhas: {len(log['failures'])}  ignorados: {len(log['skipped'])}", ""]
    if log["failures"]:
        lines += failure_summary(log["failures"]) + [""]
        (layout.root / "FALHAS_COMPLETO.txt").write_text("\n".join(log["failures"]) + "\n", encoding="utf-8")
        lines += ["(a lista completa, uma falha por linha, esta em FALHAS_COMPLETO.txt)", ""]
    lines += ["FALHAS (primeiras 200):"] + log["failures"][:200] + ["", "IGNORADOS:"] + log["skipped"][:50]
    layout.report.write_text("\n".join(lines) + "\n", encoding="utf-8")


def extract_all(game_dir, out_dir=None, opts: ExtractOptions | None = None, progress=None, cancel=None) -> dict:
    """Extrai os .grp para <out>/RETAIL e converte (opcional) para <out>/CONVERTED. out padrao: ./original_assets."""
    opts = opts or ExtractOptions()
    if not opts.raw:                          # sem regravar os originais: converte a partir do RETAIL ja existente
        return convert_retail(out_dir, opts, progress, cancel)
    layout = AssetsLayout(out_dir).ensure()
    t0 = time.time()
    log = _new_log()
    used: set = set()
    rows = []

    def cancelled():
        if cancel and cancel():
            log["cancelled"] = True
            return True
        return False

    with ResourceIndex(game_dir) as ix:
        entries = sorted(ix.rht.entries, key=lambda e: (ix.rht.pack_label(e.pack).lower(), e.number))
        name_of = lambda e: ix.rht.resource_name(e.offset) or f"recurso_{e.number}"
        planned_rows = [(ix.rht.pack_label(e.pack), e.number, e.type_name, name_of(e), "", 0) for e in entries]
        step = _Steps(progress, len(entries) + _convert_steps(planned_rows, opts))
        for e in entries:
            if cancelled():
                break
            pack = ix.rht.pack_label(e.pack)
            name = name_of(e)
            step(f"{pack}/{name}")
            if pack.lower() not in ix.grps:
                log["skipped"].append(f"{pack}#{e.number}: .grp ausente")
                continue
            try:
                data = ix.read(pack, e.number)
            except Exception as exc:
                log["failures"].append(f"{pack}#{e.number} {name}: {type(exc).__name__}: {exc}")
                continue
            ext = pick_ext(e.type, data[:64], len(data))
            rel = Path(safe_name(pack)) / e.type_name / f"{safe_name(name)}.{ext}"
            key = str(rel).lower()
            n = 1
            while key in used:
                n += 1
                rel = Path(safe_name(pack)) / e.type_name / f"{safe_name(name)}_{n}.{ext}"
                key = str(rel).lower()
            used.add(key)
            dest = layout.retail / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
            if dest.stat().st_size != len(data):
                log["failures"].append(f"{rel}: tamanho gravado diferente do original")
            log["written"] += 1
            log["bytes"] += len(data)
            rows.append((pack, e.number, e.type_name, name, str(rel).replace("\\", "/"), len(data)))

    _write_index(layout, rows)
    if not log["cancelled"]:
        _convert(layout, rows, opts, log, step, cancelled)
        if opts.png or opts.glb or opts.f3m or getattr(opts, 'maps', False):
            layout.write_converter_info()
    log["seconds"] = round(time.time() - t0, 1)
    _write_report(layout, log, "extrair + converter" if (opts.png or opts.glb or opts.f3m) else "extrair")
    return log


def _write_index(layout: AssetsLayout, rows) -> None:
    with open(layout.index_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["pack", "numero", "tipo", "nome", "caminho", "tamanho"])
        w.writerows(rows)


def convert_retail(out_dir=None, opts: ExtractOptions | None = None, progress=None, cancel=None) -> dict:
    """Refaz CONVERTED a partir de RETAIL (usa index.csv). Nao precisa da pasta do jogo nem dos .grp."""
    opts = opts or ExtractOptions(png=True, f3m=True)
    layout = AssetsLayout(out_dir).ensure()
    if not layout.index_csv.exists():
        raise FileNotFoundError(f"{layout.index_csv} nao existe: rode extract-all antes (precisa da pasta do jogo uma vez).")
    t0 = time.time()
    log = _new_log()
    ix = RetailIndex.from_csv(layout.root)
    rows = ix.rows

    def cancelled():
        if cancel and cancel():
            log["cancelled"] = True
            return True
        return False

    step = _Steps(progress, _convert_steps(rows, opts))
    _convert(layout, rows, opts, log, step, cancelled)
    if (opts.png or opts.glb or opts.f3m or getattr(opts, 'maps', False)) and not log["cancelled"]:
        layout.write_converter_info()
    log["seconds"] = round(time.time() - t0, 1)
    _write_report(layout, log, "converter a partir de RETAIL")
    return log


class ExtractJob:
    """Executa a extração numa thread e expõe mensagens por fila (a GUI só faz polling)."""

    def __init__(self, game_dir, out_dir, opts: ExtractOptions):
        self.q: queue.Queue = queue.Queue()
        self._cancel = threading.Event()
        self.result: dict | None = None
        self.error: str | None = None
        self.thread = threading.Thread(target=self._run, args=(game_dir, out_dir, opts), daemon=True)

    def start(self):
        self.thread.start()

    def cancel(self):
        self._cancel.set()

    def alive(self) -> bool:
        return self.thread.is_alive()

    def _run(self, game_dir, out_dir, opts):
        try:
            self.result = extract_all(game_dir, out_dir, opts,
                                      progress=lambda d, t, m: self.q.put(("progress", d, t, m)),
                                      cancel=self._cancel.is_set)
        except Exception as exc:
            self.error = f"{type(exc).__name__}: {exc}"
        finally:
            self.q.put(("done",))
