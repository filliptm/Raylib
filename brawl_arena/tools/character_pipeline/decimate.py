"""Decimation for dense character meshes that arrive without retopology.

Meshy's raw (un-retopologized) exports are effectively triangle soup: almost no
vertex sharing (226k vertices for 101k triangles) and a per-triangle noise
atlas for a texture. Quadric decimation on a soup mostly deletes triangles -
edge collapses need shared edges - leaving a shredded surface, and the atlas
UVs are meaningless on any other topology. So this module:

1. welds vertices by position to restore surface connectivity,
2. runs quadric decimation on the welded mesh down to the triangle budget,
3. transfers joints/weights from the nearest original vertex,
4. recomputes smooth normals from the decimated surface, and
5. bakes the source texture into per-vertex colors (the runtime's lit shaders
   multiply albedo by the vertex color, and meshes without colors default to
   white), replacing the useless atlas with a plain white texture that keeps
   the 1024x1024 asset contract satisfied.

Retopologized exports inside the budget pass through untouched - preferred,
since a real UV atlas beats vertex colors.
"""

from __future__ import annotations

import io
import struct

from .glb import (
    _append_unsigned_short_indices,
    _read_unsigned_scalar_accessor,
    append_float_accessor,
    clone_document,
    read_float_accessor,
    repack_document,
)

TARGET_TRIANGLES = 8000
WELD_DECIMALS = 3


def _primitive_triangle_count(document, primitive):
    if "indices" in primitive:
        return document["accessors"][primitive["indices"]]["count"] // 3
    return document["accessors"][primitive["attributes"]["POSITION"]]["count"] // 3


def total_triangles(document):
    return sum(
        _primitive_triangle_count(document, primitive)
        for mesh in document.get("meshes", [])
        for primitive in mesh.get("primitives", [])
    )


def _append_u8_vec4_accessor(document, binary, values, normalized):
    while len(binary) % 4:
        binary.append(0)
    start = len(binary)
    for value in values:
        binary += struct.pack("<4B", *(int(component) for component in value))
    document.setdefault("bufferViews", []).append(
        {"buffer": 0, "byteOffset": start, "byteLength": len(binary) - start}
    )
    accessor = {
        "bufferView": len(document["bufferViews"]) - 1,
        "componentType": 5121,
        "count": len(values),
        "type": "VEC4",
    }
    if normalized:
        accessor["normalized"] = True
    document.setdefault("accessors", []).append(accessor)
    return len(document["accessors"]) - 1


def _white_png(size):
    from PIL import Image

    buffer = io.BytesIO()
    Image.new("RGB", (size, size), (255, 255, 255)).save(buffer, "PNG", optimize=True)
    return buffer.getvalue()


def _base_color_image(document, binary, primitive, numpy):
    """Decode the primitive's base color texture into an RGB array, or None."""
    from PIL import Image

    material_index = primitive.get("material")
    if material_index is None:
        return None
    material = document.get("materials", [])[material_index]
    base = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
    if not base:
        return None
    texture = document.get("textures", [])[base["index"]]
    image = document.get("images", [])[texture.get("source", 0)]
    view = document.get("bufferViews", [])[image["bufferView"]]
    offset = view.get("byteOffset", 0)
    payload = binary[offset:offset + view["byteLength"]]
    decoded = Image.open(io.BytesIO(payload)).convert("RGB")
    return numpy.asarray(decoded, dtype=numpy.uint8)


def _sample_colors(image, uvs, numpy):
    """Nearest-texel colors at each UV; white when there is no texture."""
    if image is None:
        return numpy.full((len(uvs), 3), 255, dtype=numpy.uint8)
    height, width = image.shape[0], image.shape[1]
    u = numpy.clip((uvs[:, 0] % 1.0) * (width - 1), 0, width - 1).astype(numpy.int64)
    v = numpy.clip((uvs[:, 1] % 1.0) * (height - 1), 0, height - 1).astype(numpy.int64)
    return image[v, u]


