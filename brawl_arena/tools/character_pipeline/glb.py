"""Small, dependency-free helpers for the subset of glTF used by character GLBs."""

from __future__ import annotations

import copy
import json
import math
import struct
from pathlib import Path


GLB_MAGIC = 0x46546C67
GLB_VERSION = 2
JSON_CHUNK = b"JSON"
BIN_CHUNK = b"BIN\0"
FLOAT_COMPONENT = 5126
UNSIGNED_BYTE_COMPONENT = 5121
UNSIGNED_SHORT_COMPONENT = 5123
UNSIGNED_INT_COMPONENT = 5125
RAYLIB_MAX_INDEXED_VERTICES = 65535
# Whole-character budgets. The art contract is ~4-8k triangles per character
# (docs/CHARACTER_PIPELINE.md); these caps leave headroom above it while rejecting
# un-retopologized Meshy exports, which arrive at 100k+ triangles and would
# otherwise pass the per-primitive checks by being partitioned into u16 chunks.
MAX_TOTAL_TRIANGLES = 16000
MAX_TOTAL_VERTICES = 32000
COMPONENTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT4": 16,
}
COMPONENT_BYTES = {
    5120: 1,
    UNSIGNED_BYTE_COMPONENT: 1,
    5122: 2,
    UNSIGNED_SHORT_COMPONENT: 2,
    UNSIGNED_INT_COMPONENT: 4,
    FLOAT_COMPONENT: 4,
}
UNSIGNED_FORMATS = {
    UNSIGNED_BYTE_COMPONENT: "B",
    UNSIGNED_SHORT_COMPONENT: "H",
    UNSIGNED_INT_COMPONENT: "I",
}


def clone_document(document):
    return copy.deepcopy(document)


def load_glb(path):
    path = Path(path)
    payload = path.read_bytes()
    if len(payload) < 12:
        raise ValueError(f"{path}: file is shorter than a GLB header")

    magic, version, declared_length = struct.unpack_from("<III", payload)
    if magic != GLB_MAGIC or version != GLB_VERSION:
        raise ValueError(f"{path}: not a glTF 2 GLB")
    if declared_length != len(payload):
        raise ValueError(
            f"{path}: declared length {declared_length} does not match {len(payload)}"
        )

    document = None
    binary = None
    offset = 12
    while offset < len(payload):
        if offset + 8 > len(payload):
            raise ValueError(f"{path}: truncated GLB chunk header")
        chunk_length, chunk_type = struct.unpack_from("<I4s", payload, offset)
        start = offset + 8
        end = start + chunk_length
        if end > len(payload):
            raise ValueError(f"{path}: truncated GLB chunk payload")
        chunk = payload[start:end]
        if chunk_type == JSON_CHUNK:
            text = chunk.decode("utf-8").rstrip("\0 \t\r\n")
            document = json.loads(text)
        elif chunk_type == BIN_CHUNK:
            binary = bytes(chunk)
        offset = end

    if document is None or binary is None:
        raise ValueError(f"{path}: GLB must contain JSON and BIN chunks")
    buffers = document.get("buffers", [])
    if len(buffers) != 1 or buffers[0].get("uri"):
        raise ValueError(f"{path}: character GLBs must use one embedded binary buffer")
    return document, binary


def write_glb(path, document, binary):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)

    document = clone_document(document)
    blob = bytearray(binary)
    while len(blob) % 4:
        blob.append(0)
    document["buffers"] = [{"byteLength": len(blob)}]

    encoded = json.dumps(
        document, ensure_ascii=False, separators=(",", ":"), sort_keys=True
    ).encode("utf-8")
    while len(encoded) % 4:
        encoded += b" "

    total = 12 + 8 + len(encoded) + 8 + len(blob)
    output = bytearray()
    output += struct.pack("<III", GLB_MAGIC, GLB_VERSION, total)
    output += struct.pack("<I4s", len(encoded), JSON_CHUNK)
    output += encoded
    output += struct.pack("<I4s", len(blob), BIN_CHUNK)
    output += blob
    path.write_bytes(output)
    return len(output)


