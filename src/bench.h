#ifndef BENCH_MODE_H
#define BENCH_MODE_H

/* ------------------------------------------------------------------ *
 *  計測モード (--bench / ブラウザでは ?bench=1)
 *
 *  シナリオ (bench_core.c) の入力を固定 dt で与え、段階ごとの所要時間を
 *  記録して JSON / Markdown にまとめる。bench モードでないときは何もしない。
 * ------------------------------------------------------------------ */

#include "bench_core.h"
#include "render.h"

/* コマンドライン (ブラウザでは Module.arguments) を読む。
 *   --bench              計測モードで起動
 *   --bench-out=path     結果の JSON を書くファイル (ネイティブ)
 *   --bench-runs=N       繰り返す回数 (既定 1)。結果は回ごとの p50 / p95 の中央値
 *   --bench-label=name   結果に付けるラベル (既定 "bench")
 *   --bench-report=url   結果の JSON を POST する先 (ブラウザ)
 *   --bench-cull=N       飽和カリングの上限 (0 で無効)。省略時は PS_CULL_LIMIT_DEFAULT
 *   --bench-shot=dir     各シナリオの最後のフレームの粒子の描画を dir/<label>-<scenario>.ppm に保存
 *                        (ネイティブのみ。読み戻しで GPU を待つので、この指定をした回の時間は使わない)
 * 戻り値: 0 = OK、非 0 = 引数の誤り */
int bench_configure(int argc, char **argv);
int bench_enabled(void);

/* render_init の後に 1 度呼ぶ。win_w / win_h は実際のウィンドウの大きさ */
void bench_start(Renderer *r, int win_w, int win_h);

typedef enum {
    BENCH_FRAME_RUN,    /* 計測中                                       */
    BENCH_FRAME_RESET,  /* 新しい回の頭。呼び出し側で乱数・粒子・波紋を初期化 */
    BENCH_FRAME_DONE    /* 計測は終わった (結果は出力済み)                */
} BenchFrameKind;

/* フレームの頭で呼ぶ。このフレームに与える入力を *in に書く */
BenchFrameKind bench_begin_frame(BenchInput *in);

/* render_frame の直後 (GUI を描く前) に呼ぶ。--bench-shot のときだけ画面を保存する */
void bench_after_render(void);

/* フレームの終わりで呼ぶ。stage_ms は app.c で測った段階
 * (update / gui / present / frame) の ms。pack / upload / draw / gpu は
 * Renderer から取る */
void bench_end_frame(const double stage_ms[BS_COUNT], const ParticleSystem *ps);

#endif /* BENCH_MODE_H */
