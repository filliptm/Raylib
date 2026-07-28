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
COMPONENTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT4": 16,
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


def repack_document(document, binary):
    """Remove unreferenced accessors/views and rebuild one compact embedded buffer."""
    document = clone_document(document)
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