def read_float_accessor(document, binary, accessor_index):
    accessors = document.get("accessors", [])
    if not 0 <= accessor_index < len(accessors):
        raise ValueError(f"invalid accessor index {accessor_index}")
    accessor = accessors[accessor_index]
    if accessor.get("componentType") != FLOAT_COMPONENT:
        raise ValueError(f"accessor {accessor_index} is not FLOAT")
    if accessor.get("sparse"):
        raise ValueError("sparse animation accessors are not supported")

    value_type = accessor.get("type")
    if value_type not in COMPONENTS:
        raise ValueError(f"unsupported accessor type {value_type}")
    component_count = COMPONENTS[value_type]

    view_index = accessor.get("bufferView")
    views = document.get("bufferViews", [])
    if not isinstance(view_index, int) or not 0 <= view_index < len(views):
        raise ValueError(f"accessor {accessor_index} has an invalid bufferView")
    view = views[view_index]
    if view.get("buffer", 0) != 0:
        raise ValueError("character accessor does not use the embedded GLB buffer")

    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", component_count * 4)
    count = accessor.get("count", 0)
    if stride < component_count * 4 or count < 0:
        raise ValueError(f"accessor {accessor_index} has invalid layout")

    values = []
    value_format = "<" + "f" * component_count
    for item in range(count):
        offset = start + item * stride
        if offset + component_count * 4 > len(binary):
            raise ValueError(f"accessor {accessor_index} extends beyond the binary buffer")
        values.append(struct.unpack_from(value_format, binary, offset))
    return values


def _accessor_layout(document, binary, accessor_index):
    accessors = document.get("accessors", [])
    if not 0 <= accessor_index < len(accessors):
        raise ValueError(f"invalid accessor index {accessor_index}")
    accessor = accessors[accessor_index]
    if accessor.get("sparse"):
        raise ValueError("sparse mesh accessors are not supported")

    value_type = accessor.get("type")
    component_type = accessor.get("componentType")
    if value_type not in COMPONENTS or component_type not in COMPONENT_BYTES:
        raise ValueError(
            f"accessor {accessor_index} has unsupported mesh layout"
        )
    record_size = COMPONENTS[value_type] * COMPONENT_BYTES[component_type]

    views = document.get("bufferViews", [])
    view_index = accessor.get("bufferView")
    if not isinstance(view_index, int) or not 0 <= view_index < len(views):
        raise ValueError(f"accessor {accessor_index} has an invalid bufferView")
    view = views[view_index]
    if view.get("buffer", 0) != 0:
        raise ValueError("character accessor does not use the embedded GLB buffer")

    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", record_size)
    count = accessor.get("count", 0)
    if start < 0 or stride < record_size or count < 0:
        raise ValueError(f"accessor {accessor_index} has an invalid layout")
    if count and start + (count - 1) * stride + record_size > len(binary):
        raise ValueError(f"accessor {accessor_index} extends beyond the binary buffer")
    return accessor, start, stride, record_size


def _read_unsigned_scalar_accessor(document, binary, accessor_index):
    accessor, start, stride, record_size = _accessor_layout(
        document, binary, accessor_index
    )
    component_type = accessor.get("componentType")
    if accessor.get("type") != "SCALAR" or component_type not in UNSIGNED_FORMATS:
        raise ValueError("mesh indices must use an unsigned SCALAR accessor")
    value_format = "<" + UNSIGNED_FORMATS[component_type]
    return [
        struct.unpack_from(value_format, binary, start + item * stride)[0]
        for item in range(accessor["count"])
    ]


