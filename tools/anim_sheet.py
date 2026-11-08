#!/usr/bin/env python3
"""Builds contact sheets and animated GIFs from the animation filmstrips (PPM frames).

    SR_ANIM_SHOTS=out xvfb-run -a build/linux-gcc-debug/sr_platform_tests test_anim_shots
    python3 tools/anim_sheet.py out docs/screenshots/anim [--scale 2]

Frames are named <strip>_<k>.ppm; each strip becomes <strip>.gif and one row of the sheet.
"""
import argparse
import glob
import os
import re
from collections import OrderedDict

from PIL import Image, ImageDraw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--fps", type=int, default=8)
    ap.add_argument("--sheet", default="clips_sheet.png")
    ap.add_argument("--prefix", default="clip_", help="strips to put on the sheet")
    ap.add_argument("--crop", type=float, default=1.0, help="keep the middle share of each frame's width")
    args = ap.parse_args()
    os.makedirs(args.dst, exist_ok=True)
    strips = OrderedDict()
    for path in sorted(glob.glob(os.path.join(args.src, "*.ppm"))):
        name = os.path.splitext(os.path.basename(path))[0]
        m = re.match(r"(.*)_(\d+)$", name)
        if not m:
            continue
        strips.setdefault(m.group(1), []).append((int(m.group(2)), path))
    rows = []
    for strip, frames in strips.items():
        frames.sort()
        imgs = [Image.open(p).convert("RGB") for _, p in frames]
        if args.crop < 1.0:
            w, h = imgs[0].size
            cw = int(w * args.crop)
            imgs = [i.crop(((w - cw) // 2, 0, (w - cw) // 2 + cw, h)) for i in imgs]
        big = [i.resize((i.width * args.scale, i.height * args.scale), Image.NEAREST) for i in imgs]
        big[0].save(os.path.join(args.dst, strip + ".gif"), save_all=True, append_images=big[1:],
                    duration=int(1000 / args.fps), loop=0)
        if strip.startswith(args.prefix):
            rows.append((strip, imgs))
    if not rows:
        return
    w, h = rows[0][1][0].size
    n = max(len(r[1]) for r in rows)
    sheet = Image.new("RGB", (120 + n * w, len(rows) * h), (0, 0, 0))
    draw = ImageDraw.Draw(sheet)
    for y, (strip, imgs) in enumerate(rows):
        draw.text((4, y * h + h // 2 - 5), strip.replace(args.prefix, "", 1), fill=(220, 220, 220))
        for x, img in enumerate(imgs):
            sheet.paste(img, (120 + x * w, y * h))
    sheet.save(os.path.join(args.dst, args.sheet))


if __name__ == "__main__":
    main()
