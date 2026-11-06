"""Inventário de uma pasta de jogo (tudo FORA dos .grp): extensões, tamanhos, hashes."""
from __future__ import annotations

import hashlib
import os
from collections import Counter
from pathlib import Path

NOTABLE = ("f3.exe", "f3.ini", "granny2.dll", "resource.rht")


def sha256_file(path: Path, block: int = 1 << 20) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        while chunk := fh.read(block):
            h.update(chunk)
    return h.hexdigest()


def inventory(root: str | os.PathLike, with_hash: bool = False) -> dict:
    root_path = Path(root)
    if not root_path.is_dir():
        raise FileNotFoundError(f"pasta não encontrada: {root_path}")
    files = []
    by_ext_count: Counter = Counter()
    by_ext_bytes: Counter = Counter()
    for dirpath, _dirs, names in os.walk(root_path):
        for name in names:
            p = Path(dirpath) / name
            size = p.stat().st_size
            ext = p.suffix.lower().lstrip(".") or "(sem extensão)"
            rel = p.relative_to(root_path).as_posix()
            entry = {"path": rel, "size": size, "ext": ext}
            if with_hash:
                entry["sha256"] = sha256_file(p)
            files.append(entry)
            by_ext_count[ext] += 1
            by_ext_bytes[ext] += size
    files.sort(key=lambda e: e["path"].lower())
    lower = {e["path"].lower(): e for e in files}
    notable = {n: next((e for k, e in lower.items() if k.split("/")[-1] == n), None)
               for n in NOTABLE}
    by_key: dict = {}
    for e in files:
        by_key.setdefault((e["path"].split("/")[-1].lower(), e["size"]), []).append(e["path"])
    duplicates = [v for v in by_key.values() if len(v) > 1]
    dup_bytes = sum(next(e["size"] for e in files if e["path"] == v[0]) * (len(v) - 1) for v in duplicates)
    return {
        "duplicates": duplicates,
        "duplicate_bytes": dup_bytes,
        "root": str(root_path),
        "total_files": len(files),
        "total_bytes": sum(e["size"] for e in files),
        "has_override_dir": any(p.lower().startswith("override/") for p in lower),
        "by_ext": {k: {"files": by_ext_count[k], "bytes": by_ext_bytes[k]}
                   for k in sorted(by_ext_count, key=lambda k: -by_ext_bytes[k])},
        "grp_files": [e for e in files if e["ext"] == "grp"],
        "notable": notable,
        "files": files,
    }
