"""Rig contracts, reference-pose metadata, and character/library extraction."""

from __future__ import annotations

import copy
import hashlib
import json
import math
import re
from dataclasses import dataclass

from .glb import (
    clone_document,
    embedded_image_payload,
    finite_values,
    png_dimensions,
    read_float_accessor,
    repack_document,
    validate_raylib_mesh_primitives,
)


RIG_ID = "meshy_humanoid_v1"
REQUIRED_TEXTURE_SIZE = (1024, 1024)
MAX_BONES = 128
CANONICAL_CLIPS = (
    "idle",
    "combat_stance",
    "walk_forward",
    "run_forward",
    "run_backward",
    "run_forward_left",
    "run_forward_right",
    "run_back_left",
    "run_back_right",
    "hit_reaction",
    "launched_hit",
    "death",
)
# Per-character action clips an override library may add. The runtime looks these
# up by name and crossfades them over locomotion; when absent it falls back to the
# shared procedural action overlay. Generated outputs order them after the
# canonical block.
OPTIONAL_CLIPS = (
    "attack_main",
    "attack_super",
    "cast",
    "mobility",
    "guard",
    "grapple",
    "mine_deploy",
)
CHARACTER_METADATA_KEY = "brawlArenaCharacter"
LIBRARY_METADATA_KEY = "brawlArenaAnimationLibrary"


@dataclass(frozen=True)
class Rig:
    joint_names: tuple[str, ...]
    node_by_name: dict[str, int]
    parent_by_name: dict[str, str | None]
    root_name: str
    fingerprint: str


def normalize_clip_name(name):
    normalized = re.sub(r"[^a-z0-9]+", "_", (name or "").strip().lower()).strip("_")
    if not normalized:
        raise ValueError("animation clip has an empty name")
    return normalized


def is_reference_clip_name(name):
    """True for any spelling of the Meshy bind/T-pose reference clip.

    Raw Character_output exports name it ``Armature|clip0|baselayer``; the
    low-level fixer renames it ``tpose``; other tooling embeds
    ``character_output``. Ordinary motion clips (``Armature|running|baselayer``,
    ``Idle_11``) must not match.
    """
    raw = (name or "").strip().lower()
    if "|" in raw:
        parts = raw.split("|")
        raw = parts[1] if len(parts) > 1 else parts[0]
    if raw in ("clip0", "clip", "baselayer"):
        return True
    normalized = re.sub(r"[^a-z0-9]+", "_", raw).strip("_")
    return normalized == "tpose" or "character_output" in normalized


def node_trs(node):
    if "matrix" in node:
        raise ValueError("character joint nodes must use TRS, not matrix transforms")
    translation = tuple(float(value) for value in node.get("translation", (0, 0, 0)))
    rotation = tuple(float(value) for value in node.get("rotation", (0, 0, 0, 1)))
    scale = tuple(float(value) for value in node.get("scale", (1, 1, 1)))
    if len(translation) != 3 or len(rotation) != 4 or len(scale) != 3:
        raise ValueError("character joint has an invalid TRS shape")
    if not finite_values((translation, rotation, scale)):
        raise ValueError("character joint contains a non-finite transform")
    return {"translation": translation, "rotation": rotation, "scale": scale}


