# ベンチマーク計測モードと高速化の実装計画

このファイルは実装担当(別ペインの Claude)向けの仕様書です。判断・指示はディレクター(別ペインの Claude)が行い、PR のマージは人間(リポジトリ所有者)が行います。

## 背景

Particle Flow は C99/C++17 + SDL2 + OpenGL 3.3 のデスクトップアプリで、同じコードを Emscripten で WebAssembly にしてブラウザ(WebGL2)でも動かしている(`make wasm`、GitHub Pages で公開)。粒子は最大 300,000 個。

所有者の体感: **G キーで粒子を一点に集めて離した瞬間にラグがある**。コードを読んだ仮説は次の 2 つ。

1. G 中は `ps_update` が粒子ごとに `expf()` を呼んでいる(`lerp` はフレーム内で一定なのにループの中にある)。30 万回の `expf` で wasm では 1 フレーム 10ms 前後かかるはず
2. 離した瞬間は全粒子が数ピクセルに重なっていて、3px の点 30 万個の重ね塗りになる。Apple の GPU はタイルベースなので、1 タイルに全フラグメントが集中すると並列性がなくなる

**まず測って仮説を確かめ、それから効く順に直す。** 「速くなった気がする」は判定にしない。

## ルール

- 作業ディレクトリは `/Users/kobadai/Documents/ryuusi-bench`(git worktree、ブランチ `feature/benchmark`)。`/Users/kobadai/Documents/ryuusi`(メインの checkout)には触らない
- `git stash` は使わない(worktree 間で共有される)。一時退避が要るなら WIP コミットにする
- フェーズ 1 は `feature/benchmark`、フェーズ 2 は施策ごとに `perf/<名前>` ブランチを `origin/main` から切る(前の PR がマージ済みならそれを含む `origin/main` から)
- 作業の区切りごとに小さくコミットする(日本語のコミットメッセージ、何をなぜ、を 1〜3 行)
- PR は作るがマージはしない。`gh pr create` のあと、ペインに `READY: PR #<番号> <URL>` と 1 行出力する(ディレクターがこの文字列を待っている)
- ディレクターに判断を仰ぎたいときは `QUESTION: <内容>` と 1 行出力して待つ
- 既存のコードの流儀に合わせる(コメントは日本語、SoA、`#ifdef __EMSCRIPTEN__` での分岐)
- 見た目・操作・粒子数は変えない。フェーズ 2 の変更は「同じ入力に対して同じ(誤差の範囲で)動き」を保つ
- ネイティブのビルド: `make`(依存は Homebrew の sdl2 / glew / pkgconf、インストール済み)。wasm: `make wasm`(Emscripten 6.0.12 が `/opt/homebrew/bin/emcc` に入っている)
- ブラウザ版の動作確認用のローカルサーバーは自分で立てる(例: `python3 -m http.server -d bin 8126`)。8124 は別のプロセスが使っている
- ブラウザでの計測は **タブが前面に見えている必要がある**(非表示タブでは `requestAnimationFrame` が止まる)。自分では見えないので、URL を `QUESTION:` で伝えてディレクターに開いてもらう

## フェーズ 1: 計測モード(PR 1 本)

### 起動方法

- ネイティブ: `./bin/particle_flow --bench [--bench-out=path.json] [--bench-runs=N]`
- ブラウザ: `app/?bench=1[&report=http://127.0.0.1:PORT/report]`。`report` があれば結果 JSON をそこへ `POST` する(ディレクターがローカルで受ける)。画面にも結果を表示する(ImGui のウィンドウで可)

### 再現性

- 乱数の種を固定(`g_seed` を計測開始時に定数で初期化)
- 粒子数 300,000、ウィンドウ/キャンバス 1280×720 に固定(ブラウザでは bench モードのときだけキャンバスの CSS サイズを 1280×720 にする)
- **dt を 1/60 秒に固定**(実時間を使わない。速さに関係なく同じ計算にするため)
- VSync を切る(`SDL_GL_SetSwapInterval(0)`)。ブラウザは rAF 縛りなので切れない。そのため主指標は FPS ではなく「段階ごとの ms」
- 入力は `input_process_ex` を通さず、シナリオが `InputState` 相当の値(マウス座標、mode、mode_active、gather_on、timestop_on、do_ripple)をフレームごとに直接与える。既存の `app.c` の流れを壊さないよう、bench 用の入力を差し込む小さな関数を用意する
- 最初の 60 フレームはウォームアップとして捨てる

### シナリオ(各フレーム数は目安。合計 2〜3 分以内に収める)

| # | 名前 | 内容 | フレーム |
|---|---|---|---|
| 1 | idle | 入力なし(基準値) | 300 |
| 2 | attract | 中央 (640,360) で左クリック引力 | 300 |
| 3 | gather_hold | G を押し続ける(中央) | 180 |
| 4 | gather_release | G を離した直後。カーソルは中央 (640,360) のまま動かさず、全粒子が 1 点に重なった最悪ケースを測る | 60 |
| 5 | gather_release_spread | その続き。開始時にカーソルを (900,360) へ移して広がりを測る | 120 |
| 6 | vortex | V(渦) | 300 |
| 7 | explode | E を 1 回(最初の 1 フレームだけ押す) | 60 |
| 8 | ripple | B を 30 フレームごとに | 300 |

