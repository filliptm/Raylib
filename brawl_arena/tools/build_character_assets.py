#!/usr/bin/env python3
"""Build every self-contained runtime character GLB from a model and animation libraries."""

import argparse
import json
from pathlib import Path

from character_pipeline.glb import load_glb, write_glb
from character_pipeline.retarget import bake_character
from character_pipeline.rig import RIG_ID


PROJECT_ROOT = Path(__file__).resolve().parent.parent


def project_path(value):
    return PROJECT_ROOT / value


def load_manifest(path):
    manifest = json.loads(path.read_text())
    if manifest.get("format_version") != 1:
        raise ValueError("character asset manifest has an unsupported format_version")
    if manifest.get("rig") != RIG_ID:
        raise ValueError(f"character asset manifest must use rig {RIG_ID}")
    characters = manifest.get("characters", [])
    ids = [character.get("id") for character in characters]
    if not characters or any(not value for value in ids) or len(ids) != len(set(ids)):
        raise ValueError("character asset manifest must contain unique character ids")
    return manifest


def manifest_inputs(manifest):
    inputs = [manifest["animation_library"]]
    for character in manifest["characters"]:
        inputs.append(character["model"])
        inputs.extend(character.get("overrides", []))
    return list(dict.fromkeys(inputs))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest",
        type=Path,
        default=PROJECT_ROOT / "data" / "characters" / "asset_manifest.json",
    )
    parser.add_argument(
        "--print-inputs",
        action="store_true",
        help="print manifest-owned source paths for build dependency discovery",
    )
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    if args.print_inputs:
        print(" ".join(manifest_inputs(manifest)))
        return

    base_path = project_path(manifest["animation_library"])
    library_cache = {}

    def library(path):
        path = project_path(path)
        if path not in library_cache:
            document, binary = load_glb(path)
            library_cache[path] = (document, binary, str(path.relative_to(PROJECT_ROOT)))
        return library_cache[path]

    base_library = library(manifest["animation_library"])
    outputs = []
    for entry in manifest["characters"]:
        model_path = project_path(entry["model"])
        output_path = project_path(entry["output"])
        model_document, model_binary = load_glb(model_path)
        libraries = [base_library]
        libraries.extend(library(path) for path in entry.get("overrides", []))

        document, binary, clips = bake_character(
            model_document, model_binary, libraries, entry["id"]
        )
        size = write_glb(output_path, document, binary)
        outputs.append(output_path)
        print(
            f"{output_path.relative_to(PROJECT_ROOT)}:"
            f" {len(clips)} clips, {size / (1024 * 1024):.2f} MiB"
        )

    print(f"built {len(outputs)} character assets using {base_path.relative_to(PROJECT_ROOT)}")


if __name__ == "__main__":
    main()
