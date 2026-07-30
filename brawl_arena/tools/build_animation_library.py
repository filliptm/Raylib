#!/usr/bin/env python3
"""Extract named clips and a donor rest pose into a mesh-free animation library."""

import argparse
from pathlib import Path

from character_pipeline.glb import load_glb, write_glb
from character_pipeline.rig import extract_animation_library
from character_pipeline.retarget import validate_animation_library


def parse_clip(value):
    if "=" not in value:
        raise argparse.ArgumentTypeError("clips must use source_name=canonical_name")
    source, canonical = value.split("=", 1)
    if not source or not canonical:
        raise argparse.ArgumentTypeError("clips must use source_name=canonical_name")
    return source, canonical


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="fixed runtime GLB containing donor clips")
    parser.add_argument("output", type=Path, help="animation-only GLB to write")
    parser.add_argument("--id", required=True, help="versioned library identifier")
    parser.add_argument(
        "--clip",
        action="append",
        type=parse_clip,
        required=True,
        help="clip mapping in source_name=canonical_name form; repeat for each clip",
    )
    args = parser.parse_args()

    clip_map = dict(args.clip)
    if len(clip_map) != len(args.clip):
        parser.error("each source clip may only be mapped once")

    document, binary = load_glb(args.source)
    output_document, output_binary = extract_animation_library(
        document, binary, clip_map, args.id
    )
    _, clips = validate_animation_library(output_document, output_binary)
    size = write_glb(args.output, output_document, output_binary)
    print(
        f"{args.output}: {len(clips)} clips, {size / (1024 * 1024):.2f} MiB"
    )


if __name__ == "__main__":
    main()
