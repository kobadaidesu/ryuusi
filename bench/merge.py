#!/usr/bin/env python3
"""別々に走らせた結果 (JSON) を 1 つにまとめる。

    python3 bench/merge.py out.json label run1.json run2.json ...

ほかの版と交互に 1 回ずつ走らせた結果をまとめるのに使う。まとめ方は bench_core と同じで、
p50 / p95 は回ごとの値の中央値、fps も中央値。回ごとの p50 は runs_p50 に並べる
(runs_p50 の無い古い版の結果は、1 回分の結果としてその p50 を使う)。
"""

import json
import statistics
import sys


def run_p50s(st, runs):
    if st.get("runs_p50") is not None:
        return list(st["runs_p50"])
    if runs == 1:
        return [st["p50"]]
    return [None] * runs  # 回ごとの値が分からない


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    out, label, files = sys.argv[1], sys.argv[2], sys.argv[3:]
    ds = []
    for f in files:
        with open(f) as fp:
            ds.append(json.load(fp))

    builds = sorted({(d.get("build"), d.get("dirty"), d.get("platform")) for d in ds})
    if len(builds) > 1:
        print(f"> 注意: build / platform が混ざっている: {builds}", file=sys.stderr)

    m = dict(ds[0])
    m["label"] = label
    m["runs"] = sum(d.get("runs", 1) for d in ds)
    m["note"] = ((m.get("note") or "") + f" / {len(ds)} 個の結果 ({m['runs']} 回) をまとめた").lstrip(" /")
    scen = []
    for i, s0 in enumerate(ds[0]["scenarios"]):
        ss = [d["scenarios"][i] for d in ds]
        s = dict(s0)
        fps = [x["fps"] for x in ss if x.get("fps") is not None]
        s["fps"] = round(statistics.median(fps), 2) if fps else None
        ms = {}
        for st in s0["ms"]:
            vals = [(x["ms"][st], d.get("runs", 1)) for x, d in zip(ss, ds) if x["ms"].get(st)]
            if not vals:
                ms[st] = None
                continue
            r50 = [v for st_, n in vals for v in run_p50s(st_, n)]
            ms[st] = {
                "p50": round(statistics.median(v["p50"] for v, _ in vals), 4),
                "p95": round(statistics.median(v["p95"] for v, _ in vals), 4),
                "runs_p50": r50,
            }
        s["ms"] = ms
        s["checksum_stable"] = all(x.get("checksum_stable", True)
                                   and x["checksum"] == s0["checksum"]
                                   and x.get("checksum_early") == s0.get("checksum_early") for x in ss)
        scen.append(s)
    m["scenarios"] = scen
    with open(out, "w") as fp:
        json.dump(m, fp, indent=2, ensure_ascii=False)
        fp.write("\n")
    print(f"wrote {out} ({m['runs']} runs, checksum_stable: {all(s['checksum_stable'] for s in scen)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
