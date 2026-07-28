#!/usr/bin/env python3
"""Validate the character manifest, model library, animation libraries, and outputs."""

import json
import sys
from pathlib import Path

from character_pipeline.glb import load_glb
from character_pipeline.rig import (
    CANONICAL_CLIPS,
    CHARACTER_METADATA_KEY,
    RIG_ID,
    assert_compatible_rigs,
    validate_character_model,
)
from character_pipeline.retarget import (
    validate_animation_library,
    validate_generated_character,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MANIFEST = PROJECT_ROOT / "data" / "characters" / "asset_manifest.json"


def project_path(value):
    return PROJECT_ROOT / value


def validate_manifest(path):
    manifest = json.loads(path.read_text())
    if manifest.get("format_version") != 1:
        raise ValueError("unsupported character manifest format_version")
    if manifest.get("rig") != RIG_ID:
        raise ValueError(f"manifest rig must be {RIG_ID}")

    characters = manifest.get("characters", [])
    ids = [entry.get("id") for entry in characters]
    classes = [entry.get("class") for entry in characters]
    outputs = [entry.get("output") for entry in characters]
    if not characters or any(not value for value in ids):
        raise ValueError("manifest contains a character without an id")
    if len(ids) != len(set(ids)):
        raise ValueError("manifest contains duplicate character ids")
    if len(classes) != len(set(classes)) or any(not value for value in classes):
        raise ValueError("manifest contains duplicate or missing character classes")
    if len(outputs) != len(set(outputs)) or any(not value for value in outputs):
        raise ValueError("manifest contains duplicate or missing runtime outputs")
    return manifest


def main():
    manifest_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_MANIFEST
    try:
        manifest = validate_manifest(manifest_path)
        base_path = project_path(manifest["animation_library"])
        base_document, base_binary = load_glb(base_path)
        base_rig, base_clips = validate_animation_library(
            base_document, base_binary, CANONICAL_CLIPS
        )
        print(
            f"{base_path.relative_to(PROJECT_ROOT)}:"
            f" {len(base_rig.joint_names)} bones, {len(base_clips)} canonical clips"
        )

        library_cache = {}
        for entry in manifest["characters"]:
            model_path = project_path(entry["model"])
            model_document, model_binary = load_glb(model_path)
            model_rig, dimensions = validate_character_model(
                model_document, model_binary, base_rig
            )
            metadata = model_document.get("extras", {}).get(CHARACTER_METADATA_KEY, {})
            if metadata.get("characterId") != entry["id"]:
                raise ValueError(
                    f"{model_path}: metadata character id does not match {entry['id']}"
                )

            for override_value in entry.get("overrides", []):
                override_path = project_path(override_value)
                if override_path not in library_cache:
                    override_document, override_binary = load_glb(override_path)
                    override_rig, override_clips = validate_animation_library(
                        override_document, override_binary
                    )
                    assert_compatible_rigs(base_rig, override_rig)
                    library_cache[override_path] = override_clips
                    print(
                        f"{override_path.relative_to(PROJECT_ROOT)}:"
                        f" overrides {', '.join(override_clips)}"
                    )

            output_path = project_path(entry["output"])
            output_document, output_binary = load_glb(output_path)
            output_rig, output_dimensions, output_clips = validate_generated_character(
                output_document, output_binary, entry["id"]
            )
            expected_libraries = [
                manifest["animation_library"],
                *entry.get("overrides", []),
            ]
            generated_metadata = output_document["extras"][
                "brawlArenaGeneratedCharacter"
            ]
            actual_libraries = [
                item.get("path") for item in generated_metadata.get("libraries", [])
            ]
            if actual_libraries != expected_libraries:
                raise ValueError(
                    f"{output_path}: generated libraries {actual_libraries}"
                    f" do not match manifest {expected_libraries}"
                )
            assert_compatible_rigs(model_rig, output_rig)
            if dimensions != output_dimensions:
                raise ValueError(
                    f"{output_path}: generated texture set differs from its model source"
                )
            texture_summary = ", ".join(
                f"{width}x{height}" for width, height in output_dimensions
            )
            print(
                f"{model_path.relative_to(PROJECT_ROOT)} ->"
                f" {output_path.relative_to(PROJECT_ROOT)}:"
                f" {texture_summary}, {len(output_clips)} clips"
            )
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"character asset validation failed: {exc}", file=sys.stderr)
        return 1

    print("character model, animation library, and generated runtime checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
