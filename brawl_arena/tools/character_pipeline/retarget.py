"""Bind-relative animation retargeting and runtime character baking."""

from __future__ import annotations

import copy
import math

from .glb import (
    append_float_accessor,
    embedded_image_payload,
    finite_values,
    png_dimensions,
    read_float_accessor,
)
from .rig import (
    CANONICAL_CLIPS,
    LIBRARY_METADATA_KEY,
    REQUIRED_TEXTURE_SIZE,
    RIG_ID,
    assert_compatible_rigs,
    extract_animation_reference_pose,
    extract_rig,
    validate_character_model,
)


def quat_normalize(value):
    length = math.sqrt(sum(component * component for component in value))
    if length <= 1e-12:
        raise ValueError("animation contains a zero-length quaternion")
    return tuple(component / length for component in value)


def quat_inverse(value):
    value = quat_normalize(value)
    return (-value[0], -value[1], -value[2], value[3])


def quat_multiply(left, right):
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return (
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    )


def quat_rotate(rotation, vector):
    rotation = quat_normalize(rotation)
    point = (vector[0], vector[1], vector[2], 0.0)
    rotated = quat_multiply(quat_multiply(rotation, point), quat_inverse(rotation))
    return rotated[:3]


def _add(left, right):
    return tuple(a + b for a, b in zip(left, right))


def _sub(left, right):
    return tuple(a - b for a, b in zip(left, right))


def _mul(value, scalar):
    return tuple(component * scalar for component in value)


def _reference_world_transforms(rig, pose):
    world = {}

    def resolve(name):
        if name in world:
            return world[name]
        local = pose[name]
        local_rotation = quat_normalize(local["rotation"])
        parent = rig.parent_by_name[name]
        if parent is None:
            result = (tuple(local["translation"]), local_rotation)
        else:
            parent_position, parent_rotation = resolve(parent)
            result = (
                _add(parent_position, quat_rotate(parent_rotation, local["translation"])),
                quat_normalize(quat_multiply(parent_rotation, local_rotation)),
            )
        world[name] = result
        return result

    for joint_name in rig.joint_names:
        resolve(joint_name)
    return world


def _rig_height(rig, pose):
    world = _reference_world_transforms(rig, pose)
    heights = [position[1] for position, _ in world.values()]
    height = max(heights) - min(heights)
    if height <= 1e-6:
        raise ValueError("animation reference pose has zero height")
    return height


def retarget_rotation(source_reference, target_reference, animated):
    delta = quat_multiply(quat_inverse(source_reference), quat_normalize(animated))
    return quat_normalize(quat_multiply(quat_normalize(target_reference), delta))


def retarget_translation(
    source_reference,
    target_reference,
    animated,
    source_parent_world_rotation,
    target_parent_world_rotation,
    scale_ratio,
):
    delta = _sub(animated, source_reference)
    if source_parent_world_rotation is not None:
        world_delta = quat_rotate(source_parent_world_rotation, delta)
        delta = quat_rotate(quat_inverse(target_parent_world_rotation), world_delta)
    return _add(target_reference, _mul(delta, scale_ratio))


def retarget_scale(source_reference, target_reference, animated):
    relative = []
    for source, target, value in zip(source_reference, target_reference, animated):
        if abs(source) <= 1e-8:
            raise ValueError("animation reference pose contains a zero scale")
        relative.append(target * value / source)
    return tuple(relative)


def _library_metadata(document):
    metadata = document.get("extras", {}).get(LIBRARY_METADATA_KEY)
    if not metadata:
        raise ValueError("animation library is missing pipeline metadata")
    if metadata.get("rig") != RIG_ID:
        raise ValueError(f"animation library uses unsupported rig {metadata.get('rig')}")
    return metadata


def validate_animation_library(document, binary, required_clips=()):
    rig = extract_rig(document)
    metadata = _library_metadata(document)
    if metadata.get("rigFingerprint") != rig.fingerprint:
        raise ValueError("animation library rig metadata does not match its skeleton")
    if document.get("meshes") or document.get("images") or document.get("textures"):
        raise ValueError("animation library must not contain meshes or textures")
    extract_animation_reference_pose(document, binary)

    names = []
    joint_nodes = set(rig.node_by_name.values())
    for animation in document.get("animations", []):
        name = animation.get("name")
        if not name or name in names:
            raise ValueError(f"animation library has an invalid or duplicate clip {name}")
        names.append(name)
        keyed = set()
        samplers = animation.get("samplers", [])
        for channel in animation.get("channels", []):
            target = channel.get("target", {})
            node = target.get("node")
            path = target.get("path")
            if node not in joint_nodes or path not in ("translation", "rotation", "scale"):
                raise ValueError(f"clip {name} contains a non-rig animation channel")
            key = (node, path)
            if key in keyed:
                raise ValueError(f"clip {name} keys {key} more than once")
            keyed.add(key)

            sampler_index = channel.get("sampler")
            if not isinstance(sampler_index, int) or not 0 <= sampler_index < len(samplers):
                raise ValueError(f"clip {name} contains an invalid sampler")
            sampler = samplers[sampler_index]
            interpolation = sampler.get("interpolation", "LINEAR")
            if interpolation not in ("LINEAR", "STEP"):
                raise ValueError(
                    f"clip {name} uses unsupported interpolation {interpolation}"
                )
            times = read_float_accessor(document, binary, sampler["input"])
            values = read_float_accessor(document, binary, sampler["output"])
            if not times or len(times) != len(values):
                raise ValueError(f"clip {name} has mismatched time and pose keys")
            if any(times[index][0] > times[index + 1][0]
                   for index in range(len(times) - 1)):
                raise ValueError(f"clip {name} has decreasing key times")
            if not finite_values(times) or not finite_values(values):
                raise ValueError(f"clip {name} contains non-finite animation values")

        expected = {
            (node, path)
            for node in joint_nodes
            for path in ("translation", "rotation", "scale")
        }
        missing = expected - keyed
        if missing:
            raise ValueError(f"clip {name} is missing {len(missing)} full-TRS channels")

    missing_clips = sorted(set(required_clips) - set(names))
    if missing_clips:
        raise ValueError(f"animation library is missing required clips {missing_clips}")
    if metadata.get("clips") != names:
        raise ValueError("animation library clip metadata is stale")
    return rig, names


