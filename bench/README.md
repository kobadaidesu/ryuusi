# ベンチマーク

Particle Flow の重さを「段階ごとの ms」で測るための道具。計画と経緯は [PLAN.md](PLAN.md)。

## 何を測るか

決まった入力 (シナリオ) を固定の条件で流し、フレームごとに段階別の所要時間を記録する。

- 乱数の種は固定 (`BENCH_SEED`)、粒子 300,000 個、ウィンドウ / キャンバス 1280×720
- dt は常に 1/60 秒 (実時間を使わない)。VSync は切る (ブラウザは rAF 縛りなので切れない)
- 入力は SDL のイベントではなく、シナリオがフレームごとに直接与える
- 各回の最初の 60 フレームはウォームアップとして捨てる

| # | シナリオ | 入力 (カーソルは特記なければ中央 640,360) | フレーム |
|---|---|---|---|
| 1 | idle | なし | 300 |
| 2 | attract | 左クリック引力 | 300 |
| 3 | gather_hold | G を押し続ける | 180 |
| 4 | gather_release | G を離した直後 (カーソルは中央のまま。全粒子が 1 点に重なった最悪ケース) | 60 |
| 5 | gather_release_spread | その続き。開始時にカーソルを (900,360) へ移す | 120 |
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

各シナリオの開始から 10 フレーム後 (`checksum_early`) と終了時 (`checksum`) に、全粒子の x, y, vx, vy の合計 (double) と
粒子数をチェックサムとして記録する。同じビルド・同じ環境なら毎回完全に一致する (`checksum_stable`)。

変更の前後で「動きが変わっていない」の判定 (`compare.py`):

- 10 フレーム後: 粒子数が完全一致、位置の合計 (sum_x, sum_y) が相対誤差 1e-4 以内
- 終了時: 粒子数が完全一致、位置の合計が相対誤差 1e-3 以内
- 速度の合計は判定に使わない (参考値)

## ネイティブ

```sh
make bench LABEL=baseline          # 5 回走らせて bench/results/baseline-native.json に保存
make bench LABEL=try BENCH_RUNS=1  # 1 回だけ (約 20 秒)
```

直接動かすなら:

```sh
./bin/particle_flow --bench [--bench-out=path.json] [--bench-runs=N] [--bench-label=name] [--bench-cull=N]
```

`--bench-cull=N` は飽和カリング (同じ画素に中心がある粒子を N 個までしか描かない) の上限。0 で無効、省略時は既定値 (`PS_CULL_LIMIT_DEFAULT`)。
結果の JSON には `cull_limit` と、GPU の名前 `gpu_renderer` が入る。

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
| `cull=N` | 飽和カリングの上限 (`--bench-cull` と同じ。0 で無効) |
| `report=URL` | 結果の JSON を POST する先。`serve.py` なら `/report` で `bench/results/<label>-wasm.json` に保存される |

終わるとページのキャンバスの下に表と JSON が出る (`window.benchResult` にも入る)。タブのタイトルが `BENCH DONE` になる。

タブを前面に出しておけないときは headless Chrome で走らせられる (headless では rAF が止まらない):

```sh
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new \
  --user-data-dir=/tmp/pf-chrome --window-size=1400,900 \
  "http://127.0.0.1:8126/?bench=1&label=try&report=/report"
# bench/results/try-wasm.json ができたら Chrome を止める
```

`gpu_renderer` で実 GPU か確かめること。`ANGLE (Apple, ANGLE Metal Renderer: ...)` なら実 GPU、`SwiftShader` ならソフトウェア描画なので gpu の数字は参考にならない (CPU 側の update / pack / upload を比べる)。
`serve.py` は COOP / COEP ヘッダーを付けて cross-origin isolated にしている (`performance.now()` の分解能が 5 µs になる)。

## 比較

```sh
python3 bench/compare.py bench/results/baseline-native.json bench/results/after-native.json
```

シナリオ × 段階の「before → after (×倍率)」の表 (p50 と p95) と、上の判定によるチェックサムの一致 / 不一致を Markdown で出す。
倍率は before / after (×2.00 なら 2 倍速い)。

## スクリーンショットで見た目を比べる (ネイティブ)

```sh
mkdir -p /tmp/shots-before
./bin/particle_flow --bench --bench-label=before --bench-shot=/tmp/shots-before
# ... 変更してビルドし直して ...
./bin/particle_flow --bench --bench-label=after --bench-shot=/tmp/shots-after
python3 bench/imgdiff.py /tmp/shots-before /tmp/shots-after
```

`--bench-shot=dir` は各シナリオの最後のフレームで、粒子を描いた直後 (GUI を描く前) の画面を `dir/<label>-<scenario>.ppm` に保存する。
入力も乱数も固定なので、描画が同じなら画素も一致する。`imgdiff.py` はシナリオごとに最大差・差のある画素数・差の平均を出す。
読み戻しで GPU を待つので、`--bench-shot` を付けた回の時間の数字は使わない (別に走らせる)。PNG にするなら `sips -s format png x.ppm --out x.png`。

## sim_bench (物理だけのマイクロベンチ)

`ps_update` + `ps_pack_gpu` だけを同じシナリオで回す。ウィンドウを開かず数秒で終わるので、ループの書き換えを試すのに使う。

```sh
make sim-bench LABEL=try BENCH_RUNS=3        # ネイティブ → bench/results/try-native-sim.json
make sim-bench-wasm LABEL=try BENCH_RUNS=3   # wasm を Node で → bench/results/try-wasm-sim.json
```

フラグはアプリと同じ (`COMMON_FLAGS` / `WASM_COMMON`)。チェックサムは同じプラットフォームのアプリの計測モードと一致する。

## 注意

- チェックサムはプラットフォームやコンパイルフラグが変わると完全には一致しない。引力や渦は小さな丸め誤差が数百フレームで大きく育つため
  (`-ffast-math` の有無だけで attract の終了時の位置の合計が相対 4e-4、速度の合計は符号が変わるほどずれる)。判定の許容誤差はこれを見込んで決めてある
- 結果の `build` は git の short sha、`dirty` は `src` / `web` / `Makefile` に未コミットの変更があったか
