#!/usr/bin/env python3
"""Validate generated VFX atlases against the tracked manifest."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image

from build_vfx_assets import (
    ROOT,
    VfxAssetError,
    atlas_geometry,
    atlas_padding,
    load_manifest,
    manifest_inputs,
    project_path,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "manifest",
        nargs="?",
        type=Path,
        default=ROOT / "data/vfx/asset_manifest.json",
    )
    args = parser.parse_args()

    try:
        data = load_manifest(args.manifest.resolve())
        for source in manifest_inputs(data, args.manifest.resolve()):
            if not source.is_file():
                raise VfxAssetError(f"tracked VFX input is missing: {source}")
        output_directory = project_path(data["output_directory"])
        total_bytes = 0
        for atlas in data["atlases"]:
            columns, rows, frames, frame_width, frame_height = atlas_geometry(atlas)
            padding = atlas_padding(atlas)
            path = output_directory / atlas["output"]
            if not path.is_file():
                raise VfxAssetError(f"generated atlas is missing: {path.relative_to(ROOT)}")
            with Image.open(path) as image:
                if image.mode != "RGBA":
                    raise VfxAssetError(
                        f"{path.relative_to(ROOT)} must be RGBA, got {image.mode}"
                    )
                expected = (columns * frame_width, rows * frame_height)
                if image.size != expected:
                    raise VfxAssetError(
                        f"{path.relative_to(ROOT)} is {image.size}, expected {expected}"
                    )
                if frames > columns * rows:
                    raise VfxAssetError(f"{atlas['id']} has invalid frame metadata")
                if padding > 0:
                    rgba = image.convert("RGBA")
                    for frame in range(frames):
                        x = (frame % columns) * frame_width
                        y = (frame // columns) * frame_height
                        cell = rgba.crop(
                            (x, y, x + frame_width, y + frame_height)
                        )
                        borders = (
                            cell.crop((0, 0, frame_width, 1)),
                            cell.crop((0, frame_height - 1, frame_width, frame_height)),
                            cell.crop((0, 0, 1, frame_height)),
                            cell.crop((frame_width - 1, 0, frame_width, frame_height)),
                        )
                        if any(
                            border.getchannel("A").getextrema()[1] != 0
                            for border in borders
                        ):
                            raise VfxAssetError(
                                f"{atlas['id']} frame {frame} has visible pixels "
                                "on its guarded cell boundary"
                            )
            total_bytes += path.stat().st_size
    except (OSError, VfxAssetError) as exc:
        print(f"VFX validation failed: {exc}", file=sys.stderr)
        return 1

    print(
        f"VFX assets valid: {len(data['atlases'])} atlases, "
        f"{total_bytes / (1024 * 1024):.2f} MiB"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