def _retarget_channel_values(
    path,
    joint_name,
    values,
    source_rig,
    target_rig,
    source_pose,
    target_pose,
    source_world,
    target_world,
    scale_ratio,
):
    source_reference = source_pose[joint_name][path]
    target_reference = target_pose[joint_name][path]
    output = []

    if path == "rotation":
        previous = None
        for value in values:
            transformed = retarget_rotation(source_reference, target_reference, value)
            if previous is not None and sum(a * b for a, b in zip(previous, transformed)) < 0:
                transformed = tuple(-component for component in transformed)
            output.append(transformed)
            previous = transformed
    elif path == "translation":
        parent_name = source_rig.parent_by_name[joint_name]
        source_parent_rotation = (
            source_world[parent_name][1] if parent_name is not None else None
        )
        target_parent_rotation = (
            target_world[parent_name][1] if parent_name is not None else None
        )
        output = [
            retarget_translation(
                source_reference,
                target_reference,
                value,
                source_parent_rotation,
                target_parent_rotation,
                scale_ratio,
            )
            for value in values
        ]
    elif path == "scale":
        output = [
            retarget_scale(source_reference, target_reference, value)
            for value in values
        ]
    else:
        raise ValueError(f"unsupported animation path {path}")
    return output


def _remove_root_drift(values, times):
    if len(values) < 2:
        return values
    duration = times[-1][0] - times[0][0]
    if duration <= 1e-8:
        return values
    drift_x = values[-1][0] - values[0][0]
    drift_z = values[-1][2] - values[0][2]
    corrected = []
    for value, time in zip(values, times):
        progress = (time[0] - times[0][0]) / duration
        corrected.append(
            (
                value[0] - drift_x * progress,
                value[1],
                value[2] - drift_z * progress,
            )
        )
    return corrected


def bake_character(model_document, model_binary, libraries, character_id):
    """Retarget base+override libraries and return a self-contained runtime GLB."""
    target_rig, _ = validate_character_model(model_document, model_binary)
    target_pose = extract_animation_reference_pose(model_document, model_binary)
    target_world = _reference_world_transforms(target_rig, target_pose)
    target_height = _rig_height(target_rig, target_pose)

    selected = {}
    order = []
    library_reports = []
    for document, binary, label in libraries:
        source_rig, names = validate_animation_library(document, binary)
        assert_compatible_rigs(source_rig, target_rig)
        source_pose = extract_animation_reference_pose(document, binary)
        for animation in document.get("animations", []):
            name = animation["name"]
            if name not in selected:
                order.append(name)
            selected[name] = (document, binary, animation, source_rig, source_pose, label)
        library_reports.append((label, names))

    missing = sorted(set(CANONICAL_CLIPS) - set(selected))
    if missing:
        raise ValueError(f"character {character_id} is missing canonical clips {missing}")

    result = copy.deepcopy(model_document)
    result["animations"] = []
    result_binary = bytearray(model_binary)
    input_cache = {}

    for clip_name in order:
        source_document, source_binary, animation, source_rig, source_pose, label = selected[
            clip_name
        ]
        source_world = _reference_world_transforms(source_rig, source_pose)
        scale_ratio = target_height / _rig_height(source_rig, source_pose)
        output_animation = {"name": clip_name, "samplers": [], "channels": []}

        for channel in animation["channels"]:
            sampler = animation["samplers"][channel["sampler"]]
            target = channel["target"]
            source_node = target["node"]
            joint_name = source_document["nodes"][source_node]["name"]
            path = target["path"]
            target_node = target_rig.node_by_name[joint_name]

            input_key = (id(source_document), sampler["input"])
            if input_key not in input_cache:
                times = read_float_accessor(source_document, source_binary, sampler["input"])
                input_cache[input_key] = append_float_accessor(
                    result, result_binary, times, "SCALAR", include_bounds=True
                )
            else:
                times = read_float_accessor(source_document, source_binary, sampler["input"])
            input_accessor = input_cache[input_key]

            source_values = read_float_accessor(
                source_document, source_binary, sampler["output"]
            )
            target_values = _retarget_channel_values(
                path,
                joint_name,
                source_values,
                source_rig,
                target_rig,
                source_pose,
                target_pose,
                source_world,
                target_world,
                scale_ratio,
            )
            if path == "translation" and joint_name == target_rig.root_name:
                target_values = _remove_root_drift(target_values, times)

            output_accessor = append_float_accessor(
                result,
                result_binary,
                target_values,
                "VEC4" if path == "rotation" else "VEC3",
            )
            output_animation["samplers"].append(
                {
                    "input": input_accessor,
                    "output": output_accessor,
                    "interpolation": sampler.get("interpolation", "LINEAR"),
                }
            )
            output_animation["channels"].append(
                {
                    "sampler": len(output_animation["samplers"]) - 1,
                    "target": {"node": target_node, "path": path},
                }
            )
        result["animations"].append(output_animation)

    result.setdefault("extras", {})["brawlArenaGeneratedCharacter"] = {
        "formatVersion": 1,
        "characterId": character_id,
        "rig": RIG_ID,
        "rigFingerprint": target_rig.fingerprint,
        "clips": order,
        "libraries": [
            {"path": label, "clips": names}
            for label, names in library_reports
        ],
    }
    result["buffers"] = [{"byteLength": len(result_binary)}]
    return result, bytes(result_binary), order


