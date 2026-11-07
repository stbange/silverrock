#!/usr/bin/env python3
"""Converts the reference views written by SR_ATMOS_SHOTS (PPM) to PNG and builds a contact sheet.

    SR_ATMOS_SHOTS=out xvfb-run -a build/linux-gcc-debug/sr_platform_tests test_atmosphere_shots
    python3 tools/atmos_sheet.py out docs/screenshots/atmosphere [--seed 1] [--scale 2]
"""
import argparse
import glob
import os

from PIL import Image, ImageDraw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--seed", type=int, default=1, help="seed whose views go on the sheet")
    ap.add_argument("--scale", type=int, default=2, help="nearest-neighbour upscale of the PNGs")
    ap.add_argument("--sheet", default="sheet.png")
    args = ap.parse_args()
    os.makedirs(args.dst, exist_ok=True)
    tiles = []
    for path in sorted(glob.glob(os.path.join(args.src, "*.ppm"))):
        name = os.path.splitext(os.path.basename(path))[0]
        img = Image.open(path).convert("RGB")
        big = img.resize((img.width * args.scale, img.height * args.scale), Image.NEAREST)
        big.save(os.path.join(args.dst, name + ".png"))
        if name.startswith("s%d_" % args.seed):
            tiles.append((name.split("_", 2)[2], img))
    if not tiles:
        return
    cols = 4
    w, h = tiles[0][1].size
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * (h + 14)), (0, 0, 0))
    draw = ImageDraw.Draw(sheet)
    for i, (label, img) in enumerate(tiles):
        x, y = (i % cols) * w, (i // cols) * (h + 14)
        sheet.paste(img, (x, y + 14))
        draw.text((x + 4, y + 1), label, fill=(230, 230, 230))
    sheet.save(os.path.join(args.dst, args.sheet))


if __name__ == "__main__":
    main()
