# ベンチマーク

Particle Flow の重さを「段階ごとの ms」で測るための道具。計画と経緯は [PLAN.md](PLAN.md)。

## 何を測るか

決まった入力 (シナリオ) を固定の条件で流し、フレームごとに段階別の所要時間を記録する。

- 乱数の種は固定 (`BENCH_SEED`)、粒子 300,000 個、ウィンドウ / キャンバス 1280×720
- dt は常に 1/60 秒 (実時間を使わない)。VSync は切る (ブラウザは rAF 縛りなので切れない)
- 入力は SDL のイベントではなく、シナリオがフレームごとに直接与える
- 各回の最初の 60 フレームはウォームアップとして捨てる

| # | シナリオ | 入力 (カーソルは常に中央 640,360) | フレーム |
|---|---|---|---|
| 1 | idle | なし | 300 |
| 2 | attract | 左クリック引力 | 300 |
| 3 | gather_hold | G を押し続ける | 180 |
| 4 | gather_release | G を離した直後 | 60 |
| 5 | gather_release_spread | その続き | 120 |
| 6 | vortex | V (渦) | 300 |
| 7 | explode | E を最初の 1 フレームだけ押す | 60 |
| 8 | ripple | B を 30 フレームごと | 300 |

状態は前のシナリオから引き継ぐ (gather_release は gather_hold の直後の状態から始まる)。

| 段階 | 中身 |
|---|---|
| update | `ps_update` |
| pack | `ps_pack_gpu` |
| upload | `glBufferSubData` (CPU 側) |
| draw | `glDrawArrays` の呼び出し (CPU 側) |
| gui | `gui_render` |
| present | `SDL_GL_SwapWindow` (ブラウザでは実質 0) |
| frame | フレーム内の CPU 側の仕事の合計 |
| gpu | `GL_TIME_ELAPSED` で測った粒子の描画 (glClear〜glDrawArrays)。ブラウザは `EXT_disjoint_timer_query_webgl2` が使えるときだけ。使えなければ `null` |

集計はシナリオ × 段階ごとの p50 / p95。複数回走らせたときは「回ごとの p50 / p95 の中央値」。
fps はフレーム数 ÷ 実時間 (ブラウザでは rAF に縛られるので参考値)。

各シナリオの終わりに全粒子の x, y, vx, vy の合計 (double) と粒子数をチェックサムとして記録する。
同じビルド・同じ環境なら毎回完全に一致する (`checksum_stable`)。

## ネイティブ

```sh
make bench LABEL=baseline          # 5 回走らせて bench/results/baseline-native.json に保存
make bench LABEL=try BENCH_RUNS=1  # 1 回だけ (約 20 秒)
```

直接動かすなら:

```sh
./bin/particle_flow --bench [--bench-out=path.json] [--bench-runs=N] [--bench-label=name]
```

ウィンドウが開いて計測が進み、終わると標準出力に Markdown の表を出して終了する。
**ウィンドウを隠さないこと** (macOS は隠れたウィンドウの描画を間引くので数字が変わる)。

## ブラウザ (wasm)

```sh
make wasm
python3 bench/serve.py --port 8126   # bin/ を配信し、POST された結果を bench/results/ に保存
```

ブラウザで開く (**タブを前面に出したままにする**。非表示タブでは rAF が止まる):

```
http://127.0.0.1:8126/?bench=1&label=baseline&report=/report
```

| パラメーター | 意味 |
|---|---|
| `bench=1` | 計測モードで起動 (キャンバスを 1280×720 に固定) |
| `label=name` | 結果のラベル (既定 `bench`) |
| `runs=N` | 繰り返す回数 (既定 1) |
| `report=URL` | 結果の JSON を POST する先。`serve.py` なら `/report` で `bench/results/<label>-wasm.json` に保存される |

終わるとページのキャンバスの下に表と JSON が出る (`window.benchResult` にも入る)。タブのタイトルが `BENCH DONE` になる。
`serve.py` は COOP / COEP ヘッダーを付けて cross-origin isolated にしている (`performance.now()` の分解能が 5 µs になる)。

## 比較

```sh
python3 bench/compare.py bench/results/baseline-native.json bench/results/after-native.json
```

シナリオ × 段階の「before → after (×倍率)」の表 (p50 と p95) と、チェックサムの一致 / 不一致を Markdown で出す。
倍率は before / after (×2.00 なら 2 倍速い)。

## sim_bench (物理だけのマイクロベンチ)

`ps_update` + `ps_pack_gpu` だけを同じシナリオで回す。ウィンドウを開かず数秒で終わるので、ループの書き換えを試すのに使う。

```sh
make sim-bench LABEL=try BENCH_RUNS=3        # ネイティブ → bench/results/try-native-sim.json
make sim-bench-wasm LABEL=try BENCH_RUNS=3   # wasm を Node で → bench/results/try-wasm-sim.json
```

フラグはアプリと同じ (`COMMON_FLAGS` / `WASM_COMMON`)。チェックサムは同じプラットフォームのアプリの計測モードと一致する。

## 注意

- チェックサムはプラットフォームやコンパイルフラグが変わると一致しない。引力や渦は小さな丸め誤差が数百フレームで大きく育つため
  (`-ffast-math` の有無だけで attract の位置の合計が相対 4e-4、速度の合計は符号が変わるほどずれる)。比較は同じプラットフォーム同士で行う
- 結果の `build` は git の short sha、`dirty` は `src` / `web` / `Makefile` に未コミットの変更があったか
