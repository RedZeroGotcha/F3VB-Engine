"""Extração e amostragem de lumps de um .grp, com manifesto (sha256, classe, sonda)."""
from __future__ import annotations

import hashlib
import json
import zipfile
from pathlib import Path

from .classify import classify
from .gr2tool import gr2_header
from .grp import GrpFile
from .probe import hexdump
from .probes import probe_amx, probe_b3d, probe_bmp, probe_tga

SAMPLE_README = """Amostra extraída para análise técnica de formatos (interoperabilidade/preservação).
manifest.txt: índice, offset, tamanho, sha256 e sonda de cabeçalho de cada arquivo.
hexdump_*.txt: primeiros 256 bytes de cada arquivo (texto puro).
Arquivos binários (.gr2/.b3d/.tga...) só vêm se você NÃO usou --so-texto.
Não redistribua e não commite estes arquivos.
"""


def manifest_text(manifest: dict) -> str:
    L = [f"MANIFESTO  grp={manifest['source_grp']}  arquivos={manifest['count']}"]
    if manifest.get("strategy"):
        L.append(f"estratégia={manifest['strategy']}")
    for e in manifest["files"]:
        L += ["", f"[{e['file']}]", f"  lump={e['lump']} offset={e['offset']} tamanho={e['length']} classe={e['class']}",
              f"  sha256={e['sha256']}", "  sonda:",
              "    " + json.dumps(e.get("probe"), ensure_ascii=False, indent=2).replace("\n", "\n    ")]
    for c in manifest.get("not_found", []):
        L.append(f"\n(nenhum lump '{c}' no intervalo de tamanho pedido)")
    return "\n".join(L) + "\n"


def _write_texts(out: Path, manifest: dict, datas: dict) -> list:
    """Escreve manifest.txt e hexdump_<arquivo>.txt; devolve a lista de nomes criados."""
    names = []
    (out / "manifest.txt").write_text(manifest_text(manifest), encoding="utf-8")
    names.append("manifest.txt")
    for fname, data in datas.items():
        hn = f"hexdump_{fname}.txt"
        (out / hn).write_text(f"{fname}  (primeiros {min(256, len(data))} de {len(data)} bytes)\n\n"
                              + hexdump(data[:256]) + "\n", encoding="utf-8")
        names.append(hn)
    return names


def base_class(label: str) -> str:
    return label.split(" ")[0].split(":")[0].rstrip("?")


def _ext(cls: str) -> str:
    return {"tga": "tga", "bmp": "bmp", "b3d": "b3d", "gr2": "gr2", "8tre": "8", "wav": "wav",
            "amx": "amx", "chunk": "chunk", "texto": "txt"}.get(cls, "bin")


def probe_lump(cls: str, data: bytes) -> dict | None:
    try:
        if cls == "gr2":
            return gr2_header(data)
        if cls == "b3d":
            return probe_b3d(data)
        if cls == "tga":
            return probe_tga(data)
        if cls == "bmp":
            return probe_bmp(data)
        if cls == "amx":
            return probe_amx(data)
    except Exception as exc:  # a sonda nunca deve derrubar a extração
        return {"probe_error": f"{type(exc).__name__}: {exc}"}
    return None


def _entry(grp_name: str, lp, cls: str, data: bytes, file_name: str) -> dict:
    return {"file": file_name, "grp": grp_name, "lump": lp.index, "offset": lp.offset, "length": lp.length,
            "class": cls, "sha256": hashlib.sha256(data).hexdigest(), "probe": probe_lump(base_class(cls), data)}


def _valid_for_class(cls: str, data: bytes) -> bool:
    """TGA não tem magic: só aceita se a sonda achar o cabeçalho plausível e o tamanho coerente."""
    if cls != "tga":
        return True
    p = probe_tga(data)
    return bool(p.get("plausible")) and p.get("size_ok", True)


def extract(grp_path, out_dir, classes=None, indices=None, limit=None) -> dict:
    grp_path, out = Path(grp_path), Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    want = {c.lower().rstrip("?") for c in classes} if classes else None
    idx = set(indices) if indices else None
    entries = []
    datas: dict = {}
    with GrpFile(grp_path) as g:
        for lp in g.info.lumps:
            if limit is not None and len(entries) >= limit:
                break
            if idx is not None and lp.index not in idx:
                continue
            if lp.end > g.info.file_size:
                continue
            data = g.read(lp)
            cls = classify(data[:64], len(data))
            b = base_class(cls)
            if want is not None and b not in want:
                continue
            if not _valid_for_class(b, data):
                continue
            name = f"{lp.index:06d}.{_ext(b)}"
            (out / name).write_bytes(data)
            datas[name] = data
            entries.append(_entry(grp_path.name, lp, cls, data, name))
    manifest = {"source_grp": grp_path.name, "count": len(entries), "files": entries}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    _write_texts(out, manifest, datas)
    return manifest


def sample(grp_path, out_dir, classes=("gr2", "b3d", "tga"), per_class=2, min_bytes=1024,
           max_bytes=3_000_000, strategy="smallest", make_zip=True, text_only=False) -> dict:
    """Escolhe poucos lumps por classe (dentro de [min_bytes, max_bytes]) para análise."""
    grp_path, out = Path(grp_path), Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    chosen: dict[str, list] = {c: [] for c in classes}
    with GrpFile(grp_path) as g:
        for lp in g.info.lumps:
            if lp.end > g.info.file_size or not (min_bytes <= lp.length <= max_bytes):
                continue
            head = g.head(lp, 64)
            b = base_class(classify(head, lp.length))
            if b in chosen:
                chosen[b].append(lp)
        entries = []
        datas: dict = {}
        for cls, lumps in chosen.items():
            lumps.sort(key=lambda l: l.length)
            if strategy == "largest":
                lumps = lumps[::-1]
            elif strategy == "median" and lumps:
                mid = len(lumps) // 2
                lumps = lumps[mid:] + lumps[:mid][::-1]
            picked = 0
            for lp in lumps:
                if picked >= per_class:
                    break
                data = g.read(lp)
                if not _valid_for_class(cls, data):
                    continue
                name = f"{cls}_{lp.index:06d}.{_ext(cls)}"
                (out / name).write_bytes(data)
                datas[name] = data
                entries.append(_entry(grp_path.name, lp, classify(data[:64], len(data)), data, name))
                picked += 1
    manifest = {"source_grp": grp_path.name, "strategy": strategy, "count": len(entries), "files": entries,
                "not_found": [c for c in classes if not any(base_class(e["class"]) == c for e in entries)]}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    text_names = _write_texts(out, manifest, datas)
    if make_zip:
        zpath = out.with_suffix(".zip") if out.suffix == "" else out.parent / (out.name + ".zip")
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
            for tn in text_names:
                z.write(out / tn, tn)
            z.writestr("LEIA-ME.txt", SAMPLE_README)
            if not text_only:
                z.write(out / "manifest.json", "manifest.json")
                for e in entries:
                    z.write(out / e["file"], e["file"])
        manifest["zip"] = str(zpath)
    return manifest