### 計測する項目(フレームごと)

| 段階 | 測り方 |
|---|---|
| update | `ps_update` の前後で `SDL_GetPerformanceCounter` |
| pack | `ps_pack_gpu` |
| upload | `glBufferSubData`(CPU 側の所要時間) |
| draw | `glDrawArrays` の呼び出し(CPU 側) |
| gui | `gui_render` |
| present | `SDL_GL_SwapWindow` |
| frame | フレーム全体 |
| gpu | ネイティブ: `GL_TIME_ELAPSED` のタイマークエリ(描画部分を囲む。結果の取得は数フレーム遅らせて、ストールさせない)。ブラウザ: `EXT_disjoint_timer_query_webgl2` が取れれば使い、取れなければ `null` にして「推定不可」と明記 |

### 集計と出力

- シナリオ × 段階ごとに **中央値 (p50) と p95**、およびシナリオの FPS(フレーム数 ÷ 実時間)
- 各シナリオの **開始から 10 フレーム後** と **終了時** に **位置のチェックサム**: 全粒子の x, y, vx, vy を double で合計した 4 つの値と、粒子数。フェーズ 2 で「動きが変わっていない」ことの確認に使う。判定(ディレクター決定):
  - 10 フレーム後: 粒子数は完全一致、位置の合計 (sum_x, sum_y) は相対誤差 1e-4 以内
  - 終了時: 粒子数は完全一致、位置の合計は相対誤差 1e-3 以内
  - 速度の合計 (sum_vx, sum_vy) は判定に使わず、参考値として JSON に残す
  - 理由: 引力・渦などはカオス的で、`-ffast-math` の有無のような丸めの違いだけでも数百フレームで位置の合計が 3〜9e-4、速度の合計は相対 0.1〜2 ずれる。10 フレーム後なら丸めの違いでも 1e-4 に収まる
- JSON で出力。例:

```json
{
  "label": "baseline", "platform": "native|wasm", "build": "<git short sha>", "dirty": false,
  "particles": 300000, "size": [1280, 720], "dt": 0.016666,
  "gpu_timer": true,
  "scenarios": [
    { "name": "gather_hold", "frames": 180, "fps": 41.2,
      "ms": { "update": { "p50": 9.8, "p95": 10.4 }, "pack": {...}, "upload": {...}, "draw": {...}, "gui": {...}, "present": {...}, "frame": {...}, "gpu": {...} },
      "checksum": { "count": 300000, "sum_x": 1.9e8, "sum_y": 1.1e8, "sum_vx": 3.1e3, "sum_vy": -2.2e3 } }
  ]
}
```

- ネイティブは `--bench-out` のファイルに書き、標準出力にも Markdown の表を出す
- ブラウザは画面に表と JSON を表示し、`report` があれば POST する

### 周辺ツール

- `make bench`: ネイティブをビルドして `--bench-runs=5` で実行し、`bench/results/<label>.json` に保存(中央値の中央値)
- `bench/compare.py a.json b.json`: 2 つの JSON から「シナリオ × 段階: before → after (×倍率)」の Markdown 表を出す。チェックサムの一致/不一致も表示する
- `bench/README.md`: 使い方(ネイティブ、ブラウザ、比較)
- 任意だが推奨: `bench/sim_bench.c` — SDL なしで `ps_update` + `ps_pack_gpu` だけを同じシナリオで回すマイクロベンチ。ネイティブと wasm(Node で実行)の両方でビルドできるようにし、ループの書き換えを秒単位で試せるようにする

### フェーズ 1 の完了条件

- 今のコードで native と wasm の両方のベースラインを取り、`bench/results/baseline-native.json` と `baseline-wasm.json` をコミット
- PR 本文に両方の表を貼り、仮説 1・2 が数字で確認できたかを書く
- 計測コードが通常プレイに影響しないこと(bench モードでないときの挙動は変えない)

## フェーズ 2: 高速化(施策ごとに PR 1 本、各 PR に before/after の表とチェックサム比較)

ディレクターが順番と可否を判断する。原則この順で、1 つ終わるごとに `READY:` を出して次の指示を待つ。

1. **wasm のコンパイルフラグ**: `-O3 -ffast-math -msimd128 -flto`(`WASM_COMMON` と `WASM_LDFLAGS`)。ネイティブは変更なし
2. **`ps_update` のループ整理**:
   - `expf` をループの外へ(フレームに 1 回)
   - モードごとの係数(引力・斥力・渦・爆発の強さ)をループの外で決め、ループ内の `switch` / `if` を算術に置き換える(粒子によらない条件をループ内で評価しない)
   - `dx, dy, dist2, inv_dist` を 1 回だけ計算して共有
   - `mass` の代わりに `inv_mass` を持ち、割り算を掛け算に
   - `LIFE_DECAY = 0` で実際には通らない「末尾と入れ替えて削除」をループから外す(API と `life` の意味は残す)
   - 自動ベクトル化されたことを確認する(clang: `-Rpass=loop-vectorize`、wasm: `-msimd128` 付きで `wasm-objdump` か同等の方法で `f32x4` 命令を確認)