def decimate_character_mesh(document, binary, target_triangles=TARGET_TRIANGLES):
    """Reduce the whole character to the triangle budget.

    Returns ``(document, binary, before, after)``. A mesh already inside the
    budget is returned untouched.
    """
    before = total_triangles(document)
    if before <= target_triangles:
        return document, binary, before, before

    try:
        import numpy
        from fast_simplification import simplify
        from scipy.spatial import cKDTree
    except ImportError as exc:
        raise RuntimeError(
            "decimating a dense character requires numpy, scipy, and"
            " fast_simplification (python3 -m pip install fast_simplification"
            " scipy)"
        ) from exc

    result = clone_document(document)
    output_binary = bytearray(binary)
    replaced_image = False

    for mesh_index, mesh in enumerate(document.get("meshes", [])):
        for primitive_index, primitive in enumerate(mesh.get("primitives", [])):
            if primitive.get("mode", 4) != 4:
                raise ValueError("only triangle primitives can be decimated")
            if primitive.get("targets"):
                raise ValueError("morph-target primitives cannot be decimated")

            attributes = primitive["attributes"]
            positions = numpy.asarray(
                read_float_accessor(document, binary, attributes["POSITION"]),
                dtype=numpy.float32,
            )
            if "indices" in primitive:
                indices = _read_unsigned_scalar_accessor(
                    document, binary, primitive["indices"]
                )
            else:
                indices = list(range(len(positions)))
            faces = numpy.asarray(indices, dtype=numpy.int64).reshape(-1, 3)

            share = len(faces) / before
            primitive_target = max(64, int(round(target_triangles * share)))
            if len(faces) <= primitive_target:
                continue

            # 1. Weld coincident vertices so the soup becomes a connected
            # surface that edge collapse can actually simplify.
            rounded = numpy.round(positions, WELD_DECIMALS)
            _, weld_first, weld_map = numpy.unique(
                rounded, axis=0, return_index=True, return_inverse=True
            )
            welded_positions = positions[weld_first]
            welded_faces = weld_map[faces]
            keep = (
                (welded_faces[:, 0] != welded_faces[:, 1])
                & (welded_faces[:, 1] != welded_faces[:, 2])
                & (welded_faces[:, 0] != welded_faces[:, 2])
            )
            welded_faces = welded_faces[keep]

            # 2. Quadric decimation on the welded surface.
            new_positions, new_faces = simplify(
                welded_positions.astype(numpy.float32),
                welded_faces.astype(numpy.int32),
                target_count=primitive_target,
            )
            new_positions = numpy.asarray(new_positions, dtype=numpy.float32)
            new_faces = numpy.asarray(new_faces, dtype=numpy.int64)
            keep = (
                (new_faces[:, 0] != new_faces[:, 1])
                & (new_faces[:, 1] != new_faces[:, 2])
                & (new_faces[:, 0] != new_faces[:, 2])
            )
            new_faces = new_faces[keep]

            used = numpy.unique(new_faces)
            remap = numpy.full(len(new_positions), -1, dtype=numpy.int64)
            remap[used] = numpy.arange(len(used))
            new_positions = new_positions[used]
            new_faces = remap[new_faces]
            if len(new_positions) > 65535:
                raise ValueError("decimated primitive still exceeds u16 vertices")

            # 3. Discrete attributes come from the nearest original vertex, so
            # the skin binding follows the surface rather than the collapse
            # bookkeeping.
            nearest = cKDTree(positions).query(new_positions)[1]

            joints = None
            weights = None
            if "JOINTS_0" in attributes:
                joint_accessor = document["accessors"][attributes["JOINTS_0"]]
                if joint_accessor.get("componentType") != 5121:
                    raise ValueError("decimation expects u8 JOINTS_0")
                raw = _read_u8_vec4(document, binary, attributes["JOINTS_0"], numpy)
                joints = raw[nearest]
            if "WEIGHTS_0" in attributes:
                raw = numpy.asarray(
                    read_float_accessor(document, binary, attributes["WEIGHTS_0"]),
                    dtype=numpy.float32,
                )
                weights = raw[nearest]
                totals = weights.sum(axis=1, keepdims=True)
                totals[totals <= 1e-6] = 1.0
                weights /= totals

            # 4. The source atlas is meaningless on new topology: bake it to
            # per-vertex colors instead.
            colors = numpy.full((len(new_positions), 3), 255, dtype=numpy.uint8)
            if "TEXCOORD_0" in attributes:
                uvs = numpy.asarray(
                    read_float_accessor(document, binary, attributes["TEXCOORD_0"]),
                    dtype=numpy.float32,
                )
                image = _base_color_image(document, binary, primitive, numpy)
                colors = _sample_colors(image, uvs[nearest], numpy)

            # 5. Smooth normals from the decimated surface itself.
            edge1 = new_positions[new_faces[:, 1]] - new_positions[new_faces[:, 0]]
            edge2 = new_positions[new_faces[:, 2]] - new_positions[new_faces[:, 0]]
            face_normals = numpy.cross(edge1, edge2)
            normals = numpy.zeros_like(new_positions)
            for corner in range(3):
                numpy.add.at(normals, new_faces[:, corner], face_normals)
            lengths = numpy.linalg.norm(normals, axis=1, keepdims=True)
            degenerate = lengths[:, 0] <= 1e-12
            normals[degenerate] = (0.0, 1.0, 0.0)
            lengths[degenerate] = 1.0
            normals /= lengths

            new_attributes = {
                "POSITION": append_float_accessor(
                    result,
                    output_binary,
                    [tuple(value) for value in new_positions],
                    "VEC3",
                    include_bounds=True,
                ),
                "NORMAL": append_float_accessor(
                    result,
                    output_binary,
                    [tuple(value) for value in normals],
                    "VEC3",
                ),
                "TEXCOORD_0": append_float_accessor(
                    result,
                    output_binary,
                    [(0.0, 0.0)] * len(new_positions),
                    "VEC2",
                ),
                "COLOR_0": _append_u8_vec4_accessor(
                    result,
                    output_binary,
                    [(r, g, b, 255) for r, g, b in colors],
                    normalized=True,
                ),
            }
            if joints is not None:
                new_attributes["JOINTS_0"] = _append_u8_vec4_accessor(
                    result, output_binary,
                    [tuple(value) for value in joints],
                    normalized=False,
                )
            if weights is not None:
                new_attributes["WEIGHTS_0"] = append_float_accessor(
                    result,
                    output_binary,
                    [tuple(value) for value in weights],
                    "VEC4",
                )

            split = dict(primitive)
            split["attributes"] = new_attributes
            split["indices"] = _append_unsigned_short_indices(
                result, output_binary, [int(value) for value in new_faces.reshape(-1)]
            )
            result["meshes"][mesh_index]["primitives"][primitive_index] = split

            # The atlas texture no longer matches anything; swap the payload
            # for plain white so albedo comes purely from the vertex colors
            # while the 1024x1024 texture contract stays intact.
            if not replaced_image and result.get("images"):
                image_entry = result["images"][0]
                if "bufferView" in image_entry:
                    payload = _white_png(1024)
                    while len(output_binary) % 4:
                        output_binary.append(0)
                    start = len(output_binary)
                    output_binary += payload
                    result.setdefault("bufferViews", []).append(
                        {
                            "buffer": 0,
                            "byteOffset": start,
                            "byteLength": len(payload),
                        }
                    )
                    for entry in result["images"]:
                        entry["bufferView"] = len(result["bufferViews"]) - 1
                        entry["mimeType"] = "image/png"
                    replaced_image = True

    result["buffers"] = [{"byteLength": len(output_binary)}]
    result, output_binary = repack_document(result, bytes(output_binary))
    return result, output_binary, before, total_triangles(result)


def _read_u8_vec4(document, binary, accessor_index, numpy):
    accessor = document["accessors"][accessor_index]
    view = document["bufferViews"][accessor["bufferView"]]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", 4)
    count = accessor["count"]
    out = numpy.zeros((count, 4), dtype=numpy.uint8)
    for index in range(count):
        offset = start + index * stride
        out[index] = numpy.frombuffer(binary[offset:offset + 4], dtype=numpy.uint8)
    return out
