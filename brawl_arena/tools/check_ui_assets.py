#!/usr/bin/env python3
"""Validate the curated UI asset contract without requiring a render context."""

from __future__ import annotations

import hashlib
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

EXPECTED_HASHES = {
    "resources/ui/kenney_scifi/runtime/bar_shadow_square_large.png":
        "8184daab69104d5db7e2c0fdf85c2957b70a9463dcab9ea6f117364f7435de2a",
    "resources/ui/kenney_scifi/runtime/bar_square_large.png":
        "235e85b3a90d8e9c81c2711f861294d6519d5dd80b95a59d2f288fc92c47190f",
    "resources/ui/kenney_scifi/runtime/button_rectangle_depth.png":
        "2b7148964a577a69e7c988404caec9b723dbe00e12e0538f486d6a4fe6e13449",
    "resources/ui/kenney_scifi/runtime/feature_panel_notch.png":
        "f8c284908f3234035f3cf9905155a06de75069f0ff2c399abeb496c64cc6b3c1",
    "resources/ui/kenney_scifi/runtime/panel_rectangle_screws.png":
        "596c7fc0e44f6f719aaf59dcfa936d6b02a2d2f19dfabe4ba4d492ae08e14bb2",
    "resources/ui/scifi_interface/source/interface_2_no_effects.png":
        "fa4afb792c89f561167c754e4f08641f38c4dd7edf88764919a45ab2cc560c39",
    "resources/ui/scifi_interface/source/interface_3_no_effects.png":
        "f132d068a54115e05b91ec3bd58c44497cdbede20ba1d2b0799b18ad744573b7",
    "resources/ui/scifi_interface/runtime/orbital_ring.png":
        "54aab47ab2b51419486d70d22a605ebb595ece38be8f4a9fa34ac3ff26c9d025",
    "resources/ui/scifi_interface/runtime/radar_disc.png":
        "9c8f61cc0aa8070edd24279701cd822289da053344366d35cfa834e63b9df8ed",
}

EXPECTED_PNGS = {
    "resources/ui/kenney_scifi/runtime/bar_shadow_square_large.png": (192, 48),
    "resources/ui/kenney_scifi/runtime/bar_square_large.png": (192, 48),
    "resources/ui/kenney_scifi/runtime/button_rectangle_depth.png": (384, 128),
    "resources/ui/kenney_scifi/runtime/feature_panel_notch.png": (384, 128),
    "resources/ui/kenney_scifi/runtime/panel_rectangle_screws.png": (384, 128),
    "resources/ui/scifi_interface/source/interface_2_no_effects.png": (2048, 2048),
    "resources/ui/scifi_interface/source/interface_3_no_effects.png": (2048, 2048),
    "resources/ui/scifi_interface/runtime/orbital_ring.png": (768, 768),
    "resources/ui/scifi_interface/runtime/radar_disc.png": (512, 512),
}

REQUIRED_TEXT_AND_SOURCE = (
    "resources/ui/kenney_scifi/LICENSE.txt",
    "resources/ui/kenney_scifi/SOURCE.md",
    "resources/ui/kenney_scifi/source/bar_shadow_square_large.svg",
    "resources/ui/kenney_scifi/source/bar_square_large.svg",
    "resources/ui/kenney_scifi/source/button_rectangle_depth.svg",
    "resources/ui/kenney_scifi/source/feature_panel_notch.svg",
    "resources/ui/kenney_scifi/source/panel_rectangle_screws.svg",
    "resources/ui/scifi_interface/LICENSE.txt",
    "resources/ui/scifi_interface/SOURCE.md",
    "tools/build_ui_assets.py",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as source:
        header = source.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    return struct.unpack(">II", header[16:24])


def fail(message: str) -> None:
    raise SystemExit(f"UI asset policy failed: {message}")


def main() -> None:
    for relative, expected_hash in EXPECTED_HASHES.items():
        path = ROOT / relative
        if not path.is_file():
            fail(f"missing {relative}")
        if sha256(path) != expected_hash:
            fail(f"{relative} does not match the reviewed asset")

    for relative, expected_size in EXPECTED_PNGS.items():
        path = ROOT / relative
        try:
            actual_size = png_size(path)
        except ValueError as exc:
            fail(f"{relative}: {exc}")
        if actual_size != expected_size:
            fail(f"{relative} is {actual_size}, expected {expected_size}")

    for relative in REQUIRED_TEXT_AND_SOURCE:
        if not (ROOT / relative).is_file():
            fail(f"missing {relative}")

    archives = sorted((ROOT / "resources/ui").rglob("*.zip"))
    if archives:
        fail(f"downloaded archives must not be committed ({archives[0]})")

    print("curated UI asset checks passed")


if __name__ == "__main__":
    main()