def _append_accessor_subset(
    document,
    output_binary,
    source_document,
    source_binary,
    accessor_index,
    source_items,
    include_bounds=False,
):
    template, start, stride, record_size = _accessor_layout(
        source_document, source_binary, accessor_index
    )
    while len(output_binary) % 4:
        output_binary.append(0)
    output_start = len(output_binary)

    minimum = None
    maximum = None
    if include_bounds and template.get("componentType") != FLOAT_COMPONENT:
        raise ValueError("mesh POSITION bounds require FLOAT components")
    value_format = "<" + "f" * COMPONENTS[template["type"]]
    for source_item in source_items:
        if not 0 <= source_item < template["count"]:
            raise ValueError("mesh index references a vertex outside its attributes")
        source_start = start + source_item * stride
        record = source_binary[source_start:source_start + record_size]
        output_binary += record
        if include_bounds:
            values = struct.unpack(value_format, record)
            if minimum is None:
                minimum = list(values)
                maximum = list(values)
            else:
                minimum = [min(left, right) for left, right in zip(minimum, values)]
                maximum = [max(left, right) for left, right in zip(maximum, values)]

    document.setdefault("bufferViews", []).append(
        {
            "buffer": 0,
            "byteOffset": output_start,
            "byteLength": len(output_binary) - output_start,
        }
    )
    accessor = {
        key: copy.deepcopy(value)
        for key, value in template.items()
        if key not in ("bufferView", "byteOffset", "count", "min", "max", "sparse")
    }
    accessor["bufferView"] = len(document["bufferViews"]) - 1
    accessor["count"] = len(source_items)
    if minimum is not None:
        accessor["min"] = minimum
        accessor["max"] = maximum
    document.setdefault("accessors", []).append(accessor)
    return len(document["accessors"]) - 1


def _append_unsigned_short_indices(document, output_binary, indices):
    while len(output_binary) % 4:
        output_binary.append(0)
    output_start = len(output_binary)
    for index in indices:
        if not 0 <= index < RAYLIB_MAX_INDEXED_VERTICES:
            raise ValueError("partitioned mesh index exceeds raylib's u16-safe range")
        output_binary += struct.pack("<H", index)

    document.setdefault("bufferViews", []).append(
        {
            "buffer": 0,
            "byteOffset": output_start,
            "byteLength": len(output_binary) - output_start,
        }
    )
    document.setdefault("accessors", []).append(
        {
            "bufferView": len(document["bufferViews"]) - 1,
            "componentType": UNSIGNED_SHORT_COMPONENT,
            "count": len(indices),
            "type": "SCALAR",
            "min": [min(indices)] if indices else [0],
            "max": [max(indices)] if indices else [0],
        }
    )
    return len(document["accessors"]) - 1


