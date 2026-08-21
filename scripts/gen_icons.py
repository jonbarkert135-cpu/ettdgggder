#!/usr/bin/env python3
"""Generate application icons from the logo (roadmap item 65).

    python3 scripts/gen_icons.py --out out/branding

Produces, from `branding/bedrock-logo-transparent.png`:

    bedrock-16.png … bedrock-512.png   generic PNG sizes
    bedrock.ico                        Windows (all sizes in one file)
    hicolor/<size>x<size>/apps/bedrock-browser.png   Linux desktop icon theme

Derived files are not committed: they are one command away, and a committed icon
is a file that silently stops matching the logo above it.

Two things this does that a plain resize does not:

* **Trims the transparent margin first.** The source art has roughly 15 % empty
  border; keeping it wastes five pixels of a 32 px icon on nothing.
* **Swaps in the small mark below 32 px** when `branding/bedrock-mark-small.svg`
  can be rasterised, because the full mark's strata average into a grey circle
  at that size (docs/BRAND.md, "Mark").
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
SOURCE = REPO / "branding" / "bedrock-logo-transparent.png"
SMALL = REPO / "branding" / "bedrock-mark-small.svg"
SIZES = [16, 24, 32, 48, 64, 128, 256, 512]
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
SMALL_MARK_MAX = 24  # at and below this, the three-band mark reads better


def load_image():
    try:
        from PIL import Image
    except ImportError:
        sys.exit("Pillow is required: pip install pillow")
    image = Image.open(SOURCE).convert("RGBA")
    box = image.getbbox()
    if box:
        image = image.crop(box)
    # Square it, so no size distorts the circle.
    side = max(image.size)
    from PIL import Image as PILImage

    canvas = PILImage.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(image, ((side - image.width) // 2, (side - image.height) // 2))
    return canvas


def rasterise_small(size: int):
    """Small mark via rsvg-convert or Inkscape if either is installed, else None."""
    from io import BytesIO

    from PIL import Image

    for command in (
        ["rsvg-convert", "-w", str(size), "-h", str(size), str(SMALL)],
        ["inkscape", str(SMALL), "-w", str(size), "-h", str(size), "--export-type=png",
         "--export-filename=-"],
    ):
        try:
            result = subprocess.run(command, capture_output=True, check=True)
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
        return Image.open(BytesIO(result.stdout)).convert("RGBA")
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=pathlib.Path, default=REPO / "out" / "branding")
    args = parser.parse_args()

    from PIL import Image

    source = load_image()
    args.out.mkdir(parents=True, exist_ok=True)
    rendered: dict[int, Image.Image] = {}
    for size in SIZES:
        icon = rasterise_small(size) if size <= SMALL_MARK_MAX else None
        if icon is None:
            if size <= SMALL_MARK_MAX:
                print(
                    f"warning: {size} px falls back to a downscaled logo — install rsvg-convert "
                    f"or inkscape so the small mark is used instead",
                    file=sys.stderr,
                )
            icon = source.resize((size, size), Image.LANCZOS)
        rendered[size] = icon
        icon.save(args.out / f"bedrock-{size}.png")

    rendered[256].save(
        args.out / "bedrock.ico", sizes=[(size, size) for size in ICO_SIZES]
    )
    for size in SIZES:
        directory = args.out / "hicolor" / f"{size}x{size}" / "apps"
        directory.mkdir(parents=True, exist_ok=True)
        rendered[size].save(directory / "bedrock-browser.png")

    print(f"wrote {len(SIZES)} PNG sizes, bedrock.ico and a hicolor tree to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
