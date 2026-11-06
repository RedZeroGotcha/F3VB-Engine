# F3VB: código de terceiros vendorizado (blendergranny, MIT)

21 arquivos. Gerado por `scripts/gerar_codigo_md.py`.

## Índice

- `third_party/blendergranny/LICENSE`
- `third_party/blendergranny/UPSTREAM.md`
- `third_party/blendergranny/io_scene_gr2/__init__.py`
- `third_party/blendergranny/io_scene_gr2/gr2/__init__.py`
- `third_party/blendergranny/io_scene_gr2/gr2/animation.py`
- `third_party/blendergranny/io_scene_gr2/gr2/binary.py`
- `third_party/blendergranny/io_scene_gr2/gr2/constants.py`
- `third_party/blendergranny/io_scene_gr2/gr2/decompress/__init__.py`
- `third_party/blendergranny/io_scene_gr2/gr2/decompress/base.py`
- `third_party/blendergranny/io_scene_gr2/gr2/decompress/bitknit.py`
- `third_party/blendergranny/io_scene_gr2/gr2/decompress/oodle0.py`
- `third_party/blendergranny/io_scene_gr2/gr2/decompress/oodle1.py`
- `third_party/blendergranny/io_scene_gr2/gr2/export.py`
- `third_party/blendergranny/io_scene_gr2/gr2/file.py`
- `third_party/blendergranny/io_scene_gr2/gr2/fixup.py`
- `third_party/blendergranny/io_scene_gr2/gr2/geometry.py`
- `third_party/blendergranny/io_scene_gr2/gr2/material.py`
- `third_party/blendergranny/io_scene_gr2/gr2/skeleton.py`
- `third_party/blendergranny/io_scene_gr2/gr2/texture.py`
- `third_party/blendergranny/io_scene_gr2/gr2/types.py`
- `third_party/blendergranny/io_scene_gr2/gr2/write.py`

## third_party/blendergranny/LICENSE

```text
MIT License

Copyright (c) 2026 ciupix and contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## third_party/blendergranny/UPSTREAM.md

```markdown
# blendergranny (vendorizado)

- Origem: https://github.com/Rasetsuu/blendergranny (zip da branch `main`, baixado em 01/10/2026; commit não registrado no zip: fixe um commit quando possível).
- Licença: MIT (ver `LICENSE`, "Copyright (c) 2026 ciupix and contributors").
- O que foi copiado: SOMENTE `io_scene_gr2/gr2/` (parser em Python puro, sem `bpy`).
- O que foi omitido de propósito: `io_scene_gr2/__init__.py` (addon do Blender), `tools/` (inclui sondas contra a `granny2.dll`) e `decompress/research_native.py` (carrega biblioteca nativa privada).
- Regra do projeto F3VB: nunca usar `granny2.dll`, SDK, `--research-native` nem `--unsafe-oodle0`.
- Testes upstream observados: 48 passam, 4 pulam (amostras privadas) e 2 falham (gravador `.gr2`, em desenvolvimento).
```

## third_party/blendergranny/io_scene_gr2/__init__.py

```python
"""Raiz de namespace do parser vendorizado (o __init__ original importava o Blender e foi omitido)."""
```

## third_party/blendergranny/io_scene_gr2/gr2/__init__.py

```python
"""Core Granny2 parsing package."""

from .file import GR2File, GR2Header, GR2Section, read_gr2, read_gr2_bytes
from .fixup import LoadedGR2, PointerFixup, PointerRef, load_sections
from .geometry import MeshGeometry, extract_mesh_geometries

__all__ = [
    "GR2File",
    "GR2Header",
    "GR2Section",
    "LoadedGR2",
    "MeshGeometry",
    "PointerFixup",
    "PointerRef",
    "load_sections",
    "extract_mesh_geometries",
    "read_gr2",
    "read_gr2_bytes",
]
```

## third_party/blendergranny/io_scene_gr2/gr2/animation.py

```python
"""Animation metadata extraction from loaded GR2 sections."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Iterable

from .fixup import LoadedGR2, PointerRef
from .types import (
    MT_INLINE,
    TypeDefinition,
    parse_type_definition_array,
    read_array_references,
    read_reference_array_objects,
    read_string_pointer,
    summarize_object,
    summarize_root_object,
)


@dataclass(frozen=True)
class TrackGroup:
    index: int
    name: str
    vector_track_count: int
    transform_track_count: int
    text_track_count: int
    accumulation_flags: int
    loop_translation: float
    vector_track_names: tuple[str, ...]
    transform_track_names: tuple[str, ...]
    transform_tracks: tuple["TransformTrack", ...]


@dataclass(frozen=True)
class CurveMetadata:
    codec: str
    format: int
    degree: int
    dimension: int
    knot_control_count: int
    sample_value: tuple[float, ...] = ()
    knot_values: tuple[float, ...] = ()
    control_values: tuple[tuple[float, ...], ...] = ()


@dataclass(frozen=True)
class TransformTrack:
    index: int
    name: str
    flags: int
    orientation: CurveMetadata | None
    position: CurveMetadata | None
    scale_shear: CurveMetadata | None


@dataclass(frozen=True)
class Animation:
    index: int
    name: str
    duration: float
    time_step: float
    oversampling: float
    default_loop_count: int
    flags: int
    track_group_names: tuple[str, ...]


@dataclass(frozen=True)
class AnimationSet:
    track_groups: tuple[TrackGroup, ...]
    animations: tuple[Animation, ...]


def extract_animation_set(
    loaded: LoadedGR2,
    *,
    max_track_groups: int = 64,
    max_tracks_per_group: int = 512,
    max_animations: int = 64,
) -> AnimationSet:
    root = summarize_root_object(loaded, max_arrays=max(max_track_groups, max_animations))
    fields = _field_map(root)
    track_groups = _read_track_groups(
        loaded,
        fields.get("TrackGroups"),
        max_track_groups=max_track_groups,
        max_tracks_per_group=max_tracks_per_group,
    )
    animations = _read_animations(
        loaded,
        fields.get("Animations"),
        max_animations=max_animations,
    )
    return AnimationSet(track_groups=track_groups, animations=animations)


def animation_summary(animation_set: AnimationSet | Iterable[Animation]) -> dict | list[dict]:
    if isinstance(animation_set, AnimationSet):
        return {
            "track_groups": [
                {
                    "index": item.index,
                    "name": item.name,
                    "vector_track_count": item.vector_track_count,
                    "transform_track_count": item.transform_track_count,
                    "text_track_count": item.text_track_count,
                    "accumulation_flags": item.accumulation_flags,
                    "loop_translation": item.loop_translation,
                    "first_vector_tracks": list(item.vector_track_names[:8]),
                    "first_transform_tracks": [
                        {
                            "name": track.name,
                            "orientation": _curve_metadata_dict(track.orientation),
                            "position": _curve_metadata_dict(track.position),
                            "scale_shear": _curve_metadata_dict(track.scale_shear),
                        }
                        for track in item.transform_tracks[:8]
                    ],
                }
                for item in animation_set.track_groups
            ],
            "animations": animation_summary(animation_set.animations),
        }
    return [
        {
            "index": item.index,
            "name": item.name,
            "duration": item.duration,
            "time_step": item.time_step,
            "oversampling": item.oversampling,
            "default_loop_count": item.default_loop_count,
            "flags": item.flags,
            "track_groups": list(item.track_group_names),
        }
        for item in animation_set
    ]


def sample_curve_values(
    curve: CurveMetadata,
    duration: float,
    time_step: float,
) -> tuple[float, tuple[tuple[float, ...], ...]]:
    if not curve.control_values:
        return time_step, ()
    if time_step <= 0.0:
        return time_step, curve.control_values
    sample_count = max(1, int(round(duration / time_step)) + 1)
    samples = []
    for index in range(sample_count):
        t = min(duration, index * time_step)
        samples.append(_evaluate_curve(curve, t))
    return time_step, tuple(samples)


def _read_track_groups(
    loaded: LoadedGR2,
    field: dict | None,
    *,
    max_track_groups: int,
    max_tracks_per_group: int,
) -> tuple[TrackGroup, ...]:
    type_ref = _dict_ref(field.get("reference_type") if field else None)
    refs = _read_object_refs(
        loaded,
        field,
        max_count=max_track_groups,
    )
    return tuple(
        _read_track_group(
            loaded,
            index,
            summarize_object(loaded, ref, type_ref, max_array_refs=max_tracks_per_group),
            max_tracks_per_group=max_tracks_per_group,
        )
        for index, ref in enumerate(refs)
        if type_ref is not None
    )


def _read_track_group(
    loaded: LoadedGR2,
    index: int,
    obj: dict,
    *,
    max_tracks_per_group: int,
) -> TrackGroup:
    fields = _field_map(obj)
    vector_tracks = fields.get("VectorTracks", {})
    transform_tracks = fields.get("TransformTracks", {})
    text_tracks = fields.get("TextTracks", {})
    transform_track_items = _read_transform_tracks(
        loaded,
        transform_tracks,
        max_tracks_per_group,
    )
    return TrackGroup(
        index=index,
        name=str(fields.get("Name", {}).get("value") or f"TrackGroup_{index}"),
        vector_track_count=int(vector_tracks.get("count") or 0),
        transform_track_count=int(transform_tracks.get("count") or 0),
        text_track_count=int(text_tracks.get("count") or 0),
        accumulation_flags=_field_int(fields, "AccumulationFlags", 0),
        loop_translation=_field_float(fields, "LoopTranslation", 0.0),
        vector_track_names=_read_track_names(loaded, vector_tracks, max_tracks_per_group),
        transform_track_names=tuple(track.name for track in transform_track_items),
        transform_tracks=transform_track_items,
    )


def _read_animations(
    loaded: LoadedGR2,
    field: dict | None,
    *,
    max_animations: int,
) -> tuple[Animation, ...]:
    type_ref = _dict_ref(field.get("reference_type") if field else None)
    refs = _read_object_refs(
        loaded,
        field,
        max_count=max_animations,
    )
    return tuple(
        _read_animation(loaded, index, summarize_object(loaded, ref, type_ref, max_array_refs=64))
        for index, ref in enumerate(refs)
        if type_ref is not None
    )


def _read_animation(loaded: LoadedGR2, index: int, obj: dict) -> Animation:
    fields = _field_map(obj)
    track_groups = fields.get("TrackGroups", {})
    return Animation(
        index=index,
        name=str(fields.get("Name", {}).get("value") or f"Animation_{index}"),
        duration=_field_float(fields, "Duration", 0.0),
        time_step=_field_float(fields, "TimeStep", 0.0),
        oversampling=_field_float(fields, "Oversampling", 0.0),
        default_loop_count=_field_int(fields, "DefaultLoopCount", 0),
        flags=_field_int(fields, "Flags", 0),
        track_group_names=_read_track_group_reference_names(loaded, track_groups),
    )


def _read_track_names(
    loaded: LoadedGR2,
    field: dict,
    max_count: int,
) -> tuple[str, ...]:
    target = _dict_ref(field.get("target"))
    type_ref = _dict_ref(field.get("reference_type"))
    objects = read_reference_array_objects(
        loaded,
        target,
        int(field.get("count") or 0),
        type_ref,
        max_count=max_count,
    )
    names = []
    for index, obj in enumerate(objects):
        name = _field_map(obj).get("Name", {}).get("value")
        names.append(str(name or f"Track_{index}"))
    return tuple(names)


def _read_transform_tracks(
    loaded: LoadedGR2,
    field: dict,
    max_count: int,
) -> tuple[TransformTrack, ...]:
    target = _dict_ref(field.get("target"))
    type_ref = _dict_ref(field.get("reference_type"))
    if target is None or type_ref is None:
        return ()
    members = parse_type_definition_array(loaded, type_ref)
    offsets = _member_offsets(loaded, members)
    refs = [
        PointerRef(target.section, target.offset + index * _object_storage_size(loaded, members))
        for index in range(min(int(field.get("count") or 0), max_count))
    ]
    tracks = []
    for index, ref in enumerate(refs):
        data = loaded.sections_fixed[ref.section]
        name_offset = ref.offset + offsets.get("Name", 0)
        flags_offset = ref.offset + offsets.get("Flags", 4)
        name = read_string_pointer(loaded, _read_pointer(data, name_offset, loaded.gr2.header.pointer_size // 8))
        flags = struct.unpack_from("<i", data, flags_offset)[0]
        tracks.append(
            TransformTrack(
                index=index,
                name=name or f"TransformTrack_{index}",
                flags=flags,
                orientation=_read_curve_metadata(loaded, ref.offset + offsets.get("OrientationCurve", 8), ref.section),
                position=_read_curve_metadata(loaded, ref.offset + offsets.get("PositionCurve", 16), ref.section),
                scale_shear=_read_curve_metadata(loaded, ref.offset + offsets.get("ScaleShearCurve", 24), ref.section),
            )
        )
    return tuple(tracks)


def _read_curve_metadata(
    loaded: LoadedGR2,
    offset: int,
    section: int,
) -> CurveMetadata | None:
    pointer_size = loaded.gr2.header.pointer_size // 8
    data = loaded.sections_fixed[section]
    if offset + pointer_size * 2 > len(data):
        return None
    old_curve = _read_legacy_curve_metadata(loaded, offset, section)
    if old_curve is not None:
        return old_curve
    type_ref = loaded.resolve_fake_pointer(_read_pointer(data, offset, pointer_size))
    object_ref = loaded.resolve_fake_pointer(_read_pointer(data, offset + pointer_size, pointer_size))
    if type_ref is None or object_ref is None:
        return None
    members = parse_type_definition_array(loaded, type_ref)
    if not members:
        return None
    codec = members[0].name.replace("CurveDataHeader_", "")
    offsets = _member_offsets(loaded, members)
    raw = loaded.sections_original[object_ref.section]
    if object_ref.offset + 2 > len(raw):
        return None
    fmt, degree = struct.unpack_from("<BB", raw, object_ref.offset)
    dimension = _curve_dimension(codec)
    if codec == "DaIdentity":
        dim_offset = object_ref.offset + offsets.get("Dimension", 2)
        if dim_offset + 2 <= len(raw):
            dimension = struct.unpack_from("<h", raw, dim_offset)[0]
    count = 0
    kc_offset = offsets.get("KnotsControls")
    if kc_offset is not None and object_ref.offset + kc_offset + 4 <= len(raw):
        count = struct.unpack_from("<I", raw, object_ref.offset + kc_offset)[0]
    knot_values, control_values = _read_curve_knots_controls(
        loaded,
        codec,
        dimension,
        raw,
        loaded.sections_fixed[object_ref.section],
        object_ref.offset,
        offsets,
        count,
    )
    return CurveMetadata(
        codec=codec,
        format=fmt,
        degree=degree,
        dimension=dimension,
        knot_control_count=count,
        sample_value=_read_static_curve_value(codec, dimension, raw, object_ref.offset),
        knot_values=knot_values,
        control_values=control_values,
    )


def _read_legacy_curve_metadata(
    loaded: LoadedGR2,
    offset: int,
    section: int,
) -> CurveMetadata | None:
    pointer_size = loaded.gr2.header.pointer_size // 8
    raw = loaded.sections_original[section]
    fixed = loaded.sections_fixed[section]
    if offset + 4 + (4 + pointer_size) * 2 > len(raw):
        return None
    degree = struct.unpack_from("<i", raw, offset)[0]
    if degree < 0 or degree > 3:
        return None
    knot_count = struct.unpack_from("<I", raw, offset + 4)[0]
    control_count = struct.unpack_from("<I", raw, offset + 4 + pointer_size + 4)[0]
    if knot_count == 0 and control_count == 0:
        return None
    if knot_count <= 0 or control_count <= 0 or control_count % knot_count:
        return None
    dimension = control_count // knot_count
    if dimension <= 0 or dimension > 16:
        return None
    knots_ref = _read_reference_array_pointer(loaded, fixed, offset + 4)
    controls_ref = _read_reference_array_pointer(loaded, fixed, offset + 4 + pointer_size + 4)
    if knots_ref is None or controls_ref is None:
        return None
    knot_bytes = loaded.sections_fixed[knots_ref.section][knots_ref.offset : knots_ref.offset + knot_count * 4]
    control_bytes = loaded.sections_fixed[controls_ref.section][controls_ref.offset : controls_ref.offset + control_count * 4]
    if len(knot_bytes) < knot_count * 4 or len(control_bytes) < control_count * 4:
        return None
    knots = struct.unpack_from("<" + "f" * knot_count, knot_bytes, 0)
    flat_controls = struct.unpack_from("<" + "f" * control_count, control_bytes, 0)
    controls = tuple(
        tuple(flat_controls[index * dimension + dim] for dim in range(dimension))
        for index in range(knot_count)
    )
    sample_value = controls[0] if knot_count == 1 else ()
    return CurveMetadata(
        codec="LegacyCurve32f",
        format=-1,
        degree=degree,
        dimension=dimension,
        knot_control_count=knot_count + control_count,
        sample_value=sample_value,
        knot_values=tuple(knots),
        control_values=controls,
    )


def _read_track_group_reference_names(loaded: LoadedGR2, field: dict) -> tuple[str, ...]:
    target = _dict_ref(field.get("target"))
    type_ref = _dict_ref(field.get("reference_type"))
    refs = read_array_references(
        loaded,
        target,
        int(field.get("count") or 0),
        max_count=64,
    )
    names = []
    for index, ref in enumerate(refs):
        if type_ref is None:
            names.append(f"TrackGroup_{index}")
            continue
        fields = _field_map(summarize_object(loaded, ref, type_ref, max_array_refs=0))
        names.append(str(fields.get("Name", {}).get("value") or f"TrackGroup_{index}"))
    return tuple(names)


def _read_object_refs(
    loaded: LoadedGR2,
    field: dict | None,
    *,
    max_count: int,
) -> tuple[PointerRef, ...]:
    target = _dict_ref(field.get("target") if field else None)
    return read_array_references(
        loaded,
        target,
        int(field.get("count") or 0) if field else 0,
        max_count=max_count,
    )


def _dict_ref(value: dict | None) -> PointerRef | None:
    if not value:
        return None
    return PointerRef(int(value["section"]), int(value["offset"]))


def _curve_dimension(codec: str) -> int:
    if codec.startswith("D") and len(codec) >= 2 and codec[1].isdigit():
        return int(codec[1])
    return 0


def _curve_metadata_dict(curve: CurveMetadata | None) -> dict | None:
    if curve is None:
        return None
    result = {
        "codec": curve.codec,
        "format": curve.format,
        "degree": curve.degree,
        "dimension": curve.dimension,
        "knot_control_count": curve.knot_control_count,
    }
    if curve.sample_value:
        result["sample_value"] = list(curve.sample_value)
    if curve.knot_values:
        result["knot_count"] = len(curve.knot_values)
        result["first_knot_values"] = list(curve.knot_values[:4])
    if curve.control_values:
        result["control_count"] = len(curve.control_values)
        result["first_control_values"] = [list(item) for item in curve.control_values[:2]]
    return result


def _read_curve_knots_controls(
    loaded: LoadedGR2,
    codec: str,
    dimension: int,
    raw: bytes,
    fixed: bytes,
    offset: int,
    offsets: dict[str, int],
    knot_control_count: int,
) -> tuple[tuple[float, ...], tuple[tuple[float, ...], ...]]:
    if knot_control_count <= 0 or dimension <= 0:
        return (), ()
    kc_offset = offsets.get("KnotsControls")
    if kc_offset is None:
        return (), ()
    array_offset = offset + kc_offset
    array_ref = _read_reference_array_pointer(loaded, fixed, array_offset)
    if array_ref is None:
        return (), ()
    if codec in {"D3K16uC16u", "D3I1K16uC16u"}:
        return _read_d3_k16u_c16u_curve(loaded, codec, dimension, raw, offset, offsets, knot_control_count, array_ref)
    if codec == "D4nK8uC7u":
        return _read_d4n_k8u_c7u_curve(loaded, raw, offset, offsets, knot_control_count, array_ref)
    if codec == "D4nK16uC15u":
        return _read_d4n_k16u_c15u_curve(loaded, raw, offset, offsets, knot_control_count, array_ref)
    return (), ()


def _read_d3_k16u_c16u_curve(
    loaded: LoadedGR2,
    codec: str,
    dimension: int,
    raw: bytes,
    offset: int,
    offsets: dict[str, int],
    knot_control_count: int,
    array_ref: PointerRef,
) -> tuple[tuple[float, ...], tuple[tuple[float, ...], ...]]:
    knot_count = knot_control_count // (2 if codec == "D3I1K16uC16u" else dimension + 1)
    if knot_count <= 0:
        return (), ()
    one_over_offset = offset + offsets.get("OneOverKnotScaleTrunc", 2)
    scales_offset = offset + offsets.get("ControlScales", 4)
    offsets_offset = offset + offsets.get("ControlOffsets", 16)
    if offsets_offset + dimension * 4 > len(raw):
        return (), ()
    one_over = _float_from_high_u16(struct.unpack_from("<H", raw, one_over_offset)[0])
    if one_over == 0.0:
        return (), ()
    control_scales = struct.unpack_from("<" + "f" * dimension, raw, scales_offset)
    control_offsets = struct.unpack_from("<" + "f" * dimension, raw, offsets_offset)
    array_data = loaded.sections_fixed[array_ref.section]
    data = array_data[array_ref.offset : array_ref.offset + knot_control_count * 2]
    if len(data) < knot_control_count * 2:
        return (), ()
    values = struct.unpack_from("<" + "H" * knot_control_count, data, 0)
    knot_scale = 1.0 / one_over
    knots = tuple(knot_scale * value for value in values[:knot_count])
    controls: list[tuple[float, ...]] = []
    if codec == "D3I1K16uC16u":
        for param in values[knot_count : knot_count * 2]:
            controls.append(tuple(control_offsets[dim] + control_scales[dim] * param for dim in range(dimension)))
    else:
        control_data = values[knot_count : knot_count + knot_count * dimension]
        for index in range(knot_count):
            base = index * dimension
            controls.append(
                tuple(
                    control_offsets[dim] + control_scales[dim] * control_data[base + dim]
                    for dim in range(dimension)
                )
            )
    return knots, tuple(controls)


def _read_d4n_k8u_c7u_curve(
    loaded: LoadedGR2,
    raw: bytes,
    offset: int,
    offsets: dict[str, int],
    knot_control_count: int,
    array_ref: PointerRef,
) -> tuple[tuple[float, ...], tuple[tuple[float, ...], ...]]:
    knot_count = knot_control_count // 4
    if knot_count <= 0:
        return (), ()
    scale_entries_offset = offset + offsets.get("ScaleOffsetTableEntries", 2)
    one_over_offset = offset + offsets.get("OneOverKnotScale", 4)
    if one_over_offset + 4 > len(raw):
        return (), ()
    one_over = struct.unpack_from("<f", raw, one_over_offset)[0]
    if one_over == 0.0:
        return (), ()
    array_data = loaded.sections_fixed[array_ref.section]
    data = array_data[array_ref.offset : array_ref.offset + knot_control_count]
    if len(data) < knot_control_count:
        return (), ()
    knot_scale = 1.0 / one_over
    knots = tuple(knot_scale * float(value) for value in data[:knot_count])
    scales, offsets_values = _quaternion_scales_offsets(struct.unpack_from("<H", raw, scale_entries_offset)[0])
    controls = []
    control_bytes = data[knot_count : knot_count + knot_count * 3]
    for index in range(knot_count):
        cur = control_bytes[index * 3 : index * 3 + 3]
        missing_negative = (cur[0] & 0x80) != 0
        missing_index = ((cur[1] >> 6) & 0x2) | (cur[2] >> 7)
        result = [0.0, 0.0, 0.0, 0.0]
        dst = missing_index
        summed_sq = 0.0
        for src in range(3):
            dst = (dst + 1) & 0x3
            value = offsets_values[dst] + scales[dst] * float(cur[src] & 0x7F)
            result[dst] = value
            summed_sq += value * value
        missing = max(0.0, 1.0 - summed_sq) ** 0.5
        result[missing_index] = -missing if missing_negative else missing
        controls.append(tuple(result))
    return knots, tuple(controls)


def _read_d4n_k16u_c15u_curve(
    loaded: LoadedGR2,
    raw: bytes,
    offset: int,
    offsets: dict[str, int],
    knot_control_count: int,
    array_ref: PointerRef,
) -> tuple[tuple[float, ...], tuple[tuple[float, ...], ...]]:
    knot_count = knot_control_count // 4
    if knot_count <= 0:
        return (), ()
    scale_entries_offset = offset + offsets.get("ScaleOffsetTableEntries", 2)
    one_over_offset = offset + offsets.get("OneOverKnotScale", 4)
    if one_over_offset + 4 > len(raw):
        return (), ()
    one_over = struct.unpack_from("<f", raw, one_over_offset)[0]
    if one_over == 0.0:
        return (), ()
    array_data = loaded.sections_fixed[array_ref.section]
    data = array_data[array_ref.offset : array_ref.offset + knot_control_count * 2]
    if len(data) < knot_control_count * 2:
        return (), ()
    values = struct.unpack_from("<" + "H" * knot_control_count, data, 0)
    knot_scale = 1.0 / one_over
    knots = tuple(knot_scale * float(value) for value in values[:knot_count])
    scales, offsets_values = _quaternion_scales_offsets(
        struct.unpack_from("<H", raw, scale_entries_offset)[0],
        quantized_max=32767.0,
    )
    controls = []
    control_values = values[knot_count : knot_count + knot_count * 3]
    for index in range(knot_count):
        cur = control_values[index * 3 : index * 3 + 3]
        missing_negative = (cur[0] & 0x8000) != 0
        missing_index = ((cur[1] >> 14) & 0x2) | (cur[2] >> 15)
        result = [0.0, 0.0, 0.0, 0.0]
        dst = missing_index
        summed_sq = 0.0
        for src in range(3):
            dst = (dst + 1) & 0x3
            value = offsets_values[dst] + scales[dst] * float(cur[src] & 0x7FFF)
            result[dst] = value
            summed_sq += value * value
        missing = max(0.0, 1.0 - summed_sq) ** 0.5
        result[missing_index] = -missing if missing_negative else missing
        controls.append(tuple(result))
    return knots, tuple(controls)


def _read_reference_array_pointer(loaded: LoadedGR2, fixed: bytes, offset: int) -> PointerRef | None:
    pointer_size = loaded.gr2.header.pointer_size // 8
    if offset + 4 + pointer_size > len(fixed):
        return None
    return loaded.resolve_fake_pointer(_read_pointer(fixed, offset + 4, pointer_size))


def _evaluate_curve(curve: CurveMetadata, t: float) -> tuple[float, ...]:
    knots = curve.knot_values
    controls = curve.control_values
    if not knots or not controls:
        return curve.sample_value
    degree = max(0, int(curve.degree))
    if degree == 0 or len(knots) == 1:
        return controls[min(_find_knot(knots, t), len(controls) - 1)]
    knot_index = min(_find_knot(knots, t), len(knots) - 1)
    window = 2 * degree
    base = knot_index - degree
    ti = []
    pi = []
    for local_index in range(window):
        source_index = min(max(base + local_index, 0), len(knots) - 1)
        ti.append(knots[source_index])
        pi.append(controls[min(source_index, len(controls) - 1)])
    value = _sample_bspline(degree, curve.dimension, curve.dimension == 4, ti, pi, degree, t)
    if curve.dimension == 4:
        return _normalize_tuple(value)
    return value


def _find_knot(knots: tuple[float, ...], t: float) -> int:
    for index, value in enumerate(knots):
        if value > t:
            return index
    return max(0, len(knots) - 1)


def _sample_bspline(
    degree: int,
    dimension: int,
    normalize: bool,
    ti: list[float],
    pi: list[tuple[float, ...]],
    center: int,
    t: float,
) -> tuple[float, ...]:
    if degree == 1:
        c_prev, c_curr = _linear_coefficients(ti[center - 1], ti[center], t)
        result = tuple(c_prev * pi[center - 1][dim] + c_curr * pi[center][dim] for dim in range(dimension))
    elif degree == 2:
        c2, c1, c0 = _quadratic_coefficients(ti[center - 2], ti[center - 1], ti[center], ti[center + 1], t)
        result = tuple(
            c2 * pi[center - 2][dim] + c1 * pi[center - 1][dim] + c0 * pi[center][dim]
            for dim in range(dimension)
        )
    else:
        result = pi[center]
    return _normalize_tuple(result) if normalize else result


def _linear_coefficients(ti_prev: float, ti_curr: float, t: float) -> tuple[float, float]:
    blend = _safe_div(t - ti_prev, ti_curr - ti_prev)
    return 1.0 - blend, blend


def _quadratic_coefficients(
    ti_2: float,
    ti_1: float,
    ti: float,
    ti1: float,
    t: float,
) -> tuple[float, float, float]:
    l0 = _safe_div(t - ti_1, ti - ti_1)
    l1_1 = _safe_div(t - ti_2, ti - ti_2)
    l1_2 = _safe_div(t - ti_1, ti1 - ti_1)
    ci_2_plus_ci_1 = (l1_1 + l0) - l0 * l1_1
    ci = l0 * l1_2
    ci_1 = ci_2_plus_ci_1 - ci
    ci_2 = 1.0 - ci_2_plus_ci_1
    return ci_2, ci_1, ci


def _safe_div(numerator: float, denominator: float) -> float:
    if abs(denominator) < 1e-12:
        return 0.0
    return numerator / denominator


def _normalize_tuple(values: tuple[float, ...]) -> tuple[float, ...]:
    length_sq = sum(value * value for value in values)
    if length_sq <= 0.0:
        return values
    inv_length = length_sq ** -0.5
    return tuple(value * inv_length for value in values)


def _float_from_high_u16(value: int) -> float:
    return struct.unpack("<f", struct.pack("<I", value << 16))[0]


def _quaternion_scales_offsets(
    entries: int,
    *,
    quantized_max: float = 127.0,
) -> tuple[tuple[float, ...], tuple[float, ...]]:
    table = _quaternion_scale_offset_table()
    scales = []
    offsets = []
    for _ in range(4):
        index = entries & 0xF
        entries >>= 4
        scales.append(table[index][0] / quantized_max)
        offsets.append(table[index][1])
    return tuple(scales), tuple(offsets)


def _quaternion_scale_offset_table() -> tuple[tuple[float, float], ...]:
    one_over_sqrt2 = 0.707106781
    return (
        (one_over_sqrt2 * 2.0, -one_over_sqrt2),
        (one_over_sqrt2 * 1.0, -one_over_sqrt2 * 0.5),
        (one_over_sqrt2 * 0.5, -one_over_sqrt2 * 0.75),
        (one_over_sqrt2 * 0.5, -one_over_sqrt2 * 0.25),
        (one_over_sqrt2 * 0.5, one_over_sqrt2 * 0.25),
        (one_over_sqrt2 * 0.25, -one_over_sqrt2 * 0.250),
        (one_over_sqrt2 * 0.25, -one_over_sqrt2 * 0.125),
        (one_over_sqrt2 * 0.25, one_over_sqrt2 * 0.000),
        (-one_over_sqrt2 * 2.0, one_over_sqrt2),
        (-one_over_sqrt2 * 1.0, one_over_sqrt2 * 0.5),
        (-one_over_sqrt2 * 0.5, one_over_sqrt2 * 0.75),
        (-one_over_sqrt2 * 0.5, one_over_sqrt2 * 0.25),
        (-one_over_sqrt2 * 0.5, -one_over_sqrt2 * 0.25),
        (-one_over_sqrt2 * 0.25, one_over_sqrt2 * 0.250),
        (-one_over_sqrt2 * 0.25, one_over_sqrt2 * 0.125),
        (-one_over_sqrt2 * 0.25, -one_over_sqrt2 * 0.000),
    )


def _read_static_curve_value(
    codec: str,
    dimension: int,
    raw: bytes,
    offset: int,
) -> tuple[float, ...]:
    if dimension <= 0:
        return ()
    if codec.endswith("Constant32f"):
        controls_offset = offset + 4
        byte_count = dimension * 4
        if controls_offset + byte_count <= len(raw):
            return struct.unpack_from("<" + "f" * dimension, raw, controls_offset)
        return ()
    if codec == "DaIdentity":
        if dimension == 9:
            return (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
        if dimension == 4:
            return (0.0, 0.0, 0.0, 1.0)
        return tuple(0.0 for _ in range(dimension))
    return ()


def _member_offsets(loaded: LoadedGR2, members: tuple[TypeDefinition, ...]) -> dict[str, int]:
    offsets = {}
    offset = 0
    for member in members:
        offsets[member.name] = offset
        offset += _member_size(loaded, member)
    return offsets


def _object_storage_size(loaded: LoadedGR2, members: tuple[TypeDefinition, ...]) -> int:
    return sum(_member_size(loaded, member) for member in members)


def _member_size(loaded: LoadedGR2, member: TypeDefinition) -> int:
    pointer_size = loaded.gr2.header.pointer_size // 8
    if member.member_type == MT_INLINE and member.reference_type is not None:
        return _object_storage_size(loaded, parse_type_definition_array(loaded, member.reference_type))
    from .types import member_storage_size

    return member_storage_size(member, pointer_size)


def _read_pointer(data: bytes, offset: int, pointer_size: int) -> int:
    if pointer_size == 4:
        return struct.unpack_from("<I", data, offset)[0]
    return struct.unpack_from("<Q", data, offset)[0]


def _field_map(summary: dict) -> dict[str, dict]:
    return {field["name"]: field for field in summary.get("fields", [])}


def _field_int(fields: dict[str, dict], name: str, default: int) -> int:
    value = fields.get(name, {}).get("value")
    return default if value is None else int(value)


def _field_float(fields: dict[str, dict], name: str, default: float) -> float:
    value = fields.get(name, {}).get("value")
    return default if value is None else float(value)
```

## third_party/blendergranny/io_scene_gr2/gr2/binary.py

```python
"""Binary helpers."""

import struct


class BinaryReader:
    def __init__(self, data: bytes, *, byte_reversed: bool = False):
        self.data = data
        self.endian = ">" if byte_reversed else "<"

    def require(self, offset: int, size: int) -> None:
        if offset < 0 or offset + size > len(self.data):
            raise ValueError(f"read out of range at {offset:#x} size {size}")

    def u32(self, offset: int) -> int:
        self.require(offset, 4)
        return struct.unpack_from(self.endian + "I", self.data, offset)[0]

    def i32(self, offset: int) -> int:
        self.require(offset, 4)
        return struct.unpack_from(self.endian + "i", self.data, offset)[0]


def swap_u32(value: int) -> int:
    return int.from_bytes(value.to_bytes(4, "little"), "big")
```

## third_party/blendergranny/io_scene_gr2/gr2/constants.py

```python
"""Small GR2 constants used by the parser."""

MAGIC_OLD = (0xCAB067B8, 0x0FB16DF8, 0x7E8C7284, 0x1E00195E)
MAGIC_32LE = (0xC06CDE29, 0x2B53A4BA, 0xA5B7F525, 0xEEE266F6)
MAGIC_32BE = (0xB595110E, 0x4BB5A56A, 0x502828EB, 0x04B37825)
MAGIC_64LE = (0x5E499BE5, 0x141F636F, 0xA9EB131E, 0xC4EDBE90)
MAGIC_64BE = (0xE3D49531, 0x624FDC20, 0x3AD036CC, 0x89FF82B1)

MAGIC_SIZE = 32
SECTION_RECORD_SIZE = 44
EXTRA_TAG_COUNT = 4

COMPRESSION_NONE = 0
COMPRESSION_OODLE0 = 1
COMPRESSION_OODLE1 = 2
COMPRESSION_BITKNIT = 3
COMPRESSION_BITKNIT2 = 4

COMPRESSION_NAMES = {
    COMPRESSION_NONE: "none",
    COMPRESSION_OODLE0: "oodle0",
    COMPRESSION_OODLE1: "oodle1",
    COMPRESSION_BITKNIT: "bitknit",
    COMPRESSION_BITKNIT2: "bitknit2",
}

SECTION_NAMES = {
    0: "main",
    1: "rigid_vertex",
    2: "rigid_index",
    3: "deformable_vertex",
    4: "deformable_index",
    5: "texture",
    6: "discardable",
    7: "unloaded",
}
```

## third_party/blendergranny/io_scene_gr2/gr2/decompress/__init__.py

```python
"""Section decompression entry points."""

from .base import DecompressionError, DecompressionUnsupported, decompress_section

__all__ = ["DecompressionError", "DecompressionUnsupported", "decompress_section"]
```

## third_party/blendergranny/io_scene_gr2/gr2/decompress/base.py

```python
"""Clean decompression facade."""

from __future__ import annotations

from io_scene_gr2.gr2.constants import (
    COMPRESSION_BITKNIT,
    COMPRESSION_BITKNIT2,
    COMPRESSION_NONE,
    COMPRESSION_OODLE0,
    COMPRESSION_OODLE1,
)
from io_scene_gr2.gr2.file import GR2Section


class DecompressionError(RuntimeError):
    pass


class DecompressionUnsupported(DecompressionError):
    pass


def decompress_section(section: GR2Section, compressed: bytes) -> bytes:
    if section.expanded_size == 0:
        return b""

    if section.compression == COMPRESSION_NONE:
        if len(compressed) < section.expanded_size:
            raise DecompressionError(
                f"section {section.index} short: {len(compressed)} < {section.expanded_size}"
            )
        return compressed[: section.expanded_size]

    if section.compression == COMPRESSION_OODLE0:
        from io_scene_gr2.gr2.decompress.oodle0 import decompress_oodle0

        return decompress_oodle0(section, compressed)

    if section.compression in (COMPRESSION_BITKNIT, COMPRESSION_BITKNIT2):
        from io_scene_gr2.gr2.decompress.bitknit import decompress_bitknit

        return decompress_bitknit(section, compressed)

    if section.compression == COMPRESSION_OODLE1:
        from io_scene_gr2.gr2.decompress.oodle1 import decompress_oodle1

        return decompress_oodle1(section, compressed)

    raise DecompressionUnsupported(f"unknown compression id {section.compression}")
```

## third_party/blendergranny/io_scene_gr2/gr2/decompress/bitknit.py

```python
"""Clean BitKnit container reader.