def partition_raylib_mesh_primitives(document, binary):
    """Split dense triangle primitives into u16-indexed raylib-safe chunks."""
    source_document = document
    source_binary = bytes(binary)
    result = clone_document(document)
    output_binary = bytearray(binary)
    partitions = 0

    for mesh_index, mesh in enumerate(source_document.get("meshes", [])):
        safe_primitives = []
        for primitive in mesh.get("primitives", []):
            attributes = primitive.get("attributes", {})
            if "POSITION" not in attributes:
                raise ValueError(f"mesh {mesh_index} primitive has no POSITION")
            position = source_document["accessors"][attributes["POSITION"]]
            vertex_count = position.get("count", 0)
            if any(
                source_document["accessors"][index].get("count") != vertex_count
                for index in attributes.values()
            ):
                raise ValueError("mesh primitive attributes have different vertex counts")

            index_component = None
            if "indices" in primitive:
                index_accessor = source_document["accessors"][primitive["indices"]]
                index_component = index_accessor.get("componentType")
            needs_partition = (
                vertex_count > RAYLIB_MAX_INDEXED_VERTICES or
                index_component == UNSIGNED_INT_COMPONENT
            )
            if not needs_partition:
                safe_primitives.append(copy.deepcopy(primitive))
                continue

            if primitive.get("mode", 4) != 4:
                raise ValueError("only triangle primitives can be partitioned for raylib")
            if primitive.get("targets"):
                raise ValueError("morph-target primitives cannot be partitioned")
            indices = (
                _read_unsigned_scalar_accessor(
                    source_document, source_binary, primitive["indices"]
                )
                if "indices" in primitive
                else list(range(vertex_count))
            )
            if len(indices) % 3:
                raise ValueError("triangle primitive index count is not divisible by three")

            chunks = []
            source_vertices = []
            remap = {}
            remapped_indices = []
            for triangle_start in range(0, len(indices), 3):
                triangle = indices[triangle_start:triangle_start + 3]
                new_vertices = sum(index not in remap for index in triangle)
                if (remapped_indices and
                        len(source_vertices) + new_vertices >
                        RAYLIB_MAX_INDEXED_VERTICES):
                    chunks.append((source_vertices, remapped_indices))
                    source_vertices = []
                    remap = {}
                    remapped_indices = []
                for index in triangle:
                    if not 0 <= index < vertex_count:
                        raise ValueError("mesh index references a vertex outside POSITION")
                    if index not in remap:
                        remap[index] = len(source_vertices)
                        source_vertices.append(index)
                    remapped_indices.append(remap[index])
            if remapped_indices:
                chunks.append((source_vertices, remapped_indices))

            for source_vertices, remapped_indices in chunks:
                split = copy.deepcopy(primitive)
                split["attributes"] = {
                    semantic: _append_accessor_subset(
                        result,
                        output_binary,
                        source_document,
                        source_binary,
                        accessor_index,
                        source_vertices,
                        include_bounds=semantic == "POSITION",
                    )
                    for semantic, accessor_index in attributes.items()
                }
                split["indices"] = _append_unsigned_short_indices(
                    result, output_binary, remapped_indices
                )
                safe_primitives.append(split)
            partitions += len(chunks)
        result["meshes"][mesh_index]["primitives"] = safe_primitives

    result["buffers"] = [{"byteLength": len(output_binary)}]
    return result, bytes(output_binary), partitions


def validate_raylib_mesh_primitives(document, binary):
    total_vertices = 0
    total_indices = 0
    for mesh_index, mesh in enumerate(document.get("meshes", [])):
        for primitive_index, primitive in enumerate(mesh.get("primitives", [])):
            attributes = primitive.get("attributes", {})
            position_index = attributes.get("POSITION")
            if position_index is None:
                raise ValueError(
                    f"mesh {mesh_index} primitive {primitive_index} has no POSITION"
                )
            position_count = document["accessors"][position_index].get("count", 0)
            if position_count > RAYLIB_MAX_INDEXED_VERTICES:
                raise ValueError(
                    f"mesh {mesh_index} primitive {primitive_index} has"
                    f" {position_count} vertices; raylib permits at most"
                    f" {RAYLIB_MAX_INDEXED_VERTICES}"
                )
            if any(
                document["accessors"][index].get("count") != position_count
                for index in attributes.values()
            ):
                raise ValueError("mesh primitive attributes have different vertex counts")
            total_vertices += position_count
            if "indices" not in primitive:
                total_indices += position_count
                continue
            accessor = document["accessors"][primitive["indices"]]
            if accessor.get("componentType") not in (
                UNSIGNED_BYTE_COMPONENT,
                UNSIGNED_SHORT_COMPONENT,
            ):
                raise ValueError("raylib character meshes require u8/u16 indices")
            indices = _read_unsigned_scalar_accessor(
                document, binary, primitive["indices"]
            )
            if indices and max(indices) >= position_count:
                raise ValueError("mesh index references a vertex outside POSITION")
            total_indices += len(indices)

    total_triangles = total_indices // 3
    if total_triangles > MAX_TOTAL_TRIANGLES:
        raise ValueError(
            f"character mesh has {total_triangles} triangles; the budget is"
            f" {MAX_TOTAL_TRIANGLES}. Re-export with retopology or run the"
            " import decimation stage"
        )
    if total_vertices > MAX_TOTAL_VERTICES:
        raise ValueError(
            f"character mesh has {total_vertices} vertices; the budget is"
            f" {MAX_TOTAL_VERTICES}. Re-export with retopology or run the"
            " import decimation stage"
        )


