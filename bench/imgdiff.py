#!/usr/bin/env python3
"""2 つのスクリーンショット (--bench-shot が書く PPM) を画素で比べる。

    python3 bench/imgdiff.py before_dir after_dir [--before-label L] [--after-label L]

ディレクトリ内の <label>-<scenario>.ppm を組にして、シナリオごとに
「チャンネルの差の最大値」「差がある画素の数 (割合)」「差の平均」を Markdown の表で出す。
"""

import argparse
import glob
import os
import sys


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    # ヘッダー: P6 <w> <h> <max> と空白 1 つ (コメントは書いていない)
    parts = data.split(maxsplit=4)
    if parts[0] != b"P6":
        raise ValueError(f"{path}: not P6")
    w, h, mx = int(parts[1]), int(parts[2]), int(parts[3])
    if mx != 255:
        raise ValueError(f"{path}: maxval {mx}")
    pix = parts[4]
    return w, h, pix[: w * h * 3]


def scenario_map(d, label):
    out = {}
    for p in sorted(glob.glob(os.path.join(d, f"{label}-*.ppm"))):
        out[os.path.basename(p)[len(label) + 1 : -4]] = p
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("before_dir")
    ap.add_argument("after_dir")
    ap.add_argument("--before-label", default="before")
    ap.add_argument("--after-label", default="after")
    args = ap.parse_args()

    a = scenario_map(args.before_dir, args.before_label)
    b = scenario_map(args.after_dir, args.after_label)
    names = [n for n in a if n in b]
    if not names:
        print("比べるスクリーンショットが無い", file=sys.stderr)
        return 1

    print("| scenario | 最大差 (0-255) | 差のある画素 | 差の平均 |")
    print("|---|---:|---:|---:|")
    for n in names:
        wa, ha, pa = read_ppm(a[n])
        wb, hb, pb = read_ppm(b[n])
        if (wa, ha) != (wb, hb):
            print(f"| {n} | 大きさが違う ({wa}x{ha} / {wb}x{hb}) | - | - |")
            continue
        mx = 0
        total = 0
        npix = 0
        for i in range(0, len(pa), 3):
            d = max(abs(pa[i] - pb[i]), abs(pa[i + 1] - pb[i + 1]), abs(pa[i + 2] - pb[i + 2]))
            if d:
                npix += 1
                total += d
                if d > mx:
                    mx = d
        n_all = wa * ha
        print(f"| {n} | {mx} | {npix} ({100.0 * npix / n_all:.3f}%) | {total / n_all:.4f} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
