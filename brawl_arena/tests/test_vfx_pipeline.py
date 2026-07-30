#!/usr/bin/env python3
"""Regression checks for deterministic VFX atlas generation."""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from build_vfx_assets import (  # noqa: E402
    VfxAssetError,
    atlas_geometry,
    atlas_padding,
    build,
    load_manifest,
    prepare_frame,
)


def hashes(directory: Path) -> dict[str, str]:
    return {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(directory.glob("*.png"))
    }


def main() -> int:
    manifest_path = ROOT / "data/vfx/asset_manifest.json"
    manifest = load_manifest(manifest_path)

    with tempfile.TemporaryDirectory(prefix="brawl-vfx-test-") as temporary:
        first = Path(temporary) / "first"
        second = Path(temporary) / "second"
        build(manifest_path, first)
        build(manifest_path, second)

        if hashes(first) != hashes(second):
            raise AssertionError("VFX atlas build is not deterministic")
        if len(hashes(first)) != len(manifest["atlases"]):
            raise AssertionError("VFX build did not produce every declared atlas")

        for atlas in manifest["atlases"]:
            columns, rows, frames, frame_width, frame_height = atlas_geometry(atlas)
            padding = atlas_padding(atlas)
            if frames > columns * rows:
                raise AssertionError(f"{atlas['id']} overflows its atlas grid")
            if columns * frame_width > 2048 or rows * frame_height > 2048:
                raise AssertionError(f"{atlas['id']} exceeds the runtime size budget")
            if padding > 0:
                with Image.open(first / atlas["output"]) as image:
                    rgba = image.convert("RGBA")
                    for frame in range(frames):
                        x = (frame % columns) * frame_width
                        y = (frame // columns) * frame_height
                        cell = rgba.crop(
                            (x, y, x + frame_width, y + frame_height)
                        )
                        border_alpha = [
                            cell.getpixel((pixel, 0))[3]
                            for pixel in range(frame_width)
                        ]
                        border_alpha.extend(
                            cell.getpixel((pixel, frame_height - 1))[3]
                            for pixel in range(frame_width)
                        )
                        border_alpha.extend(
                            cell.getpixel((0, pixel))[3]
                            for pixel in range(frame_height)
                        )
                        border_alpha.extend(
                            cell.getpixel((frame_width - 1, pixel))[3]
                            for pixel in range(frame_height)
                        )
                        if max(border_alpha) != 0:
                            raise AssertionError(
                                f"{atlas['id']} frame {frame} has no transparent guard"
                            )

        matte_probe = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
        for y in range(3, 5):
            for x in range(3, 5):
                matte_probe.putpixel((x, y), (220, 180, 120, 255))
        bled = prepare_frame(matte_probe, 8, 8, 0)
        if bled.getpixel((2, 3)) != (220, 180, 120, 0):
            raise AssertionError("transparent RGB was not bled away from a black matte")

        invalid = json.loads(json.dumps(manifest))
        invalid["atlases"][0]["frames"] = 99
        invalid_path = Path(temporary) / "invalid.json"
        invalid_path.write_text(json.dumps(invalid), encoding="utf-8")
        try:
            build(invalid_path, Path(temporary) / "invalid-output")
        except VfxAssetError:
            pass
        else:
            raise AssertionError("invalid atlas geometry was accepted")

        invalid_padding = json.loads(json.dumps(manifest))
        invalid_padding["atlases"][0]["padding"] = 256
        invalid_padding_path = Path(temporary) / "invalid-padding.json"
        invalid_padding_path.write_text(
            json.dumps(invalid_padding), encoding="utf-8"
        )
        try:
            build(
                invalid_padding_path,
                Path(temporary) / "invalid-padding-output",
            )
        except VfxAssetError:
            pass
        else:
            raise AssertionError("invalid atlas padding was accepted")

    print(
        f"VFX pipeline tests passed: {len(manifest['atlases'])} deterministic "
        "CC0 atlases"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
