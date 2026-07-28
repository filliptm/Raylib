#!/usr/bin/env python3
"""Normalize one Meshy character into a mesh-only, 1K-textured character asset."""

import argparse
import shutil
import tempfile
import zipfile
from pathlib import Path

from character_pipeline.glb import load_glb, write_glb
from character_pipeline.rig import extract_model_only, validate_character_model
from fix_meshy_glb import CHARACTER_TEXTURE_SIZE, main as fix_meshy_glb


def safe_extract(archive, destination):
    destination = destination.resolve()
    with zipfile.ZipFile(archive) as source:
        for member in source.infolist():
            target = (destination / member.filename).resolve()
            if destination not in target.parents and target != destination:
                raise ValueError(f"ZIP member escapes extraction directory: {member.filename}")
        source.extractall(destination)


def find_character_output(source):
    candidates = sorted(
        path for path in source.rglob("*.glb")
        if "character_output" in path.name.lower()
    )
    if len(candidates) != 1:
        raise ValueError(
            f"expected one Character_output GLB under {source}, found {len(candidates)}"
        )
    return candidates[0]


def normalize_raw_source(source):
    with tempfile.TemporaryDirectory(prefix="brawl-character-import-") as temporary:
        temporary = Path(temporary)
        extracted = temporary / "extracted"
        staged = temporary / "staged"
        extracted.mkdir()
        staged.mkdir()

        if source.is_dir():
            character = find_character_output(source)
        elif source.suffix.lower() == ".zip":
            safe_extract(source, extracted)
            character = find_character_output(extracted)
        elif source.suffix.lower() == ".glb":
            character = source
        else:
            raise ValueError("source must be a Meshy directory, ZIP, or Character_output GLB")

        shutil.copy2(character, staged / character.name)
        fixed = temporary / "fixed.glb"
        fix_meshy_glb(str(staged), str(fixed), CHARACTER_TEXTURE_SIZE)
        return load_glb(fixed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--id", required=True, help="stable character asset identifier")
    parser.add_argument(
        "--fixed-runtime",
        action="store_true",
        help="source is an already-fixed combined runtime GLB; used for migration",
    )
    args = parser.parse_args()

    if args.fixed_runtime:
        document, binary = load_glb(args.source)
    else:
        document, binary = normalize_raw_source(args.source)
    output_document, output_binary = extract_model_only(
        document, binary, args.id
    )
    rig, dimensions = validate_character_model(output_document, output_binary)
    size = write_glb(args.output, output_document, output_binary)
    textures = ", ".join(f"{width}x{height}" for width, height in dimensions)
    print(
        f"{args.output}: {len(rig.joint_names)} bones, textures {textures},"
        f" {size / (1024 * 1024):.2f} MiB"
    )


if __name__ == "__main__":
    main()