def validate_generated_character(document, binary, expected_id=None):
    rig = extract_rig(document)
    metadata = document.get("extras", {}).get("brawlArenaGeneratedCharacter")
    if not metadata:
        raise ValueError("generated character is missing build metadata")
    if expected_id is not None and metadata.get("characterId") != expected_id:
        raise ValueError(
            f"generated character id {metadata.get('characterId')} does not match {expected_id}"
        )
    if metadata.get("rig") != RIG_ID or metadata.get("rigFingerprint") != rig.fingerprint:
        raise ValueError("generated character rig metadata does not match its skeleton")
    if not document.get("meshes"):
        raise ValueError("generated character contains no mesh")

    images = document.get("images", [])
    if not images:
        raise ValueError("generated character contains no embedded textures")
    dimensions = []
    for image in images:
        size = png_dimensions(embedded_image_payload(document, binary, image))
        if size != REQUIRED_TEXTURE_SIZE:
            raise ValueError(
                f"generated character texture is {size[0]}x{size[1]};"
                f" required size is {REQUIRED_TEXTURE_SIZE[0]}x{REQUIRED_TEXTURE_SIZE[1]}"
            )
        dimensions.append(size)

    animations = document.get("animations", [])
    names = [animation.get("name") for animation in animations]
    if names != list(CANONICAL_CLIPS):
        raise ValueError(
            f"generated character clips are {names}; expected {list(CANONICAL_CLIPS)}"
        )
    if metadata.get("clips") != names:
        raise ValueError("generated character clip metadata is stale")

    joint_nodes = set(rig.node_by_name.values())
    root_node = rig.node_by_name[rig.root_name]
    for animation in animations:
        keyed = set()
        samplers = animation.get("samplers", [])
        for channel in animation.get("channels", []):
            target = channel.get("target", {})
            node = target.get("node")
            path = target.get("path")
            if node not in joint_nodes or path not in ("translation", "rotation", "scale"):
                raise ValueError(
                    f"generated clip {animation['name']} contains a non-rig channel"
                )
            key = (node, path)
            if key in keyed:
                raise ValueError(
                    f"generated clip {animation['name']} keys {key} more than once"
                )
            keyed.add(key)

            sampler_index = channel.get("sampler")
            if not isinstance(sampler_index, int) or not 0 <= sampler_index < len(samplers):
                raise ValueError(
                    f"generated clip {animation['name']} has an invalid sampler"
                )
            sampler = samplers[sampler_index]
            times = read_float_accessor(document, binary, sampler["input"])
            values = read_float_accessor(document, binary, sampler["output"])
            if not times or len(times) != len(values):
                raise ValueError(
                    f"generated clip {animation['name']} has mismatched key counts"
                )
            if not finite_values(times) or not finite_values(values):
                raise ValueError(
                    f"generated clip {animation['name']} contains non-finite values"
                )
            if node == root_node and path == "translation" and len(values) > 1:
                drift = math.hypot(
                    values[-1][0] - values[0][0],
                    values[-1][2] - values[0][2],
                )
                if drift > 1e-3:
                    raise ValueError(
                        f"generated clip {animation['name']} has {drift:.5f}"
                        " units of horizontal root drift"
                    )

        expected = {
            (node, path)
            for node in joint_nodes
            for path in ("translation", "rotation", "scale")
        }
        if keyed != expected:
            raise ValueError(
                f"generated clip {animation['name']} does not key full rig TRS"
            )
    return rig, dimensions, names
