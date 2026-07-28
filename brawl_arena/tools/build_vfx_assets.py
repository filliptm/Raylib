#!/usr/bin/env python3
"""Validate and build the curated, CC0 ability-VFX runtime atlases."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "data/vfx/asset_manifest.json"


class VfxAssetError(RuntimeError):
    pass


def load_manifest(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise VfxAssetError(f"cannot read VFX manifest {path}: {exc}") from exc

    if data.get("version") != 1:
        raise VfxAssetError("VFX manifest version must be 1")
    atlases = data.get("atlases")
    if not isinstance(atlases, list) or not atlases:
        raise VfxAssetError("VFX manifest must contain at least one atlas")
    if not isinstance(data.get("licenses"), dict):
        raise VfxAssetError("VFX manifest is missing its license table")
    return data


def project_path(value: str) -> Path:
    path = (ROOT / value).resolve()
    if ROOT not in path.parents:
        raise VfxAssetError(f"path escapes project root: {value}")
    return path


def manifest_inputs(data: dict, manifest_path: Path) -> list[Path]:
    inputs = [manifest_path.resolve()]
    licenses = data["licenses"]
    for atlas in data["atlases"]:
        pack = atlas.get("pack")
        if pack not in licenses:
            raise VfxAssetError(f"atlas {atlas.get('id')} references unknown pack {pack}")
        license_data = licenses[pack]
        if license_data.get("license") != "CC0-1.0":
            raise VfxAssetError(
                f"pack {pack} is not approved for tracked redistribution (CC0-1.0)"
            )
        source_url = license_data.get("source")
        if not isinstance(source_url, str) or not source_url.startswith("https://"):
            raise VfxAssetError(f"pack {pack} has no HTTPS source URL")
        notice = license_data.get("notice")
        if not notice:
            raise VfxAssetError(f"pack {pack} has no notice")
        inputs.append(project_path(notice))
        if "source" in atlas:
            inputs.append(project_path(atlas["source"]))
        else:
            sources = atlas.get("sources")
            if not isinstance(sources, list) or not sources:
                raise VfxAssetError(f"atlas {atlas.get('id')} has no sources")
            inputs.extend(project_path(source) for source in sources)
    return list(dict.fromkeys(inputs))


def atlas_geometry(atlas: dict) -> tuple[int, int, int, int, int]:
    try:
        columns = int(atlas["columns"])
        rows = int(atlas["rows"])
        frames = int(atlas["frames"])
        frame_width = int(atlas["frame_width"])
        frame_height = int(atlas["frame_height"])
    except (KeyError, TypeError, ValueError) as exc:
        raise VfxAssetError(f"invalid geometry for atlas {atlas.get('id')}") from exc
    if min(columns, rows, frames, frame_width, frame_height) <= 0:
        raise VfxAssetError(f"atlas {atlas.get('id')} geometry must be positive")
    if frames > columns * rows:
        raise VfxAssetError(f"atlas {atlas.get('id')} has more frames than cells")
    if columns * frame_width > 2048 or rows * frame_height > 2048:
        raise VfxAssetError(f"atlas {atlas.get('id')} exceeds the 2048px runtime limit")
    return columns, rows, frames, frame_width, frame_height


def atlas_padding(atlas: dict) -> int:
    try:
        padding = int(atlas.get("padding", 0))
    except (TypeError, ValueError) as exc:
        raise VfxAssetError(f"invalid padding for atlas {atlas.get('id')}") from exc
    _, _, _, frame_width, frame_height = atlas_geometry(atlas)
    if padding < 0 or padding * 2 >= min(frame_width, frame_height):
        raise VfxAssetError(
            f"atlas {atlas.get('id')} padding must leave a positive frame interior"
        )
    return padding


def open_rgba(path: Path) -> Image.Image:
    if not path.is_file():
        raise VfxAssetError(f"missing VFX source {path.relative_to(ROOT)}")
    with Image.open(path) as source:
        # Some Kenney PNGs are palette encoded even though they carry transparency.
        # Runtime output is always normalized RGBA; source encoding is not gameplay
        # metadata and should not make otherwise valid art unusable.
        return source.convert("RGBA")


def bleed_transparent_rgb(image: Image.Image) -> Image.Image:
    """Extrude nearby RGB into zero-alpha texels without changing transparency.

    Bilinear filtering samples RGB and alpha independently. Leaving black RGB just
    outside a soft sprite edge creates a dark matte fringe even though those texels
    are transparent. A small color dilation gives the sampler a compatible color.
    """

    alpha = image.getchannel("A")
    transparent = alpha.point(lambda value: 255 if value == 0 else 0)
    rgb = image.convert("RGB")
    dilated = rgb.filter(ImageFilter.MaxFilter(5))
    rgb.paste(dilated, mask=transparent)
    red, green, blue = rgb.split()
    return Image.merge("RGBA", (red, green, blue, alpha))


def prepare_frame(
    image: Image.Image, frame_width: int, frame_height: int, padding: int
) -> Image.Image:
    if image.size != (frame_width, frame_height):
        raise VfxAssetError(
            f"frame is {image.size}, expected {(frame_width, frame_height)}"
        )
    if padding > 0:
        interior = image.resize(
            (frame_width - padding * 2, frame_height - padding * 2),
            Image.Resampling.LANCZOS,
        )
        guarded = Image.new("RGBA", image.size, (0, 0, 0, 0))
        guarded.alpha_composite(interior, (padding, padding))
        image = guarded
    return bleed_transparent_rgb(image)


def build_one(atlas: dict, output_directory: Path) -> Path:
    atlas_id = atlas.get("id")
    output_name = atlas.get("output")
    if not isinstance(atlas_id, str) or not atlas_id:
        raise VfxAssetError("every VFX atlas needs an id")
    if not isinstance(output_name, str) or Path(output_name).name != output_name:
        raise VfxAssetError(f"atlas {atlas_id} has an invalid output filename")

    columns, rows, frames, frame_width, frame_height = atlas_geometry(atlas)
    padding = atlas_padding(atlas)
    output = output_directory / output_name
    output.parent.mkdir(parents=True, exist_ok=True)
    expected_size = (columns * frame_width, rows * frame_height)

    if "source" in atlas:
        source_path = project_path(atlas["source"])
        image = open_rgba(source_path)
        if image.size != expected_size:
            raise VfxAssetError(
                f"{source_path.relative_to(ROOT)} is {image.size}, expected {expected_size}"
            )
        result = Image.new("RGBA", expected_size, (0, 0, 0, 0))
        for index in range(frames):
            x = (index % columns) * frame_width
            y = (index // columns) * frame_height
            frame = image.crop((x, y, x + frame_width, y + frame_height))
            frame = prepare_frame(frame, frame_width, frame_height, padding)
            result.alpha_composite(frame, (x, y))
        result.save(output, format="PNG", optimize=False)
        return output

    sources = atlas.get("sources", [])
    if len(sources) != frames:
        raise VfxAssetError(
            f"atlas {atlas_id} declares {frames} frames but has {len(sources)} sources"
        )
    result = Image.new("RGBA", expected_size, (0, 0, 0, 0))
    for index, source_value in enumerate(sources):
        source_path = project_path(source_value)
        image = open_rgba(source_path)
        if image.size != (frame_width, frame_height):
            raise VfxAssetError(
                f"{source_path.relative_to(ROOT)} is {image.size}, "
                f"expected {(frame_width, frame_height)}"
            )
        image = prepare_frame(image, frame_width, frame_height, padding)
        x = (index % columns) * frame_width
        y = (index // columns) * frame_height
        result.alpha_composite(image, (x, y))
    result.save(output, format="PNG", optimize=False)
    return output


def build(manifest_path: Path, output_override: Path | None = None) -> list[Path]:
    data = load_manifest(manifest_path)
    inputs = manifest_inputs(data, manifest_path)
    for path in inputs:
        if not path.is_file():
            raise VfxAssetError(f"missing VFX input {path}")

    output_directory = (
        output_override.resolve()
        if output_override
        else project_path(data["output_directory"])
    )
    output_directory.mkdir(parents=True, exist_ok=True)

    expected_names = {atlas["output"] for atlas in data["atlases"]}
    for old in output_directory.glob("*.png"):
        if old.name not in expected_names:
            old.unlink()

    outputs = [build_one(atlas, output_directory) for atlas in data["atlases"]]
    shutil.copyfile(manifest_path, output_directory / "asset_manifest.json")
    return outputs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--print-inputs", action="store_true")
    args = parser.parse_args()

    manifest = args.manifest.resolve()
    try:
        data = load_manifest(manifest)
        if args.print_inputs:
            print(" ".join(str(path.relative_to(ROOT)) for path in manifest_inputs(data, manifest)))
            return 0
        outputs = build(manifest, args.output)
    except VfxAssetError as exc:
        print(f"VFX asset error: {exc}", file=sys.stderr)
        return 1

    total_bytes = sum(path.stat().st_size for path in outputs)
    print(f"Built {len(outputs)} VFX atlases ({total_bytes / (1024 * 1024):.2f} MiB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
