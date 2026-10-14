#!/usr/bin/env python3
"""2 つの計測結果 (JSON) を比べて Markdown の表を出す。

    python3 bench/compare.py before.json after.json [--noise noise.json]

- 段階ごとの p50 / p95 を「before → after (×倍率)」で並べる。倍率は before / after
  (×2.00 なら 2 倍速くなった、×0.50 なら 2 倍遅くなった)
- fps も同じ形で並べる (fps は after / before)
- 回ごとの p50 (runs_p50) があれば、同じビルドの回ごとの p50 のばらつき (最小〜最大の幅) を出す。
  ばらつき = before と after (と --noise の結果) の幅の大きいほう。p50 の差がばらつきの 2 倍未満なら
  「ぶれの範囲」として印を付ける (p50 の表では ≈)
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


def run_range(scenario, stage):
    """回ごとの p50 の (最小, 最大)。2 回分以上無ければ None"""
    st = (scenario or {}).get("ms", {}).get(stage) or {}
    vals = [v for v in st.get("runs_p50") or [] if v is not None]
    return (min(vals), max(vals)) if len(vals) >= 2 else None


def noise_check(scenarios, stage):
    """(差 after - before, ばらつき, ぶれの範囲か)。ばらつきが出せなければ None

    scenarios は [before, after, (noise)] のシナリオ。ばらつきは各々の最大 - 最小の大きいほう
    """
    before, after = scenarios[0], scenarios[1]
    a, b = stage_value(before, stage, "p50"), stage_value(after, stage, "p50")
    ranges = [r for r in (run_range(s, stage) for s in scenarios) if r is not None]
    if a is None or b is None or not ranges:
        return None
    spread = max(hi - lo for lo, hi in ranges)
    diff = b - a
    return diff, spread, abs(diff) < 2 * spread


def describe(d):
    dirty = "-dirty" if d.get("dirty") else ""
    gpu = "" if d.get("gpu_timer") else ", gpu 推定不可"
    if d.get("gpu_renderer"):
        gpu = f", GPU {d['gpu_renderer']}" + gpu
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
    ap.add_argument("--noise", help="ばらつきの見積もりに足す結果 (同じビルドを続けて走らせたもの)")
    ap.add_argument("--tol-early", type=float, default=1e-4, help="早期チェックサムの位置の許容相対誤差")
    ap.add_argument("--tol-end", type=float, default=1e-3, help="終了時チェックサムの位置の許容相対誤差")
    args = ap.parse_args()

    a, b = load(args.before), load(args.after)
    c = load(args.noise) if args.noise else None
    if a.get("platform") != b.get("platform"):
        print(f"> 注意: platform が違う ({a.get('platform')} と {b.get('platform')})\n")

    sa = {s["name"]: s for s in a.get("scenarios", [])}
    sb = {s["name"]: s for s in b.get("scenarios", [])}
    sc = {s["name"]: s for s in (c or {}).get("scenarios", [])}
    names = [s["name"] for s in a.get("scenarios", [])]
    names += [n for n in sb if n not in sa]

    present = set()
    for s in list(sa.values()) + list(sb.values()):
        present.update(k for k, v in s.get("ms", {}).items())
    stages = [st for st in STAGES if st in present]

    print(f"before: {describe(a)}  \nafter: {describe(b)}" + (f"  \nnoise: {describe(c)}" if c else "") + "\n")
    print("倍率は before / after (×2.00 = 2 倍速い)。fps は after / before。"
          "p50 の ≈ は差がばらつきの 2 倍未満 (ぶれの範囲)\n")

    def triple(n):
        return [sa.get(n), sb.get(n)] + ([sc.get(n)] if c else [])

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
            cells = []
            for st in stages:
                t = cell(stage_value(x, st, key), stage_value(y, st, key))
                nc = noise_check(triple(n), st) if key == "p50" else None
                cells.append(t + " ≈" if nc and nc[2] else t)
            print(f"| {n} | {fps} | " + " | ".join(cells) + " |")
        print()

    # 回ごとの p50 のばらつき
    if any(noise_check(triple(n), st) for n in names for st in stages):
        src = "before / after / noise" if c else "before / after"
        print("#### p50 のばらつき (同じビルドの回ごとの p50 の最小〜最大)\n")
        print(f"差は after - before の p50 (ms)。ばらつきは {src} の (最大 - 最小) の大きいほう。"
              "差がばらつきの 2 倍未満なら「ぶれの範囲」\n")
        print("| scenario | " + " | ".join(stages) + " |")
        print("|---|" + "---|" * len(stages))
        for n in names:
            cells = []
            for st in stages:
                nc = noise_check(triple(n), st)
                if nc is None:
                    cells.append("-")
                    continue
                diff, spread, within = nc
                ranges = " / ".join("-" if r is None else f"{r[0]:.2f}〜{r[1]:.2f}"
                                    for r in (run_range(s, st) for s in triple(n)))
                mark = "ぶれの範囲" if within else "**差あり**"
                cells.append(f"{diff:+.2f} (ばらつき {spread:.2f}; {ranges}) {mark}")
            print(f"| {n} | " + " | ".join(cells) + " |")
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
