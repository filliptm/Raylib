#!/usr/bin/env python3
"""Build the small, tintable UI motifs used by the Helios interface.

The OpenGameArt source sheets contain complete dashboard compositions. Brawl
Arena intentionally derives only two geometric motifs from the no-effects
variants. Dark sheet backgrounds are converted to transparency so the runtime
can tint the motifs with the active Helios color language.
"""

from __future__ import annotations

import argparse
from pathlib import Path

try:
    from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps
except ImportError as exc:  # pragma: no cover - developer dependency guard
    raise SystemExit(
        "Pillow is required to rebuild UI derivatives: python3 -m pip install Pillow"
    ) from exc


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "resources/ui/scifi_interface/source"
RUNTIME_ROOT = PROJECT_ROOT / "resources/ui/scifi_interface/runtime"

# (source filename, crop box, output filename, square output size)
DERIVATIVES = (
    (
        "interface_2_no_effects.png",
        (276, 80, 800, 604),
        "radar_disc.png",
        512,
    ),
    (
        "interface_3_no_effects.png",
        (96, 780, 1160, 1844),
        "orbital_ring.png",
        768,
    ),
)

def tintable_linework(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    grayscale = ImageOps.grayscale(rgba)
    edges = grayscale.filter(ImageFilter.FIND_EDGES)
    edges = ImageOps.autocontrast(edges, cutoff=1)
    edges = edges.point(lambda value: 0 if value < 12 else min(255, int(value * 2.4)))
    edges = edges.filter(ImageFilter.GaussianBlur(0.65))
    edges = ImageChops.multiply(edges, rgba.getchannel("A"))

    # Both selected motifs are circular. This rejects neighboring dashboard
    # widgets that overlap the square crop without altering the authored ring.
    circle = Image.new("L", rgba.size, 0)
    inset = max(4, int(min(rgba.size) * 0.008))
    ImageDraw.Draw(circle).ellipse(
        (inset, inset, rgba.width - inset, rgba.height - inset), fill=255
    )
    edges = ImageChops.multiply(edges, circle)

    output = Image.new("RGBA", rgba.size, (255, 255, 255, 0))
    output.putalpha(edges)
    return output


def build(output_root: Path) -> None:
    output_root.mkdir(parents=True, exist_ok=True)
    for source_name, crop, output_name, size in DERIVATIVES:
        source_path = SOURCE_ROOT / source_name
        if not source_path.is_file():
            raise SystemExit(f"missing UI source sheet: {source_path}")
        source = Image.open(source_path)
        motif = tintable_linework(source.crop(crop))
        motif = motif.resize((size, size), Image.Resampling.LANCZOS)
        destination = output_root / output_name
        motif.save(destination, optimize=True)
        print(f"built {destination.relative_to(PROJECT_ROOT)}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=RUNTIME_ROOT,
        help="output directory (defaults to tracked runtime UI resources)",
    )
    return parser.parse_args()


if __name__ == "__main__":
    build(parse_args().output.resolve())
