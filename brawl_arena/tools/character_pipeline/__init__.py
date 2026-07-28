"""Reusable build-time tooling for Brawl Arena's rigged character assets."""

from .glb import (
    append_float_accessor,
    clone_document,
    load_glb,
    read_float_accessor,
    repack_document,
    write_glb,
)
from .rig import (
    RIG_ID,
    REQUIRED_TEXTURE_SIZE,
    extract_animation_reference_pose,
    extract_rig,
    normalize_clip_name,
    validate_character_model,
)
from .retarget import (
    bake_character,
    validate_animation_library,
    validate_generated_character,
)

__all__ = [
    "RIG_ID",
    "REQUIRED_TEXTURE_SIZE",
    "append_float_accessor",
    "bake_character",
    "clone_document",
    "extract_animation_reference_pose",
    "extract_rig",
    "load_glb",
    "normalize_clip_name",
    "read_float_accessor",
    "repack_document",
    "validate_animation_library",
    "validate_character_model",
    "validate_generated_character",
    "write_glb",
]