3. **色の静的化と転送量の削減**:
   - `hue` から RGB への変換は生成時と Recolor 時だけ行い、色は別の静的 VBO(RGBA8、4 バイト)に置く
   - 毎フレーム送るのは位置だけ(`float×2`。可能なら `GL_UNSIGNED_SHORT` 正規化で 4 バイト)
   - 位置用 VBO を 2〜3 本用意して交互に使う(または `glBufferData(NULL)` で orphaning)。macOS は GL 4.1 までなので永続マップは使わない
   - `ps_pack_gpu` の仕事を位置のコピーだけにする
4. **フラグメントシェーダーの `discard` をやめる**: `alpha = 0` にしてブレンドで消す。ブレンドが有効になっていることを確認する。見た目の変化はスクリーンショットで確認して PR に貼る
5. **(ディレクターの判断待ち)半解像度 FBO**: 粒子を半分の解像度の FBO に描いて画面に拡大。パラメーターで切り替えられるようにし、gather_release での効果を測る
6. **(ディレクターの判断待ち。指示なしに始めない)** Transform Feedback による GPU 物理、密度テクスチャへのスプラッティング

各 PR で必ず:

- `make bench` と wasm 計測の両方を before/after で取り、`bench/compare.py` の表を PR 本文に貼る
- チェックサムが許容誤差内であることを書く(超えたら理由を書き、ディレクターに `QUESTION:` で確認)
- 通常プレイで見た目が変わっていないことを確認する(スクリーンショット)

## 困ったら

`QUESTION:` で聞く。推測で仕様を変えない。

## 試したが不採用

### フラグメントシェーダーの `discard` をやめる (フェーズ 2 の 4、ブランチ `perf/no-discard`、PR にはしていない)

- 変更: `if (dist > 0.25) discard;` を消し、`fade = max(1.0 - dist * 4.0, 0.0)` にして円の外を alpha = 0 にする (加算合成 `GL_SRC_ALPHA, GL_ONE` なので何も足さない)
- 見た目: `--bench-shot` で撮った全 8 シナリオの画面が画素まで完全一致 (imgdiff の最大差 0)。チェックサムも完全一致
- 既定の点の大きさ 3px では、`discard` はもともと一度も発火していない。点の中の画素の中心は `gl_PointCoord` = 1/6, 1/2, 5/6 なので、角の画素でも dist = 2 × (1/3)² = 0.222 で、0.25 を超えない
- 結果 (native、5 回の中央値、gpu の p50): gather_hold / gather_release / gather_release_spread で **22.25 → 24.98 ms (約 12% 遅くなった)**。交互に走らせた A/B でも before 22.26 / 22.27、after 24.98 / 24.99 と再現した。発火しない `discard` を外しても、シェーダーのコード生成が変わって遅くなるだけ
- 判断 (ディレクター): GPU が悪化し、見た目は同じなので入れる理由がない。不採用。wasm は測っていない

### 粒子を縮小 FBO に描いて拡大する (フェーズ 2 の 5、ブランチ `perf/half-res-fbo`、PR にはしていない)

- 変更: `SimParams.render_scale` (1 / 0.5 / 0.25)。1 未満なら粒子だけを縮小した RGBA8 の FBO に点の大きさも縮めて描き、画面には線形補間で拡大して貼る。ImGui はフル解像度
- 結果 (native、5 回の中央値、gpu の p50、ms):

  | scenario | before | 1.0 | 0.5 | 0.25 |
  |---|---:|---:|---:|---:|
  | idle | 1.00 | 1.04 | 0.92 | 0.51 |
  | gather_hold | 22.34 | 22.32 | 22.41 | 23.19 |
  | gather_release | 22.26 | 22.25 | 23.28 | 23.33 |
  | gather_release_spread | 22.24 | 22.25 | 23.10 | 23.23 |

- G 系にはまったく効かない。フル解像度のまま点の大きさを 1px / 6px に変えて試しても (未コミット)、gather 系は約 22ms で変わらなかった。重さはフラグメントの数ではなく、**同じ画素に重なる約 30 万個の点のブレンドが順番待ちになること** (1 点あたり約 74ns) で決まる。解像度を下げても、同じ画素に重なる点の数は減らない
- 見た目: 1.0 は元と画素まで一致。0.5 / 0.25 は粒が粗くなり、平均の明るさが idle で 82.7 → 121 (約 +45%)、attract で 76.0 → 110 になる。小さい点が FBO の 1 画素に明るく乗り、それが 4〜16 画素に引き伸ばされるため
- 判断 (ディレクター): G 系に効かず、見た目も明るくなるので不採用。wasm は測っていない
