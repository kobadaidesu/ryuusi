#!/usr/bin/env python3
"""2 つの計測結果 (JSON) を比べて Markdown の表を出す。

    python3 bench/compare.py before.json after.json [--tol 1e-4]

- 段階ごとの p50 / p95 を「before → after (×倍率)」で並べる。倍率は before / after
  (×2.00 なら 2 倍速くなった、×0.50 なら 2 倍遅くなった)
- fps も同じ形で並べる (fps は after / before)
- チェックサムの一致 / 不一致 (判定は PLAN.md のとおり)
  - 早期 (各シナリオ開始から 10 フレーム後): 粒子数が完全一致、位置の合計 (sum_x, sum_y) が相対 --tol-early 以内
  - 終了時: 粒子数が完全一致、位置の合計が相対 --tol-end 以内
  - 速度の合計 (sum_vx, sum_vy) はカオス的に育つので判定に使わず参考値として出す
"""

import argparse
import json
import sys

STAGES = ["update", "pack", "upload", "draw", "gui", "present", "frame", "gpu"]
POS = ["sum_x", "sum_y"]
VEL = ["sum_vx", "sum_vy"]


def load(path):
    with open(path) as f:
        return json.load(f)


def fmt_ms(v):
    return "-" if v is None else f"{v:.2f}"


def ratio(before, after):
    if before is None or after is None or after <= 0:
        return ""
    return f" (×{before / after:.2f})"


def cell(before, after):
    if before is None and after is None:
        return "-"
    return f"{fmt_ms(before)} → {fmt_ms(after)}{ratio(before, after)}"


def stage_value(scenario, stage, key):
    st = (scenario or {}).get("ms", {}).get(stage)
    return None if st is None else st.get(key)


def describe(d):
    dirty = "-dirty" if d.get("dirty") else ""
    gpu = "" if d.get("gpu_timer") else ", gpu 推定不可"
    if "cull_limit" in d:
        gpu = f", cull {d['cull_limit']}" + gpu
    return f"{d.get('label')} ({d.get('platform')}, {d.get('build')}{dirty}, {d.get('runs', 1)} runs{gpu})"


def rel_err(a, b):
    if a == b:
        return 0.0
    denom = max(abs(a), abs(b))
    return abs(a - b) / denom if denom > 0 else 0.0


def main():
    ap = argparse.ArgumentParser(description="ベンチ結果の比較")
    ap.add_argument("before")
    ap.add_argument("after")
    ap.add_argument("--tol-early", type=float, default=1e-4, help="早期チェックサムの位置の許容相対誤差")
    ap.add_argument("--tol-end", type=float, default=1e-3, help="終了時チェックサムの位置の許容相対誤差")
    args = ap.parse_args()

    a, b = load(args.before), load(args.after)
    if a.get("platform") != b.get("platform"):
        print(f"> 注意: platform が違う ({a.get('platform')} と {b.get('platform')})\n")

    sa = {s["name"]: s for s in a.get("scenarios", [])}
    sb = {s["name"]: s for s in b.get("scenarios", [])}
    names = [s["name"] for s in a.get("scenarios", [])]
    names += [n for n in sb if n not in sa]

    present = set()
    for s in list(sa.values()) + list(sb.values()):
        present.update(k for k, v in s.get("ms", {}).items())
    stages = [st for st in STAGES if st in present]

    print(f"before: {describe(a)}  \nafter: {describe(b)}\n")
    print("倍率は before / after (×2.00 = 2 倍速い)。fps は after / before\n")

    for key in ("p50", "p95"):
        print(f"#### {key} (ms)\n")
        print("| scenario | fps | " + " | ".join(stages) + " |")
        print("|---|" + "---:|" * (len(stages) + 1))
        for n in names:
            x, y = sa.get(n), sb.get(n)
            fa = (x or {}).get("fps")
            fb = (y or {}).get("fps")
            if fa is None or fb is None:
                fps = "-"
            else:
                fps = f"{fa:.1f} → {fb:.1f}" + (f" (×{fb / fa:.2f})" if fa > 0 else "")
            cells = [cell(stage_value(x, st, key), stage_value(y, st, key)) for st in stages]
            print(f"| {n} | {fps} | " + " | ".join(cells) + " |")
        print()

    print(f"#### チェックサム\n")
    print(f"早期 (開始から 10 フレーム後) は位置の相対誤差 {args.tol_early:g} 以内、"
          f"終了時は {args.tol_end:g} 以内、粒子数は完全一致で「一致」。速度は参考値\n")
    print("| scenario | count | early sum_x | early sum_y | end sum_x | end sum_y | (参考) end sum_vx | (参考) end sum_vy | 判定 |")
    print("|---|---|---:|---:|---:|---:|---:|---:|---|")
    all_ok = True
    for n in names:
        x, y = sa.get(n) or {}, sb.get(n) or {}
        ea, eb = x.get("checksum_early"), y.get("checksum_early")
        ca, cb = x.get("checksum"), y.get("checksum")
        if None in (ea, eb, ca, cb):
            print(f"| {n} | - |" + " - |" * 6 + " 比較不可 |")
            all_ok = False
            continue
        count_ok = ea["count"] == eb["count"] and ca["count"] == cb["count"]
        early = [rel_err(ea[k], eb[k]) for k in POS]
        endp = [rel_err(ca[k], cb[k]) for k in POS]
        vel = [rel_err(ca[k], cb[k]) for k in VEL]
        ok = (count_ok and all(e <= args.tol_early for e in early)
              and all(e <= args.tol_end for e in endp))
        all_ok &= ok
        count = f"{ca['count']}" if count_ok else f"{ea['count']}/{ca['count']} → {eb['count']}/{cb['count']}"
        print(f"| {n} | {count} | " + " | ".join(f"{e:.1e}" for e in early + endp + vel)
              + f" | {'一致' if ok else '**不一致**'} |")
    print()
    print("チェックサム: " + ("すべて一致" if all_ok else "**不一致あり**"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