Only lightweight structure checks live here for now. The entropy/LZ decoder is
still pending, but this gives us safe sample inspection and stable tests.
"""

from __future__ import annotations

import struct
from bisect import bisect_right
from dataclasses import dataclass

from io_scene_gr2.gr2.constants import COMPRESSION_BITKNIT, COMPRESSION_BITKNIT2
from io_scene_gr2.gr2.decompress.base import DecompressionUnsupported
from io_scene_gr2.gr2.file import GR2Section

BITKNIT_HEADER_SIZE = 48
BITKNIT_BLOCK_COUNT = 4
BITKNIT2_MARKER = 0x75B1
BITKNIT_WORD_MASK = 0xFFFFFFFF
BITKNIT_MODEL_UPDATE = 31
BITKNIT_INITIAL_WEIGHT = 1
BITKNIT_LITERAL_SYMBOLS = 0x205
BITKNIT_MATCH_SYMBOLS = 0x45
BITKNIT_DLL_LITERAL_TABLE_SYMBOLS = 0x12C
BITKNIT_DLL_SHORT_DISTANCE_SYMBOLS = 0x15
BITKNIT_DLL_LONG_DISTANCE_SYMBOLS = 0x28
BITKNIT_LITERAL_LIMIT = 0x100
BITKNIT_SHORT_MATCH_BASE = 0x100
BITKNIT_EXTRA_MATCH_BASE = 0x120
BITKNIT_CONTEXT_STRIDE = 4
BITKNIT_RECENT_DISTANCE_COUNT = 8
BITKNIT_DISTANCE_LOW_BITS = 5
BITKNIT_CHUNK_SIZE = 16
BITKNIT_RANGE_INITIAL = 0x80
BITKNIT_RANGE_NORMALIZE_LIMIT = 0x800000
BITKNIT_SECTION6_RECORD_SIZE = 32
BITKNIT_SECTION6_INITIAL_FILL_RECORDS = 2
BITKNIT_INITIAL_RECENT_DISTANCE = 1
BITKNIT_INITIAL_RECENT_SELECTOR = 0xFAC688
BITKNIT_FSM_INITIAL = 1
BITKNIT_FSM_CHECK_BLOCK = 2
BITKNIT_FSM_DONE = 3
BITKNIT_FSM_COPY_EVEN = 4
BITKNIT_FSM_COPY_ODD = 5
BITKNIT_FSM_COMPRESSED = 7
BITKNIT_FSM_COMPRESSED_ALT = 8
BITKNIT2_STATE7_VERIFIED_TAGS = frozenset((0x1, 0x2, 0x3, 0x12, 0x14, 0x50, 0x14E, 0x518, 0x7D18))
BITKNIT2_BLOCK_OUTPUT_SIZE = 0x10000


@dataclass(frozen=True)
class BitKnitHeader:
    words: tuple[int, ...]

    @property
    def marker(self) -> int:
        return self.words[0] & 0xFFFF if self.words else 0

    @property
    def header_tag(self) -> int:
        return self.words[0] >> 16 if self.words else 0

    @property
    def looks_like_bitknit2(self) -> bool:
        return self.marker == BITKNIT2_MARKER

    def to_dict(self) -> dict:
        return {
            "marker": self.marker,
            "header_tag": self.header_tag,
            "looks_like_bitknit2": self.looks_like_bitknit2,
            "words": list(self.words),
        }


@dataclass(frozen=True)
class BitKnitPlan:
    section_index: int
    compression_name: str
    compressed_size: int
    expanded_size: int
    first_16bit: int
    first_8bit: int
    header: BitKnitHeader | None
    payload_offset: int

    @property
    def is_empty(self) -> bool:
        return self.expanded_size == 0

    def to_dict(self) -> dict:
        return {
            "section_index": self.section_index,
            "compression": self.compression_name,
            "compressed_size": self.compressed_size,
            "expanded_size": self.expanded_size,
            "first_16bit": self.first_16bit,
            "first_8bit": self.first_8bit,
            "payload_offset": self.payload_offset,
            "header": self.header.to_dict() if self.header else None,
        }


@dataclass
class BitKnitBitReader:
    data: bytes
    byte_offset: int = BITKNIT_HEADER_SIZE
    bit_buffer: int = 0
    bit_count: int = 0

    @property
    def remaining_bytes(self) -> int:
        return max(0, len(self.data) - self.byte_offset)

    def read_bits(self, count: int) -> int:
        if count < 0:
            raise ValueError("bit count must be non-negative")
        if count == 0:
            return 0
        while self.bit_count < count:
            if self.byte_offset >= len(self.data):
                raise EOFError("bitknit bitstream exhausted")
            self.bit_buffer |= self.data[self.byte_offset] << self.bit_count
            self.byte_offset += 1
            self.bit_count += 8
        mask = (1 << count) - 1
        value = self.bit_buffer & mask
        self.bit_buffer >>= count
        self.bit_count -= count
        return value


@dataclass
class BitKnitHeaderBitReader:
    """LSB-first reader over the 48-byte BitKnit header/control seed area."""

    data: bytes
    bit_offset: int = 0
    byte_limit: int = BITKNIT_HEADER_SIZE

    @property
    def remaining_bits(self) -> int:
        return max(0, self.byte_limit * 8 - self.bit_offset)

    def read_bits(self, count: int) -> int:
        value = peek_bitknit_header_bits(self.data, self.bit_offset, count, self.byte_limit)
        self.bit_offset += count
        return value

    def read_signed_bits(self, count: int) -> int:
        value = self.read_bits(count)
        return sign_extend(value, count)


@dataclass
class BitKnitWordStream:
    data: bytes
    byte_offset: int = BITKNIT_HEADER_SIZE

    @property
    def remaining_bytes(self) -> int:
        return max(0, len(self.data) - self.byte_offset)

    def read_u32le(self) -> int:
        if self.byte_offset + 4 > len(self.data):
            raise EOFError("bitknit word stream exhausted")
        value = struct.unpack_from("<I", self.data, self.byte_offset)[0]
        self.byte_offset += 4
        return value & BITKNIT_WORD_MASK


@dataclass(frozen=True)
class BitKnitPayloadChunk:
    offset: int
    data: bytes
    words: tuple[int, ...]

    @property
    def is_full(self) -> bool:
        return len(self.data) == BITKNIT_CHUNK_SIZE


@dataclass(frozen=True)
class BitKnitChunkFsmCheckpoint:
    state: int
    input_offset: int
    marker: int
    block_word: int | None

    @property
    def uses_compressed_handler(self) -> bool:
        return self.state in (BITKNIT_FSM_COMPRESSED, BITKNIT_FSM_COMPRESSED_ALT)

    def to_dict(self) -> dict:
        return {
            "state": self.state,
            "input_offset": self.input_offset,
            "marker": self.marker,
            "block_word": self.block_word,
            "uses_compressed_handler": self.uses_compressed_handler,
        }


@dataclass
class BitKnitChunkStream:
    data: bytes
    byte_offset: int = BITKNIT_HEADER_SIZE
    chunk_size: int = BITKNIT_CHUNK_SIZE

    @property
    def remaining_bytes(self) -> int:
        return max(0, len(self.data) - self.byte_offset)

    def read_chunk(self) -> BitKnitPayloadChunk:
        if self.byte_offset >= len(self.data):
            raise EOFError("bitknit chunk stream exhausted")
        offset = self.byte_offset
        end = min(len(self.data), offset + self.chunk_size)
        chunk = self.data[offset:end]
        self.byte_offset = end
        padded = chunk + b"\x00" * ((4 - len(chunk) % 4) % 4)
        words = struct.unpack("<" + "I" * (len(padded) // 4), padded) if padded else ()
        return BitKnitPayloadChunk(offset=offset, data=chunk, words=words)


@dataclass
class BitKnitRangeDecoder:
    """Range-decoder state matching observed BitKnit byte/refill mechanics."""

    data: bytes
    byte_offset: int
    code: int
    span: int = BITKNIT_RANGE_INITIAL
    carry_bit: int = 0
    scale: int = 0

    @classmethod
    def from_payload(cls, data: bytes, payload_offset: int) -> "BitKnitRangeDecoder":
        if payload_offset >= len(data):
            raise EOFError("bitknit range decoder needs at least one payload byte")
        first = data[payload_offset]
        return cls(
            data=data,
            byte_offset=payload_offset + 1,
            code=first >> 1,
            carry_bit=first & 1,
        )

    @property
    def remaining_bytes(self) -> int:
        return max(0, len(self.data) - self.byte_offset)

    def normalize(self) -> None:
        while self.span <= BITKNIT_RANGE_NORMALIZE_LIMIT:
            if self.byte_offset >= len(self.data):
                raise EOFError("bitknit range decoder exhausted")
            byte = self.data[self.byte_offset]
            self.byte_offset += 1
            self.code = (((self.code * 2) + self.carry_bit) << 7) | (byte >> 1)
            self.span <<= 8
            self.carry_bit = byte & 1

    def peek(self, total: int) -> int:
        if total <= 0:
            raise ValueError("range total must be positive")
        self.normalize()
        scale = self.span // total
        if scale <= 0:
            raise ValueError("range total exceeds decoder span")
        self.scale = scale
        value = self.code // scale
        return min(value, total - 1)

    def peek_shifted(self, shift: int, cap: int) -> int:
        if shift < 0:
            raise ValueError("range shift must be non-negative")
        if cap <= 0:
            raise ValueError("range cap must be positive")
        self.normalize()
        scale = self.span >> shift
        if scale <= 0:
            raise ValueError("range shift exceeds decoder span")
        self.scale = scale
        value = self.code // scale
        return min(value, cap - 1)

    def take(self, symbol_range: BitKnitSymbolRange) -> None:
        self.normalize()
        scale = self.span // symbol_range.total
        if scale <= 0:
            raise ValueError("range total exceeds decoder span")
        self.scale = scale
        self.code -= symbol_range.low * scale
        if symbol_range.high < symbol_range.total:
            self.span = (symbol_range.high - symbol_range.low) * scale
        else:
            self.span -= symbol_range.low * scale

    def take_uniform(self, total: int) -> int:
        value = self.peek(total)
        low = value
        high = value + 1
        self.code -= low * self.scale
        if high < total:
            self.span = self.scale
        else:
            self.span -= low * self.scale
        return value

    def take_shifted(self, shift: int, cap: int) -> int:
        value = self.peek_shifted(shift, cap)
        low = value
        high = value + 1
        self.code -= low * self.scale
        if high < cap:
            self.span = self.scale
        else:
            self.span -= low * self.scale
        return value

    def take_scaled_range(self, low: int, width: int, total: int) -> None:
        if low < 0 or width <= 0 or total <= 0:
            raise ValueError("invalid scaled range")
        if low + width > total:
            raise ValueError("scaled range exceeds total")
        self.normalize()
        scale = self.span // total
        if scale <= 0:
            raise ValueError("range total exceeds decoder span")
        self.scale = scale
        self.code -= low * scale
        if low + width < total:
            self.span = width * scale
        else:
            self.span -= low * scale

    def take_from_model(self, model: "BitKnitAdaptiveModel") -> BitKnitSymbolRange:
        value = self.peek(model.total)
        decoded = model.lookup(value)
        self.take(decoded)
        model.update(decoded.symbol)
        return decoded


@dataclass(frozen=True)
class BitKnitSymbolRange:
    symbol: int
    low: int
    high: int
    total: int


def lookup_dll_table_symbol(
    profile: "BitKnitDllTableProfile",
    value: int,
) -> BitKnitSymbolRange:
    if value < 0:
        raise ValueError("DLL table value must be non-negative")
    cumulative = profile.cumulative
    if value >= cumulative[-1]:
        raise ValueError(f"DLL table value out of range: {value}")
    symbol = bisect_right(cumulative, value) - 1
    return BitKnitSymbolRange(
        symbol=symbol,
        low=cumulative[symbol],
        high=cumulative[symbol + 1],
        total=cumulative[-1],
    )


@dataclass
class BitKnitWordRangeDecoder:
    """15-bit/u16 range state used by the `0x10068f10` BitKnit2 loop."""

    data: bytes
    word_offset: int
    window: int

    @property
    def value15(self) -> int:
        return self.window & 0x7FFF

    @property
    def remaining_bytes(self) -> int:
        return max(0, len(self.data) - self.word_offset)

    def read_u16le(self) -> int:
        if self.word_offset + 2 > len(self.data):
            raise EOFError("bitknit word-range decoder exhausted")
        value = struct.unpack_from("<H", self.data, self.word_offset)[0]
        self.word_offset += 2
        return value

    def take(self, symbol_range: BitKnitSymbolRange) -> None:
        width = symbol_range.high - symbol_range.low
        if width <= 0:
            raise ValueError("DLL table symbol range must be non-empty")
        next_window = (
            self.value15
            + ((self.window >> 15) * width)
            - symbol_range.low
        )
        if next_window < 0x10000:
            next_window = ((next_window & 0xFFFF) << 16) | self.read_u16le()
        self.window = next_window & BITKNIT_WORD_MASK

    def take_from_dll_profile(self, profile: "BitKnitDllTableProfile") -> BitKnitSymbolRange:
        decoded = lookup_dll_table_symbol(profile, self.value15)
        self.take(decoded)
        return decoded

    def take_from_dll_model(self, model: "BitKnitDllAdaptiveModel") -> BitKnitSymbolRange:
        decoded = model.lookup(self.value15)
        self.take(decoded)
        model.update(decoded.symbol)
        return decoded


@dataclass(frozen=True)
class BitKnitState7Seed:
    input_offset: int
    initial_word: int
    primary_window: int
    range_window: int
    extra_window: int
    extra_bits: int

    def to_dict(self) -> dict:
        return {
            "input_offset": self.input_offset,
            "initial_word": f"0x{self.initial_word:08x}",
            "primary_window": f"0x{self.primary_window:08x}",
            "range_window": f"0x{self.range_window:08x}",
            "extra_window": f"0x{self.extra_window:08x}",
            "extra_bits": self.extra_bits,
        }


@dataclass(frozen=True)
class BitKnitState7PrefixResult:
    output: bytes
    trace: tuple[dict, ...]
    stopped: str | None
    checkpoint: BitKnitChunkFsmCheckpoint
    seed: BitKnitState7Seed


@dataclass(frozen=True)
class BitKnitState7StreamResult:
    output: bytes
    trace: tuple[dict, ...]
    stopped: str | None
    checkpoint: BitKnitChunkFsmCheckpoint
    first_seed: BitKnitState7Seed
    block_count: int


@dataclass
class BitKnitState7WindowDecoder:
    """Shared word pointer for state-7 range and extra-bit windows."""

    data: bytes
    word_offset: int
    range_window: int
    extra_window: int

    @classmethod
    def from_seed(cls, data: bytes, seed: BitKnitState7Seed) -> "BitKnitState7WindowDecoder":
        return cls(
            data=data,
            word_offset=seed.input_offset,
            range_window=seed.range_window,
            extra_window=seed.extra_window,
        )

    @property
    def value15(self) -> int:
        return self.range_window & 0x7FFF

    def read_u16le(self) -> int:
        value, self.word_offset = _read_bitknit_u16le(self.data, self.word_offset)
        return value

    def take_range(self, symbol_range: BitKnitSymbolRange) -> None:
        width = symbol_range.high - symbol_range.low
        if width <= 0:
            raise ValueError("DLL table symbol range must be non-empty")
        next_window = (
            self.value15
            + ((self.range_window >> 15) * width)
            - symbol_range.low
        )
        if next_window < 0x10000:
            next_window = ((next_window & 0xFFFF) << 16) | self.read_u16le()
        self.range_window = next_window & BITKNIT_WORD_MASK

    def take_from_dll_profile(self, profile: "BitKnitDllTableProfile") -> BitKnitSymbolRange:
        decoded = lookup_dll_table_symbol(profile, self.value15)
        self.take_range(decoded)
        return decoded

    def take_from_dll_model(self, model: "BitKnitDllAdaptiveModel") -> BitKnitSymbolRange:
        decoded = model.lookup(self.value15)
        self.take_range(decoded)
        model.update(decoded.symbol)
        return decoded

    def take_swapped_from_dll_profile(
        self,
        profile: "BitKnitDllTableProfile",
    ) -> BitKnitSymbolRange:
        old_other = self.extra_window
        decoded = self.take_from_dll_profile(profile)
        updated_current = self.range_window
        self.range_window = old_other
        self.extra_window = updated_current
        return decoded

    def take_swapped_from_dll_model(
        self,
        model: "BitKnitDllAdaptiveModel",
    ) -> BitKnitSymbolRange:
        old_other = self.extra_window
        decoded = self.take_from_dll_model(model)
        updated_current = self.range_window
        self.range_window = old_other
        self.extra_window = updated_current
        return decoded

    def read_extra_bits(self, count: int) -> int:
        if count < 0:
            raise ValueError("extra bit count must be non-negative")
        if count == 0:
            return 0
        before = self.extra_window
        shifted = before >> count
        if shifted < 0x10000:
            shifted = ((shifted & 0xFFFF) << 16) | self.read_u16le()
        self.extra_window = shifted & BITKNIT_WORD_MASK
        return before & ((1 << count) - 1)

    def read_swapped_extra_bits(self, count: int) -> int:
        if count < 0:
            raise ValueError("extra bit count must be non-negative")
        if count == 0:
            return 0
        before = self.range_window
        shifted = before >> count
        if shifted < 0x10000:
            shifted = ((shifted & 0xFFFF) << 16) | self.read_u16le()
        self.range_window, self.extra_window = self.extra_window, shifted & BITKNIT_WORD_MASK
        return before & ((1 << count) - 1)


@dataclass
class BitKnitRecentDistanceState:
    distances: list[int]
    selector: int = BITKNIT_INITIAL_RECENT_SELECTOR

    @classmethod
    def initial(cls) -> "BitKnitRecentDistanceState":
        return cls([BITKNIT_INITIAL_RECENT_DISTANCE] * BITKNIT_RECENT_DISTANCE_COUNT)

    def take_recent(self, symbol: int) -> tuple[int, int]:
        if symbol < 0 or symbol >= BITKNIT_RECENT_DISTANCE_COUNT:
            raise ValueError(f"recent distance symbol out of range: {symbol}")
        shift = symbol * 3
        slot = (self.selector >> shift) & 7
        distance = self.distances[slot]
        clear_mask = (0xFFFFFFF8 << shift) & BITKNIT_WORD_MASK
        low_mask = (~clear_mask) & BITKNIT_WORD_MASK
        promoted = (slot + ((self.selector << 3) & BITKNIT_WORD_MASK)) & low_mask
        preserved = self.selector & clear_mask
        self.selector = (promoted | preserved) & BITKNIT_WORD_MASK
        return distance, slot

    def insert_direct(self, distance: int) -> tuple[int, int]:
        if distance <= 0:
            raise ValueError("direct distance must be positive")
        source_slot = (self.selector >> 0x12) & 7
        target_slot = (self.selector >> 0x15) & 7
        self.distances[target_slot] = self.distances[source_slot]
        self.distances[source_slot] = distance
        return source_slot, target_slot


class BitKnitState7Core:
    def __init__(
        self,
        compressed: bytes,
        expanded_size: int,
        *,
        with_trace: bool = False,
    ) -> None:
        self.compressed = compressed
        self.literal_models = [
            BitKnitDllAdaptiveModel(make_dll_literal_table_profile()) for _ in range(4)
        ]
        self.long_distance_models = [
            BitKnitDllAdaptiveModel(make_dll_long_distance_table_profile()) for _ in range(4)
        ]
        self.short_distance_model = BitKnitDllAdaptiveModel(make_dll_short_distance_table_profile())
        self.output = BitKnitOutputWindow(expanded_size)
        self.recent = BitKnitRecentDistanceState.initial()
        self.last_distance = BITKNIT_INITIAL_RECENT_DISTANCE
        self.trace: list[dict] = []
        self.with_trace = with_trace
        self.step = 0

    def start_block(
        self,
        input_offset: int,
        *,
        emit_initial_literal: bool,
    ) -> tuple[BitKnitState7Seed, BitKnitState7WindowDecoder]:
        seed = seed_bitknit_state7_word_ranges(self.compressed, input_offset)
        if emit_initial_literal:
            initial_literal, decoder = make_state7_decoder_after_initial_literal(self.compressed, seed)
            if self.output.offset < self.output.expected_size:
                self.output.append_literal_delta(initial_literal)
                if self.with_trace:
                    self.trace.append(
                        {
                            "step": self.step,
                            "offset": self.output.offset - 1,
                            "context": (self.output.offset - 1) & 3,
                            "symbol": initial_literal,
                            "kind": "initial-literal",
                            "range_window": f"0x{decoder.range_window:08x}",
                            "extra_window": f"0x{decoder.extra_window:08x}",
                            "word_offset": decoder.word_offset,
                            "output_byte": self.output.data[-1],
                        }
                    )
                self.step += 1
        else:
            decoder = BitKnitState7WindowDecoder.from_seed(self.compressed, seed)
        return seed, decoder

    def decode_block(
        self,
        decoder: BitKnitState7WindowDecoder,
        target_size: int,
        *,
        max_steps: int | None = None,
        stop_at_word_sentinel: bool = False,
    ) -> str | None:
        while self.output.offset < target_size:
            if max_steps is not None and self.step >= max_steps:
                return "step limit reached"

            offset = self.output.offset
            context = offset & 3
            symbol_range = decoder.take_swapped_from_dll_model(self.literal_models[context])
            kind = classify_bitknit_symbol(symbol_range.symbol)
            row = {
                "step": self.step,
                "offset": offset,
                "context": context,
                "symbol": symbol_range.symbol,
                "kind": kind.name,
                "range": [symbol_range.low, symbol_range.high, symbol_range.total],
                "range_window": f"0x{decoder.range_window:08x}",
                "extra_window": f"0x{decoder.extra_window:08x}",
                "word_offset": decoder.word_offset,
            }
            if kind.name == "literal":
                self.output.append_literal_from_distance(symbol_range.symbol, self.last_distance)
                row["output_byte"] = self.output.data[-1]
            else:
                extra_value = 0
                if kind.needs_extra_bits:
                    extra_bits = symbol_range.symbol - 0x11F
                    extra_value = decoder.read_swapped_extra_bits(extra_bits)
                    row["length_extra_value"] = extra_value
                length = describe_length_symbol(symbol_range.symbol, extra_value)
                if self.output.offset + length.length > self.output.expected_size:
                    row["length"] = length.to_dict()
                    row["stop"] = "match exceeds output"
                    if self.with_trace:
                        self.trace.append(row)
                    return "match exceeds output"
                long_symbol = decoder.take_swapped_from_dll_model(
                    self.long_distance_models[offset & 3]
                ).symbol
                distance = describe_distance_symbol(long_symbol)
                row["length"] = length.to_dict()
                row["distance_symbol"] = distance.to_dict()
                if distance.is_recent:
                    match_distance, slot = self.recent.take_recent(long_symbol)
                    row["recent_slot"] = slot
                    row["resolved_distance"] = match_distance
                    self.output.copy_match(match_distance, length.length)
                    self.last_distance = match_distance
                else:
                    short_symbol = decoder.take_from_dll_model(self.short_distance_model).symbol
                    direct_extra = decoder.read_extra_bits(short_symbol)
                    direct = describe_direct_distance_symbols(
                        long_symbol,
                        short_symbol,
                        direct_extra,
                    )
                    row["direct_distance"] = direct.to_dict()
                    if direct.distance > self.output.offset:
                        row["stop"] = "direct distance exceeds output"
                        if self.with_trace:
                            self.trace.append(row)
                        return "direct distance exceeds output"
                    source_slot, target_slot = self.recent.insert_direct(direct.distance)
                    row["direct_recent_source_slot"] = source_slot
                    row["direct_recent_target_slot"] = target_slot
                    self.output.copy_match(direct.distance, length.length)
                    self.last_distance = direct.distance
                row["output_offset_after"] = self.output.offset
                row["last_distance"] = self.last_distance
            if self.with_trace:
                self.trace.append(row)
            self.step += 1
            if (
                stop_at_word_sentinel
                and self.output.offset < target_size
                and decoder.range_window == 0x10000
                and decoder.extra_window == 0x10000
            ):
                return "state7 block sentinel"
        return None


def make_state7_decoder_after_initial_literal(
    data: bytes,
    seed: BitKnitState7Seed,
) -> tuple[int, BitKnitState7WindowDecoder]:
    """Model the one-byte pre-loop literal at `0x100690ac..0x10069104`."""

    literal = seed.range_window & 0xFF
    offset = seed.input_offset
    extra_window = seed.range_window >> 8
    if extra_window < 0x10000:
        refill, offset = _read_bitknit_u16le(data, offset)
        extra_window = ((extra_window & 0xFFFF) << 16) | refill
    return literal, BitKnitState7WindowDecoder(
        data=data,
        word_offset=offset,
        range_window=seed.extra_window,
        extra_window=extra_window & BITKNIT_WORD_MASK,
    )


def decode_bitknit_state7_prefix(
    compressed: bytes,
    expanded_size: int,
    *,
    max_output_size: int | None = None,
    max_steps: int | None = None,
    with_trace: bool = False,
) -> BitKnitState7PrefixResult:
    if expanded_size < 0:
        raise ValueError("expanded size must be non-negative")
    target_size = expanded_size if max_output_size is None else min(max_output_size, expanded_size)
    if target_size < 0:
        raise ValueError("max output size must be non-negative")

    checkpoint = probe_bitknit_chunk_fsm_entry(compressed)
    if not checkpoint.uses_compressed_handler:
        raise ValueError(f"bitknit section does not enter state7: {checkpoint.to_dict()}")
    core = BitKnitState7Core(compressed, expanded_size, with_trace=with_trace)
    seed, decoder = core.start_block(checkpoint.input_offset, emit_initial_literal=True)
    stopped = core.decode_block(decoder, target_size, max_steps=max_steps)

    return BitKnitState7PrefixResult(
        output=bytes(core.output.data[:target_size]),
        trace=tuple(core.trace),
        stopped=stopped,
        checkpoint=checkpoint,
        seed=seed,
    )


def decode_bitknit_state7_stream(
    compressed: bytes,
    expanded_size: int,
    *,
    max_output_size: int | None = None,
    max_steps: int | None = None,
    with_trace: bool = False,
    emit_initial_literal_each_block: bool = False,
) -> BitKnitState7StreamResult:
    if expanded_size < 0:
        raise ValueError("expanded size must be non-negative")
    target_size = expanded_size if max_output_size is None else min(max_output_size, expanded_size)
    if target_size < 0:
        raise ValueError("max output size must be non-negative")

    checkpoint = probe_bitknit_chunk_fsm_entry(compressed)
    if not checkpoint.uses_compressed_handler:
        raise ValueError(f"bitknit section does not enter state7: {checkpoint.to_dict()}")

    core = BitKnitState7Core(compressed, expanded_size, with_trace=with_trace)
    input_offset = checkpoint.input_offset
    block_count = 0
    first_seed: BitKnitState7Seed | None = None
    stopped: str | None = None

    while core.output.offset < target_size:
        block_end = min(
            ((core.output.offset // BITKNIT2_BLOCK_OUTPUT_SIZE) + 1)
            * BITKNIT2_BLOCK_OUTPUT_SIZE,
            target_size,
        )
        seed, decoder = core.start_block(
            input_offset,
            emit_initial_literal=block_count == 0 or emit_initial_literal_each_block,
        )
        if first_seed is None:
            first_seed = seed
        block_count += 1
        stopped = core.decode_block(decoder, block_end, max_steps=max_steps)
        input_offset = decoder.word_offset
        if stopped is not None:
            break
        if core.output.offset >= target_size:
            break
        if decoder.range_window != 0x10000 or decoder.extra_window != 0x10000:
            stopped = (
                "state7 block ended without word-window sentinels "
                f"(range=0x{decoder.range_window:08x}, extra=0x{decoder.extra_window:08x})"
            )
            break
        if input_offset + 2 > len(compressed):
            stopped = "missing next block word"
            break
        block_word = struct.unpack_from("<H", compressed, input_offset)[0]
        if block_word == 0:
            stopped = "raw copy block not implemented"
            break

    if first_seed is None:
        first_seed = BitKnitState7Seed(
            input_offset=checkpoint.input_offset,
            initial_word=0,
            primary_window=0,
            range_window=0,
            extra_window=0,
            extra_bits=0,
        )
    return BitKnitState7StreamResult(
        output=bytes(core.output.data[:target_size]),
        trace=tuple(core.trace),
        stopped=stopped,
        checkpoint=checkpoint,
        first_seed=first_seed,
        block_count=block_count,
    )


def _read_bitknit_u16le(data: bytes, offset: int) -> tuple[int, int]:
    if offset + 2 > len(data):
        raise EOFError("bitknit u16 stream exhausted")
    return struct.unpack_from("<H", data, offset)[0], offset + 2


def seed_bitknit_state7_word_ranges(data: bytes, input_offset: int) -> BitKnitState7Seed:
    """Clean port of the `0x1006ba30` state-7 word-window seeding helper."""

    if input_offset < 0:
        raise ValueError("input offset must be non-negative")
    high, input_offset = _read_bitknit_u16le(data, input_offset)
    low, input_offset = _read_bitknit_u16le(data, input_offset)
    initial_word = ((high << 16) | low) & BITKNIT_WORD_MASK

    primary = initial_word >> 4
    if primary < 0x10000:
        refill, input_offset = _read_bitknit_u16le(data, input_offset)
        primary = ((primary << 16) | refill) & BITKNIT_WORD_MASK

    extra_bits = (initial_word & 0xF) + 0x10
    range_window = primary >> (extra_bits & 0xF)
    if range_window < 0x10000:
        refill, input_offset = _read_bitknit_u16le(data, input_offset)
        range_window = ((range_window << 16) | refill) & BITKNIT_WORD_MASK

    extra_source = primary
    if extra_bits >= 0x10:
        refill, input_offset = _read_bitknit_u16le(data, input_offset)
        extra_source = ((primary << 16) | refill) & BITKNIT_WORD_MASK

    mask = (1 << extra_bits) - 1
    extra_window = ((extra_source & mask) | (1 << extra_bits)) & BITKNIT_WORD_MASK
    return BitKnitState7Seed(
        input_offset=input_offset,
        initial_word=initial_word,
        primary_window=primary,
        range_window=range_window,
        extra_window=extra_window,
        extra_bits=extra_bits,
    )


@dataclass(frozen=True)
class BitKnitSymbolKind:
    name: str
    needs_extra_bits: bool = False


@dataclass(frozen=True)
class BitKnitMatchDescriptor:
    symbol: int
    index: int
    needs_extra_bits: bool

    def to_dict(self) -> dict:
        return {
            "symbol": self.symbol,
            "index": self.index,
            "needs_extra_bits": self.needs_extra_bits,
        }


@dataclass(frozen=True)
class BitKnitControlDescriptor:
    symbol: int
    index: int
    needs_extra_bits: bool
    low5: int
    group: int

    def to_dict(self) -> dict:
        return {
            "symbol": self.symbol,
            "index": self.index,
            "needs_extra_bits": self.needs_extra_bits,
            "low5": self.low5,
            "group": self.group,
        }


@dataclass(frozen=True)
class BitKnitDistanceDescriptor:
    symbol: int
    is_recent: bool
    recent_index: int | None
    direct_index: int | None
    low5: int | None
    group: int
    needs_extra_bits: bool

    def to_dict(self) -> dict:
        return {
            "symbol": self.symbol,
            "is_recent": self.is_recent,
            "recent_index": self.recent_index,
            "direct_index": self.direct_index,
            "low5": self.low5,
            "group": self.group,
            "needs_extra_bits": self.needs_extra_bits,
        }


@dataclass(frozen=True)
class BitKnitDirectDistanceDescriptor:
    long_symbol: int
    short_symbol: int
    extra_bits: int
    extra_value: int
    distance: int

    @property
    def needs_extra_bits(self) -> bool:
        return self.extra_bits > 0

    def to_dict(self) -> dict:
        return {
            "long_symbol": self.long_symbol,
            "short_symbol": self.short_symbol,
            "extra_bits": self.extra_bits,
            "extra_value": self.extra_value,
            "needs_extra_bits": self.needs_extra_bits,
            "distance": self.distance,
        }


@dataclass(frozen=True)
class BitKnitLengthDescriptor:
    symbol: int
    length: int
    extra_bits: int
    extra_value: int

    @property
    def needs_extra_bits(self) -> bool:
        return self.extra_bits > 0

    def to_dict(self) -> dict:
        return {
            "symbol": self.symbol,
            "length": self.length,
            "extra_bits": self.extra_bits,
            "extra_value": self.extra_value,
            "needs_extra_bits": self.needs_extra_bits,
        }


@dataclass(frozen=True)
class BitKnitExtraBitsRecord:
    low: int
    tag: int

    @property
    def bit_count(self) -> int:
        if self.tag & 0xC000 == 0xC000:
            return 16
        return self.tag & 0x3FFF

    @property
    def is_terminal16(self) -> bool:
        return (self.tag & 0xC000) == 0xC000


@dataclass(frozen=True)
class BitKnitDllTableProfile:
    cumulative: tuple[int, ...]
    update_weight: int
    rebuild_interval: int

    @property
    def weights(self) -> tuple[int, ...]:
        return tuple(
            self.cumulative[index + 1] - self.cumulative[index]
            for index in range(len(self.cumulative) - 1)
        )


class BitKnitDllAdaptiveModel:
    """DLL-style delayed adaptive cumulative table.

    The hot loop only increments per-symbol weights and decrements a countdown.
    When the countdown reaches zero, the DLL applies a larger final increment,
    rebuilds cumulative entries halfway toward the running weight sums, resets
    weights to 1, and resets the countdown.
    """

    def __init__(self, profile: BitKnitDllTableProfile) -> None:
        self.cumulative = list(profile.cumulative)
        self.weights = [1] * (len(self.cumulative) - 1)
        self.update_weight = profile.update_weight
        self.rebuild_interval = profile.rebuild_interval
        self.countdown = profile.rebuild_interval

    @property
    def total(self) -> int:
        return self.cumulative[-1]

    def lookup(self, value: int) -> BitKnitSymbolRange:
        return lookup_dll_table_symbol(
            BitKnitDllTableProfile(tuple(self.cumulative), self.update_weight, self.rebuild_interval),
            value,
        )

    def update(self, symbol: int) -> None:
        if symbol < 0 or symbol >= len(self.weights):
            raise ValueError(f"symbol out of range: {symbol}")
        self.weights[symbol] += BITKNIT_MODEL_UPDATE
        self.countdown -= 1
        if self.countdown <= 0:
            self.weights[symbol] += self.update_weight
            self._rebuild()

    def _rebuild(self) -> None:
        running = 0
        for index, weight in enumerate(self.weights):
            running += weight
            old = self.cumulative[index + 1]
            self.cumulative[index + 1] = (old + running) >> 1
            self.weights[index] = 1
        self.countdown = self.rebuild_interval


@dataclass(frozen=True)
class BitKnitDecodedToken:
    output_offset: int
    context: int
    symbol_range: BitKnitSymbolRange
    kind: BitKnitSymbolKind

    @property
    def symbol(self) -> int:
        return self.symbol_range.symbol

    def to_dict(self) -> dict:
        result = {
            "offset": self.output_offset,
            "context": self.context,
            "kind": self.kind.name,
            "needs_extra_bits": self.kind.needs_extra_bits,
            "symbol": self.symbol,
            "range": [
                self.symbol_range.low,
                self.symbol_range.high,
                self.symbol_range.total,
            ],
        }
        match_descriptor = describe_match_symbol(self.symbol)
        if match_descriptor:
            result["match"] = match_descriptor.to_dict()
        control_descriptor = describe_control_symbol(self.symbol)
        if control_descriptor:
            result["control"] = control_descriptor.to_dict()
            result["distance"] = describe_distance_symbol(control_descriptor.index).to_dict()
        return result


@dataclass(frozen=True)
class BitKnitSection6FillCandidate:
    bit_offset: int
    seed: int
    fill: int
    prefix_size: int

    def to_dict(self) -> dict:
        return {
            "bit_offset": self.bit_offset,
            "seed": self.seed,
            "fill": self.fill,
            "prefix_size": self.prefix_size,
        }


BITKNIT_LITERAL_KIND = BitKnitSymbolKind("literal")
BITKNIT_SHORT_MATCH_KIND = BitKnitSymbolKind("match")
BITKNIT_EXTRA_MATCH_KIND = BitKnitSymbolKind("match", needs_extra_bits=True)


def classify_bitknit_symbol(symbol: int) -> BitKnitSymbolKind:
    if symbol < 0:
        raise ValueError(f"symbol out of range: {symbol}")
    if symbol < BITKNIT_LITERAL_LIMIT:
        return BITKNIT_LITERAL_KIND
    if symbol < BITKNIT_EXTRA_MATCH_BASE:
        return BITKNIT_SHORT_MATCH_KIND
    return BITKNIT_EXTRA_MATCH_KIND


def describe_match_symbol(symbol: int) -> BitKnitMatchDescriptor | None:
    kind = classify_bitknit_symbol(symbol)
    if kind.name != "match":
        return None
    if symbol < BITKNIT_EXTRA_MATCH_BASE:
        return BitKnitMatchDescriptor(
            symbol=symbol,
            index=symbol - BITKNIT_SHORT_MATCH_BASE,
            needs_extra_bits=False,
        )
    return BitKnitMatchDescriptor(
        symbol=symbol,
        index=symbol - BITKNIT_EXTRA_MATCH_BASE,
        needs_extra_bits=True,
    )


def describe_control_symbol(symbol: int) -> BitKnitControlDescriptor | None:
    kind = classify_bitknit_symbol(symbol)
    if kind.name != "match":
        return None
    if symbol < BITKNIT_EXTRA_MATCH_BASE:
        index = symbol - BITKNIT_SHORT_MATCH_BASE
        return BitKnitControlDescriptor(
            symbol=symbol,
            index=index,
            needs_extra_bits=False,
            low5=index & 0x1F,
            group=0,
        )
    index = symbol - BITKNIT_EXTRA_MATCH_BASE
    return BitKnitControlDescriptor(
        symbol=symbol,
        index=index,
        needs_extra_bits=True,
        low5=index & 0x1F,
        group=index >> 5,
    )


def bitknit_extra_group(value: int) -> int:
    if value < 0:
        raise ValueError("extra value must be non-negative")
    if value == 0:
        return 0
    return value.bit_length() - 1


def describe_distance_symbol(symbol: int) -> BitKnitDistanceDescriptor:
    if symbol < 0:
        raise ValueError(f"distance symbol out of range: {symbol}")
    if symbol < BITKNIT_RECENT_DISTANCE_COUNT:
        return BitKnitDistanceDescriptor(
            symbol=symbol,
            is_recent=True,
            recent_index=symbol,
            direct_index=None,
            low5=None,
            group=0,
            needs_extra_bits=False,
        )

    direct_index = symbol - BITKNIT_RECENT_DISTANCE_COUNT
    high = (direct_index >> BITKNIT_DISTANCE_LOW_BITS) + 1
    group = bitknit_extra_group(high)
    return BitKnitDistanceDescriptor(
        symbol=symbol,
        is_recent=False,
        recent_index=None,
        direct_index=direct_index,
        low5=direct_index & ((1 << BITKNIT_DISTANCE_LOW_BITS) - 1),
        group=group,
        needs_extra_bits=group != 0,
    )


def describe_direct_distance_symbols(
    long_symbol: int,
    short_symbol: int,
    extra_value: int = 0,
) -> BitKnitDirectDistanceDescriptor:
    if long_symbol < BITKNIT_RECENT_DISTANCE_COUNT:
        raise ValueError("direct distance long symbol must be >= recent count")
    if long_symbol >= BITKNIT_DLL_LONG_DISTANCE_SYMBOLS:
        raise ValueError(f"direct distance long symbol out of range: {long_symbol}")
    if short_symbol < 0 or short_symbol >= BITKNIT_DLL_SHORT_DISTANCE_SYMBOLS:
        raise ValueError(f"direct distance short symbol out of range: {short_symbol}")

    mask = (1 << short_symbol) - 1
    extra_value &= mask
    distance = (
        long_symbol
        + ((extra_value - 1) << BITKNIT_DISTANCE_LOW_BITS)
        + ((1 << (BITKNIT_DISTANCE_LOW_BITS + short_symbol)) - 7)
    )
    return BitKnitDirectDistanceDescriptor(
        long_symbol=long_symbol,
        short_symbol=short_symbol,
        extra_bits=short_symbol,
        extra_value=extra_value,
        distance=distance,
    )


def describe_length_symbol(symbol: int, extra_value: int = 0) -> BitKnitLengthDescriptor:
    if symbol < BITKNIT_SHORT_MATCH_BASE:
        raise ValueError(f"length symbol out of range: {symbol}")
    if symbol < BITKNIT_EXTRA_MATCH_BASE:
        return BitKnitLengthDescriptor(
            symbol=symbol,
            length=symbol - 0xFE,
            extra_bits=0,
            extra_value=0,
        )
    extra_bits = symbol - 0x11F
    if extra_bits <= 0:
        raise ValueError(f"length extra bit count out of range: {symbol}")
    mask = (1 << extra_bits) - 1
    extra_value &= mask
    expanded_symbol = (1 << extra_bits) + 0x11E + extra_value
    return BitKnitLengthDescriptor(
        symbol=symbol,
        length=expanded_symbol - 0xFE,
        extra_bits=extra_bits,
        extra_value=extra_value,
    )


def split_bitknit_extra_bits(value: int, bit_count: int) -> tuple[BitKnitExtraBitsRecord, ...]:
    if value < 0:
        raise ValueError("extra value must be non-negative")
    if bit_count < 0:
        raise ValueError("extra bit count must be non-negative")
    if bit_count == 0:
        return ()
    mask = (1 << bit_count) - 1
    value &= mask
    if bit_count < 16:
        return (BitKnitExtraBitsRecord(low=value, tag=0x8000 | bit_count),)
    return (
        BitKnitExtraBitsRecord(low=value >> 16, tag=0x8000 | (bit_count - 16)),
        BitKnitExtraBitsRecord(low=value & 0xFFFF, tag=0xC000),
    )


def make_dll_linear_cumulative(symbol_count: int, step: int, divisor: int) -> tuple[int, ...]:
    if symbol_count <= 0 or step <= 0 or divisor <= 0:
        raise ValueError("table parameters must be positive")
    return tuple((index * step) // divisor for index in range(symbol_count + 1))


def make_dll_literal_table_profile() -> BitKnitDllTableProfile:
    # `0x1006b3d0`: first 264 cumulative entries use a reciprocal divide by
    # 0x108, then entries 264..300 are a dense tail up to 0x8000.
    head = tuple(
        ((0x3E0F83E1 * (index * 0x7FDC)) >> 32) >> 6
        for index in range(0x108)
    )
    tail = tuple((index + 0x7ED4) & 0xFFFF for index in range(0x108, 0x12D))
    cumulative = head + tail
    return BitKnitDllTableProfile(
        cumulative=cumulative,
        update_weight=0x2D5,
        rebuild_interval=0x400,
    )


def make_dll_short_distance_table_profile() -> BitKnitDllTableProfile:
    # `0x10068aa0`: 0x15 cumulative entries, 0x3ec update weight.
    cumulative = make_dll_linear_cumulative(
        BITKNIT_DLL_SHORT_DISTANCE_SYMBOLS,
        0x8000,
        BITKNIT_DLL_SHORT_DISTANCE_SYMBOLS,
    )
    return BitKnitDllTableProfile(
        cumulative=cumulative,
        update_weight=0x3EC,
        rebuild_interval=0x400,
    )


def make_dll_long_distance_table_profile() -> BitKnitDllTableProfile:
    # `0x10068b50`: 0x28 cumulative entries, 0x3d9 update weight.
    cumulative = make_dll_linear_cumulative(
        BITKNIT_DLL_LONG_DISTANCE_SYMBOLS,
        0x8000,
        BITKNIT_DLL_LONG_DISTANCE_SYMBOLS,
    )
    return BitKnitDllTableProfile(
        cumulative=cumulative,
        update_weight=0x3D9,
        rebuild_interval=0x400,
    )


def probe_bitknit_chunk_fsm_entry(data: bytes) -> BitKnitChunkFsmCheckpoint:
    """Model the first `0x10069770` FSM decisions before compressed handler."""

    if len(data) < 2:
        raise EOFError("bitknit chunk FSM needs marker word")
    marker = struct.unpack_from("<H", data, 0)[0]
    if marker != BITKNIT2_MARKER:
        return BitKnitChunkFsmCheckpoint(
            state=-1,
            input_offset=0,
            marker=marker,
            block_word=None,
        )

    input_offset = 2
    if input_offset + 2 > len(data):
        return BitKnitChunkFsmCheckpoint(
            state=BITKNIT_FSM_CHECK_BLOCK,
            input_offset=input_offset,
            marker=marker,
            block_word=None,
        )

    block_word = struct.unpack_from("<H", data, input_offset)[0]
    if block_word == 0:
        return BitKnitChunkFsmCheckpoint(
            state=BITKNIT_FSM_COPY_EVEN,
            input_offset=input_offset + 2,
            marker=marker,
            block_word=block_word,
        )
    return BitKnitChunkFsmCheckpoint(
        state=BITKNIT_FSM_COMPRESSED,
        input_offset=input_offset,
        marker=marker,
        block_word=block_word,
    )


def sign_extend(value: int, bits: int) -> int:
    if bits <= 0:
        raise ValueError("bit count must be positive")
    sign = 1 << (bits - 1)
    mask = (1 << bits) - 1
    value &= mask
    return (value ^ sign) - sign


def peek_bitknit_header_bits(
    data: bytes,
    bit_offset: int,
    count: int,
    byte_limit: int = BITKNIT_HEADER_SIZE,
) -> int:
    if bit_offset < 0:
        raise ValueError("bit offset must be non-negative")
    if count < 0:
        raise ValueError("bit count must be non-negative")
    if count == 0:
        return 0
    if bit_offset + count > byte_limit * 8:
        raise EOFError("bitknit header bitstream exhausted")
    value = 0
    for index in range(count):
        absolute_bit = bit_offset + index
        byte_index = absolute_bit >> 3
        bit_index = absolute_bit & 7
        if byte_index >= len(data) or byte_index >= byte_limit:
            raise EOFError("bitknit header bitstream exhausted")
        bit = (data[byte_index] >> bit_index) & 1
        value |= bit << index
    return value


def read_bitknit_header_signed_byte(data: bytes, bit_offset: int) -> int:
    return sign_extend(peek_bitknit_header_bits(data, bit_offset, 8), 8)


def read_section6_fill_from_header(data: bytes, bit_offset: int) -> int:
    seed = peek_bitknit_header_bits(data, bit_offset, 8)
    return sign_extend((seed - 2) & 0xFF, 8)


def make_section6_fill_record(fill: int, size: int = BITKNIT_SECTION6_RECORD_SIZE) -> bytes:
    if size <= 0:
        raise ValueError("record size must be positive")
    fill_byte = fill & 0xFF
    first = (fill + 2) & 0xFF
    return bytes([first]) + bytes([fill_byte]) * (size - 1)


def decode_section6_fill_record_from_header(
    data: bytes,
    bit_offset: int,
    size: int = BITKNIT_SECTION6_RECORD_SIZE,
) -> bytes:
    return make_section6_fill_record(
        read_section6_fill_from_header(data, bit_offset),
        size,
    )


def decode_section6_initial_fill_records_from_header(
    data: bytes,
    bit_offset: int,
    record_count: int = BITKNIT_SECTION6_INITIAL_FILL_RECORDS,
    record_size: int = BITKNIT_SECTION6_RECORD_SIZE,
) -> bytes:
    if record_count < 0:
        raise ValueError("record count must be non-negative")
    record = decode_section6_fill_record_from_header(data, bit_offset, record_size)
    return record * record_count


def find_section6_fill_candidates(
    data: bytes,
    oracle_prefix: bytes,
    *,
    byte_limit: int = BITKNIT_HEADER_SIZE,
    min_bit_offset: int = 0,
    max_bit_offset: int | None = None,
) -> tuple[BitKnitSection6FillCandidate, ...]:
    if min_bit_offset < 0:
        raise ValueError("min bit offset must be non-negative")
    if max_bit_offset is None:
        max_bit_offset = byte_limit * 8 - 8
    if max_bit_offset < min_bit_offset:
        return ()
    candidates = []
    prefix_size = min(len(oracle_prefix), BITKNIT_SECTION6_RECORD_SIZE)
    for bit_offset in range(min_bit_offset, max_bit_offset + 1):
        try:
            seed = peek_bitknit_header_bits(data, bit_offset, 8, byte_limit)
        except EOFError:
            break
        fill = sign_extend((seed - 2) & 0xFF, 8)
        if make_section6_fill_record(fill, prefix_size) == oracle_prefix[:prefix_size]:
            candidates.append(
                BitKnitSection6FillCandidate(
                    bit_offset=bit_offset,
                    seed=seed,
                    fill=fill,
                    prefix_size=prefix_size,
            )
        )
    return tuple(candidates)


def find_section6_control_fill_candidate(
    data: bytes,
    oracle_prefix: bytes,
    *,
    byte_limit: int = BITKNIT_HEADER_SIZE,
) -> BitKnitSection6FillCandidate | None:
    """Find the first section-6 fill seed after the marker/tag header word."""

    candidates = find_section6_fill_candidates(
        data,
        oracle_prefix,
        byte_limit=byte_limit,
        min_bit_offset=32,
    )
    return candidates[0] if candidates else None


class BitKnitOutputWindow:
    """Output helper for BitKnit's modulo-4 literal and match copy paths."""

    def __init__(self, expected_size: int) -> None:
        if expected_size < 0:
            raise ValueError("expected_size must be non-negative")
        self.expected_size = expected_size
        self.data = bytearray()

    @property
    def offset(self) -> int:
        return len(self.data)

    def context_index(self) -> int:
        return self.offset & (BITKNIT_CONTEXT_STRIDE - 1)

    def append_literal_delta(self, symbol: int) -> None:
        if symbol < 0 or symbol >= BITKNIT_LITERAL_LIMIT:
            raise ValueError(f"literal symbol out of range: {symbol}")
        self._ensure_can_write(1)
        base = (
            self.data[self.offset - BITKNIT_CONTEXT_STRIDE]
            if self.offset >= BITKNIT_CONTEXT_STRIDE
            else 0
        )
        self.data.append((base + symbol) & 0xFF)

    def append_literal_from_distance(self, symbol: int, distance: int) -> None:
        if symbol < 0 or symbol >= BITKNIT_LITERAL_LIMIT:
            raise ValueError(f"literal symbol out of range: {symbol}")
        if distance <= 0:
            raise ValueError("literal distance must be positive")
        self._ensure_can_write(1)
        base = self.data[self.offset - distance] if distance <= self.offset else 0
        self.data.append((base + symbol) & 0xFF)

    def copy_match(self, distance: int, length: int) -> None:
        if distance <= 0:
            raise ValueError("match distance must be positive")
        if length < 0:
            raise ValueError("match length must be non-negative")
        if distance > self.offset:
            raise ValueError("match distance exceeds output size")
        self._ensure_can_write(length)
        for _ in range(length):
            self.data.append(self.data[self.offset - distance])

    def finish(self) -> bytes:
        if self.offset != self.expected_size:
            raise ValueError(
                f"bitknit output size mismatch: {self.offset} != {self.expected_size}"
            )
        return bytes(self.data)

    def _ensure_can_write(self, count: int) -> None:
        if self.offset + count > self.expected_size:
            raise ValueError("bitknit output exceeds expected size")