def append_float_accessor(document, binary, values, value_type, include_bounds=False):
    if value_type not in COMPONENTS:
        raise ValueError(f"unsupported accessor type {value_type}")
    component_count = COMPONENTS[value_type]
    normalized = [tuple(float(component) for component in value) for value in values]
    if any(len(value) != component_count for value in normalized):
        raise ValueError(f"{value_type} accessor values have the wrong component count")

    while len(binary) % 4:
        binary.append(0)
    start = len(binary)
    value_format = "<" + "f" * component_count
    for value in normalized:
        binary += struct.pack(value_format, *value)

    document.setdefault("bufferViews", []).append(
        {"buffer": 0, "byteOffset": start, "byteLength": len(binary) - start}
    )
    view_index = len(document["bufferViews"]) - 1

    accessor = {
        "bufferView": view_index,
        "componentType": FLOAT_COMPONENT,
        "count": len(normalized),
        "type": value_type,
    }
    if include_bounds and normalized:
        accessor["min"] = [
            min(value[component] for value in normalized)
            for component in range(component_count)
        ]
        accessor["max"] = [
            max(value[component] for value in normalized)
            for component in range(component_count)
        ]
    document.setdefault("accessors", []).append(accessor)
    document["buffers"] = [{"byteLength": len(binary)}]
    return len(document["accessors"]) - 1


def _mesh_accessor_references(document):
    references = set()
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            references.update(primitive.get("attributes", {}).values())
            if "indices" in primitive:
                references.add(primitive["indices"])
            for target in primitive.get("targets", []):
                references.update(target.values())
    return references


def _animation_accessor_references(document):
    references = set()
    for animation in document.get("animations", []):
        for sampler in animation.get("samplers", []):
            references.add(sampler["input"])
            references.add(sampler["output"])
    return references


def referenced_accessors(document):
    references = _mesh_accessor_references(document)
    references.update(_animation_accessor_references(document))
    for skin in document.get("skins", []):
        if "inverseBindMatrices" in skin:
            references.add(skin["inverseBindMatrices"])
    return references


def _remap_accessor_references(document, accessor_map):
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            primitive["attributes"] = {
                name: accessor_map[index]
                for name, index in primitive.get("attributes", {}).items()
            }
            if "indices" in primitive:
                primitive["indices"] = accessor_map[primitive["indices"]]
            for target in primitive.get("targets", []):
                for name, index in tuple(target.items()):
                    target[name] = accessor_map[index]

    for skin in document.get("skins", []):
        if "inverseBindMatrices" in skin:
            skin["inverseBindMatrices"] = accessor_map[skin["inverseBindMatrices"]]

    for animation in document.get("animations", []):
        for sampler in animation.get("samplers", []):
            sampler["input"] = accessor_map[sampler["input"]]
            sampler["output"] = accessor_map[sampler["output"]]


def _dedupe_images(document, binary):
    """Collapse byte-identical embedded images onto one copy.

    Meshy exports routinely embed the same PNG twice (base color + a second
    texture slot), which nearly doubles a character's file size for no visual
    difference. Textures are remapped; the orphaned bufferViews are garbage
    collected by the repack that follows.
    """
    images = document.get("images", [])
    if len(images) < 2:
        return

    image_map = {}
    kept = []
    by_payload = {}
    for index, image in enumerate(images):
        key = None
        view_index = image.get("bufferView")
        if isinstance(view_index, int):
            view = document.get("bufferViews", [])[view_index]
            start = view.get("byteOffset", 0)
            key = bytes(binary[start:start + view.get("byteLength", 0)])
        if key is not None and key in by_payload:
            image_map[index] = by_payload[key]
            continue
        if key is not None:
            by_payload[key] = len(kept)
        image_map[index] = len(kept)
        kept.append(image)

    if len(kept) == len(images):
        return
    document["images"] = kept
    for texture in document.get("textures", []):
        if "source" in texture:
            texture["source"] = image_map[texture["source"]]


