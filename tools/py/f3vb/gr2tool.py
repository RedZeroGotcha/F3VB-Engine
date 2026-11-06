"""Ponte para o parser GR2 vendorizado (blendergranny, MIT). Python puro, sem Blender,
sem DLL. NUNCA usa granny2.dll, SDK, --research-native nem --unsafe-oodle0."""
from __future__ import annotations

import sys
from pathlib import Path

_VENDOR = Path(__file__).resolve().parents[3] / "third_party" / "blendergranny"


def _api():
    if str(_VENDOR) not in sys.path:
        sys.path.insert(0, str(_VENDOR))
    from io_scene_gr2.gr2 import animation, file, fixup, geometry, skeleton  # noqa: E402
    return file, fixup, geometry, skeleton, animation


def gr2_header(data: bytes) -> dict:
    file, *_ = _api()
    gr2 = file.read_gr2_bytes(data)
    h = gr2.header
    return {
        "version": h.version, "pointer_size": h.pointer_size, "byte_reversed": h.byte_reversed,
        "total_size_declared": h.total_size, "actual_size": len(data), "section_count": h.section_count,
        "sections": [{"index": s.index, "name": s.semantic_name, "compression": s.compression_name,
                      "data_size": s.data_size, "expanded_size": s.expanded_size,
                      "pointer_fixups": s.pointer_fixup_count, "mixed_marshalling": s.mixed_marshalling_count}
                     for s in gr2.sections],
        "compressions": sorted({s.compression_name for s in gr2.sections}),
    }


def summarize_gr2(data: bytes, deep: bool = True) -> dict:
    """Resumo em etapas; cada etapa que falhar vira uma entrada em `errors` (não derruba as outras)."""
    file, fixup, geometry, skeleton, animation = _api()
    out: dict = {"format": "gr2", "errors": []}
    try:
        out["header"] = gr2_header(data)
    except Exception as exc:
        out["errors"].append({"stage": "header", "error": f"{type(exc).__name__}: {exc}"})
        return out
    if not deep:
        return out
    try:
        loaded = fixup.load_sections(file.read_gr2_bytes(data))
    except Exception as exc:
        out["errors"].append({"stage": "load_sections (descompressão/fixups)", "error": f"{type(exc).__name__}: {exc}"})
        return out
    stages = (
        ("geometry", lambda: geometry.geometry_summary(geometry.extract_mesh_geometries(loaded))),
        ("skeletons", lambda: skeleton.skeleton_summary(skeleton.extract_skeletons(loaded))),
        ("animations", lambda: animation.animation_summary(animation.extract_animation_set(loaded))),
    )
    for name, fn in stages:
        try:
            out[name] = fn()
        except Exception as exc:
            out["errors"].append({"stage": name, "error": f"{type(exc).__name__}: {exc}"})
    return out