class BitKnitAdaptiveModel:
    """Small adaptive cumulative model used by the clean BitKnit decoder.

    This is an independently written model skeleton from observed behavior:
    symbols gain 31 weight per hit and the table rebuilds when the countdown
    reaches zero. The exact initial distributions still need oracle matching.
    """

    def __init__(
        self,
        symbol_count: int,
        *,
        initial_weight: int = BITKNIT_INITIAL_WEIGHT,
        update_weight: int = BITKNIT_MODEL_UPDATE,
        rebuild_interval: int | None = None,
        initial_weights: tuple[int, ...] | None = None,
    ) -> None:
        if symbol_count <= 0:
            raise ValueError("symbol_count must be positive")
        if initial_weight <= 0:
            raise ValueError("initial_weight must be positive")
        if update_weight <= 0:
            raise ValueError("update_weight must be positive")
        if initial_weights is not None and len(initial_weights) != symbol_count:
            raise ValueError("initial_weights length must match symbol_count")
        self.symbol_count = symbol_count
        self.update_weight = update_weight
        self.rebuild_interval = rebuild_interval or max(1, symbol_count)
        self.weights = (
            list(initial_weights)
            if initial_weights is not None
            else [initial_weight] * symbol_count
        )
        self.countdown = self.rebuild_interval
        self.cumulative: list[int] = []
        self.total = 0
        self.rebuild()

    def lookup(self, value: int) -> BitKnitSymbolRange:
        if value < 0 or value >= self.total:
            raise ValueError(f"model value out of range: {value}")
        symbol = bisect_right(self.cumulative, value) - 1
        return BitKnitSymbolRange(
            symbol=symbol,
            low=self.cumulative[symbol],
            high=self.cumulative[symbol + 1],
            total=self.total,
        )

    def take(self, value: int) -> BitKnitSymbolRange:
        decoded = self.lookup(value)
        self.update(decoded.symbol)
        return decoded

    def range_for(self, symbol: int) -> BitKnitSymbolRange:
        if symbol < 0 or symbol >= self.symbol_count:
            raise ValueError(f"symbol out of range: {symbol}")
        return BitKnitSymbolRange(
            symbol=symbol,
            low=self.cumulative[symbol],
            high=self.cumulative[symbol + 1],
            total=self.total,
        )

    def update(self, symbol: int) -> None:
        if symbol < 0 or symbol >= self.symbol_count:
            raise ValueError(f"symbol out of range: {symbol}")
        self.weights[symbol] += self.update_weight
        self.countdown -= 1
        if self.countdown <= 0:
            self.rescale()
        else:
            self.rebuild()

    def rescale(self) -> None:
        self.weights = [max(1, (weight + 1) // 2) for weight in self.weights]
        self.countdown = self.rebuild_interval
        self.rebuild()

    def rebuild(self) -> None:
        cumulative = [0]
        total = 0
        for weight in self.weights:
            if weight <= 0:
                raise ValueError("model weights must be positive")
            total += weight
            cumulative.append(total)
        self.cumulative = cumulative
        self.total = total


@dataclass(frozen=True)
class BitKnitModelProfile:
    literal_weights: tuple[int, ...] | None = None
    match_weights: tuple[int, ...] | None = None


BITKNIT_UNIFORM_PROFILE = BitKnitModelProfile()


def make_zero_biased_literal_profile(zero_weight: int) -> BitKnitModelProfile:
    if zero_weight <= 0:
        raise ValueError("zero_weight must be positive")
    if zero_weight == BITKNIT_INITIAL_WEIGHT:
        return BITKNIT_UNIFORM_PROFILE
    weights = (zero_weight,) + (BITKNIT_INITIAL_WEIGHT,) * (BITKNIT_LITERAL_SYMBOLS - 1)
    return BitKnitModelProfile(literal_weights=weights)


def make_literal_models(
    profile: BitKnitModelProfile = BITKNIT_UNIFORM_PROFILE,
) -> tuple[BitKnitAdaptiveModel, ...]:
    return tuple(
        BitKnitAdaptiveModel(
            BITKNIT_LITERAL_SYMBOLS,
            initial_weights=profile.literal_weights,
        )
        for _ in range(4)
    )


def make_match_models(
    profile: BitKnitModelProfile = BITKNIT_UNIFORM_PROFILE,
) -> tuple[BitKnitAdaptiveModel, ...]:
    return tuple(
        BitKnitAdaptiveModel(
            BITKNIT_MATCH_SYMBOLS,
            initial_weights=profile.match_weights,
        )
        for _ in range(4)
    )


@dataclass
class BitKnitDecoderState:
    plan: BitKnitPlan
    compressed: bytes
    literal_models: tuple[BitKnitAdaptiveModel, ...]
    match_models: tuple[BitKnitAdaptiveModel, ...]
    input_words: BitKnitWordStream
    input_chunks: BitKnitChunkStream
    range_decoder: BitKnitRangeDecoder

    def context_index(self, output_offset: int) -> int:
        return output_offset & (BITKNIT_CONTEXT_STRIDE - 1)

    def literal_model(self, output_offset: int) -> BitKnitAdaptiveModel:
        return self.literal_models[self.context_index(output_offset)]

    def match_model(self, output_offset: int) -> BitKnitAdaptiveModel:
        return self.match_models[self.context_index(output_offset)]

    def take_literal_symbol(self, output_offset: int, model_value: int) -> BitKnitSymbolRange:
        return self.literal_model(output_offset).take(model_value)

    def take_match_symbol(self, output_offset: int, model_value: int) -> BitKnitSymbolRange:
        return self.match_model(output_offset).take(model_value)

    def decode_literal_symbol(self, output_offset: int) -> BitKnitSymbolRange:
        return self.range_decoder.take_from_model(self.literal_model(output_offset))

    def decode_match_symbol(self, output_offset: int) -> BitKnitSymbolRange:
        return self.range_decoder.take_from_model(self.match_model(output_offset))

    def decode_literal_token(self, output_offset: int) -> BitKnitDecodedToken:
        decoded = self.decode_literal_symbol(output_offset)
        return BitKnitDecodedToken(
            output_offset=output_offset,
            context=self.context_index(output_offset),
            symbol_range=decoded,
            kind=classify_bitknit_symbol(decoded.symbol),
        )


def make_decoder_state(
    plan: BitKnitPlan,
    compressed: bytes,
    profile: BitKnitModelProfile = BITKNIT_UNIFORM_PROFILE,
) -> BitKnitDecoderState:
    return BitKnitDecoderState(
        plan=plan,
        compressed=compressed,
        literal_models=make_literal_models(profile),
        match_models=make_match_models(profile),
        input_words=make_word_stream(plan, compressed),
        input_chunks=make_chunk_stream(plan, compressed),
        range_decoder=make_range_decoder(plan, compressed),
    )


def parse_bitknit_plan(section: GR2Section, compressed: bytes) -> BitKnitPlan:
    if section.compression not in (COMPRESSION_BITKNIT, COMPRESSION_BITKNIT2):
        raise ValueError(f"section {section.index} is not BitKnit")
    if section.expanded_size == 0:
        return BitKnitPlan(
            section_index=section.index,
            compression_name=section.compression_name,
            compressed_size=len(compressed),
            expanded_size=section.expanded_size,
            first_16bit=section.first_16bit,
            first_8bit=section.first_8bit,
            header=None,
            payload_offset=0,
        )
    if len(compressed) < BITKNIT_HEADER_SIZE:
        raise ValueError(
            f"{section.compression_name} section {section.index} too short for header"
        )
    header = BitKnitHeader(struct.unpack_from("<12I", compressed, 0))
    return BitKnitPlan(
        section_index=section.index,
        compression_name=section.compression_name,
        compressed_size=len(compressed),
        expanded_size=section.expanded_size,
        first_16bit=section.first_16bit,
        first_8bit=section.first_8bit,
        header=header,
        payload_offset=BITKNIT_HEADER_SIZE,
    )


def make_bit_reader(plan: BitKnitPlan, compressed: bytes) -> BitKnitBitReader:
    if plan.is_empty:
        return BitKnitBitReader(compressed, 0)
    return BitKnitBitReader(compressed, plan.payload_offset)


def make_word_stream(plan: BitKnitPlan, compressed: bytes) -> BitKnitWordStream:
    if plan.is_empty:
        return BitKnitWordStream(compressed, 0)
    return BitKnitWordStream(compressed, plan.payload_offset)


def make_chunk_stream(plan: BitKnitPlan, compressed: bytes) -> BitKnitChunkStream:
    if plan.is_empty:
        return BitKnitChunkStream(compressed, 0)
    return BitKnitChunkStream(compressed, plan.payload_offset)


def make_range_decoder(plan: BitKnitPlan, compressed: bytes) -> BitKnitRangeDecoder:
    if plan.is_empty:
        return BitKnitRangeDecoder(compressed, 0, 0, 0, 0)
    return BitKnitRangeDecoder.from_payload(compressed, plan.payload_offset)


def make_header_bit_reader(
    plan: BitKnitPlan,
    compressed: bytes,
    bit_offset: int = 0,
) -> BitKnitHeaderBitReader:
    if plan.is_empty:
        return BitKnitHeaderBitReader(compressed, bit_offset, 0)
    return BitKnitHeaderBitReader(compressed, bit_offset, plan.payload_offset)


def decompress_bitknit(section: GR2Section, compressed: bytes) -> bytes:
    plan = parse_bitknit_plan(section, compressed)
    if plan.is_empty:
        return b""
    if (
        section.compression == COMPRESSION_BITKNIT
        and plan.header
        and plan.header.looks_like_bitknit2
    ):
        last_error = None
        for candidate in _classic_bitknit_state7_candidates(compressed):
            try:
                return _decode_bitknit_raw_copy_blocks(candidate, section.expanded_size)
            except DecompressionUnsupported:
                pass
            try:
                return _decode_classic_bitknit_state7_chunks(candidate, section.expanded_size)
            except (EOFError, ValueError, DecompressionUnsupported) as exc:
                last_error = exc
            try:
                result = decode_bitknit_state7_stream(candidate, section.expanded_size)
            except (EOFError, ValueError) as exc:
                last_error = exc
                continue
            if result.stopped is None and len(result.output) == section.expanded_size:
                return result.output
            last_error = DecompressionUnsupported(
                f"size={len(result.output)}/{section.expanded_size}, stopped={result.stopped}"
            )
        raise DecompressionUnsupported(
            f"{section.compression_name} native state7 decode incomplete ({last_error})"
        )
    if (
        section.compression == COMPRESSION_BITKNIT2
        and plan.header
        and plan.header.looks_like_bitknit2
    ):
        try:
            return _decode_bitknit_raw_copy_blocks(compressed, section.expanded_size)
        except DecompressionUnsupported:
            pass
        try:
            result = decode_bitknit_state7_stream(compressed, section.expanded_size)
        except (EOFError, ValueError) as exc:
            raise DecompressionUnsupported(
                f"{section.compression_name} native state7 decode stopped: {exc}"
            ) from exc
        if result.stopped is None and len(result.output) == section.expanded_size:
            return result.output
        raise DecompressionUnsupported(
            f"{section.compression_name} native state7 decode incomplete "
            f"(size={len(result.output)}/{section.expanded_size}, stopped={result.stopped})"
        )
    detail = ""
    if plan.header and plan.header.looks_like_bitknit2:
        detail = f", marker=0x{plan.header.marker:04x}, tag=0x{plan.header.header_tag:x}"
    raise DecompressionUnsupported(
        f"{section.compression_name} entropy/LZ core not implemented yet "
        f"(parsed 48-byte header{detail})"
    )


def _classic_bitknit_state7_candidates(compressed: bytes) -> tuple[bytes, ...]:
    """Return public-safe fmt3 wrapper variants that share BitKnit2 state7 core."""

    candidates = [compressed]
    if len(compressed) >= 6 and compressed[:6] == b"\xb1u\xb1u\xb1u":
        candidates.append(compressed[4:])
    if len(compressed) >= 4 and compressed[:4] == b"\xb1u\xb1u":
        candidates.append(compressed[:2] + compressed[4:])
    return tuple(candidates)


def _decode_classic_bitknit_state7_chunks(compressed: bytes, expanded_size: int) -> bytes:
    """Decode classic Granny fmt3 BitKnit chunks with per-chunk model reset."""

    output = bytearray()
    input_offset = 2
    last_error = None
    core: BitKnitState7Core | None = None
    while len(output) < expanded_size:
        if compressed[input_offset:input_offset + 2] == b"\x00\x00":
            copy_start = input_offset + 2
            copy_end = compressed.find(b"\xb1u", copy_start)
            if copy_end < 0:
                copy_end = len(compressed)
            raw = compressed[copy_start:copy_end]
            remaining = expanded_size - len(output)
            output.extend(raw[:remaining])
            input_offset = copy_end
            if len(output) >= expanded_size:
                return bytes(output)
            input_offset = _skip_classic_bitknit_marker(compressed, input_offset)
            if input_offset >= len(compressed):
                last_error = DecompressionUnsupported("classic bitknit chunk input exhausted")
                break
            core = None
            continue

        if core is None:
            core = BitKnitState7Core(compressed, expanded_size)
            core.output.data.extend(output)
            emit_initial_literal = True
        else:
            emit_initial_literal = False
        seed, decoder = core.start_block(input_offset, emit_initial_literal=emit_initial_literal)
        stopped = core.decode_block(
            decoder,
            expanded_size,
            stop_at_word_sentinel=True,
        )
        output = core.output.data
        if stopped is None and len(output) == expanded_size:
            return bytes(output)
        if stopped != "state7 block sentinel":
            last_error = DecompressionUnsupported(
                f"size={len(output)}/{expanded_size}, stopped={stopped}"
            )
            break

        input_offset = decoder.word_offset
        try:
            input_offset = _skip_classic_bitknit_marker(compressed, input_offset)
        except DecompressionUnsupported:
            pass
        else:
            core = None
        if input_offset >= len(compressed):
            last_error = DecompressionUnsupported("classic bitknit chunk input exhausted")
            break

    raise DecompressionUnsupported(
        f"classic bitknit chunk decode incomplete ({last_error})"
    )


def _skip_classic_bitknit_marker(compressed: bytes, input_offset: int) -> int:
    if compressed[input_offset:input_offset + 4] == b"\xb1u\xb1u":
        return input_offset + 4
    if compressed[input_offset:input_offset + 2] == b"\xb1u":
        return input_offset + 2
    raise DecompressionUnsupported("classic bitknit chunk marker missing")


def _decode_bitknit_raw_copy_blocks(compressed: bytes, expanded_size: int) -> bytes:
    """Decode BitKnit copy-only 64 KiB blocks seen in Granny fmt3 samples."""

    if expanded_size < 0:
        raise ValueError("expanded size must be non-negative")
    if expanded_size == 0:
        return b""
    if len(compressed) < 4 or compressed[:2] != b"\xb1u":
        raise DecompressionUnsupported("bitknit raw-copy marker missing")

    block_count = (expanded_size + BITKNIT2_BLOCK_OUTPUT_SIZE - 1) // BITKNIT2_BLOCK_OUTPUT_SIZE
    inter_block_skip = max(0, block_count - 1) * 2
    prefix_size = len(compressed) - expanded_size - inter_block_skip
    if prefix_size not in (4, 6, 8):
        raise DecompressionUnsupported(
            f"bitknit raw-copy prefix size unsupported: {prefix_size}"
        )
    if prefix_size > len(compressed):
        raise DecompressionUnsupported("bitknit raw-copy prefix exceeds input")

    output = bytearray()
    input_offset = prefix_size
    remaining = expanded_size
    for block_index in range(block_count):
        if block_index:
            input_offset += 2
        block_size = min(BITKNIT2_BLOCK_OUTPUT_SIZE, remaining)
        block_end = input_offset + block_size
        if block_end > len(compressed):
            raise DecompressionUnsupported("bitknit raw-copy block exceeds input")
        output.extend(compressed[input_offset:block_end])
        input_offset = block_end
        remaining -= block_size

    if len(output) != expanded_size:
        raise DecompressionUnsupported(
            f"bitknit raw-copy size mismatch: {len(output)}/{expanded_size}"
        )
    return bytes(output)
```

## third_party/blendergranny/io_scene_gr2/gr2/decompress/oodle0.py

```python
"""Clean Oodle0 decompressor for Granny2 section streams."""

from __future__ import annotations

import struct
from dataclasses import dataclass

from io_scene_gr2.gr2.decompress.base import DecompressionError
from io_scene_gr2.gr2.file import GR2Section

OODLE0_HEADER_SIZE = 36
OODLE0_BLOCK_COUNT = 3
OFFSET_SPLIT_SHIFT = 2
LOW_OFFSET_MASK = (1 << OFFSET_SPLIT_SHIFT) - 1
MAX_LENS = 64
LONG_LENGTHS = (MAX_LENS * 2, MAX_LENS * 3, MAX_LENS * 4, MAX_LENS * 8)
MASK32 = 0xFFFFFFFF
MASK31 = 0x7FFFFFFF
_OFFSET_BYTE_MASK = 0x1FF


@dataclass(frozen=True)
class Oodle0LZHeader:
    max_offset_and_byte: int
    uniq_offset_and_byte: int
    uniq_lens: int

    @property
    def max_byte_value(self) -> int:
        return self.max_offset_and_byte & _OFFSET_BYTE_MASK

    @property
    def max_offset(self) -> int:
        return self.max_offset_and_byte >> 9

    @property
    def unique_byte_values(self) -> int:
        return self.uniq_offset_and_byte & _OFFSET_BYTE_MASK

    @property
    def unique_offsets(self) -> int:
        return self.uniq_offset_and_byte >> 9

    @property
    def unique_length_contexts(self) -> tuple[int, int, int, int]:
        return tuple((self.uniq_lens >> (index * 8)) & 0xFF for index in range(4))

    def to_dict(self) -> dict:
        return {
            "max_byte_value": self.max_byte_value,
            "max_offset": self.max_offset,
            "unique_byte_values": self.unique_byte_values,
            "unique_offsets": self.unique_offsets,
            "unique_length_contexts": list(self.unique_length_contexts),
        }

    def length_unique(self, index: int) -> int:
        group = min(index // (MAX_LENS // 4), 3)
        return (self.uniq_lens >> ((3 - group) * 8)) & 0xFF


@dataclass(frozen=True)
class Oodle0Block:
    index: int
    output_start: int
    output_end: int
    header: Oodle0LZHeader

    @property
    def output_size(self) -> int:
        return max(0, self.output_end - self.output_start)

    @property
    def is_empty(self) -> bool:
        return self.output_size == 0

    def to_dict(self) -> dict:
        return {
            "index": self.index,
            "output_start": self.output_start,
            "output_end": self.output_end,
            "output_size": self.output_size,
            "header": self.header.to_dict(),
        }


@dataclass(frozen=True)
class Oodle0Plan:
    section_index: int
    expanded_size: int
    blocks: tuple[Oodle0Block, ...]
    bitstream_offset: int = OODLE0_HEADER_SIZE

    def to_dict(self) -> dict:
        return {
            "section_index": self.section_index,
            "expanded_size": self.expanded_size,
            "bitstream_offset": self.bitstream_offset,
            "blocks": [block.to_dict() for block in self.blocks],
        }


def parse_oodle0_plan(section: GR2Section, compressed: bytes) -> Oodle0Plan:
    if len(compressed) < OODLE0_HEADER_SIZE:
        raise ValueError(f"oodle0 section {section.index} too short for header")
    words = struct.unpack_from("<9I", compressed, 0)
    headers = tuple(
        Oodle0LZHeader(*words[index * 3 : index * 3 + 3])
        for index in range(OODLE0_BLOCK_COUNT)
    )
    stops = _block_stops(section)
    blocks = tuple(
        Oodle0Block(
            index=index,
            output_start=stops[index],
            output_end=stops[index + 1],
            header=headers[index],
        )
        for index in range(OODLE0_BLOCK_COUNT)
    )
    return Oodle0Plan(
        section_index=section.index,
        expanded_size=section.expanded_size,
        blocks=blocks,
    )


class _VarBits:
    def __init__(self, data: bytes, offset: int):
        self.data = data
        self.cur = offset
        self.bits = 0
        self.bitlen = 0

    def get(self, nbits: int) -> int:
        if nbits == 0:
            return 0
        mask = (1 << nbits) - 1
        if self.bitlen >= nbits:
            value = self.bits & mask
            self.bits >>= nbits
            self.bitlen -= nbits
            return value
        word = _u32le_padded(self.data, self.cur)
        self.cur += 4
        value = (self.bits | (word << self.bitlen)) & mask
        self.bits = word >> (nbits - self.bitlen)
        self.bitlen = self.bitlen + 32 - nbits
        return value

    def get1(self) -> int:
        if self.bitlen:
            value = self.bits & 1
            self.bits >>= 1
            self.bitlen -= 1
            return value
        word = _u32le_padded(self.data, self.cur)
        self.cur += 4
        self.bits = word >> 1
        self.bitlen = 31
        return word & 1


class _ArithBits:
    def __init__(self, data: bytes, offset: int):
        self.vbits = _VarBits(data, offset)
        self.high = MASK31
        self.low = 0
        self.code = _bit_reverse(self.vbits.get(31), 31)

    def get_count(self, scale: int) -> int:
        if scale <= 0:
            return 0
        return ((((self.code - self.low) + 1) * scale) - 1) // ((self.high - self.low) + 1)

    def get_value(self, scale: int) -> int:
        value = self.get_count(scale)
        if value >= scale:
            value = scale - 1
        self.remove(value, 1, scale)
        return value

    def remove(self, start: int, count: int, scale: int) -> None:
        if scale <= 0:
            return
        high = self.high
        low = self.low
        code = self.code
        width = (high - low) + 1
        high = _u32(low + ((width * (start + count)) // scale) - 1)
        low = _u32(low + ((width * start) // scale))
        if ((high ^ low) & 0x40000000) == 0:
            while ((high ^ low) & 0x7F800000) == 0:
                low = _u32(low << 8)
                high = _u32((high << 8) | 0xFF)
                byte = self.vbits.get(8)
                code = _u32((code << 8) | (_bit_reverse(byte & 0xF, 4) << 4) | _bit_reverse(byte >> 4, 4))
            if ((high ^ low) & 0x78000000) == 0:
                low = _u32(low << 4)
                high = _u32((high << 4) | 0xF)
                code = _u32((code << 4) | _bit_reverse(self.vbits.get(4), 4))
            while ((high ^ low) & 0x40000000) == 0:
                low = _u32(low << 1)
                high = _u32((high << 1) | 1)
                code = _u32((code << 1) | self.vbits.get1())
        while (low & 0x20000000) and not (high & 0x20000000):
            code = _u32(code ^ 0x20000000)
            low = _u32((low & 0x1FFFFFFF) << 1)
            high = _u32((high << 1) | 0x40000001)
            code = _u32((code << 1) | self.vbits.get1())
        self.high = high & MASK31
        self.low = low & MASK31
        self.code = code & MASK31


class _EscapeSymbol:
    def __init__(self, index: int):
        self.index = index


class _ArithModel:
    def __init__(self, unique_values: int):
        self.unique_values = unique_values
        count = _aligned_count(unique_values)
        self.totals = [0] * 16
        self.counts = [0] * count
        self.values = [0] * count
        self.number = 0
        self.bin_size, self.bin_shift, self.last_bin_start = _best_shift(unique_values + 1)
        self._quick_increment(0, 0x30003)

    def decompress(self, bits: _ArithBits) -> int | _EscapeSymbol:
        if self.totals[15] >= 16384:
            self._rescale()
        scale = self.totals[15]
        count = bits.get_count(scale)
        pos, start = self._find_pos(count)
        old_count = self.counts[pos]
        self._increment_totals(pos, 0x10001)
        bits.remove(start, old_count, self.totals[15] - 1)
        self.counts[pos] = _u16(self.counts[pos] + 1)
        if pos == 0:
            self.number += 1
            if self.number >= len(self.counts):
                raise DecompressionError("oodle0 escape exceeded model capacity")
            self._quick_increment(self.number, 0x20002)
            if self.number == self.unique_values:
                self._decrement_counts(0, self.counts[0])
            return _EscapeSymbol(self.number)
        return self.values[pos]

    def set_escaped(self, marker: _EscapeSymbol, value: int) -> None:
        self.values[marker.index] = _u16(value)

    def _find_pos(self, count: int) -> tuple[int, int]:
        start = 0
        for pos, entry_count in enumerate(self.counts):
            if count < start + entry_count:
                return pos, start
            start += entry_count
        raise DecompressionError(f"oodle0 model count {count} outside total {start}")

    def _quick_increment(self, value: int, amount: int) -> None:
        self._increment_totals(value, amount)
        self.counts[value] = _u16(self.counts[value] + (amount & 0xFFFF))

    def _increment_totals(self, value: int, amount: int) -> None:
        amount &= MASK32
        low = amount & 0xFFFF
        if value >= self.last_bin_start:
            self.totals[15] = _u16(self.totals[15] + low)
            return
        bin_index = value >> self.bin_shift
        if bin_index & 1:
            self.totals[bin_index] = _u16(self.totals[bin_index] + low)
            bin_index += 1
        for pair_index in range(bin_index >> 1, 8):
            self._add_total_pair_u32(pair_index, amount)

    def _add_total_pair_u32(self, pair_index: int, amount: int) -> None:
        index = pair_index * 2
        old = self.totals[index] | (self.totals[index + 1] << 16)
        new = (old + amount) & MASK32
        self.totals[index] = new & 0xFFFF
        self.totals[index + 1] = (new >> 16) & 0xFFFF

    def _decrement_counts(self, value: int, amount: int) -> None:
        neg = (-amount) & MASK32
        packed = (((neg - 1) & MASK32) << 16) | (neg & 0xFFFF)
        self._quick_increment(value, packed)

    def _rescale(self) -> None:
        self.bin_size, self.bin_shift, self.last_bin_start = _best_shift(self.number + 1)
        bins = [0] * 16
        self.counts[0] >>= 1
        bins[0 if 0 < self.last_bin_start else 15] += self.counts[0]
        max_count = 0
        max_pos = 0
        index = 1
        done = False
        while index <= self.number and not done:
            while self.counts[index] <= 1:
                if index < self.number:
                    self.counts[index] = self.counts[self.number]
                    self.values[index] = self.values[self.number]
                    self.counts[self.number] = 0
                    self.number -= 1
                else:
                    self.counts[index] = 0
                    self.number -= 1
                    done = True
                    break
            if done:
                break
            self.counts[index] >>= 1
            if self.counts[index] > max_count:
                max_count = self.counts[index]
                max_pos = index
            bins[(index >> self.bin_shift) if index < self.last_bin_start else 15] += self.counts[index]
            index += 1
        if max_count:
            swap_pos = ((self.number >> self.bin_shift) << self.bin_shift) if self.number < self.last_bin_start else self.last_bin_start
            if swap_pos == 0:
                swap_pos = 1
            if max_pos != swap_pos:
                old_count = self.counts[swap_pos]
                self.counts[swap_pos] = self.counts[max_pos]
                bins[(swap_pos >> self.bin_shift) if swap_pos < self.last_bin_start else 15] += -old_count + self.counts[swap_pos]
                bins[(max_pos >> self.bin_shift) if max_pos < self.last_bin_start else 15] += old_count - self.counts[swap_pos]
                self.counts[max_pos] = old_count
                self.values[swap_pos], self.values[max_pos] = self.values[max_pos], self.values[swap_pos]
        if self.number != self.unique_values and self.counts[0] == 0:
            self.counts[0] = _u16(self.counts[0] + 2)
            bins[0 if 0 < self.last_bin_start else 15] += 2
        running = 0
        for index, value in enumerate(bins):
            running += value
            self.totals[index] = _u16(running)


class _LZState:
    def __init__(self, header: Oodle0LZHeader):
        self.max_bytes = header.max_byte_value
        self.max_offsets = header.max_offset
        self.max_offset_low = min(self.max_offsets, LOW_OFFSET_MASK + 1)
        self.bytes = _ArithModel(header.unique_byte_values)
        self.lengths = [_ArithModel(header.length_unique(index)) for index in range(MAX_LENS + 1)]
        self.offset_low = _ArithModel(self.max_offset_low)
        self.offset_high = _ArithModel(header.unique_offsets)
        self.bytes_decompressed = 0
        self.last_length = 0


def decompress_oodle0(section: GR2Section, compressed: bytes) -> bytes:
    if section.expanded_size == 0:
        return b""
    if len(compressed) < OODLE0_HEADER_SIZE:
        raise DecompressionError("oodle0 data too short for 3 block headers")
    plan = parse_oodle0_plan(section, compressed)
    bits = _ArithBits(compressed, OODLE0_HEADER_SIZE)
    output = bytearray()
    for block in plan.blocks:
        if block.is_empty:
            continue
        state = _LZState(block.header)
        _decode_block(state, bits, output, block.output_end)
    if len(output) != section.expanded_size:
        raise DecompressionError(f"oodle0 decompressed {len(output)}, expected {section.expanded_size}")
    return bytes(output)


def _decode_block(state: _LZState, bits: _ArithBits, output: bytearray, stop: int) -> None:
    while len(output) < stop:
        previous_length = state.last_length
        length_symbol = _read_model_symbol(state.lengths[previous_length], bits, MAX_LENS + 1)
        state.last_length = length_symbol
        if length_symbol:
            length = (
                LONG_LENGTHS[length_symbol - (MAX_LENS - 3)]
                if length_symbol >= MAX_LENS - 3
                else length_symbol + 1
            )
            low = _read_model_symbol(state.offset_low, bits, state.max_offset_low)
            high_scale = (min(state.max_offsets, state.bytes_decompressed) >> OFFSET_SPLIT_SHIFT) + 1
            high = _read_model_symbol(state.offset_high, bits, high_scale)
            distance = low + 1 + (high << OFFSET_SPLIT_SHIFT)
            if distance <= 0 or distance > len(output):
                raise DecompressionError(f"oodle0 invalid copy distance {distance}")
            for _ in range(length):
                output.append(output[-distance])
            state.bytes_decompressed += length
        else:
            literal = _read_model_symbol(state.bytes, bits, state.max_bytes)
            if not 0 <= literal <= 255:
                raise DecompressionError(f"oodle0 invalid literal {literal}")
            output.append(literal)
            state.bytes_decompressed += 1


def _read_model_symbol(model: _ArithModel, bits: _ArithBits, escape_scale: int) -> int:
    value = model.decompress(bits)
    if isinstance(value, _EscapeSymbol):
        escaped = bits.get_value(escape_scale)
        model.set_escaped(value, escaped)
        return escaped
    return int(value)


def _block_stops(section: GR2Section) -> tuple[int, int, int, int]:
    expanded = section.expanded_size
    first_16bit = _clamp_stop(section.first_16bit, expanded)
    first_8bit = _clamp_stop(section.first_8bit, expanded)
    if first_8bit < first_16bit:
        first_8bit = first_16bit
    return 0, first_16bit, first_8bit, expanded


def _clamp_stop(value: int, expanded_size: int) -> int:
    if value <= 0:
        return 0
    if value >= expanded_size:
        return expanded_size
    return value


def _u16(value: int) -> int:
    return value & 0xFFFF


def _u32(value: int) -> int:
    return value & MASK32


def _u32le_padded(data: bytes, offset: int) -> int:
    if offset + 4 > len(data):
        chunk = data[offset:] + b"\x00" * (offset + 4 - len(data))
    else:
        chunk = data[offset : offset + 4]
    return int.from_bytes(chunk, "little")


def _bit_reverse(value: int, nbits: int) -> int:
    result = 0
    for index in range(nbits):
        result = (result << 1) | ((value >> index) & 1)
    return result


def _aligned_count(unique_values: int) -> int:
    return (unique_values + 5) & ~3


def _best_shift(value: int) -> tuple[int, int, int]:
    if value < 6:
        return 0, 15, 0
    best_max = MASK32
    best_bin = 0
    for index in range(16):
        size = 1 << index
        bins = min((value + size - 1) // size, 16)
        last = value - (size * (bins - 1))
        if last < size:
            last = size
        if last < best_max:
            best_bin = index
            best_max = last
        if size > value:
            break
    bin_size = 1 << best_bin
    return bin_size, best_bin, 15 * bin_size
```

## third_party/blendergranny/io_scene_gr2/gr2/decompress/oodle1.py

```python
"""Clean Oodle1 decompressor for Granny2 section streams."""

from __future__ import annotations

from dataclasses import dataclass

from io_scene_gr2.gr2.decompress.base import DecompressionError
from io_scene_gr2.gr2.file import GR2Section

NORM_BITS = 14
NORM_COUNT = 1 << NORM_BITS
ADJ_SUMS = NORM_COUNT << 1
MAX_LENS = 64
LOW_BITS = 2
MED_BITS = 8
ADDRESS_MASK = 3
LONG_LENGTHS = (128, 192, 256, 512)


@dataclass(frozen=True)
class _EscapeRef:
    index: int


class _ArithBits:
    def __init__(self, data: bytes):
        if not data:
            raise DecompressionError("oodle1 missing arithmetic stream")
        first = data[0]
        self.data = data
        self.ptr = 1
        self.low = first >> 1
        self.range = 1 << 7
        self.underflow = 0
        self.carry = first & 1

    def renorm(self) -> None:
        if self.range > 0x800000:
            return
        low = self.low
        rng = self.range
        carry = self.carry
        while rng <= 0x800000:
            low = ((low + low + carry) << 7) & 0xFFFFFFFF
            byte = self.data[self.ptr] if self.ptr < len(self.data) else 0
            self.ptr += 1
            low |= byte >> 1
            carry = byte & 1
            rng = (rng << 8) & 0xFFFFFFFF
        self.low = low
        self.range = rng
        self.carry = carry

    def get_bits(self, bits: int, scale: int) -> int:
        self.renorm()
        self.underflow = self.range >> bits
        value = self.low // self.underflow if self.underflow else 0
        return scale - 1 if value >= scale else value

    def get_bits_value(self, bits: int, scale: int) -> int:
        self.renorm()
        divisor = self.range >> bits
        value = self.low // divisor if divisor else 0
        if value >= scale:
            value = scale - 1
        offset = (divisor * value) & 0xFFFFFFFF
        self.low = (self.low - offset) & 0xFFFFFFFF
        self.range = divisor if value + 1 < scale else (self.range - offset) & 0xFFFFFFFF
        return value

    def get_value(self, scale: int) -> int:
        self.renorm()
        divisor = self.range // scale if scale else 0
        value = self.low // divisor if divisor else 0
        if value >= scale:
            value = scale - 1
        offset = (divisor * value) & 0xFFFFFFFF
        self.low = (self.low - offset) & 0xFFFFFFFF
        self.range = divisor if value + 1 < scale else (self.range - offset) & 0xFFFFFFFF
        return value

    def remove(self, start: int, count: int, scale: int) -> None:
        offset = (self.underflow * start) & 0xFFFFFFFF
        self.low = (self.low - offset) & 0xFFFFFFFF
        if start + count < scale:
            self.range = (self.underflow * count) & 0xFFFFFFFF
        else:
            self.range = (self.range - offset) & 0xFFFFFFFF


class _ArithModel:
    def __init__(self, max_value: int, unique_count: int):
        self.unique_count = unique_count
        size = max(max_value, unique_count) + 16
        self.values = [0] * size
        self.single_counts = [0] * (size + 16)
        self.summed_counts = [NORM_COUNT + ADJ_SUMS] * (size + 32)
        self.singles_tot = 4
        self.update_tot = 8
        self.update_range = 4
        self.rescale_tot = min(max(max_value * 32, 256), 15160)
        update_max = max_value * 2
        if update_max < 128:
            update_max = 128
        else:
            cap = (self.rescale_tot >> 1) - 32
            if update_max > cap:
                update_max = cap
        self.update_max = max(0, update_max)
        self.singles_length = 0
        self.summed_length = 0
        self.single_counts[0] = 4
        self.summed_counts[0] = ADJ_SUMS
        for index in range(1, 6):
            self.summed_counts[index] = NORM_COUNT + ADJ_SUMS

    def _rescale(self) -> None:
        self.single_counts[0] >>= 1
        self.singles_tot = self.single_counts[0]
        max_count = 0
        max_pos = 0
        index = 1
        while index <= self.singles_length:
            while self.single_counts[index] <= 1:
                if index < self.singles_length:
                    self.single_counts[index] = self.single_counts[self.singles_length]
                    self.single_counts[self.singles_length] = 0
                    self.values[index] = self.values[self.singles_length]
                    self.singles_length -= 1
                else:
                    self.single_counts[index] = 0
                    self.singles_length -= 1
                    break
            if index > self.singles_length:
                break
            self.single_counts[index] >>= 1
            self.singles_tot += self.single_counts[index]
            if self.single_counts[index] > max_count:
                max_count = self.single_counts[index]
                max_pos = index
            index += 1
        if max_count and self.singles_length:
            last = self.singles_length
            if max_pos != last:
                self.single_counts[last], self.single_counts[max_pos] = (
                    self.single_counts[max_pos],
                    self.single_counts[last],
                )
                self.values[last], self.values[max_pos] = self.values[max_pos], self.values[last]
        if self.singles_length != self.unique_count and self.single_counts[0] == 0:
            self.single_counts[0] += 1
            self.singles_tot += 1

    def _update_counts(self) -> None:
        adj = (NORM_COUNT * 8) // max(self.singles_tot, 1)
        total = (((self.single_counts[0]) * adj) >> 3) + ADJ_SUMS
        self.summed_counts[0] = ADJ_SUMS
        index = 1
        while True:
            self.summed_counts[index] = total
            if index > self.singles_length:
                break
            total += ((self.single_counts[index] * adj) >> 3)
            index += 1
        next_range = self.update_range << 1
        if next_range > self.update_max:
            self.update_tot = self.singles_tot + self.update_max
        else:
            self.update_range = next_range
            self.update_tot = self.singles_tot + next_range
        self.summed_length = self.singles_length
        for pad in range(1, 8):
            self.summed_counts[self.summed_length + pad] = NORM_COUNT + ADJ_SUMS

    def decode(self, bits: _ArithBits) -> int | _EscapeRef:
        if self.singles_tot >= self.update_tot:
            if self.update_tot >= self.rescale_tot:
                self._rescale()
            self._update_counts()

        offset = bits.get_bits(NORM_BITS, NORM_COUNT) + ADJ_SUMS
        low = 0
        high = self.summed_length + 1
        while low < high:
            mid = (low + high + 1) >> 1
            if self.summed_counts[mid] > offset:
                high = mid - 1
            else:
                low = mid
        index = low
        bits.remove(
            self.summed_counts[index] - ADJ_SUMS,
            self.summed_counts[index + 1] - self.summed_counts[index],
            NORM_COUNT,
        )
        self.single_counts[index] += 1
        self.singles_tot += 1

        if index <= 0:
            if self.singles_length == self.summed_length or bits.get_bits_value(1, 2) == 0:
                index = self._new_index()
                return _EscapeRef(index)
            index = bits.get_value(self.singles_length - self.summed_length) + self.summed_length + 1
            if index < len(self.single_counts):
                self.single_counts[index] += 2
                self.singles_tot += 2
            return self.values[index] if index < len(self.values) else 0
        return self.values[index]

    def _new_index(self) -> int:
        if self.singles_length >= len(self.single_counts) - 1:
            return 0
        self.singles_length += 1
        index = self.singles_length
        self.single_counts[index] += 2
        self.singles_tot += 2
        if self.singles_length == self.unique_count:
            self.singles_tot -= self.single_counts[0]
            self.single_counts[0] = 0
        return index

    def set_decompressed_symbol(self, ref: _EscapeRef, value: int) -> None:
        if ref.index >= len(self.values):
            self.values.extend([0] * (ref.index + 1 - len(self.values)))
        self.values[ref.index] = value


def _decode_symbol(model: _ArithModel, bits: _ArithBits, escape_scale: int) -> int:
    value = model.decode(bits)
    if isinstance(value, _EscapeRef):
        escaped = bits.get_value(escape_scale)
        model.set_decompressed_symbol(value, escaped)
        return escaped
    return value


def _u32le(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 4], "little")


def _top_offset(value: int) -> int:
    return value >> (LOW_BITS + MED_BITS)


def _init_lz_models(header: bytes):
    max_offset_and_byte = _u32le(header, 0)
    unique_offset_and_byte = _u32le(header, 4)
    unique_lengths = _u32le(header, 8)
    max_byte = max_offset_and_byte & 0x1FF
    max_offset = max_offset_and_byte >> 9
    unique_byte = unique_offset_and_byte & 0x1FF
    low_count = min(max_offset + 1, 1 << LOW_BITS)
    mid_count = min((max_offset >> LOW_BITS) + 1, 1 << MED_BITS)
    high_count = _top_offset(max_offset) + 1
    byte_models = [_ArithModel(max_byte - 1, unique_byte) for _ in range(ADDRESS_MASK + 1)]
    length_models = []
    last_unique = 0
    for group in range(4):
        last_unique = (unique_lengths >> ((3 - group) * 8)) & 0xFF
        for _ in range(MAX_LENS // 4):
            length_models.append(_ArithModel(MAX_LENS, last_unique))
    for _ in range((MAX_LENS // 4) * 4, MAX_LENS + 1):
        length_models.append(_ArithModel(MAX_LENS, last_unique))
    offset_low = _ArithModel(low_count - 1, low_count)
    offset_high = _ArithModel(high_count - 1, _top_offset(unique_offset_and_byte >> 9) + 1)
    offset_mid = [_ArithModel(mid_count - 1, mid_count) for _ in range(high_count)]
    return byte_models, length_models, offset_low, offset_high, offset_mid, max_byte, max_offset, low_count, mid_count


def _decode_lz_block(models, bits: _ArithBits, output: bytearray, start: int, stop: int) -> int:
    byte_models, length_models, offset_low, offset_high, offset_mid, max_byte, max_offset_limit, low_count, mid_count = models
    last_length = 0
    pos = start
    bytes_decompressed = 0
    while pos < stop:
        length_symbol = _decode_symbol(length_models[min(last_length, MAX_LENS)], bits, MAX_LENS + 1)
        last_length = length_symbol
        if length_symbol:
            max_offset = min(max_offset_limit, bytes_decompressed)
            length = LONG_LENGTHS[length_symbol - (MAX_LENS - 3)] if length_symbol >= MAX_LENS - 3 else length_symbol + 1
            low = _decode_symbol(offset_low, bits, low_count)
            high = _decode_symbol(offset_high, bits, _top_offset(max_offset) + 1)
            if max_offset >= ((1 << MED_BITS) << LOW_BITS):
                mid_scale = 1 << MED_BITS
            else:
                mid_scale = (max_offset >> LOW_BITS) + 1
            mid = _decode_symbol(offset_mid[high], bits, mid_scale)
            distance = low + 1 + (mid << LOW_BITS) + (high << (LOW_BITS + MED_BITS))
            for _ in range(length):
                if pos < len(output) and pos >= distance:
                    output[pos] = output[pos - distance]
                pos += 1
            bytes_decompressed += length
        else:
            output[pos] = _decode_symbol(byte_models[pos & ADDRESS_MASK], bits, max_byte) & 0xFF
            pos += 1
            bytes_decompressed += 1
    return pos


def decompress_oodle1(section: GR2Section, compressed: bytes) -> bytes:
    if section.expanded_size == 0:
        return b""
    if len(compressed) < 36:
        raise DecompressionError("oodle1 data too short for 3 block headers")
    bits = _ArithBits(compressed[36:])
    output = bytearray(section.expanded_size)
    pos = 0
    for block_index, stop in enumerate((section.first_16bit, section.first_8bit, section.expanded_size)):
        if pos >= stop:
            if block_index == 2:
                break
            continue
        models = _init_lz_models(compressed[block_index * 12 : block_index * 12 + 12])
        pos = _decode_lz_block(models, bits, output, pos, min(stop, section.expanded_size))
    if pos < section.expanded_size:
        raise DecompressionError(f"oodle1 decompressed {pos}, expected {section.expanded_size}")
    return bytes(output)
```

## third_party/blendergranny/io_scene_gr2/gr2/export.py

```python
"""Blender scene collection for future GR2 export writers."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class ExportVertexWeight:
    bone: str
    weight: float


@dataclass(frozen=True)
class ExportVertex:
    position: tuple[float, float, float]
    normal: tuple[float, float, float]
    uv: tuple[float, float] | None
    weights: tuple[ExportVertexWeight, ...]


@dataclass(frozen=True)
class ExportMesh:
    name: str
    vertices: tuple[ExportVertex, ...]
    indices: tuple[int, ...]
    material_names: tuple[str, ...]
    material_indices: tuple[int, ...] = ()
    material_texture_files: tuple[str, ...] = ()
    material_texture_sizes: tuple[tuple[int, int] | None, ...] = ()
    bone_binding_names: tuple[str, ...] = ()

    @property
    def triangle_count(self) -> int:
        return len(self.indices) // 3


@dataclass(frozen=True)
class ExportBone:
    name: str
    parent: str
    matrix_local: tuple[float, ...]
    granny_rest_local: tuple[float, ...] = ()
    inverse_world_transform: tuple[float, ...] = ()
    granny_transform_flags: int = 0
    granny_local_position: tuple[float, ...] = ()
    granny_local_orientation_xyzw: tuple[float, ...] = ()


@dataclass(frozen=True)
class ExportSkeleton:
    name: str
    bones: tuple[ExportBone, ...]
    initial_placement: tuple[float, ...] = ()


@dataclass(frozen=True)
class ExportScene:
    meshes: tuple[ExportMesh, ...]
    skeletons: tuple[ExportSkeleton, ...]

    @property
    def mesh_count(self) -> int:
        return len(self.meshes)

    @property
    def skeleton_count(self) -> int:
        return len(self.skeletons)

    @property
    def vertex_count(self) -> int:
        return sum(len(mesh.vertices) for mesh in self.meshes)

    @property
    def triangle_count(self) -> int:
        return sum(mesh.triangle_count for mesh in self.meshes)


def collect_export_scene(
    context,
    *,
    selected_only: bool = True,
    apply_modifiers: bool = False,
    scale: float = 1.0,
    flip_uv_v: bool = True,
) -> ExportScene:
    """Collect Blender data into writer-ready, Blender-free dataclasses."""

    objects = _export_objects(context, selected_only=selected_only)
    mesh_objects = [obj for obj in objects if getattr(obj, "type", None) == "MESH"]
    armatures = _referenced_armatures(mesh_objects)
    armatures.extend(obj for obj in objects if getattr(obj, "type", None) == "ARMATURE" and obj not in armatures)
    return ExportScene(
        meshes=tuple(
            _collect_mesh(context, obj, apply_modifiers=apply_modifiers, scale=scale, flip_uv_v=flip_uv_v)
            for obj in mesh_objects
        ),
        skeletons=tuple(_collect_skeleton(obj) for obj in armatures),
    )


def write_export_manifest(path: str | Path, scene: ExportScene) -> Path:
    """Write exporter manifest used to verify raw-writer inputs."""

    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "format": "blendergranny.export_manifest.v1",
        "exporter_schema": 2,
        "summary": {
            "mesh_count": scene.mesh_count,
            "skeleton_count": scene.skeleton_count,
            "vertex_count": scene.vertex_count,
            "triangle_count": scene.triangle_count,
        },
        "scene": asdict(scene),
    }
    output_path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    return output_path


def _export_objects(context, *, selected_only: bool) -> list:
    if selected_only:
        selected = list(getattr(context, "selected_objects", ()))
        if selected:
            return selected
    return [obj for obj in context.scene.objects if not getattr(obj, "hide_get", lambda: False)()]


def _collect_mesh(context, obj, *, apply_modifiers: bool, scale: float, flip_uv_v: bool) -> ExportMesh:
    depsgraph = context.evaluated_depsgraph_get() if apply_modifiers else None
    eval_obj = obj.evaluated_get(depsgraph) if depsgraph is not None else obj
    mesh = eval_obj.to_mesh() if depsgraph is not None else obj.data
    try:
        mesh.calc_loop_triangles()
        uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None
        vertices: list[ExportVertex] = []
        indices: list[int] = []
        material_indices: list[int] = []
        vertex_cache: dict[tuple, int] = {}
        for triangle in mesh.loop_triangles:
            material_indices.append(int(triangle.material_index))
            for loop_index in triangle.loops:
                loop = mesh.loops[loop_index]
                source_vertex = mesh.vertices[loop.vertex_index]
                uv = None
                if uv_layer is not None:
                    raw_uv = uv_layer[loop_index].uv
                    uv = (float(raw_uv.x), float(1.0 - raw_uv.y) if flip_uv_v else float(raw_uv.y))
                vertex = ExportVertex(
                    position=_scaled_tuple(source_vertex.co, scale),
                    normal=_float_tuple(loop.normal, 3),
                    uv=uv,
                    weights=_vertex_weights(obj, source_vertex.index, source_vertex.co),
                )
                key = _vertex_key(vertex)
                index = vertex_cache.get(key)
                if index is None:
                    index = len(vertices)
                    vertex_cache[key] = index
                    vertices.append(vertex)
                indices.append(index)
        return ExportMesh(
            name=obj.name,
            vertices=tuple(vertices),
            indices=tuple(indices),
            material_names=tuple(_material_names(mesh.materials)),
            material_indices=tuple(material_indices),
            material_texture_files=tuple(_material_texture_files(mesh.materials)),
            material_texture_sizes=tuple(_material_texture_sizes(mesh.materials)),
            bone_binding_names=_stored_bone_bindings(obj),
        )
    finally:
        if depsgraph is not None:
            eval_obj.to_mesh_clear()


def _vertex_weights(obj, vertex_index: int, position=None) -> tuple[ExportVertexWeight, ...]:
    groups = []
    vertex_groups = obj.vertex_groups
    for item in obj.data.vertices[vertex_index].groups:
        if item.weight <= 0.0 or item.group >= len(vertex_groups):
            continue
        groups.append(ExportVertexWeight(vertex_groups[item.group].name, float(item.weight)))
    groups.sort(key=lambda item: item.weight, reverse=True)
    total = sum(item.weight for item in groups[:4])
    if total <= 0.0:
        fallback_bone = _rigid_mesh_fallback_bone(obj, position)
        return (ExportVertexWeight(fallback_bone, 1.0),) if fallback_bone else ()
    return tuple(ExportVertexWeight(item.bone, item.weight / total) for item in groups[:4])


def _rigid_mesh_fallback_bone(obj, position=None) -> str:
    name = getattr(obj, "name", "").lower()
    armature = _object_armature(obj)
    if armature is None:
        return ""
    bones = getattr(getattr(armature, "data", None), "bones", {})
    if "face" in name or "head" in name:
        for preferred in ("Bip01 Head", "Head", "head"):
            if preferred in bones:
                return preferred
        for bone in bones:
            if "head" in bone.name.lower():
                return bone.name
    if position is not None:
        best_name = ""
        best_distance = None
        for bone in bones:
            head = getattr(bone, "head_local", None)
            if head is None:
                continue
            dx = float(position.x) - float(head.x)
            dy = float(position.y) - float(head.y)
            dz = float(position.z) - float(head.z)
            distance = dx * dx + dy * dy + dz * dz
            if best_distance is None or distance < best_distance:
                best_name = bone.name
                best_distance = distance
        if best_name:
            return best_name
    return ""


def _object_armature(obj):
    for modifier in getattr(obj, "modifiers", ()):
        if modifier.type == "ARMATURE" and modifier.object is not None:
            return modifier.object
    return None


def _referenced_armatures(mesh_objects: Iterable) -> list:
    armatures = []
    for obj in mesh_objects:
        for modifier in obj.modifiers:
            if modifier.type == "ARMATURE" and modifier.object is not None and modifier.object not in armatures:
                armatures.append(modifier.object)
    return armatures


def _collect_skeleton(obj) -> ExportSkeleton:
    bones = tuple(
        ExportBone(
            name=bone.name,
            parent=bone.parent.name if bone.parent else "",
            matrix_local=_matrix_tuple(bone.matrix_local),
            granny_rest_local=_prop_tuple(bone, "gr2_granny_rest_local", 16),
            inverse_world_transform=_prop_tuple(bone, "gr2_inverse_world_transform", 16),
            granny_transform_flags=_pose_prop_int(obj, bone.name, "gr2_transform_flags"),
            granny_local_position=_pose_prop_tuple(obj, bone.name, "gr2_local_position", 3),
            granny_local_orientation_xyzw=_pose_prop_tuple(obj, bone.name, "gr2_local_orientation_xyzw", 4),
        )
        for bone in obj.data.bones
    )
    return ExportSkeleton(
        name=obj.name,
        bones=bones,
        initial_placement=_skeleton_initial_placement(bones),
    )


def _material_names(materials) -> list[str]:
    names = []
    for index, material in enumerate(materials):
        names.append(material.name if material is not None else f"Material_{index}")
    return names


def _material_texture_files(materials) -> list[str]:
    return [_material_texture_file(material) if material is not None else "" for material in materials]


def _material_texture_sizes(materials) -> list[tuple[int, int] | None]:
    return [_material_texture_size(material) if material is not None else None for material in materials]


def _stored_bone_bindings(obj) -> tuple[str, ...]:
    value = obj.get("gr2_bone_bindings") if hasattr(obj, "get") else None
    if not value:
        return ()
    names = []
    for item in str(value).split(","):
        name = item.strip()
        if name and name not in names:
            names.append(name)
    return tuple(names)


def _material_texture_file(material) -> str:
    for key in ("gr2_texture_file", "gr2_texture_resolved"):
        value = material.get(key)
        if value:
            return str(value)
    if getattr(material, "use_nodes", False) and material.node_tree is not None:
        for node in material.node_tree.nodes:
            image = getattr(node, "image", None)
            if getattr(node, "type", "") == "TEX_IMAGE" and image is not None:
                filepath = getattr(image, "filepath", "")
                return str(filepath or getattr(image, "name", ""))
    return ""


def _material_texture_size(material) -> tuple[int, int] | None:
    width = material.get("gr2_texture_width")
    height = material.get("gr2_texture_height")
    if width is not None and height is not None:
        return int(width), int(height)
    if getattr(material, "use_nodes", False) and material.node_tree is not None:
        for node in material.node_tree.nodes:
            image = getattr(node, "image", None)
            if getattr(node, "type", "") == "TEX_IMAGE" and image is not None and len(image.size) >= 2:
                return int(image.size[0]), int(image.size[1])
    return None


def _scaled_tuple(vector, scale: float) -> tuple[float, float, float]:
    return (float(vector.x) * scale, float(vector.y) * scale, float(vector.z) * scale)


def _float_tuple(vector, width: int) -> tuple[float, ...]:
    return tuple(float(vector[index]) for index in range(width))


def _matrix_tuple(matrix) -> tuple[float, ...]:
    return tuple(float(matrix[row][column]) for row in range(4) for column in range(4))


def _prop_tuple(item, name: str, count: int) -> tuple[float, ...]:
    value = item.get(name)
    if value is None or len(value) != count:
        return ()
    return tuple(float(item) for item in value)


def _pose_prop_tuple(obj, bone_name: str, name: str, count: int) -> tuple[float, ...]:
    pose_bone = getattr(getattr(obj, "pose", None), "bones", {}).get(bone_name)
    if pose_bone is None:
        return ()
    return _prop_tuple(pose_bone, name, count)


def _pose_prop_int(obj, bone_name: str, name: str) -> int:
    pose_bone = getattr(getattr(obj, "pose", None), "bones", {}).get(bone_name)
    if pose_bone is None:
        return 0
    value = pose_bone.get(name)
    return int(value) if value is not None else 0


def _skeleton_initial_placement(bones: tuple[ExportBone, ...]) -> tuple[float, ...]:
    for bone in bones:
        if bone.parent:
            continue
        if bone.granny_local_position and bone.granny_local_orientation_xyzw:
            return bone.matrix_local
        break
    return ()


def _vertex_key(vertex: ExportVertex) -> tuple:
    weights = tuple((item.bone, round(item.weight, 8)) for item in vertex.weights)
    uv = None if vertex.uv is None else tuple(round(value, 8) for value in vertex.uv)
    return (
        tuple(round(value, 8) for value in vertex.position),
        tuple(round(value, 8) for value in vertex.normal),
        uv,
        weights,
    )
```

## third_party/blendergranny/io_scene_gr2/gr2/file.py

```python
"""GR2 file header and section table parsing."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from .binary import BinaryReader, swap_u32
from .constants import (
    COMPRESSION_NAMES,
    EXTRA_TAG_COUNT,
    MAGIC_32BE,
    MAGIC_32LE,
    MAGIC_64BE,
    MAGIC_64LE,
    MAGIC_OLD,
    MAGIC_SIZE,
    SECTION_NAMES,
    SECTION_RECORD_SIZE,
)


@dataclass(frozen=True)
class GR2Section:
    index: int
    compression: int
    data_offset: int
    data_size: int
    expanded_size: int
    internal_alignment: int
    first_16bit: int
    first_8bit: int
    pointer_fixup_offset: int
    pointer_fixup_count: int
    mixed_marshalling_offset: int
    mixed_marshalling_count: int

    @property
    def compression_name(self) -> str:
        return COMPRESSION_NAMES.get(self.compression, f"unknown_{self.compression}")

    @property
    def semantic_name(self) -> str:
        return SECTION_NAMES.get(self.index, f"section_{self.index}")


@dataclass(frozen=True)
class GR2Header:
    version: int
    total_size: int
    crc: int
    section_array_offset: int
    section_count: int
    root_type: tuple[int, int]
    root_object: tuple[int, int]
    type_tag: int
    extra_tags: tuple[int, ...]
    string_db_crc: int
    reserved: tuple[int, ...]
    pointer_size: int
    byte_reversed: bool


@dataclass(frozen=True)
class GR2File:
    path: Path
    header: GR2Header
    sections: tuple[GR2Section, ...]
    data: bytes

    def section_bytes(self, section: GR2Section) -> bytes:
        start = section.data_offset
        end = start + section.data_size
        if start < 0 or end > len(self.data):
            raise ValueError(f"section {section.index} points outside file")
        return self.data[start:end]


def _magic_words(data: bytes, *, byte_reversed: bool) -> tuple[int, int, int, int]:
    reader = BinaryReader(data, byte_reversed=byte_reversed)
    return tuple(reader.u32(i * 4) for i in range(4))


def detect_gr2(data: bytes) -> tuple[bool, bool, int]:
    if len(data) < MAGIC_SIZE:
        return False, False, 0

    little = _magic_words(data, byte_reversed=False)
    if little in (MAGIC_OLD, MAGIC_32LE):
        return True, False, 32
    if little == MAGIC_64LE:
        return True, False, 64

    swapped = tuple(swap_u32(word) for word in little)
    if swapped in (MAGIC_OLD, MAGIC_32BE):
        return True, True, 32
    if swapped == MAGIC_64BE:
        return True, True, 64

    return False, False, 0


def read_gr2(path: str | Path) -> GR2File:
    file_path = Path(path)
    return read_gr2_bytes(file_path.read_bytes(), path=file_path)


def read_gr2_bytes(data: bytes, *, path: str | Path = "<memory>") -> GR2File:
    ok, byte_reversed, pointer_size = detect_gr2(data)
    if not ok:
        raise ValueError("not a Granny2 file")

    reader = BinaryReader(data, byte_reversed=byte_reversed)
    header_offset = MAGIC_SIZE
    version = reader.u32(header_offset)
    total_size = reader.u32(header_offset + 4)
    crc = reader.u32(header_offset + 8)
    section_array_offset = reader.u32(header_offset + 12)
    section_count = reader.u32(header_offset + 16)
    root_type = (reader.u32(header_offset + 20), reader.u32(header_offset + 24))
    root_object = (reader.u32(header_offset + 28), reader.u32(header_offset + 32))
    type_tag = reader.u32(header_offset + 36)
    extra_tags = tuple(reader.u32(header_offset + 40 + i * 4) for i in range(EXTRA_TAG_COUNT))

    string_db_crc = 0
    reserved: tuple[int, ...] = ()
    if version >= 7:
        string_db_crc = reader.u32(header_offset + 56)
        reserved = tuple(reader.u32(header_offset + 60 + i * 4) for i in range(3))

    sections = tuple(_read_sections(reader, section_array_offset, section_count))
    header = GR2Header(
        version=version,
        total_size=total_size,
        crc=crc,
        section_array_offset=section_array_offset,
        section_count=section_count,
        root_type=root_type,
        root_object=root_object,
        type_tag=type_tag,
        extra_tags=extra_tags,
        string_db_crc=string_db_crc,
        reserved=reserved,
        pointer_size=pointer_size,
        byte_reversed=byte_reversed,
    )
    return GR2File(Path(path), header, sections, data)


def _read_sections(reader: BinaryReader, section_array_offset: int, count: int) -> Iterable[GR2Section]:
    base = MAGIC_SIZE + section_array_offset
    for index in range(count):
        offset = base + index * SECTION_RECORD_SIZE
        yield GR2Section(
            index=index,
            compression=reader.u32(offset),
            data_offset=reader.u32(offset + 4),
            data_size=reader.u32(offset + 8),
            expanded_size=reader.u32(offset + 12),
            internal_alignment=reader.u32(offset + 16),
            first_16bit=reader.u32(offset + 20),
            first_8bit=reader.u32(offset + 24),
            pointer_fixup_offset=reader.u32(offset + 28),
            pointer_fixup_count=reader.u32(offset + 32),
            mixed_marshalling_offset=reader.u32(offset + 36),
            mixed_marshalling_count=reader.u32(offset + 40),
        )
```

## third_party/blendergranny/io_scene_gr2/gr2/fixup.py

```python
"""Decompressed section bundle and pointer fixup handling."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Protocol

from .binary import BinaryReader
from .constants import COMPRESSION_BITKNIT2
from .decompress import decompress_section
from .file import GR2File, GR2Section

FAKE_POINTER_BASE = 0x10000000
FAKE_SECTION_STRIDE = 0x100000


class SectionBackend(Protocol):
    def decompress(self, section, compressed: bytes) -> bytes:
        ...


@dataclass(frozen=True)
class PointerRef:
    section: int
    offset: int


@dataclass(frozen=True)
class PointerFixup:
    source_section: int
    source_offset: int
    target: PointerRef


@dataclass(frozen=True)
class MixedMarshallingFixup:
    source_section: int
    count: int
    offset: int
    type_ref: PointerRef | None


@dataclass(frozen=True)
class LoadedGR2:
    gr2: GR2File
    sections_original: tuple[bytes, ...]
    sections_fixed: tuple[bytes, ...]
    pointer_fixups: tuple[PointerFixup, ...]
    mixed_marshalling_fixups: tuple[MixedMarshallingFixup, ...]

    def resolve_fake_pointer(self, pointer: int) -> PointerRef | None:
        return decode_fake_pointer(pointer, len(self.sections_fixed))

    def read_ref(self, ref: PointerRef, size: int | None = None, *, fixed: bool = True) -> bytes:
        sections = self.sections_fixed if fixed else self.sections_original
        if ref.section < 0 or ref.section >= len(sections):
            raise ValueError(f"section {ref.section} out of range")
        data = sections[ref.section]
        end = len(data) if size is None else ref.offset + size
        if ref.offset < 0 or end > len(data):
            raise ValueError(f"reference {ref.section}:{ref.offset:#x} out of range")
        return data[ref.offset:end]


def make_fake_pointer(ref: PointerRef) -> int:
    return FAKE_POINTER_BASE + ref.section * FAKE_SECTION_STRIDE + ref.offset


def decode_fake_pointer(pointer: int, section_count: int) -> PointerRef | None:
    if pointer < FAKE_POINTER_BASE:
        return None
    value = pointer - FAKE_POINTER_BASE
    section = value // FAKE_SECTION_STRIDE
    offset = value - section * FAKE_SECTION_STRIDE
    if section < 0 or section >= section_count:
        return None
    return PointerRef(section, offset)


def load_sections(gr2: GR2File, backend: SectionBackend | None = None) -> LoadedGR2:
    validate_file = getattr(backend, "validate_file", None)
    if validate_file is not None:
        validate_file(gr2)

    original = tuple(
        backend.decompress(section, gr2.section_bytes(section))
        if backend
        else decompress_section(section, gr2.section_bytes(section))
        for section in gr2.sections
    )
    fixed = [bytearray(section) for section in original]
    pointer_fixups = tuple(_read_pointer_fixups(gr2))
    mixed_fixups = tuple(_read_mixed_marshalling_fixups(gr2))

    pointer_size = gr2.header.pointer_size // 8
    if pointer_size not in (4, 8):
        raise ValueError(f"unsupported pointer size {gr2.header.pointer_size}")

    for fixup in pointer_fixups:
        if fixup.source_section >= len(fixed):
            continue
        data = fixed[fixup.source_section]
        if fixup.source_offset + pointer_size > len(data):
            continue
        fake = make_fake_pointer(fixup.target)
        if pointer_size == 4:
            struct.pack_into("<I", data, fixup.source_offset, fake)
        else:
            struct.pack_into("<Q", data, fixup.source_offset, fake)

    return LoadedGR2(
        gr2=gr2,
        sections_original=original,
        sections_fixed=tuple(bytes(section) for section in fixed),
        pointer_fixups=pointer_fixups,
        mixed_marshalling_fixups=mixed_fixups,
    )


def _read_pointer_fixups(gr2: GR2File):
    for section in gr2.sections:
        if section.pointer_fixup_count == 0 or section.pointer_fixup_offset == 0:
            continue
        entry_size = _pointer_fixup_entry_size(gr2, section.index)
        data = _read_fixup_blob(
            gr2,
            section.index,
            section.pointer_fixup_offset,
            section.pointer_fixup_count,
            entry_size,
        )
        reader = BinaryReader(data, byte_reversed=gr2.header.byte_reversed)
        for index in range(section.pointer_fixup_count):
            offset = index * entry_size
            if offset + entry_size > len(data):
                break
            source_offset = reader.u32(offset)
            if entry_size == 12:
                target = PointerRef(reader.u32(offset + 4), reader.u32(offset + 8))
            else:
                target = _decode_legacy_fake_pointer(reader.u32(offset + 4), len(gr2.sections))
            yield PointerFixup(section.index, source_offset, target)


def _read_mixed_marshalling_fixups(gr2: GR2File):
    for section in gr2.sections:
        if section.mixed_marshalling_count == 0 or section.mixed_marshalling_offset == 0:
            continue
        entry_size = 16 if gr2.header.version >= 7 else 8
        data = _read_fixup_blob(
            gr2,
            section.index,
            section.mixed_marshalling_offset,
            section.mixed_marshalling_count,
            entry_size,
        )
        reader = BinaryReader(data, byte_reversed=gr2.header.byte_reversed)
        for index in range(section.mixed_marshalling_count):
            offset = index * entry_size
            if offset + entry_size > len(data):
                break
            type_ref = None
            if entry_size == 16:
                type_ref = PointerRef(reader.u32(offset + 8), reader.u32(offset + 12))
            yield MixedMarshallingFixup(
                source_section=section.index,
                count=reader.u32(offset),
                offset=reader.u32(offset + 4),
                type_ref=type_ref,
            )


def _pointer_fixup_entry_size(gr2: GR2File, section_index: int) -> int:
    section = gr2.sections[section_index]
    span = _metadata_span_after(gr2, section.pointer_fixup_offset)
    if span is not None:
        if span >= section.pointer_fixup_count * 12:
            return 12
        if span >= section.pointer_fixup_count * 8:
            return 8
    if gr2.header.version >= 7:
        return 12
    if section.pointer_fixup_offset + section.pointer_fixup_count * 12 <= len(gr2.data):
        return 12
    return 8


def _read_fixup_blob(
    gr2: GR2File,
    section_index: int,
    offset: int,
    count: int,
    entry_size: int,
) -> bytes:
    expected_size = count * entry_size
    span = _metadata_span_after(gr2, offset)
    raw = gr2.data[offset : offset + span] if span is not None else gr2.data[offset:]
    if len(raw) >= 4:
        payload_size = struct.unpack_from("<I", raw, 0)[0]
        payload = raw[4 : 4 + min(payload_size, max(0, len(raw) - 4))]
        if payload[:2] == b"\xb1\x75":
            if len(payload) >= 48:
                return _decompress_bitknit2_fixup_blob(
                    section_index,
                    payload,
                    expected_size,
                )
            if len(payload) >= 4 + expected_size:
                return payload[4 : 4 + expected_size]
        if len(payload) == expected_size:
            return payload
    if len(raw) >= expected_size:
        return raw[:expected_size]
    return raw


def _decompress_bitknit2_fixup_blob(section_index: int, compressed: bytes, expanded_size: int) -> bytes:
    from .decompress.bitknit import decompress_bitknit

    section = GR2Section(
        index=section_index,
        compression=COMPRESSION_BITKNIT2,
        data_offset=0,
        data_size=len(compressed),
        expanded_size=expanded_size,
        internal_alignment=4,
        first_16bit=expanded_size,
        first_8bit=expanded_size,
        pointer_fixup_offset=0,
        pointer_fixup_count=0,
        mixed_marshalling_offset=0,
        mixed_marshalling_count=0,
    )
    return decompress_bitknit(section, compressed)


def _metadata_span_after(gr2: GR2File, offset: int) -> int | None:
    candidates: list[int] = []
    for section in gr2.sections:
        for value in (
            section.pointer_fixup_offset,
            section.mixed_marshalling_offset,
            section.data_offset,
        ):
            if value > offset:
                candidates.append(value)
    if not candidates:
        return None
    return min(candidates) - offset


def _decode_legacy_fake_pointer(pointer: int, section_count: int) -> PointerRef:
    ref = decode_fake_pointer(pointer, section_count)
    if ref is not None:
        return ref
    section = pointer // FAKE_SECTION_STRIDE
    offset = pointer - section * FAKE_SECTION_STRIDE
    return PointerRef(section, offset)
```

## third_party/blendergranny/io_scene_gr2/gr2/geometry.py

```python
"""Mesh geometry extraction from loaded GR2 sections."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Iterable

from .fixup import LoadedGR2, PointerRef
from .material import MaterialInfo, material_map
from .types import (
    MT_BINORMAL_INT8,
    MT_BINORMAL_INT16,
    MT_INT16,
    MT_INT32,
    MT_INT8,
    MT_NORMAL_UINT8,
    MT_NORMAL_UINT16,
    MT_REAL32,
    MT_UINT16,
    MT_UINT32,
    MT_UINT8,
    TypeDefinition,
    parse_type_definition_array,
    read_reference_array_objects,
    summarize_meshes,
)


@dataclass(frozen=True)
class VertexComponent:
    name: str
    offset: int
    width: int
    member_type: int


@dataclass(frozen=True)
class BoneBinding:
    index: int
    name: str


@dataclass(frozen=True)
class VertexBoneWeight:
    bone_index: int
    weight: float


@dataclass(frozen=True)
class MeshTriangleGroup:
    material_index: int
    tri_first: int
    tri_count: int


@dataclass(frozen=True)
class MeshGeometry:
    name: str
    vertex_count: int
    index_count: int
    vertex_stride: int
    components: tuple[VertexComponent, ...]
    positions: tuple[tuple[float, float, float], ...]
    normals: tuple[tuple[float, float, float], ...]
    uvs: tuple[tuple[float, float], ...]
    indices: tuple[int, ...]
    bone_bindings: tuple[BoneBinding, ...]
    vertex_weights: tuple[tuple[VertexBoneWeight, ...], ...]
    materials: tuple[MaterialInfo, ...]
    triangle_groups: tuple[MeshTriangleGroup, ...]

    @property
    def triangles(self) -> tuple[tuple[int, int, int], ...]:
        usable = len(self.indices) - (len(self.indices) % 3)
        return tuple(
            (self.indices[index], self.indices[index + 1], self.indices[index + 2])
            for index in range(0, usable, 3)
        )

    @property
    def bounds(self) -> tuple[tuple[float, float, float], tuple[float, float, float]] | None:
        if not self.positions:
            return None
        mins = tuple(min(vertex[axis] for vertex in self.positions) for axis in range(3))
        maxs = tuple(max(vertex[axis] for vertex in self.positions) for axis in range(3))
        return mins, maxs


def extract_mesh_geometries(loaded: LoadedGR2, max_meshes: int = 32) -> tuple[MeshGeometry, ...]:
    summary = summarize_meshes(loaded, max_meshes=max_meshes)
    materials_by_ref = material_map(loaded)
    meshes: list[MeshGeometry] = []
    for mesh in summary.get("meshes", []):
        vertex_data = mesh.get("primary_vertex_data", {})
        topology = mesh.get("primary_topology", {})
        vertex_fields = _field_map(vertex_data)
        topology_fields = _field_map(topology)

        vertices = vertex_fields.get("Vertices", {})
        vertex_ref = _dict_ref(vertices.get("target"))
        vertex_type_ref = _dict_ref(vertices.get("variant_type"))
        vertex_count = int(vertices.get("count") or 0)
        if vertex_ref is None or vertex_type_ref is None or vertex_count <= 0:
            continue

        type_members = parse_type_definition_array(loaded, vertex_type_ref)
        components = _vertex_components(type_members)
        stride = _vertex_stride(components)
        try:
            vertex_bytes = loaded.read_ref(vertex_ref, vertex_count * stride, fixed=False)
        except ValueError:
            continue

        index_field = _best_index_field(topology_fields)
        index_ref = _dict_ref(index_field.get("target") if index_field else None)
        index_count = int(index_field.get("count") or 0) if index_field else 0
        index_member_type = _index_member_type(index_field, default=MT_UINT16)
        try:
            indices = _read_indices(loaded, index_ref, index_count, index_member_type)
        except ValueError:
            indices = ()

        bone_bindings = _read_bone_bindings(loaded, mesh)
        materials = _read_material_bindings(loaded, mesh, materials_by_ref)
        meshes.append(
            MeshGeometry(
                name=str(mesh.get("name") or f"Mesh_{len(meshes)}"),
                vertex_count=vertex_count,
                index_count=len(indices),
                vertex_stride=stride,
                components=components,
                positions=_read_float_component(vertex_bytes, vertex_count, stride, components, "Position", 3),
                normals=_read_float_component(vertex_bytes, vertex_count, stride, components, "Normal", 3),
                uvs=_read_float_component(
                    vertex_bytes,
                    vertex_count,
                    stride,
                    components,
                    "TextureCoordinates",
                    2,
                    prefix=True,
                ),
                indices=indices,
                bone_bindings=bone_bindings,
                vertex_weights=_read_vertex_weights(vertex_bytes, vertex_count, stride, components, len(bone_bindings)),
                materials=materials,
                triangle_groups=_read_triangle_groups(loaded, topology_fields),
            )
        )
    return tuple(meshes)


def geometry_summary(geometries: Iterable[MeshGeometry]) -> list[dict]:
    result = []
    for mesh in geometries:
        result.append(
            {
                "name": mesh.name,
                "vertex_count": mesh.vertex_count,
                "index_count": mesh.index_count,
                "triangle_count": len(mesh.triangles),
                "vertex_stride": mesh.vertex_stride,
                "components": [component.name for component in mesh.components],
                "bone_bindings": [binding.name for binding in mesh.bone_bindings],
                "materials": [
                    {"name": material.name, "texture_file": material.texture_file}
                    for material in mesh.materials
                ],
                "triangle_groups": [
                    {
                        "material_index": group.material_index,
                        "tri_first": group.tri_first,
                        "tri_count": group.tri_count,
                    }
                    for group in mesh.triangle_groups
                ],
                "bounds": mesh.bounds,
                "first_position": mesh.positions[0] if mesh.positions else None,
                "first_normal": mesh.normals[0] if mesh.normals else None,
                "first_uv": mesh.uvs[0] if mesh.uvs else None,
                "first_triangle": mesh.triangles[0] if mesh.triangles else None,
                "first_weights": [
                    {"bone": mesh.bone_bindings[item.bone_index].name, "weight": item.weight}
                    for item in (mesh.vertex_weights[0] if mesh.vertex_weights else ())
                    if item.bone_index < len(mesh.bone_bindings)
                ],
            }
        )
    return result


def _vertex_components(members: tuple[TypeDefinition, ...]) -> tuple[VertexComponent, ...]:
    components: list[VertexComponent] = []
    offset = 0
    for member in members:
        size = _component_size(member)
        components.append(
            VertexComponent(
                name=member.name,
                offset=offset,
                width=member.array_width or 1,
                member_type=member.member_type,
            )
        )
        offset += size
    return tuple(components)


def _vertex_stride(components: tuple[VertexComponent, ...]) -> int:
    if not components:
        return 0
    return max(component.offset + _component_storage_size(component) for component in components)


def _component_size(member: TypeDefinition) -> int:
    if member.member_type == MT_REAL32:
        return 4 * (member.array_width or 1)
    if member.member_type in (MT_INT32, MT_UINT32):
        return 4 * (member.array_width or 1)
    if member.member_type in (MT_INT16, MT_UINT16, MT_BINORMAL_INT16, MT_NORMAL_UINT16):
        return 2 * (member.array_width or 1)
    if member.member_type in (MT_INT8, MT_UINT8, MT_BINORMAL_INT8, MT_NORMAL_UINT8):
        return member.array_width or 1
    return 4 * (member.array_width or 1)


def _component_storage_size(component: VertexComponent) -> int:
    if component.member_type == MT_REAL32:
        return 4 * component.width
    if component.member_type in (MT_INT32, MT_UINT32):
        return 4 * component.width
    if component.member_type in (MT_INT16, MT_UINT16, MT_BINORMAL_INT16, MT_NORMAL_UINT16):
        return 2 * component.width
    if component.member_type in (MT_INT8, MT_UINT8, MT_BINORMAL_INT8, MT_NORMAL_UINT8):
        return component.width
    return 4 * component.width


def _read_float_component(
    data: bytes,
    vertex_count: int,
    stride: int,
    components: tuple[VertexComponent, ...],
    name: str,
    width: int,
    *,
    prefix: bool = False,
) -> tuple:
    component = next(
        (
            item
            for item in components
            if item.member_type == MT_REAL32
            and (item.name.startswith(name) if prefix else item.name == name)
            and item.width >= width
        ),
        None,
    )
    if component is None:
        return ()
    fmt = "<" + "f" * width
    values = []
    for index in range(vertex_count):
        offset = index * stride + component.offset
        if offset + width * 4 > len(data):
            break
        values.append(struct.unpack_from(fmt, data, offset))
    return tuple(values)


def _read_indices(
    loaded: LoadedGR2,
    ref: PointerRef | None,
    count: int,
    member_type: int,
) -> tuple[int, ...]:
    if ref is None or count <= 0:
        return ()
    if member_type in (MT_INT32, MT_UINT32):
        data = loaded.read_ref(ref, count * 4, fixed=False)
        return tuple(struct.unpack_from("<" + "I" * count, data, 0))
    data = loaded.read_ref(ref, count * 2, fixed=False)
    return tuple(struct.unpack_from("<" + "H" * count, data, 0))


def _read_bone_bindings(loaded: LoadedGR2, mesh: dict) -> tuple[BoneBinding, ...]:
    binding_field = _field_map({"fields": mesh.get("fields", [])}).get("BoneBindings")
    if not binding_field:
        return ()
    target = _dict_ref(binding_field.get("target"))
    type_ref = _dict_ref(binding_field.get("reference_type"))
    objects = read_reference_array_objects(
        loaded,
        target,
        int(binding_field.get("count") or 0),
        type_ref,
        max_count=4096,
    )
    bindings: list[BoneBinding] = []
    for index, obj in enumerate(objects):
        name = _field_map(obj).get("BoneName", {}).get("value")
        if isinstance(name, str) and name:
            bindings.append(BoneBinding(index=index, name=name))
        else:
            bindings.append(BoneBinding(index=index, name=f"Bone_{index}"))
    return tuple(bindings)


def _read_material_bindings(
    loaded: LoadedGR2,
    mesh: dict,
    materials_by_ref: dict[tuple[int, int], MaterialInfo],
) -> tuple[MaterialInfo, ...]:
    binding_field = _field_map({"fields": mesh.get("fields", [])}).get("MaterialBindings")
    if not binding_field:
        return ()
    target = _dict_ref(binding_field.get("target"))
    type_ref = _dict_ref(binding_field.get("reference_type"))
    objects = read_reference_array_objects(
        loaded,
        target,
        int(binding_field.get("count") or 0),
        type_ref,
        max_count=4096,
    )
    materials: list[MaterialInfo] = []
    for index, obj in enumerate(objects):
        material_ref = _dict_ref(_field_map(obj).get("Material", {}).get("target"))
        material = materials_by_ref.get((material_ref.section, material_ref.offset)) if material_ref else None
        materials.append(material or MaterialInfo(index=index, name=f"Material_{index}", texture_file="", texture_size=None))
    return tuple(materials)


def _read_triangle_groups(
    loaded: LoadedGR2,
    topology_fields: dict[str, dict],
) -> tuple[MeshTriangleGroup, ...]:
    group_field = topology_fields.get("Groups")
    if not group_field:
        return ()
    target = _dict_ref(group_field.get("target"))
    type_ref = _dict_ref(group_field.get("reference_type"))
    objects = read_reference_array_objects(
        loaded,
        target,
        int(group_field.get("count") or 0),
        type_ref,
        max_count=4096,
    )
    groups: list[MeshTriangleGroup] = []
    for obj in objects:
        fields = _field_map(obj)
        groups.append(
            MeshTriangleGroup(
                material_index=_field_int(fields, "MaterialIndex", 0),
                tri_first=_field_int(fields, "TriFirst", 0),
                tri_count=_field_int(fields, "TriCount", 0),
            )
        )
    return tuple(groups)


def _read_vertex_weights(
    data: bytes,
    vertex_count: int,
    stride: int,
    components: tuple[VertexComponent, ...],
    binding_count: int,
) -> tuple[tuple[VertexBoneWeight, ...], ...]:
    weights_component = _component_by_name(components, "BoneWeights")
    indices_component = _component_by_name(components, "BoneIndices")
    if weights_component is None or indices_component is None or binding_count <= 0:
        return ()

    width = min(weights_component.width, indices_component.width)
    vertices: list[tuple[VertexBoneWeight, ...]] = []
    for vertex_index in range(vertex_count):
        weights = []
        base = vertex_index * stride
        for lane in range(width):
            weight = _read_weight_value(data, base + weights_component.offset, lane, weights_component)
            bone_index = _read_int_value(data, base + indices_component.offset, lane, indices_component)
            if weight <= 0.0 or bone_index < 0 or bone_index >= binding_count:
                continue
            weights.append(VertexBoneWeight(bone_index=bone_index, weight=weight))
        vertices.append(tuple(weights))
    return tuple(vertices)


def _component_by_name(
    components: tuple[VertexComponent, ...],
    name: str,
) -> VertexComponent | None:
    return next((component for component in components if component.name == name), None)


def _read_weight_value(data: bytes, offset: int, lane: int, component: VertexComponent) -> float:
    if component.member_type == MT_REAL32:
        start = offset + lane * 4
        if start + 4 > len(data):
            return 0.0
        return max(0.0, min(1.0, struct.unpack_from("<f", data, start)[0]))
    if component.member_type in (MT_UINT16, MT_NORMAL_UINT16):
        start = offset + lane * 2
        if start + 2 > len(data):
            return 0.0
        value = struct.unpack_from("<H", data, start)[0]
        return value / 65535.0 if component.member_type == MT_NORMAL_UINT16 else float(value)
    if component.member_type in (MT_UINT8, MT_NORMAL_UINT8):
        start = offset + lane
        if start >= len(data):
            return 0.0
        value = data[start]
        return value / 255.0 if component.member_type == MT_NORMAL_UINT8 else float(value)
    return 0.0


def _read_int_value(data: bytes, offset: int, lane: int, component: VertexComponent) -> int:
    if component.member_type in (MT_INT32, MT_UINT32):
        start = offset + lane * 4
        if start + 4 > len(data):
            return -1
        fmt = "<i" if component.member_type == MT_INT32 else "<I"
        return int(struct.unpack_from(fmt, data, start)[0])
    if component.member_type in (MT_INT16, MT_UINT16):
        start = offset + lane * 2
        if start + 2 > len(data):
            return -1
        fmt = "<h" if component.member_type == MT_INT16 else "<H"
        return int(struct.unpack_from(fmt, data, start)[0])
    if component.member_type in (MT_INT8, MT_UINT8):
        start = offset + lane
        if start >= len(data):
            return -1
        if component.member_type == MT_INT8:
            return struct.unpack_from("<b", data, start)[0]
        return data[start]
    return -1


def _index_member_type(field: dict | None, *, default: int) -> int:
    if not field:
        return default
    if field.get("name") == "Indices":
        return MT_UINT32
    return default


def _best_index_field(topology_fields: dict[str, dict]) -> dict | None:
    indices16 = topology_fields.get("Indices16")
    indices32 = topology_fields.get("Indices")
    if indices16 and int(indices16.get("count") or 0) > 0:
        return indices16
    if indices32 and int(indices32.get("count") or 0) > 0:
        return indices32
    return indices16 or indices32


def _dict_ref(value: dict | None) -> PointerRef | None:
    if not value:
        return None
    return PointerRef(int(value["section"]), int(value["offset"]))


def _field_map(summary: dict) -> dict[str, dict]:
    return {field["name"]: field for field in summary.get("fields", [])}


def _field_int(fields: dict[str, dict], name: str, default: int) -> int:
    value = fields.get(name, {}).get("value")
    return default if value is None else int(value)
```

## third_party/blendergranny/io_scene_gr2/gr2/material.py

```python
"""Material and texture metadata extraction."""

from __future__ import annotations

from dataclasses import dataclass

from .fixup import LoadedGR2, PointerRef
from .types import read_reference_array_objects, summarize_object, summarize_root_object


@dataclass(frozen=True)
class MaterialInfo:
    index: int
    name: str
    texture_file: str
    texture_size: tuple[int, int] | None


def material_map(loaded: LoadedGR2, max_materials: int = 4096) -> dict[tuple[int, int], MaterialInfo]:
    root = summarize_root_object(loaded, max_arrays=max_materials)
    material_field = _field_map(root).get("Materials")
    material_type = _dict_ref(material_field.get("reference_type") if material_field else None)
    refs = [
        _dict_ref(ref)
        for ref in (material_field or {}).get("element_refs", [])[:max_materials]
    ]
    result: dict[tuple[int, int], MaterialInfo] = {}
    for index, ref in enumerate(ref for ref in refs if ref is not None):
        result[(ref.section, ref.offset)] = _read_material(loaded, ref, material_type, index, result, frozenset())
    return result


def extract_materials(loaded: LoadedGR2, max_materials: int = 4096) -> tuple[MaterialInfo, ...]:
    return tuple(sorted(material_map(loaded, max_materials).values(), key=lambda item: item.index))


def _read_material(
    loaded: LoadedGR2,
    ref: PointerRef,
    material_type: PointerRef | None,
    index: int,
    cache: dict[tuple[int, int], MaterialInfo],
    seen: frozenset[tuple[int, int]],
) -> MaterialInfo:
    key = (ref.section, ref.offset)
    if key in cache:
        return cache[key]
    if material_type is None or key in seen:
        return MaterialInfo(index=index, name=f"Material_{index}", texture_file="", texture_size=None)

    summary = summarize_object(loaded, ref, material_type, max_array_refs=16)
    fields = _field_map(summary)
    texture = _read_texture_field(loaded, fields.get("Texture"))
    if texture is None:
        texture = _read_first_map_texture(loaded, fields.get("Maps"), material_type, cache, seen | {key})

    texture_file = texture[0] if texture else ""
    texture_size = texture[1] if texture else None
    return MaterialInfo(
        index=index,
        name=str(fields.get("Name", {}).get("value") or f"Material_{index}"),
        texture_file=texture_file,
        texture_size=texture_size,
    )


def _read_first_map_texture(
    loaded: LoadedGR2,
    maps_field: dict | None,
    material_type: PointerRef,
    cache: dict[tuple[int, int], MaterialInfo],
    seen: frozenset[tuple[int, int]],
) -> tuple[str, tuple[int, int] | None] | None:
    if not maps_field:
        return None
    target = _dict_ref(maps_field.get("target"))
    map_type = _dict_ref(maps_field.get("reference_type"))
    maps = read_reference_array_objects(
        loaded,
        target,
        int(maps_field.get("count") or 0),
        map_type,
        max_count=16,
    )
    for item in maps:
        fields = _field_map(item)
        map_ref = _dict_ref(fields.get("Map", {}).get("target"))
        if map_ref is None:
            continue
        material = _read_material(loaded, map_ref, material_type, -1, cache, seen)
        if material.texture_file:
            return material.texture_file, material.texture_size
    return None


def _read_texture_field(
    loaded: LoadedGR2,
    texture_field: dict | None,
) -> tuple[str, tuple[int, int] | None] | None:
    if not texture_field:
        return None
    target = _dict_ref(texture_field.get("target"))
    texture_type = _dict_ref(texture_field.get("reference_type"))
    if target is None or texture_type is None:
        return None
    summary = summarize_object(loaded, target, texture_type, max_array_refs=8)
    fields = _field_map(summary)
    width = fields.get("Width", {}).get("value")
    height = fields.get("Height", {}).get("value")
    size = (int(width), int(height)) if width is not None and height is not None else None
    return str(fields.get("FromFileName", {}).get("value") or ""), size


def _dict_ref(value: dict | None) -> PointerRef | None:
    if not value:
        return None
    return PointerRef(int(value["section"]), int(value["offset"]))


def _field_map(summary: dict) -> dict[str, dict]:
    return {field["name"]: field for field in summary.get("fields", [])}
```

## third_party/blendergranny/io_scene_gr2/gr2/skeleton.py

```python
"""Skeleton extraction from loaded GR2 sections."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Iterable

from .fixup import LoadedGR2, PointerRef
from .types import (
    read_reference_array_objects,
    summarize_object,
    summarize_root_object,
)


@dataclass(frozen=True)
class Transform:
    flags: int
    position: tuple[float, float, float]
    orientation: tuple[float, float, float, float]
    scale_shear: tuple[float, ...]


@dataclass(frozen=True)
class SkeletonBone:
    index: int
    name: str
    parent_index: int
    transform: Transform
    inverse_world_transform: tuple[float, ...]


@dataclass(frozen=True)
class Skeleton:
    name: str
    bones: tuple[SkeletonBone, ...]
    lod_type: int


def extract_skeletons(loaded: LoadedGR2, max_skeletons: int = 16) -> tuple[Skeleton, ...]:
    root = summarize_root_object(loaded, max_arrays=max_skeletons)
    skeleton_field = _field_map(root).get("Skeletons")
    if not skeleton_field:
        return ()
    skeleton_type = _dict_ref(skeleton_field.get("reference_type"))
    skeleton_refs = [
        _dict_ref(ref)
        for ref in skeleton_field.get("element_refs", [])[:max_skeletons]
    ]

    skeletons: list[Skeleton] = []
    for skeleton_ref in skeleton_refs:
        if skeleton_ref is None or skeleton_type is None:
            continue
        summary = summarize_object(loaded, skeleton_ref, skeleton_type, max_array_refs=4096)
        fields = _field_map(summary)
        bone_field = fields.get("Bones", {})
        bone_target = _dict_ref(bone_field.get("target"))
        bone_type = _dict_ref(bone_field.get("reference_type"))
        bone_objects = read_reference_array_objects(
            loaded,
            bone_target,
            int(bone_field.get("count") or 0),
            bone_type,
            max_count=4096,
        )
        bones = tuple(
            _read_bone(loaded, bone_index, bone_object)
            for bone_index, bone_object in enumerate(bone_objects)
        )
        skeletons.append(
            Skeleton(
                name=str(fields.get("Name", {}).get("value") or f"Skeleton_{len(skeletons)}"),
                bones=bones,
                lod_type=_field_int(fields, "LODType", 0),
            )
        )
    return tuple(skeletons)


def skeleton_summary(skeletons: Iterable[Skeleton]) -> list[dict]:
    result = []
    for skeleton in skeletons:
        result.append(
            {
                "name": skeleton.name,
                "bone_count": len(skeleton.bones),
                "lod_type": skeleton.lod_type,
                "root_bones": [bone.name for bone in skeleton.bones if bone.parent_index < 0],
                "first_bones": [
                    {
                        "name": bone.name,
                        "parent_index": bone.parent_index,
                        "position": bone.transform.position,
                    }
                    for bone in skeleton.bones[:8]
                ],
            }
        )
    return result


def _read_bone(loaded: LoadedGR2, index: int, bone_object: dict) -> SkeletonBone:
    fields = _field_map(bone_object)
    transform_offset = int(fields.get("Transform", {}).get("offset") or 0)
    object_ref = _dict_ref(bone_object.get("object"))
    transform = Transform(0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0))
    inverse_world_transform: tuple[float, ...] = ()

    if object_ref is not None:
        transform = _read_transform(loaded, PointerRef(object_ref.section, transform_offset))
        inverse_offset = int(fields.get("InverseWorldTransform", {}).get("offset") or 0)
        inverse_world_transform = _read_real32_array(
            loaded,
            PointerRef(object_ref.section, inverse_offset),
            16,
        )

    return SkeletonBone(
        index=index,
        name=str(fields.get("Name", {}).get("value") or f"Bone_{index}"),
        parent_index=_field_int(fields, "ParentIndex", -1),
        transform=transform,
        inverse_world_transform=inverse_world_transform,
    )


def _read_transform(loaded: LoadedGR2, ref: PointerRef) -> Transform:
    data = loaded.read_ref(ref, 68, fixed=False)
    flags = struct.unpack_from("<I", data, 0)[0]
    values = struct.unpack_from("<16f", data, 4)
    return Transform(
        flags=flags,
        position=values[0:3],
        orientation=values[3:7],
        scale_shear=values[7:16],
    )


def _read_real32_array(loaded: LoadedGR2, ref: PointerRef, count: int) -> tuple[float, ...]:
    data = loaded.read_ref(ref, count * 4, fixed=False)
    return struct.unpack_from("<" + "f" * count, data, 0)


def _dict_ref(value: dict | None) -> PointerRef | None:
    if not value:
        return None
    return PointerRef(int(value["section"]), int(value["offset"]))


def _field_map(summary: dict) -> dict[str, dict]:
    return {field["name"]: field for field in summary.get("fields", [])}


def _field_int(fields: dict[str, dict], name: str, default: int) -> int:
    value = fields.get(name, {}).get("value")
    return default if value is None else int(value)
```

## third_party/blendergranny/io_scene_gr2/gr2/texture.py

```python
"""Texture path resolution helpers."""

from __future__ import annotations

import ntpath
from pathlib import Path


def texture_basename(texture_file: str) -> str:
    normalized = texture_file.replace("\\", "/")
    return ntpath.basename(normalized)


def texture_candidates(texture_file: str, gr2_path: Path, roots: tuple[Path, ...] = ()) -> tuple[Path, ...]:
    if not texture_file:
        return ()
    raw = Path(texture_file)
    names = tuple(dict.fromkeys(name for name in (texture_file, texture_basename(texture_file)) if name))
    candidates: list[Path] = []

    if raw.is_absolute():
        candidates.append(raw)

    search_roots = _search_roots(gr2_path, roots)
    for root in search_roots:
        for name in names:
            candidates.append(root / name)
    return tuple(dict.fromkeys(candidates))


def resolve_texture_path(texture_file: str, gr2_path: Path, roots: tuple[Path, ...] = ()) -> Path | None:
    for path in texture_candidates(texture_file, gr2_path, roots):
        if path.is_file():
            return path
    basename = texture_basename(texture_file)
    if not basename:
        return None
    for root in _search_roots(gr2_path, roots):
        try:
            match = next(root.rglob(basename))
        except (OSError, StopIteration):
            continue
        if match.is_file():
            return match
    return None


def _search_roots(gr2_path: Path, roots: tuple[Path, ...]) -> tuple[Path, ...]:
    base = gr2_path.parent if gr2_path.suffix else gr2_path
    candidates = [base, *base.parents[:4], *roots]
    return tuple(dict.fromkeys(path.resolve() for path in candidates if path))
```

## third_party/blendergranny/io_scene_gr2/gr2/types.py

```python
"""Small dynamic Granny type-tree reader."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Any

from .fixup import LoadedGR2, PointerRef, decode_fake_pointer

MT_END = 0
MT_INLINE = 1
MT_REFERENCE = 2
MT_REFERENCE_TO_ARRAY = 3
MT_ARRAY_OF_REFERENCES = 4
MT_VARIANT_REFERENCE = 5
MT_UNSUPPORTED = 6
MT_REFERENCE_TO_VARIANT_ARRAY = 7
MT_STRING = 8
MT_TRANSFORM = 9
MT_REAL32 = 10
MT_INT8 = 11
MT_UINT8 = 12
MT_BINORMAL_INT8 = 13
MT_NORMAL_UINT8 = 14
MT_INT16 = 15
MT_UINT16 = 16
MT_BINORMAL_INT16 = 17
MT_NORMAL_UINT16 = 18
MT_INT32 = 19
MT_UINT32 = 20
MT_REAL16 = 21
MT_EMPTY_REFERENCE = 22

MEMBER_TYPE_NAMES = {
    MT_END: "end",
    MT_INLINE: "inline",
    MT_REFERENCE: "reference",
    MT_REFERENCE_TO_ARRAY: "reference_to_array",
    MT_ARRAY_OF_REFERENCES: "array_of_references",
    MT_VARIANT_REFERENCE: "variant_reference",
    MT_UNSUPPORTED: "unsupported",
    MT_REFERENCE_TO_VARIANT_ARRAY: "reference_to_variant_array",
    MT_STRING: "string",
    MT_TRANSFORM: "transform",
    MT_REAL32: "real32",
    MT_INT8: "int8",
    MT_UINT8: "uint8",
    MT_BINORMAL_INT8: "binormal_int8",
    MT_NORMAL_UINT8: "normal_uint8",
    MT_INT16: "int16",
    MT_UINT16: "uint16",
    MT_BINORMAL_INT16: "binormal_int16",
    MT_NORMAL_UINT16: "normal_uint16",
    MT_INT32: "int32",
    MT_UINT32: "uint32",
    MT_REAL16: "real16",
    MT_EMPTY_REFERENCE: "empty_reference",
}

_SCALAR_SIZES = {
    MT_REAL32: 4,
    MT_INT8: 1,
    MT_UINT8: 1,
    MT_BINORMAL_INT8: 1,
    MT_NORMAL_UINT8: 1,
    MT_INT16: 2,
    MT_UINT16: 2,
    MT_BINORMAL_INT16: 2,
    MT_NORMAL_UINT16: 2,
    MT_INT32: 4,
    MT_UINT32: 4,
    MT_REAL16: 2,
}


@dataclass(frozen=True)
class TypeDefinition:
    member_type: int
    name: str
    reference_type: PointerRef | None
    array_width: int
    extra: tuple[int, int, int]
    offset: int

    @property
    def member_type_name(self) -> str:
        return MEMBER_TYPE_NAMES.get(self.member_type, f"type_{self.member_type}")


def parse_type_definition_array(
    loaded: LoadedGR2,
    ref: PointerRef,
    *,
    max_members: int = 512,
) -> tuple[TypeDefinition, ...]:
    if ref.section >= len(loaded.sections_fixed):
        return ()
    data = loaded.sections_fixed[ref.section]
    entries: list[TypeDefinition] = []
    offset = ref.offset
    for _ in range(max_members):
        if offset + 32 > len(data):
            break
        member_type, name_ptr, type_ptr, array_width, extra0, extra1, extra2, _unused = struct.unpack_from(
            "<8I", data, offset
        )
        if member_type == MT_END:
            break
        name = read_string_pointer(loaded, name_ptr) or f"member_{len(entries)}"
        type_ref = loaded.resolve_fake_pointer(type_ptr)
        entries.append(
            TypeDefinition(
                member_type=member_type,
                name=name,
                reference_type=type_ref,
                array_width=array_width or 1,
                extra=(extra0, extra1, extra2),
                offset=offset,
            )
        )
        offset += 32
    return tuple(entries)


def read_string_pointer(loaded: LoadedGR2, pointer: int) -> str:
    ref = loaded.resolve_fake_pointer(pointer)
    if ref is None:
        return ""
    return read_granny_string(loaded, ref)


def read_granny_string(loaded: LoadedGR2, ref: PointerRef, max_length: int = 1024) -> str:
    if ref.section >= len(loaded.sections_original):
        return ""
    data = loaded.sections_original[ref.section]
    if ref.offset >= len(data):
        return ""

    if ref.offset + 4 <= len(data):
        length = struct.unpack_from("<I", data, ref.offset)[0]
        end = ref.offset + 4 + length
        if 0 < length <= max_length and end <= len(data) and _looks_text(data[ref.offset + 4 : end]):
            return data[ref.offset + 4 : end].decode("utf-8", "replace").rstrip("\x00")

    end = data.find(b"\x00", ref.offset, min(len(data), ref.offset + max_length))
    if end < 0:
        end = min(len(data), ref.offset + max_length)
    raw = data[ref.offset:end]
    if not _looks_text(raw):
        return ""
    return raw.decode("utf-8", "replace")


def member_storage_size(member: TypeDefinition, pointer_size: int) -> int:
    width = member.array_width or 1
    if member.member_type in _SCALAR_SIZES:
        return _SCALAR_SIZES[member.member_type] * width
    if member.member_type in (MT_REFERENCE, MT_EMPTY_REFERENCE, MT_STRING):
        return pointer_size
    if member.member_type in (MT_REFERENCE_TO_ARRAY, MT_ARRAY_OF_REFERENCES):
        return 4 + pointer_size
    if member.member_type == MT_VARIANT_REFERENCE:
        return pointer_size * 2
    if member.member_type == MT_REFERENCE_TO_VARIANT_ARRAY:
        return pointer_size + 4 + pointer_size
    if member.member_type == MT_TRANSFORM:
        return 68
    if member.member_type == MT_INLINE:
        return 0
    return pointer_size


def summarize_object(
    loaded: LoadedGR2,
    object_ref: PointerRef,
    type_ref: PointerRef,
    *,
    max_array_refs: int = 16,
) -> dict[str, Any]:
    members = parse_type_definition_array(loaded, type_ref)
    return _summarize_object_members(
        loaded,
        object_ref,
        members,
        type_ref=type_ref,
        max_array_refs=max_array_refs,
    )


def summarize_root_object(loaded: LoadedGR2, max_arrays: int = 64) -> dict[str, Any]:
    root_type = PointerRef(*loaded.gr2.header.root_type)
    root_object = PointerRef(*loaded.gr2.header.root_object)
    members = parse_type_definition_array(loaded, root_type)
    return _summarize_object_members(
        loaded,
        root_object,
        members,
        type_ref=root_type,
        max_array_refs=max_arrays,
    )


def summarize_meshes(loaded: LoadedGR2, max_meshes: int = 32) -> dict[str, Any]:
    root_type = PointerRef(*loaded.gr2.header.root_type)
    root_object = PointerRef(*loaded.gr2.header.root_object)
    root_members = parse_type_definition_array(loaded, root_type)
    mesh_member = next((member for member in root_members if member.name == "Meshes"), None)
    if mesh_member is None or mesh_member.reference_type is None:
        return {"meshes": []}

    root_fields = _field_map(
        _summarize_object_members(
            loaded,
            root_object,
            root_members,
            type_ref=root_type,
            max_array_refs=max_meshes,
        )
    )
    mesh_field = root_fields.get("Meshes", {})
    mesh_refs = [
        PointerRef(ref["section"], ref["offset"])
        for ref in mesh_field.get("element_refs", [])[:max_meshes]
    ]
    mesh_type = mesh_member.reference_type
    mesh_type_members = parse_type_definition_array(loaded, mesh_type)
    mesh_type_map = {member.name: member for member in mesh_type_members}

    meshes = []
    for index, mesh_ref in enumerate(mesh_refs):
        mesh_summary = summarize_object(loaded, mesh_ref, mesh_type)
        fields = _field_map(mesh_summary)
        mesh = {
            "index": index,
            "name": fields.get("Name", {}).get("value") or f"Mesh_{index}",
            "object": _pointer_ref_dict(mesh_ref),
            "fields": mesh_summary["fields"],
        }
        for field_name, output_name in (
            ("PrimaryVertexData", "primary_vertex_data"),
            ("PrimaryTopology", "primary_topology"),
        ):
            target = fields.get(field_name, {}).get("target")
            member = mesh_type_map.get(field_name)
            if target and member and member.reference_type:
                target_ref = PointerRef(target["section"], target["offset"])
                mesh[output_name] = summarize_object(loaded, target_ref, member.reference_type)
        vertex_data = mesh.get("primary_vertex_data")
        if isinstance(vertex_data, dict):
            vd_fields = _field_map(vertex_data)
            component_field = vd_fields.get("VertexComponentNames")
            mesh["vertex_components"] = _read_string_object_array(loaded, component_field)
        meshes.append(mesh)

    return {
        "count": mesh_field.get("count", 0),
        "meshes": meshes,
    }


def read_array_references(
    loaded: LoadedGR2,
    array_ref: PointerRef | None,
    count: int,
    *,
    max_count: int = 64,
) -> tuple[PointerRef, ...]:
    if array_ref is None or count <= 0:
        return ()
    pointer_size = loaded.gr2.header.pointer_size // 8
    data = loaded.sections_fixed[array_ref.section]
    refs: list[PointerRef] = []
    for index in range(min(count, max_count)):
        offset = array_ref.offset + index * pointer_size
        if offset + pointer_size > len(data):
            break
        ref = loaded.resolve_fake_pointer(_read_pointer(data, offset, pointer_size))
        if ref is not None:
            refs.append(ref)
    return tuple(refs)


def read_reference_array_objects(
    loaded: LoadedGR2,
    array_ref: PointerRef | None,
    count: int,
    type_ref: PointerRef | None,
    *,
    max_count: int = 64,
) -> tuple[dict[str, Any], ...]:
    if array_ref is None or type_ref is None or count <= 0:
        return ()
    pointer_size = loaded.gr2.header.pointer_size // 8
    members = parse_type_definition_array(loaded, type_ref)
    stride = _object_storage_size(loaded, members, pointer_size)
    if stride <= 0:
        return ()
    objects = []
    for index in range(min(count, max_count)):
        ref = PointerRef(array_ref.section, array_ref.offset + index * stride)
        objects.append(
            _summarize_object_members(
                loaded,
                ref,
                members,
                type_ref=type_ref,
                max_array_refs=max_count,
            )
        )
    return tuple(objects)


def _summarize_object_members(
    loaded: LoadedGR2,
    object_ref: PointerRef,
    members: tuple[TypeDefinition, ...],
    *,
    type_ref: PointerRef,
    max_array_refs: int,
) -> dict[str, Any]:
    pointer_size = loaded.gr2.header.pointer_size // 8
    data = loaded.sections_fixed[object_ref.section]
    raw = loaded.sections_original[object_ref.section]
    offset = object_ref.offset

    fields: list[dict[str, Any]] = []
    for member in members:
        size = member_storage_size(member, pointer_size)
        if offset + size > len(data):
            break
        field: dict[str, Any] = {
            "name": member.name,
            "type": member.member_type_name,
            "offset": offset,
        }
        if member.reference_type:
            field["reference_type"] = _pointer_ref_dict(member.reference_type)
        if member.member_type in (MT_REFERENCE, MT_EMPTY_REFERENCE):
            field["target"] = _pointer_field(loaded, data, offset, pointer_size)
        elif member.member_type == MT_STRING:
            pointer = _read_pointer(data, offset, pointer_size)
            field["value"] = read_string_pointer(loaded, pointer)
            field["target"] = _pointer_ref_dict(loaded.resolve_fake_pointer(pointer))
        elif member.member_type in (MT_REFERENCE_TO_ARRAY, MT_ARRAY_OF_REFERENCES):
            count = struct.unpack_from("<I", raw, offset)[0]
            pointer = _read_pointer(data, offset + 4, pointer_size)
            field["count"] = count
            target = loaded.resolve_fake_pointer(pointer)
            field["target"] = _pointer_ref_dict(target)
            field["truncated"] = count > max_array_refs
            if member.member_type == MT_ARRAY_OF_REFERENCES:
                field["element_refs"] = [
                    _pointer_ref_dict(ref)
                    for ref in read_array_references(
                        loaded,
                        target,
                        count,
                        max_count=max_array_refs,
                    )
                ]
        elif member.member_type == MT_REFERENCE_TO_VARIANT_ARRAY:
            type_pointer = _read_pointer(data, offset, pointer_size)
            count = struct.unpack_from("<I", raw, offset + pointer_size)[0]
            object_pointer = _read_pointer(data, offset + pointer_size + 4, pointer_size)
            field["variant_type"] = _pointer_ref_dict(loaded.resolve_fake_pointer(type_pointer))
            field["count"] = count
            field["target"] = _pointer_ref_dict(loaded.resolve_fake_pointer(object_pointer))
            field["truncated"] = count > max_array_refs
        elif member.member_type == MT_VARIANT_REFERENCE:
            type_pointer = _read_pointer(data, offset, pointer_size)
            object_pointer = _read_pointer(data, offset + pointer_size, pointer_size)
            field["variant_type"] = _pointer_ref_dict(loaded.resolve_fake_pointer(type_pointer))
            field["target"] = _pointer_ref_dict(loaded.resolve_fake_pointer(object_pointer))
        elif member.member_type in (MT_INT32, MT_UINT32):
            fmt = "<i" if member.member_type == MT_INT32 else "<I"
            field["value"] = struct.unpack_from(fmt, raw, offset)[0]
        elif member.member_type == MT_REAL32:
            field["value"] = struct.unpack_from("<f", raw, offset)[0]
        fields.append(field)
        offset += size

    return {
        "type": _pointer_ref_dict(type_ref),
        "object": _pointer_ref_dict(object_ref),
        "member_count": len(members),
        "fields": fields,
    }


def _object_storage_size(
    loaded: LoadedGR2,
    members: tuple[TypeDefinition, ...],
    pointer_size: int,
    *,
    seen: frozenset[PointerRef] = frozenset(),
) -> int:
    size = 0
    for member in members:
        if member.member_type == MT_INLINE and member.reference_type and member.reference_type not in seen:
            sub_members = parse_type_definition_array(loaded, member.reference_type)
            size += _object_storage_size(
                loaded,
                sub_members,
                pointer_size,
                seen=seen | {member.reference_type},
            )
        else:
            size += member_storage_size(member, pointer_size)
    return size


def _read_pointer(data: bytes, offset: int, pointer_size: int) -> int:
    if pointer_size == 4:
        return struct.unpack_from("<I", data, offset)[0]
    return struct.unpack_from("<Q", data, offset)[0]


def _pointer_field(loaded: LoadedGR2, data: bytes, offset: int, pointer_size: int) -> dict[str, int] | None:
    return _pointer_ref_dict(loaded.resolve_fake_pointer(_read_pointer(data, offset, pointer_size)))


def _pointer_ref_dict(ref: PointerRef | None) -> dict[str, int] | None:
    if ref is None:
        return None
    return {"section": ref.section, "offset": ref.offset}


def _field_map(summary: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {field["name"]: field for field in summary.get("fields", [])}


def _read_string_object_array(
    loaded: LoadedGR2,
    field: dict[str, Any] | None,
) -> list[str]:
    if not field:
        return []
    target = field.get("target")
    type_ref = field.get("reference_type")
    if not target or not type_ref:
        return []
    objects = read_reference_array_objects(
        loaded,
        PointerRef(target["section"], target["offset"]),
        field.get("count", 0),
        PointerRef(type_ref["section"], type_ref["offset"]),
    )
    values = []
    for obj in objects:
        value = _field_map(obj).get("String", {}).get("value")
        if isinstance(value, str):
            values.append(value)
    return values


def _looks_text(raw: bytes) -> bool:
    if not raw:
        return False
    for value in raw:
        if value in (9, 10, 13):
            continue
        if value < 32 or value == 127:
            return False
    return True
```

## third_party/blendergranny/io_scene_gr2/gr2/write.py

```python
"""Minimal raw GR2 writer."""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

from .constants import (
    COMPRESSION_NONE,
    MAGIC_32LE,
    MAGIC_SIZE,
    SECTION_RECORD_SIZE,
)
from .export import ExportScene
from .fixup import PointerRef
from .types import (
    MT_ARRAY_OF_REFERENCES,
    MT_END,
    MT_INT16,
    MT_INT32,
    MT_NORMAL_UINT8,
    MT_REAL32,
    MT_REFERENCE,
    MT_REFERENCE_TO_ARRAY,
    MT_REFERENCE_TO_VARIANT_ARRAY,
    MT_STRING,
    MT_TRANSFORM,
    MT_UINT8,
    MT_UINT16,
    MT_UINT32,
    MT_VARIANT_REFERENCE,
)


SECTION_COUNT = 8
HEADER_SIZE = 72
SECTION_ARRAY_OFFSET = HEADER_SIZE
DATA_START = MAGIC_SIZE + HEADER_SIZE + SECTION_COUNT * SECTION_RECORD_SIZE
TYPE_TAG = 0x80000037


@dataclass(frozen=True)
class _Fixup:
    source: PointerRef
    target: PointerRef


@dataclass
class _SectionBuilder:
    index: int
    data: bytearray
    fixups: list[_Fixup]
    internal_alignment: int = 4

    def tell(self) -> int:
        return len(self.data)

    def align(self, amount: int = 4) -> None:
        while len(self.data) % amount:
            self.data.append(0)

    def reserve(self, size: int, *, align: int = 4) -> int:
        self.align(align)
        offset = len(self.data)
        self.data.extend(b"\x00" * size)
        return offset

    def u32(self, value: int) -> int:
        offset = len(self.data)
        self.data.extend(struct.pack("<I", value))
        return offset

    def f32(self, value: float) -> int:
        offset = len(self.data)
        self.data.extend(struct.pack("<f", float(value)))
        return offset

    def bytes(self, value: bytes, *, align: int = 4) -> int:
        self.align(align)
        offset = len(self.data)
        self.data.extend(value)
        return offset

    def c_string(self, value: str) -> int:
        raw = value.encode("utf-8", "replace") + b"\x00"
        return self.bytes(raw, align=1)

    def pointer_to(self, source_offset: int, target: PointerRef) -> None:
        self.fixups.append(_Fixup(PointerRef(self.index, source_offset), target))

    def patch_pointer(self, source_offset: int, target: PointerRef) -> None:
        struct.pack_into("<I", self.data, source_offset, 0)
        self.pointer_to(source_offset, target)


class _TypeWriter:
    def __init__(self, section: _SectionBuilder):
        self.section = section
        self.type_offsets: dict[str, int] = {}
        self.string_offsets: dict[str, int] = {}
        self.entries: list[tuple[str, tuple[tuple[int, str, str, int], ...]]] = []

    def add(self, name: str, members: tuple[tuple[int, str, str, int], ...]) -> None:
        self.type_offsets[name] = self.section.reserve((len(members) + 1) * 32)
        self.entries.append((name, members))

    def finish(self) -> None:
        for type_name, members in self.entries:
            base = self.type_offsets[type_name]
            for index, (member_type, name, ref_type, width) in enumerate(members):
                offset = base + index * 32
                name_ptr = offset + 4
                type_ptr = offset + 8
                struct.pack_into("<I", self.section.data, offset, member_type)
                self.section.patch_pointer(name_ptr, PointerRef(self.section.index, self._string(name)))
                if ref_type:
                    self.section.patch_pointer(type_ptr, PointerRef(self.section.index, self.type_offsets[ref_type]))
                struct.pack_into("<I", self.section.data, offset + 12, width or 1)
            struct.pack_into("<I", self.section.data, base + len(members) * 32, MT_END)

    def _string(self, value: str) -> int:
        offset = self.string_offsets.get(value)
        if offset is None:
            offset = self.section.c_string(value)
            self.string_offsets[value] = offset
        return offset


def write_raw_gr2(path: str | Path, scene: ExportScene) -> Path:
    """Write uncompressed mesh-only GR2 that this addon can re-import."""

    if not scene.meshes:
        raise ValueError("cannot export GR2 with no meshes")
    sections = [_SectionBuilder(index, bytearray(), []) for index in range(SECTION_COUNT)]
    main = sections[0]
    skeleton_bone_names = tuple(bone.name for bone in scene.skeletons[0].bones) if scene.skeletons else ()
    mesh_bone_binding_plan = tuple(_mesh_bone_bindings(mesh, skeleton_bone_names) for mesh in scene.meshes)
    all_rigid = all(not bone_bindings for bone_bindings in mesh_bone_binding_plan)
    vertex_section = main if all_rigid else sections[3]
    index_section = main if all_rigid else sections[4]
    if not all_rigid:
        vertex_section.internal_alignment = 32
    type_section = sections[6]
    type_writer = _build_type_tree(type_section)

    materials = _scene_materials(scene)

    root_offset = main.reserve(92)
    art_tool_info_ref = PointerRef(0, main.reserve(76))
    exporter_info_ref = PointerRef(0, main.reserve(28))
    texture_root_ref_array_offset = main.reserve(4 * len([material for material in materials if material[1]]))
    skeleton_ref_array_offset = main.reserve(4 * len(scene.skeletons))
    material_ref_array_offset = main.reserve(4 * len(materials))
    vertex_data_ref_array_offset = main.reserve(4 * len(scene.meshes))
    topology_ref_array_offset = main.reserve(4 * len(scene.meshes))
    mesh_ref_array_offset = main.reserve(4 * len(scene.meshes))
    model_ref_array_offset = main.reserve(4)
    skeleton_refs: list[PointerRef] = []
    bone_array_refs: list[PointerRef] = []
    material_refs: list[PointerRef] = []
    texture_refs: list[PointerRef | None] = []
    mesh_refs: list[PointerRef] = []
    model_ref = PointerRef(0, 0)
    model_mesh_binding_ref = PointerRef(0, 0)
    vertex_data_refs: list[PointerRef] = []
    topology_refs: list[PointerRef] = []
    material_binding_refs: list[PointerRef] = []
    triangle_group_refs: list[PointerRef] = []
    bone_binding_refs: list[PointerRef] = []
    mesh_bounds_refs: list[PointerRef] = []
    vertex_component_array_refs: list[PointerRef] = []
    vertex_component_object_refs: list[list[PointerRef]] = []
    topology_map_refs: list[tuple[PointerRef, PointerRef, PointerRef]] = []
    bone_bindings_by_mesh: list[tuple[str, ...]] = []
    vertex_offsets: list[int] = []
    index_offsets: list[int] = []
    index32_flags: list[bool] = []

    for mesh, bone_bindings in zip(scene.meshes, mesh_bone_binding_plan):
        bone_bindings_by_mesh.append(bone_bindings)
        vertex_offsets.append(_write_vertices(vertex_section, mesh, bone_bindings))
        index_offsets.append(_write_indices(index_section, mesh))
        index32_flags.append(_uses_32bit_indices(mesh))

    for skeleton in scene.skeletons:
        bone_array_refs.append(PointerRef(0, main.reserve(140 * len(skeleton.bones))))
        skeleton_refs.append(PointerRef(0, main.reserve(16)))

    for material in materials:
        material_refs.append(PointerRef(0, main.reserve(16)))
        if material[1]:
            texture_refs.append(PointerRef(0, main.reserve(16)))
        else:
            texture_refs.append(None)

    for mesh in scene.meshes:
        vertex_data_refs.append(PointerRef(0, main.reserve(28)))
        topology_refs.append(PointerRef(0, main.reserve(72)))
        triangle_group_refs.append(PointerRef(0, main.reserve(12 * len(_triangle_groups(mesh)))))
        material_binding_refs.append(PointerRef(0, main.reserve(4 * len(mesh.material_names))))
        bone_binding_refs.append(PointerRef(0, main.reserve(36 * max(1, len(bone_bindings_by_mesh[len(bone_binding_refs)])))))
        mesh_bounds_refs.append(PointerRef(0, main.reserve(24)))
        component_names = _vertex_component_names(bone_bindings_by_mesh[len(vertex_component_array_refs)])
        vertex_component_array_refs.append(PointerRef(0, main.reserve(4 * len(component_names))))
        vertex_component_object_refs.append([PointerRef(0, main.reserve(4)) for _ in component_names])
        topology_map_refs.append((PointerRef(0, 0), PointerRef(0, 0), PointerRef(0, 0)))
        mesh_refs.append(PointerRef(0, main.reserve(44)))
    model_mesh_binding_ref = PointerRef(0, main.reserve(4 * len(scene.meshes)))
    model_ref = PointerRef(0, main.reserve(92))

    from_file_name = Path(path).name
    from_file_offset = main.c_string(from_file_name)
    main.patch_pointer(root_offset, art_tool_info_ref)
    main.patch_pointer(root_offset + 4, exporter_info_ref)
    main.patch_pointer(root_offset + 8, PointerRef(0, from_file_offset))
    _write_art_tool_info(main, art_tool_info_ref.offset)
    _write_exporter_info(main, exporter_info_ref.offset)
    root_texture_refs = [] if all_rigid else [texture_ref for texture_ref in texture_refs if texture_ref is not None]
    root_material_refs = [] if all_rigid else material_refs
    root_vertex_data_refs = [] if all_rigid else vertex_data_refs
    root_topology_refs = [] if all_rigid else topology_refs
    root_model_refs_count = 0 if all_rigid else 1
    _patch_reference_array_or_empty(main, root_offset + 12, len(root_texture_refs), PointerRef(0, texture_root_ref_array_offset))
    _patch_reference_array_or_empty(main, root_offset + 20, len(root_material_refs), PointerRef(0, material_ref_array_offset))
    _patch_reference_array_or_empty(main, root_offset + 28, len(skeleton_refs), PointerRef(0, skeleton_ref_array_offset))
    _patch_reference_array_or_empty(main, root_offset + 36, len(root_vertex_data_refs), PointerRef(0, vertex_data_ref_array_offset))
    _patch_reference_array_or_empty(main, root_offset + 44, len(root_topology_refs), PointerRef(0, topology_ref_array_offset))
    _patch_reference_array_or_empty(main, root_offset + 52, len(mesh_refs), PointerRef(0, mesh_ref_array_offset))
    _patch_reference_array_or_empty(main, root_offset + 60, root_model_refs_count, PointerRef(0, model_ref_array_offset))
    struct.pack_into("<IIIIII", main.data, root_offset + 68, 0, 0, 0, 0, 0, 0)
    for index, texture_ref in enumerate(root_texture_refs):
        main.patch_pointer(texture_root_ref_array_offset + index * 4, texture_ref)
    for index, skeleton_ref in enumerate(skeleton_refs):
        main.patch_pointer(skeleton_ref_array_offset + index * 4, skeleton_ref)
    for index, material_ref in enumerate(root_material_refs):
        main.patch_pointer(material_ref_array_offset + index * 4, material_ref)
    for index, vertex_data_ref in enumerate(root_vertex_data_refs):
        main.patch_pointer(vertex_data_ref_array_offset + index * 4, vertex_data_ref)
    for index, topology_ref in enumerate(root_topology_refs):
        main.patch_pointer(topology_ref_array_offset + index * 4, topology_ref)
    for index, mesh_ref in enumerate(mesh_refs):
        main.patch_pointer(mesh_ref_array_offset + index * 4, mesh_ref)
        main.patch_pointer(model_mesh_binding_ref.offset + index * 4, mesh_ref)
    if root_model_refs_count:
        main.patch_pointer(model_ref_array_offset, model_ref)

    for index, skeleton in enumerate(scene.skeletons):
        _write_bones(main, bone_array_refs[index].offset, skeleton, root_identity=bool(skeleton.initial_placement))
        _write_skeleton(main, skeleton_refs[index].offset, skeleton, bone_array_refs[index])

    for index, material in enumerate(materials):
        texture_ref = texture_refs[index]
        if texture_ref is not None:
            _write_texture(main, texture_ref.offset, material[1], material[2])
        _write_material(main, material_refs[index].offset, material[0], texture_ref)

    material_index_by_key = {material: index for index, material in enumerate(materials)}
    for index, mesh in enumerate(scene.meshes):
        _write_material_bindings(
            main,
            material_binding_refs[index].offset,
            mesh,
            [] if all_rigid else material_refs,
            material_index_by_key,
        )
        _write_triangle_groups(main, triangle_group_refs[index].offset, mesh)
        _write_bone_bindings(main, bone_binding_refs[index].offset, mesh, bone_bindings_by_mesh[index])
        _write_vertex_component_names(
            main,
            _vertex_component_names(bone_bindings_by_mesh[index]),
            vertex_component_array_refs[index],
            vertex_component_object_refs[index],
        )
        _write_vertex_data(
            main,
            vertex_data_refs[index].offset,
            len(mesh.vertices),
            PointerRef(vertex_section.index, vertex_offsets[index]),
            PointerRef(6, type_writer.type_offsets[_vertex_type_name(bone_bindings_by_mesh[index])]),
            len(_vertex_component_names(bone_bindings_by_mesh[index])),
            vertex_component_array_refs[index],
        )
        _write_topology(
            main,
            topology_refs[index].offset,
            len(mesh.indices),
            PointerRef(index_section.index, index_offsets[index]),
            index32_flags[index],
            triangle_group_refs[index],
            len(_triangle_groups(mesh)),
            topology_map_refs[index],
            len(mesh.vertices),
        )
        _write_mesh(
            main,
            mesh_refs[index].offset,
            mesh.name,
            vertex_data_refs[index],
            topology_refs[index],
            material_binding_refs[index],
            len(mesh.material_names),
            bone_binding_refs[index],
            len(bone_bindings_by_mesh[index]),
            mesh_bounds_refs[index],
            PointerRef(6, type_writer.type_offsets["MeshBounds"]),
            bool(bone_bindings_by_mesh[index]),
        )
        _write_mesh_bounds(main, mesh_bounds_refs[index].offset, mesh)

    _write_model(
        main,
        model_ref.offset,
        _model_name(scene),
        skeleton_refs[0] if skeleton_refs else None,
        scene.skeletons[0].initial_placement if skeleton_refs else (),
        model_mesh_binding_ref,
        len(mesh_refs),
    )

    type_writer.finish()
    output = _assemble_file(
        sections,
        root_type=PointerRef(6, type_writer.type_offsets["Root"]),
        root_object=PointerRef(0, root_offset),
    )
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(output)
    return output_path


def _build_type_tree(section: _SectionBuilder) -> _TypeWriter:
    writer = _TypeWriter(section)
    writer.add(
        "RigidExportVertex",
        (
            (MT_REAL32, "Position", "", 3),
            (MT_REAL32, "Normal", "", 3),
            (MT_REAL32, "Tangent", "", 3),
            (MT_REAL32, "TextureCoordinates0", "", 2),
        ),
    )
    writer.add(
        "Root",
        (
            (MT_REFERENCE, "ArtToolInfo", "ArtToolInfo", 1),
            (MT_REFERENCE, "ExporterInfo", "ExporterInfo", 1),
            (MT_STRING, "FromFileName", "", 1),
            (MT_ARRAY_OF_REFERENCES, "Textures", "Texture", 1),
            (MT_ARRAY_OF_REFERENCES, "Materials", "Material", 1),
            (MT_ARRAY_OF_REFERENCES, "Skeletons", "Skeleton", 1),
            (MT_ARRAY_OF_REFERENCES, "VertexDatas", "VertexData", 1),
            (MT_ARRAY_OF_REFERENCES, "TriTopologies", "TriTopology", 1),
            (MT_ARRAY_OF_REFERENCES, "Meshes", "Mesh", 1),
            (MT_ARRAY_OF_REFERENCES, "Models", "Model", 1),
            (MT_ARRAY_OF_REFERENCES, "TrackGroups", "TrackGroup", 1),
            (MT_ARRAY_OF_REFERENCES, "Animations", "Animation", 1),
            (MT_VARIANT_REFERENCE, "ExtendedData", "", 1),
        ),
    )
    writer.add(
        "ArtToolInfo",
        (
            (MT_STRING, "FromArtToolName", "", 1),
            (MT_INT32, "ArtToolMajorRevision", "", 1),
            (MT_INT32, "ArtToolMinorRevision", "", 1),
            (MT_INT32, "ArtToolPointerSize", "", 1),
            (MT_REAL32, "UnitsPerMeter", "", 1),
            (MT_REAL32, "Origin", "", 3),
            (MT_REAL32, "RightVector", "", 3),
            (MT_REAL32, "UpVector", "", 3),
            (MT_REAL32, "BackVector", "", 3),
            (MT_VARIANT_REFERENCE, "ExtendedData", "", 1),
        ),
    )
    writer.add(
        "ExporterInfo",
        (
            (MT_STRING, "ExporterName", "", 1),
            (MT_INT32, "ExporterMajorRevision", "", 1),
            (MT_INT32, "ExporterMinorRevision", "", 1),
            (MT_INT32, "ExporterCustomization", "", 1),
            (MT_INT32, "ExporterBuildNumber", "", 1),
            (MT_VARIANT_REFERENCE, "ExtendedData", "", 1),
        ),
    )
    writer.add(
        "MeshBounds",
        (
            (MT_REAL32, "MinPosition", "", 3),
            (MT_REAL32, "MaxPosition", "", 3),
        ),
    )
    writer.add(
        "Mesh",
        (
            (MT_STRING, "Name", "", 1),
            (MT_REFERENCE, "PrimaryVertexData", "VertexData", 1),
            (MT_REFERENCE_TO_ARRAY, "MorphTargets", "Mesh", 1),
            (MT_REFERENCE, "PrimaryTopology", "TriTopology", 1),
            (MT_REFERENCE_TO_ARRAY, "MaterialBindings", "MaterialBinding", 1),
            (MT_REFERENCE_TO_ARRAY, "BoneBindings", "BoneBinding", 1),
            (MT_VARIANT_REFERENCE, "ExtendedData", "", 1),
        ),
    )
    writer.add(
        "VertexData",
        (
            (MT_REFERENCE_TO_VARIANT_ARRAY, "Vertices", "", 1),
            (MT_REFERENCE_TO_ARRAY, "VertexComponentNames", "StringObject", 1),
            (MT_REFERENCE_TO_ARRAY, "VertexAnnotationSets", "VertexAnnotationSet", 1),
        ),
    )
    writer.add("StringObject", ((MT_STRING, "String", "", 1),))
    writer.add(
        "TriTopology",
        (
            (MT_REFERENCE_TO_ARRAY, "Groups", "TriMaterialGroup", 1),
            (MT_REFERENCE_TO_ARRAY, "Indices", "Int32", 1),
            (MT_REFERENCE_TO_ARRAY, "Indices16", "Int16", 1),
            (MT_REFERENCE_TO_ARRAY, "VertexToVertexMap", "Int32", 1),
            (MT_REFERENCE_TO_ARRAY, "VertexToTriangleMap", "Int32", 1),
            (MT_REFERENCE_TO_ARRAY, "SideToNeighborMap", "Int32", 1),
            (MT_REFERENCE_TO_ARRAY, "BonesForTriangle", "Int32", 1),
            (MT_REFERENCE_TO_ARRAY, "TriangleToBoneIndices", "Int32", 1),
            (MT_REFERENCE_TO_ARRAY, "TriAnnotationSets", "TriAnnotationSet", 1),
        ),
    )
    writer.add(
        "ExportVertex",
        (
            (MT_REAL32, "Position", "", 3),
            (MT_NORMAL_UINT8, "BoneWeights", "", 4),
            (MT_UINT8, "BoneIndices", "", 4),
            (MT_REAL32, "Normal", "", 3),
            (MT_REAL32, "TextureCoordinates0", "", 2),
        ),
    )
    writer.add(
        "Skeleton",
        (
            (MT_STRING, "Name", "", 1),
            (MT_REFERENCE_TO_ARRAY, "Bones", "Bone", 1),
            (MT_INT32, "LODType", "", 1),
        ),
    )
    writer.add(
        "Bone",
        (
            (MT_STRING, "Name", "", 1),
            (MT_INT32, "ParentIndex", "", 1),
            (MT_TRANSFORM, "Transform", "", 1),
            (MT_REAL32, "InverseWorldTransform", "", 16),
        ),
    )
    writer.add(
        "Model",
        (
            (MT_STRING, "Name", "", 1),
            (MT_REFERENCE, "Skeleton", "Skeleton", 1),
            (MT_TRANSFORM, "InitialPlacement", "", 1),
            (MT_REFERENCE_TO_ARRAY, "MeshBindings", "ModelMeshBinding", 1),
            (MT_VARIANT_REFERENCE, "ExtendedData", "", 1),
        ),
    )
    writer.add("ModelMeshBinding", ((MT_REFERENCE, "Mesh", "Mesh", 1),))
    writer.add("TrackGroup", ())
    writer.add("Animation", ())
    writer.add("VertexAnnotationSet", ())
    writer.add("TriAnnotationSet", ())
    writer.add(
        "Material",
        (
            (MT_STRING, "Name", "", 1),
            (MT_REFERENCE_TO_ARRAY, "Maps", "MaterialMap", 1),
            (MT_REFERENCE, "Texture", "Texture", 1),
        ),
    )
    writer.add("MaterialMap", ())
    writer.add(
        "Texture",
        (
            (MT_STRING, "FromFileName", "", 1),
            (MT_INT32, "TextureType", "", 1),
            (MT_INT32, "Width", "", 1),
            (MT_INT32, "Height", "", 1),
        ),
    )
    writer.add("MaterialBinding", ((MT_REFERENCE, "Material", "Material", 1),))
    writer.add(
        "BoneBinding",
        (
            (MT_STRING, "BoneName", "", 1),
            (MT_REAL32, "OBBMin", "", 3),
            (MT_REAL32, "OBBMax", "", 3),
            (MT_REFERENCE_TO_ARRAY, "TriangleIndices", "Int32", 1),
        ),
    )
    writer.add(
        "TriMaterialGroup",
        (
            (MT_INT32, "MaterialIndex", "", 1),
            (MT_INT32, "TriFirst", "", 1),
            (MT_INT32, "TriCount", "", 1),
        ),
    )
    writer.add("Int16", ((MT_INT16, "Int16", "", 1),))
    writer.add("Int32", ((MT_INT32, "Int32", "", 1),))
    return writer


def _write_vertices(section: _SectionBuilder, mesh, bone_bindings: tuple[str, ...]) -> int:
    section.align(32)
    offset = section.tell()
    if not bone_bindings:
        for vertex in mesh.vertices:
            for value in vertex.position:
                section.f32(value)
            for value in vertex.normal:
                section.f32(value)
            tangent = _orthogonal_tangent(vertex.normal)
            for value in tangent:
                section.f32(value)
            uv = vertex.uv or (0.0, 0.0)
            section.f32(uv[0])
            section.f32(uv[1])
        return offset

    bone_indices = {name: index for index, name in enumerate(bone_bindings)}
    for vertex in mesh.vertices:
        for value in vertex.position:
            section.f32(value)
        lanes = list(vertex.weights[:4])
        encoded_weights = _encoded_weights([item.weight for item in lanes])
        encoded_indices = [bone_indices.get(item.bone, 0) for item in lanes]
        while len(encoded_indices) < 4:
            encoded_indices.append(0)
        section.data.extend(bytes(encoded_weights))
        section.data.extend(bytes(encoded_indices[:4]))
        for value in vertex.normal:
            section.f32(value)
        uv = vertex.uv or (0.0, 0.0)
        section.f32(uv[0])
        section.f32(uv[1])
    return offset


def _vertex_type_name(bone_bindings: tuple[str, ...]) -> str:
    return "ExportVertex" if bone_bindings else "RigidExportVertex"


def _vertex_component_names(bone_bindings: tuple[str, ...]) -> tuple[str, ...]:
    if bone_bindings:
        return ("Position", "BoneWeights", "BoneIndices", "Normal", "TextureCoordinates0")
    return ("Position", "Normal", "Tangent", "TextureCoordinates0")


def _orthogonal_tangent(normal: tuple[float, float, float]) -> tuple[float, float, float]:
    nx, ny, nz = normal
    if abs(nz) < 0.9:
        tangent = (ny, -nx, 0.0)
    else:
        tangent = (1.0, 0.0, 0.0)
    length = (tangent[0] * tangent[0] + tangent[1] * tangent[1] + tangent[2] * tangent[2]) ** 0.5
    if length <= 0.0:
        return (1.0, 0.0, 0.0)
    return (tangent[0] / length, tangent[1] / length, tangent[2] / length)


def _write_indices(section: _SectionBuilder, mesh) -> int:
    offset = section.tell()
    for index in mesh.indices:
        section.data.extend(struct.pack("<I", index))
    section.align(4)
    return offset


def _write_vertex_component_names(
    main: _SectionBuilder,
    names: tuple[str, ...],
    array_ref: PointerRef,
    object_refs: list[PointerRef],
) -> None:
    for index, name in enumerate(names):
        if index >= len(object_refs):
            break
        main.patch_pointer(array_ref.offset + index * 4, object_refs[index])
        name_offset = main.c_string(name)
        main.patch_pointer(object_refs[index].offset, PointerRef(0, name_offset))


def _write_topology_maps(
    main: _SectionBuilder,
    mesh,
    vertex_to_vertex_ref: PointerRef,
    vertex_to_triangle_ref: PointerRef,
    side_to_neighbor_ref: PointerRef,
) -> None:
    first_triangle_by_vertex = [0 for _ in mesh.vertices]
    for tri_index, base in enumerate(range(0, len(mesh.indices), 3)):
        for index in mesh.indices[base : base + 3]:
            if 0 <= index < len(first_triangle_by_vertex):
                first_triangle_by_vertex[index] = tri_index
    for index in range(len(mesh.vertices)):
        struct.pack_into("<I", main.data, vertex_to_vertex_ref.offset + index * 4, index)
        struct.pack_into("<I", main.data, vertex_to_triangle_ref.offset + index * 4, first_triangle_by_vertex[index])
    for index in range(len(mesh.indices)):
        struct.pack_into("<I", main.data, side_to_neighbor_ref.offset + index * 4, index)


def _write_vertex_data(
    main: _SectionBuilder,
    offset: int,
    vertex_count: int,
    vertices_ref: PointerRef,
    vertex_type_ref: PointerRef,
    component_name_count: int,
    component_names_ref: PointerRef,
) -> None:
    main.patch_pointer(offset, vertex_type_ref)
    struct.pack_into("<I", main.data, offset + 4, vertex_count)
    main.patch_pointer(offset + 8, vertices_ref)
    _patch_reference_array_or_empty(main, offset + 12, component_name_count, component_names_ref)
    struct.pack_into("<II", main.data, offset + 20, 0, 0)


def _write_topology(
    main: _SectionBuilder,
    offset: int,
    index_count: int,
    indices_ref: PointerRef,
    use_32bit: bool,
    group_ref: PointerRef,
    group_count: int,
    map_refs: tuple[PointerRef, PointerRef, PointerRef],
    vertex_count: int,
) -> None:
    _patch_reference_array_or_empty(main, offset, group_count, group_ref)
    _patch_reference_array(main, offset + 8, index_count, indices_ref)
    struct.pack_into("<II", main.data, offset + 16, 0, 0)
    vertex_to_vertex_ref, vertex_to_triangle_ref, side_to_neighbor_ref = map_refs
    struct.pack_into("<IIIIII", main.data, offset + 24, 0, 0, 0, 0, 0, 0)
    struct.pack_into("<IIIIII", main.data, offset + 48, 0, 0, 0, 0, 0, 0)


def _write_mesh(
    main: _SectionBuilder,
    offset: int,
    name: str,
    vertex_data_ref: PointerRef,
    topology_ref: PointerRef,
    material_binding_ref: PointerRef,
    material_binding_count: int,
    bone_binding_ref: PointerRef,
    bone_binding_count: int,
    bounds_ref: PointerRef,
    bounds_type_ref: PointerRef,
    use_bounds: bool,
) -> None:
    name_offset = main.c_string(name or "Mesh")
    main.patch_pointer(offset, PointerRef(0, name_offset))
    main.patch_pointer(offset + 4, vertex_data_ref)
    struct.pack_into("<II", main.data, offset + 8, 0, 0)
    main.patch_pointer(offset + 16, topology_ref)
    _patch_reference_array_or_empty(main, offset + 20, material_binding_count, material_binding_ref)
    _patch_reference_array_or_empty(main, offset + 28, bone_binding_count, bone_binding_ref)
    if use_bounds:
        main.patch_pointer(offset + 36, bounds_type_ref)
        main.patch_pointer(offset + 40, bounds_ref)
    else:
        struct.pack_into("<II", main.data, offset + 36, 0, 0)


def _write_mesh_bounds(main: _SectionBuilder, offset: int, mesh) -> None:
    bound_min, bound_max = _mesh_bounds(mesh)
    struct.pack_into("<3f", main.data, offset, *bound_min)
    struct.pack_into("<3f", main.data, offset + 12, *bound_max)


def _write_art_tool_info(main: _SectionBuilder, offset: int) -> None:
    name_offset = main.c_string("Blender")
    main.patch_pointer(offset, PointerRef(0, name_offset))
    struct.pack_into(
        "<iii f 3f 3f 3f 3f II",
        main.data,
        offset + 4,
        5,
        1,
        0,
        100.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        -1.0,
        0.0,
        0,
        0,
    )


def _write_exporter_info(main: _SectionBuilder, offset: int) -> None:
    name_offset = main.c_string("blendergranny native exporter")
    main.patch_pointer(offset, PointerRef(0, name_offset))
    struct.pack_into("<iiiiII", main.data, offset + 4, 0, 1, 0, 42, 0, 0)


def _write_material(main: _SectionBuilder, offset: int, name: str, texture_ref: PointerRef | None) -> None:
    name_offset = main.c_string(name or "Material")
    main.patch_pointer(offset, PointerRef(0, name_offset))
    struct.pack_into("<II", main.data, offset + 4, 0, 0)
    if texture_ref is not None:
        main.patch_pointer(offset + 12, texture_ref)
    else:
        struct.pack_into("<I", main.data, offset + 12, 0)


def _write_texture(main: _SectionBuilder, offset: int, texture_file: str, texture_size: tuple[int, int] | None) -> None:
    name_offset = main.c_string(texture_file)
    width, height = texture_size or (0, 0)
    main.patch_pointer(offset, PointerRef(0, name_offset))
    struct.pack_into("<iii", main.data, offset + 4, 0, int(width), int(height))


def _write_material_bindings(
    main: _SectionBuilder,
    offset: int,
    mesh,
    material_refs: list[PointerRef],
    material_index_by_key: dict[tuple[str, str, tuple[int, int] | None], int],
) -> None:
    for index, material in enumerate(_mesh_materials(mesh)):
        material_index = material_index_by_key.get(material, 0)
        if material_index < len(material_refs):
            main.patch_pointer(offset + index * 4, material_refs[material_index])


def _write_triangle_groups(main: _SectionBuilder, offset: int, mesh) -> None:
    for index, (material_index, tri_first, tri_count) in enumerate(_triangle_groups(mesh)):
        struct.pack_into("<iii", main.data, offset + index * 12, material_index, tri_first, tri_count)


def _write_bone_bindings(main: _SectionBuilder, offset: int, mesh, bone_names: tuple[str, ...]) -> None:
    bounds = _bone_bounds(mesh, bone_names)
    for index, name in enumerate(bone_names):
        binding_offset = offset + index * 36
        bound_min, bound_max = bounds.get(name, _mesh_bounds(mesh))
        name_offset = main.c_string(name)
        main.patch_pointer(binding_offset, PointerRef(0, name_offset))
        struct.pack_into("<3f", main.data, binding_offset + 4, *bound_min)
        struct.pack_into("<3f", main.data, binding_offset + 16, *bound_max)
        struct.pack_into("<III", main.data, binding_offset + 28, 0, 0, 0)


def _write_model(
    main: _SectionBuilder,
    offset: int,
    name: str,
    skeleton_ref: PointerRef | None,
    initial_placement: tuple[float, ...],
    mesh_bindings_ref: PointerRef,
    mesh_binding_count: int,
) -> None:
    name_offset = main.c_string(name or "Model")
    main.patch_pointer(offset, PointerRef(0, name_offset))
    if skeleton_ref is not None:
        main.patch_pointer(offset + 4, skeleton_ref)
    else:
        struct.pack_into("<I", main.data, offset + 4, 0)
    if len(initial_placement) == 16:
        _pack_transform(main.data, offset + 8, initial_placement, flags=3)
    else:
        _pack_identity_transform(main.data, offset + 8)
    _patch_reference_array(main, offset + 76, mesh_binding_count, mesh_bindings_ref)
    struct.pack_into("<II", main.data, offset + 84, 0, 0)


def _write_skeleton(main: _SectionBuilder, offset: int, skeleton, bones_ref: PointerRef) -> None:
    name_offset = main.c_string(skeleton.name or "Skeleton")
    main.patch_pointer(offset, PointerRef(0, name_offset))
    _patch_reference_array(main, offset + 4, len(skeleton.bones), bones_ref)
    struct.pack_into("<i", main.data, offset + 12, 0)


def _write_bones(main: _SectionBuilder, offset: int, skeleton, *, root_identity: bool = False) -> None:
    bone_indices = {bone.name: index for index, bone in enumerate(skeleton.bones)}
    for index, bone in enumerate(skeleton.bones):
        bone_offset = offset + index * 140
        name_offset = main.c_string(bone.name or f"Bone_{index}")
        main.patch_pointer(bone_offset, PointerRef(0, name_offset))
        parent_index = bone_indices.get(bone.parent, -1)
        struct.pack_into("<i", main.data, bone_offset + 4, parent_index)
        if root_identity and index == 0 and not bone.parent:
            _pack_transform_values(
                main.data,
                bone_offset + 8,
                int(getattr(bone, "granny_transform_flags", 0) or 0),
                (0.0, 0.0, 0.0),
                (0.0, 0.0, 0.0, 1.0),
            )
        else:
            _pack_bone_transform(main.data, bone_offset + 8, bone)
        _pack_inverse_world(main.data, bone_offset + 76, bone.inverse_world_transform)


def _pack_bone_transform(data: bytearray, offset: int, bone) -> None:
    position = tuple(getattr(bone, "granny_local_position", ()) or ())
    orientation = tuple(getattr(bone, "granny_local_orientation_xyzw", ()) or ())
    if len(position) == 3 and len(orientation) == 4:
        _pack_transform_values(
            data,
            offset,
            int(getattr(bone, "granny_transform_flags", 0) or 0),
            position,
            orientation,
        )
        return
    _pack_transform(data, offset, _bone_local_matrix(bone))


def _pack_transform(data: bytearray, offset: int, matrix: tuple[float, ...], *, flags: int = 0) -> None:
    position = _matrix_translation(matrix)
    orientation = _matrix_quaternion_xyzw(matrix)
    _pack_transform_values(data, offset, flags, position, orientation)


def _pack_transform_values(
    data: bytearray,
    offset: int,
    flags: int,
    position: tuple[float, float, float],
    orientation: tuple[float, float, float, float],
) -> None:
    values = (
        flags,
        position[0],
        position[1],
        position[2],
        orientation[0],
        orientation[1],
        orientation[2],
        orientation[3],
        1.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        0.0,
        0.0,
        1.0,
    )
    struct.pack_into("<I16f", data, offset, *values)


def _pack_identity_transform(data: bytearray, offset: int) -> None:
    struct.pack_into(
        "<I16f",
        data,
        offset,
        0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0,
        1.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        0.0,
        0.0,
        1.0,
    )


def _pack_inverse_world(data: bytearray, offset: int, values: tuple[float, ...]) -> None:
    if len(values) == 16:
        struct.pack_into("<16f", data, offset, *values)
        return
    struct.pack_into(
        "<16f",
        data,
        offset,
        1.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0,
    )


def _bone_local_matrix(bone) -> tuple[float, ...]:
    return bone.granny_rest_local if len(bone.granny_rest_local) == 16 else bone.matrix_local


def _matrix_translation(matrix: tuple[float, ...]) -> tuple[float, float, float]:
    if len(matrix) != 16:
        return (0.0, 0.0, 0.0)
    return (float(matrix[3]), float(matrix[7]), float(matrix[11]))


def _matrix_quaternion_xyzw(matrix: tuple[float, ...]) -> tuple[float, float, float, float]:
    if len(matrix) != 16:
        return (0.0, 0.0, 0.0, 1.0)
    m00, m01, m02 = matrix[0], matrix[1], matrix[2]
    m10, m11, m12 = matrix[4], matrix[5], matrix[6]
    m20, m21, m22 = matrix[8], matrix[9], matrix[10]
    trace = m00 + m11 + m22
    if trace > 0.0:
        scale = (trace + 1.0) ** 0.5 * 2.0
        w = 0.25 * scale
        x = (m21 - m12) / scale
        y = (m02 - m20) / scale
        z = (m10 - m01) / scale
    elif m00 > m11 and m00 > m22:
        scale = (1.0 + m00 - m11 - m22) ** 0.5 * 2.0
        w = (m21 - m12) / scale
        x = 0.25 * scale
        y = (m01 + m10) / scale
        z = (m02 + m20) / scale
    elif m11 > m22:
        scale = (1.0 + m11 - m00 - m22) ** 0.5 * 2.0
        w = (m02 - m20) / scale
        x = (m01 + m10) / scale
        y = 0.25 * scale
        z = (m12 + m21) / scale
    else:
        scale = (1.0 + m22 - m00 - m11) ** 0.5 * 2.0
        w = (m10 - m01) / scale
        x = (m02 + m20) / scale
        y = (m12 + m21) / scale
        z = 0.25 * scale
    length = (x * x + y * y + z * z + w * w) ** 0.5
    if length <= 0.0:
        return (0.0, 0.0, 0.0, 1.0)
    return (x / length, y / length, z / length, w / length)


def _mesh_bone_bindings(mesh, skeleton_bone_names: tuple[str, ...] = ()) -> tuple[str, ...]:
    stored = tuple(getattr(mesh, "bone_binding_names", ()) or ())
    if stored:
        return stored
    names: list[str] = []
    for vertex in mesh.vertices:
        for weight in vertex.weights:
            if weight.bone and weight.bone not in names:
                names.append(weight.bone)
    return tuple(names)


def _bone_bounds(mesh, bone_names: tuple[str, ...]) -> dict[str, tuple[tuple[float, float, float], tuple[float, float, float]]]:
    points: dict[str, list[tuple[float, float, float]]] = {name: [] for name in bone_names}
    for vertex in mesh.vertices:
        for weight in vertex.weights:
            if weight.weight > 0.0 and weight.bone in points:
                points[weight.bone].append(vertex.position)
    return {name: _bounds_from_points(points[name]) for name in bone_names if points[name]}


def _mesh_bounds(mesh) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    return _bounds_from_points([vertex.position for vertex in mesh.vertices])


def _bounds_from_points(points: list[tuple[float, float, float]]) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    if not points:
        return ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
    mins = [min(point[axis] for point in points) for axis in range(3)]
    maxs = [max(point[axis] for point in points) for axis in range(3)]
    padding = 0.001
    return (
        (mins[0] - padding, mins[1] - padding, mins[2] - padding),
        (maxs[0] + padding, maxs[1] + padding, maxs[2] + padding),
    )


def _scene_materials(scene: ExportScene) -> tuple[tuple[str, str, tuple[int, int] | None], ...]:
    materials: list[tuple[str, str, tuple[int, int] | None]] = []
    for mesh in scene.meshes:
        for material in _mesh_materials(mesh):
            if material not in materials:
                materials.append(material)
    return tuple(materials)


def _mesh_materials(mesh) -> tuple[tuple[str, str, tuple[int, int] | None], ...]:
    result = []
    for index, name in enumerate(mesh.material_names):
        texture_file = mesh.material_texture_files[index] if index < len(mesh.material_texture_files) else ""
        texture_size = mesh.material_texture_sizes[index] if index < len(mesh.material_texture_sizes) else None
        result.append((name, texture_file, texture_size))
    return tuple(result)


def _model_name(scene: ExportScene) -> str:
    if scene.skeletons and scene.skeletons[0].name:
        return scene.skeletons[0].name
    if scene.meshes and scene.meshes[0].name:
        return scene.meshes[0].name
    return "Model"


def _triangle_groups(mesh) -> tuple[tuple[int, int, int], ...]:
    if not mesh.material_indices:
        return ()
    groups: list[tuple[int, int, int]] = []
    start = 0
    current = int(mesh.material_indices[0])
    for index, material_index in enumerate(mesh.material_indices[1:], start=1):
        material_index = int(material_index)
        if material_index == current:
            continue
        groups.append((current, start, index - start))
        start = index
        current = material_index
    groups.append((current, start, len(mesh.material_indices) - start))
    return tuple(groups)


def _uses_32bit_indices(mesh) -> bool:
    return any(index > 0xFFFF for index in mesh.indices) or len(mesh.vertices) > 0xFFFF


def _encoded_weights(weights: list[float]) -> list[int]:
    encoded = [max(0, min(255, int(round(weight * 255.0)))) for weight in weights[:4]]
    while len(encoded) < 4:
        encoded.append(0)
    total = sum(encoded)
    if total > 0:
        encoded[0] = max(0, min(255, encoded[0] + (255 - total)))
    return encoded[:4]


def _patch_reference_array(main: _SectionBuilder, offset: int, count: int, target: PointerRef) -> None:
    struct.pack_into("<I", main.data, offset, count)
    main.patch_pointer(offset + 4, target)


def _patch_reference_array_or_empty(main: _SectionBuilder, offset: int, count: int, target: PointerRef) -> None:
    if count <= 0:
        struct.pack_into("<II", main.data, offset, 0, 0)
        return
    _patch_reference_array(main, offset, count, target)


def _assemble_file(sections: list[_SectionBuilder], *, root_type: PointerRef, root_object: PointerRef) -> bytes:
    data_offsets: list[int] = []
    cursor = DATA_START
    for section in sections:
        cursor = _align(cursor, section.internal_alignment)
        data_offsets.append(cursor)
        cursor += len(section.data)

    fixup_offsets: list[int] = []
    fixup_blobs: list[bytes] = []
    for section in sections:
        cursor = _align(cursor, 4)
        fixup_offsets.append(cursor if section.fixups else 0)
        blob = b"".join(
            struct.pack("<III", fixup.source.offset, fixup.target.section, fixup.target.offset)
            for fixup in section.fixups
        )
        fixup_blobs.append(blob)
        cursor += len(blob)

    total_size = cursor
    output = bytearray()
    for word in MAGIC_32LE:
        output.extend(struct.pack("<I", word))
    output.extend(struct.pack("<4I", DATA_START, 0, 0, 0))
    output.extend(
        struct.pack(
            "<18I",
            7,
            total_size,
            0,
            SECTION_ARRAY_OFFSET,
            SECTION_COUNT,
            root_type.section,
            root_type.offset,
            root_object.section,
            root_object.offset,
            TYPE_TAG,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
        )
    )
    for section, data_offset, fixup_offset in zip(sections, data_offsets, fixup_offsets):
        output.extend(
            struct.pack(
                "<11I",
                COMPRESSION_NONE,
                data_offset,
                len(section.data),
                len(section.data),
                section.internal_alignment,
                len(section.data),
                len(section.data),
                fixup_offset,
                len(section.fixups),
                total_size,
                0,
            )
        )
    for section, data_offset in zip(sections, data_offsets):
        _pad_to(output, data_offset)
        output.extend(section.data)
    for blob, fixup_offset in zip(fixup_blobs, fixup_offsets):
        if not blob:
            continue
        _pad_to(output, fixup_offset)
        output.extend(blob)
    _pad_to(output, total_size)
    _patch_crc(output, SECTION_ARRAY_OFFSET)
    return bytes(output)


def _align(value: int, amount: int) -> int:
    return (value + amount - 1) & -amount


def _pad_to(output: bytearray, offset: int) -> None:
    if len(output) > offset:
        raise ValueError("GR2 writer layout overlap")
    output.extend(b"\x00" * (offset - len(output)))


def _patch_crc(output: bytearray, section_array_offset: int) -> None:
    crc_offset = MAGIC_SIZE + 8
    table_offset = MAGIC_SIZE + section_array_offset
    struct.pack_into("<I", output, crc_offset, 0)
    crc = zlib.crc32(bytes(output[table_offset:])) & 0xFFFFFFFF
    struct.pack_into("<I", output, crc_offset, crc)
```
