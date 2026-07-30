#!/usr/bin/env python3
"""Validate the Arena Ink UI resource policy without a render context."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

REQUIRED = (
    "resources/ui/README.md",
    "resources/ui/kenney_scifi/LICENSE.txt",
    "resources/ui/kenney_scifi/SOURCE.md",
    "resources/ui/scifi_interface/LICENSE.txt",
    "resources/ui/scifi_interface/SOURCE.md",
)


def fail(message: str) -> None:
    raise SystemExit(f"UI asset policy failed: {message}")


def main() -> None:
    for relative in REQUIRED:
        if not (ROOT / relative).is_file():
            fail(f"missing {relative}")

    archives = sorted((ROOT / "resources/ui").rglob("*.zip"))
    if archives:
        fail(f"downloaded archives must not be committed ({archives[0]})")

    skin_source = (ROOT / "src/ui/ui_skin.c").read_text(encoding="utf-8")
    if "LoadTexture" in skin_source:
        fail("Arena Ink skin must remain procedural and texture-free")
    if "DrawComicShape" not in skin_source or "DrawHalftone" not in skin_source:
        fail("Arena Ink procedural primitives are missing")

    print("Arena Ink procedural UI asset checks passed")


if __name__ == "__main__":
    main()
