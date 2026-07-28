"""Bind-relative animation retargeting and runtime character baking."""

from __future__ import annotations

import bisect
import copy
import math

from .glb import (
    append_float_accessor,
    embedded_image_payload,
    finite_values,
    png_dimensions,
    read_float_accessor,
    repack_document,
    validate_raylib_mesh_primitives,
)
from .rig import (
    CANONICAL_CLIPS,
    LIBRARY_METADATA_KEY,
    OPTIONAL_CLIPS,
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


def _sample_channel(channel, time, rotation):
    """Sample one animation channel at `time` (clamped, LINEAR/STEP aware)."""
    times, values, interpolation = channel
    if time <= times[0]:
        return values[0]
    if time >= times[-1]:
        return values[-1]
    high = bisect.bisect_right(times, time)
    low = high - 1
    if interpolation == "STEP" or times[high] - times[low] <= 1e-9:
        return values[low]
    fraction = (time - times[low]) / (times[high] - times[low])
    before, after = values[low], values[high]
    if rotation:
        if sum(a * b for a, b in zip(before, after)) < 0:
            after = tuple(-component for component in after)
        blended = tuple(a + (b - a) * fraction for a, b in zip(before, after))
        return quat_normalize(blended)
    return tuple(a + (b - a) * fraction for a, b in zip(before, after))


def _animated_world_pose(rig, locals_):
    """Joints-only world rotations/positions for one frame of local TRS values."""
    rotations = {}
    positions = {}

    def resolve(name):
        if name in rotations:
            return
        local = locals_[name]
        local_rotation = quat_normalize(local["rotation"])
        parent = rig.parent_by_name[name]
        if parent is None:
            rotations[name] = local_rotation
            positions[name] = tuple(local["translation"])
        else:
            resolve(parent)
            rotations[name] = quat_normalize(
                quat_multiply(rotations[parent], local_rotation)
            )
            positions[name] = _add(
                positions[parent],
                quat_rotate(rotations[parent], local["translation"]),
            )

    for joint_name in rig.joint_names:
        resolve(joint_name)
    return rotations, positions


def _retarget_clip(
    source_document,
    source_binary,
    animation,
    source_rig,
    target_rig,
    source_pose,
    target_pose,
    source_world,
    target_world,
    scale_ratio,
):
    """World-space hierarchical retarget of one clip onto the target rig.

    Rotation deltas are transferred in WORLD space - Wt = (Ws_anim * inv(Ws_ref))
    * Wt_ref - then converted back to target locals through the animated parent
    chain. Applying local deltas directly (the previous approach) silently
    assumes the two rest poses use similar joint frames; a character whose rest
    is an action stance (no T-pose in the export) came out scrambled because
    every delta was applied around the wrong axis. When the rest poses coincide
    this reduces exactly to replaying the source clip.
    """
    nodes = source_document["nodes"]
    channels = {}
    for channel in animation["channels"]:
        sampler = animation["samplers"][channel["sampler"]]
        joint_name = nodes[channel["target"]["node"]]["name"]
        path = channel["target"]["path"]
        times = [
            value[0]
            for value in read_float_accessor(
                source_document, source_binary, sampler["input"]
            )
        ]
        values = read_float_accessor(source_document, source_binary, sampler["output"])
        channels[(joint_name, path)] = (
            times,
            values,
            sampler.get("interpolation", "LINEAR"),
        )

    grid = sorted({
        round(time, 6)
        for times, _, _ in channels.values()
        for time in times
    })

    source_ref_rot = {name: source_world[name][1] for name in source_rig.joint_names}
    target_ref_rot = {name: target_world[name][1] for name in target_rig.joint_names}

    tracks = {}
    previous_rotation = {}
    for time in grid:
        locals_ = {
            name: {
                "translation": _sample_channel(channels[(name, "translation")], time, False),
                "rotation": _sample_channel(channels[(name, "rotation")], time, True),
                "scale": _sample_channel(channels[(name, "scale")], time, False),
            }
            for name in source_rig.joint_names
        }
        anim_rot, anim_pos = _animated_world_pose(source_rig, locals_)

        world_rotation = {
            name: quat_normalize(
                quat_multiply(
                    quat_multiply(anim_rot[name], quat_inverse(source_ref_rot[name])),
                    target_ref_rot[name],
                )
            )
            for name in target_rig.joint_names
        }

        for name in target_rig.joint_names:
            parent = target_rig.parent_by_name[name]
            if parent is None:
                local_rotation = world_rotation[name]
                # The root follows the donor's world trajectory (scaled), so a
                # character whose rest stance offsets its hips does not carry
                # that offset into every animation frame.
                local_translation = _mul(anim_pos[name], scale_ratio)
            else:
                local_rotation = quat_normalize(
                    quat_multiply(
                        quat_inverse(world_rotation[parent]), world_rotation[name]
                    )
                )
                local_translation = retarget_translation(
                    source_pose[name]["translation"],
                    target_pose[name]["translation"],
                    locals_[name]["translation"],
                    source_world[parent][1],
                    target_world[parent][1],
                    scale_ratio,
                )
            local_scale = retarget_scale(
                source_pose[name]["scale"],
                target_pose[name]["scale"],
                locals_[name]["scale"],
            )

            previous = previous_rotation.get(name)
            if previous is not None and sum(
                a * b for a, b in zip(previous, local_rotation)
            ) < 0:
                local_rotation = tuple(-component for component in local_rotation)
            previous_rotation[name] = local_rotation

            tracks.setdefault((name, "translation"), []).append(local_translation)
            tracks.setdefault((name, "rotation"), []).append(local_rotation)
            tracks.setdefault((name, "scale"), []).append(local_scale)

    return grid, tracks


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
    library_reports = []
    for document, binary, label in libraries:
        source_rig, names = validate_animation_library(document, binary)
        assert_compatible_rigs(source_rig, target_rig)
        source_pose = extract_animation_reference_pose(document, binary)
        for animation in document.get("animations", []):
            name = animation["name"]
            selected[name] = (document, binary, animation, source_rig, source_pose, label)
        library_reports.append((label, names))

    missing = sorted(set(CANONICAL_CLIPS) - set(selected))
    if missing:
        raise ValueError(f"character {character_id} is missing canonical clips {missing}")
    unknown = sorted(set(selected) - set(CANONICAL_CLIPS) - set(OPTIONAL_CLIPS))
    if unknown:
        raise ValueError(
            f"character {character_id} libraries supply unknown clips {unknown};"
            f" allowed extras are {sorted(OPTIONAL_CLIPS)}"
        )

    # The runtime resolves clips by name, but the on-disk order is still part of
    # the asset contract. Deriving it from the canonical tuple - never from
    # library iteration or CLI argument order - keeps every generated character
    # identical no matter how the libraries were assembled.
    order = [
        name for name in (*CANONICAL_CLIPS, *OPTIONAL_CLIPS) if name in selected
    ]

    result = copy.deepcopy(model_document)
    result["animations"] = []
    result_binary = bytearray(model_binary)

    for clip_name in order:
        source_document, source_binary, animation, source_rig, source_pose, label = selected[
            clip_name
        ]
        source_world = _reference_world_transforms(source_rig, source_pose)
        scale_ratio = target_height / _rig_height(source_rig, source_pose)

        grid, tracks = _retarget_clip(
            source_document,
            source_binary,
            animation,
            source_rig,
            target_rig,
            source_pose,
            target_pose,
            source_world,
            target_world,
            scale_ratio,
        )
        grid_times = [(time,) for time in grid]
        root_key = (target_rig.root_name, "translation")
        tracks[root_key] = _remove_root_drift(tracks[root_key], grid_times)

        input_accessor = append_float_accessor(
            result, result_binary, grid_times, "SCALAR", include_bounds=True
        )
        output_animation = {"name": clip_name, "samplers": [], "channels": []}
        for joint_name in target_rig.joint_names:
            for path in ("translation", "rotation", "scale"):
                output_accessor = append_float_accessor(
                    result,
                    result_binary,
                    tracks[(joint_name, path)],
                    "VEC4" if path == "rotation" else "VEC3",
                )
                output_animation["samplers"].append(
                    {
                        "input": input_accessor,
                        "output": output_accessor,
                        "interpolation": "LINEAR",
                    }
                )
                output_animation["channels"].append(
                    {
                        "sampler": len(output_animation["samplers"]) - 1,
                        "target": {
                            "node": target_rig.node_by_name[joint_name],
                            "path": path,
                        },
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
    # The repack garbage-collects accessors orphaned by retargeting and collapses
    # byte-identical embedded textures the source model may carry.
    result, result_binary = repack_document(result, bytes(result_binary))
    return result, result_binary, order


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
    validate_raylib_mesh_primitives(document, binary)

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
    expected = list(CANONICAL_CLIPS) + [
        name for name in OPTIONAL_CLIPS if name in names
    ]
    if names != expected:
        raise ValueError(
            f"generated character clips are {names}; expected {expected}"
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