def extract_rig(document):
    skins = document.get("skins", [])
    if len(skins) != 1:
        raise ValueError(f"character asset must contain exactly one skin, found {len(skins)}")
    joints = skins[0].get("joints", [])
    if not joints or len(joints) > MAX_BONES:
        raise ValueError(f"character skin must contain 1..{MAX_BONES} joints")

    nodes = document.get("nodes", [])
    if any(not isinstance(index, int) or not 0 <= index < len(nodes) for index in joints):
        raise ValueError("character skin contains an invalid joint node")

    parent_node = {}
    for parent_index, node in enumerate(nodes):
        for child_index in node.get("children", []):
            if child_index in parent_node:
                raise ValueError("character node has multiple parents")
            parent_node[child_index] = parent_index

    node_by_name = {}
    for node_index in joints:
        name = nodes[node_index].get("name")
        if not isinstance(name, str) or not name:
            raise ValueError("every character joint must have a name")
        if name in node_by_name:
            raise ValueError(f"duplicate character joint name {name}")
        node_by_name[name] = node_index
        node_trs(nodes[node_index])

    joint_set = set(joints)
    parent_by_name = {}
    roots = []
    for name, node_index in node_by_name.items():
        parent_index = parent_node.get(node_index)
        if parent_index in joint_set:
            parent_name = nodes[parent_index].get("name")
        else:
            parent_name = None
            roots.append(name)
        parent_by_name[name] = parent_name
    if len(roots) != 1:
        raise ValueError(f"character rig must have one joint root, found {roots}")

    topology = sorted((name, parent_by_name[name]) for name in node_by_name)
    fingerprint = hashlib.sha256(
        json.dumps(topology, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    ordered_names = tuple(nodes[index]["name"] for index in joints)
    return Rig(ordered_names, node_by_name, parent_by_name, roots[0], fingerprint)


def assert_compatible_rigs(source, target):
    if source.fingerprint != target.fingerprint:
        source_only = sorted(set(source.joint_names) - set(target.joint_names))
        target_only = sorted(set(target.joint_names) - set(source.joint_names))
        parent_mismatch = sorted(
            name for name in set(source.joint_names) & set(target.joint_names)
            if source.parent_by_name[name] != target.parent_by_name[name]
        )
        raise ValueError(
            "rig topology mismatch"
            f" (source-only={source_only}, target-only={target_only},"
            f" parent-mismatch={parent_mismatch})"
        )


def _metadata_pose(document):
    extras = document.get("extras", {})
    for key in (CHARACTER_METADATA_KEY, LIBRARY_METADATA_KEY):
        metadata = extras.get(key)
        if metadata and metadata.get("animationRestPose"):
            pose = {}
            for joint in metadata["animationRestPose"]:
                name = joint["name"]
                pose[name] = {
                    "translation": tuple(joint["translation"]),
                    "rotation": tuple(joint["rotation"]),
                    "scale": tuple(joint["scale"]),
                }
            return pose
    return None


def _reference_animation(document):
    candidates = [
        animation for animation in document.get("animations", [])
        if is_reference_clip_name(animation.get("name", "clip"))
    ]
    if len(candidates) != 1:
        raise ValueError(
            "character asset must contain exactly one T-pose/Character_output"
            f" reference animation, found {len(candidates)}"
        )
    return candidates[0]


def extract_animation_reference_pose(document, binary):
    rig = extract_rig(document)
    stored = _metadata_pose(document)
    if stored is not None:
        if set(stored) != set(rig.joint_names):
            raise ValueError("stored animation rest pose does not match the character rig")
        return stored

    animation = _reference_animation(document)
    pose = {
        name: {
            "translation": None,
            "rotation": None,
            "scale": None,
        }
        for name in rig.joint_names
    }
    nodes = document["nodes"]
    for channel in animation.get("channels", []):
        target = channel.get("target", {})
        node_index = target.get("node")
        path = target.get("path")
        if node_index not in set(rig.node_by_name.values()) or path not in pose[nodes[node_index]["name"]]:
            continue
        sampler_index = channel.get("sampler")
        samplers = animation.get("samplers", [])
        if not isinstance(sampler_index, int) or not 0 <= sampler_index < len(samplers):
            raise ValueError("reference animation contains an invalid sampler")
        values = read_float_accessor(
            document, binary, samplers[sampler_index]["output"]
        )
        if not values:
            raise ValueError("reference animation channel contains no poses")
        pose[nodes[node_index]["name"]][path] = tuple(values[0])

    missing = [
        f"{name}.{path}"
        for name, transform in pose.items()
        for path, value in transform.items()
        if value is None
    ]
    if missing:
        raise ValueError(f"reference animation is missing channels: {missing}")
    return pose


def serialized_pose(pose, rig):
    return [
        {
            "name": name,
            "parent": rig.parent_by_name[name],
            "translation": list(pose[name]["translation"]),
            "rotation": list(pose[name]["rotation"]),
            "scale": list(pose[name]["scale"]),
        }
        for name in rig.joint_names
    ]


def _set_metadata(document, key, rig, pose, extra=None):
    metadata = {
        "formatVersion": 1,
        "rig": RIG_ID,
        "rigFingerprint": rig.fingerprint,
        "animationRestPose": serialized_pose(pose, rig),
    }
    if extra:
        metadata.update(extra)
    document.setdefault("extras", {})[key] = metadata


def extract_model_only(document, binary, character_id, reference_pose=None):
    rig = extract_rig(document)
    if reference_pose is None:
        pose = extract_animation_reference_pose(document, binary)
    else:
        if set(reference_pose) != set(rig.joint_names):
            raise ValueError("source animation rest pose does not match the character rig")
        pose = {
            name: node_trs(reference_pose[name])
            for name in rig.joint_names
        }
    result = clone_document(document)
    result.pop("animations", None)
    _set_metadata(
        result,
        CHARACTER_METADATA_KEY,
        rig,
        pose,
        {"characterId": character_id},
    )
    return repack_document(result, binary)


def extract_animation_library(document, binary, clip_map, library_id):
    rig = extract_rig(document)
    pose = extract_animation_reference_pose(document, binary)
    available = {
        normalize_clip_name(animation.get("name")): animation
        for animation in document.get("animations", [])
    }

    animations = []
    for source_name, canonical_name in clip_map.items():
        source_name = normalize_clip_name(source_name)
        canonical_name = normalize_clip_name(canonical_name)
        if source_name not in available:
            raise ValueError(f"animation source does not contain clip {source_name}")
        animation = copy.deepcopy(available[source_name])
        animation["name"] = canonical_name
        animations.append(animation)
    names = [animation["name"] for animation in animations]
    if len(names) != len(set(names)):
        raise ValueError("animation library contains duplicate canonical clip names")

    result = clone_document(document)
    result["animations"] = animations
    for node in result.get("nodes", []):
        node.pop("mesh", None)
        node.pop("weights", None)
    for skin in result.get("skins", []):
        skin.pop("inverseBindMatrices", None)
    for key in ("meshes", "materials", "textures", "images", "samplers", "cameras"):
        result.pop(key, None)
    _set_metadata(
        result,
        LIBRARY_METADATA_KEY,
        rig,
        pose,
        {"libraryId": library_id, "clips": names},
    )
    return repack_document(result, binary)


def validate_character_model(document, binary, required_rig=None):
    rig = extract_rig(document)
    if document.get("animations"):
        raise ValueError("normalized character model must not contain animation clips")
    validate_raylib_mesh_primitives(document, binary)

    metadata = document.get("extras", {}).get(CHARACTER_METADATA_KEY)
    if not metadata:
        raise ValueError("normalized character model is missing pipeline metadata")
    if metadata.get("rig") != RIG_ID or metadata.get("rigFingerprint") != rig.fingerprint:
        raise ValueError("character model rig metadata does not match its skeleton")
    extract_animation_reference_pose(document, binary)

    if required_rig is not None:
        assert_compatible_rigs(required_rig, rig)

    images = document.get("images", [])
    if not images:
        raise ValueError("normalized character model has no embedded textures")
    dimensions = []
    for image in images:
        payload = embedded_image_payload(document, binary, image)
        size = png_dimensions(payload)
        if size != REQUIRED_TEXTURE_SIZE:
            raise ValueError(
                f"character texture is {size[0]}x{size[1]};"
                f" required size is {REQUIRED_TEXTURE_SIZE[0]}x{REQUIRED_TEXTURE_SIZE[1]}"
            )
        dimensions.append(size)

    for name, node_index in rig.node_by_name.items():
        scale = node_trs(document["nodes"][node_index])["scale"]
        if any(abs(component - 1.0) > 1e-3 for component in scale):
            raise ValueError(f"joint {name} has non-unit bind scale {scale}")
    return rig, dimensions