def repack_document(document, binary):
    """Remove unreferenced accessors/views and rebuild one compact embedded buffer."""
    document = clone_document(document)
    _dedupe_images(document, binary)
    accessors = document.get("accessors", [])
    used_accessors = sorted(referenced_accessors(document))
    if any(not isinstance(index, int) or not 0 <= index < len(accessors)
           for index in used_accessors):
        raise ValueError("document contains an invalid accessor reference")

    accessor_map = {old: new for new, old in enumerate(used_accessors)}
    _remap_accessor_references(document, accessor_map)
    document["accessors"] = [copy.deepcopy(accessors[index]) for index in used_accessors]

    views = document.get("bufferViews", [])
    used_views = set()
    for accessor in document["accessors"]:
        if "bufferView" in accessor:
            used_views.add(accessor["bufferView"])
        sparse = accessor.get("sparse")
        if sparse:
            used_views.add(sparse["indices"]["bufferView"])
            used_views.add(sparse["values"]["bufferView"])
    for image in document.get("images", []):
        if "bufferView" in image:
            used_views.add(image["bufferView"])

    if any(not isinstance(index, int) or not 0 <= index < len(views)
           for index in used_views):
        raise ValueError("document contains an invalid bufferView reference")

    view_map = {old: new for new, old in enumerate(sorted(used_views))}
    compact_views = []
    compact_binary = bytearray()
    for old_index in sorted(used_views):
        view = copy.deepcopy(views[old_index])
        if view.get("buffer", 0) != 0:
            raise ValueError("character GLBs must use one embedded buffer")
        start = view.get("byteOffset", 0)
        length = view.get("byteLength", 0)
        if start < 0 or length < 0 or start + length > len(binary):
            raise ValueError(f"bufferView {old_index} has an invalid byte range")
        while len(compact_binary) % 4:
            compact_binary.append(0)
        view["buffer"] = 0
        view["byteOffset"] = len(compact_binary)
        compact_binary += binary[start:start + length]
        compact_views.append(view)

    for accessor in document["accessors"]:
        if "bufferView" in accessor:
            accessor["bufferView"] = view_map[accessor["bufferView"]]
        sparse = accessor.get("sparse")
        if sparse:
            sparse["indices"]["bufferView"] = view_map[sparse["indices"]["bufferView"]]
            sparse["values"]["bufferView"] = view_map[sparse["values"]["bufferView"]]
    for image in document.get("images", []):
        if "bufferView" in image:
            image["bufferView"] = view_map[image["bufferView"]]

    document["bufferViews"] = compact_views
    document["buffers"] = [{"byteLength": len(compact_binary)}]
    return document, bytes(compact_binary)


def accessor_duration(document, binary, accessor_index):
    accessor = document["accessors"][accessor_index]
    if accessor.get("type") != "SCALAR":
        raise ValueError("animation time accessor must be SCALAR")
    if accessor.get("max"):
        return float(accessor["max"][0])
    values = read_float_accessor(document, binary, accessor_index)
    return max((value[0] for value in values), default=0.0)


def png_dimensions(payload):
    signature = b"\x89PNG\r\n\x1a\n"
    if len(payload) < 24 or not payload.startswith(signature):
        raise ValueError("embedded character texture is not a PNG")
    return struct.unpack(">II", payload[16:24])


def embedded_image_payload(document, binary, image):
    if image.get("mimeType") != "image/png":
        raise ValueError("embedded character texture must declare image/png")
    view_index = image.get("bufferView")
    views = document.get("bufferViews", [])
    if not isinstance(view_index, int) or not 0 <= view_index < len(views):
        raise ValueError("character texture must use an embedded bufferView")
    view = views[view_index]
    start = view.get("byteOffset", 0)
    length = view.get("byteLength", 0)
    if view.get("buffer", 0) != 0 or start < 0 or length <= 0:
        raise ValueError("character texture has an invalid bufferView")
    if start + length > len(binary):
        raise ValueError("character texture extends beyond the binary buffer")
    return binary[start:start + length]


def finite_values(values):
    return all(math.isfinite(component) for value in values for component in value)
