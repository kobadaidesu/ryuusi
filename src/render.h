#ifndef RENDER_H
#define RENDER_H

#include "particle.h"

/* ------------------------------------------------------------------ *
 *  レンダラ状態 (不透明ハンドル経由でアクセス)
 * ------------------------------------------------------------------ */
typedef struct Renderer Renderer;

/* ------------------------------------------------------------------ *
 *  API
 * ------------------------------------------------------------------ */

/* OpenGL リソースを初期化 (SDL_GL_CreateContext 後に呼ぶ) */
Renderer *render_init(int win_w, int win_h);

/* 1フレーム描画:
 *   1) 画面クリア
 *   2) 位置のパック → VBO 転送 → glDrawArrays
 *   3) FPS テキスト(デバッグ用; 必要なら ImGui で置換可)     */
void render_frame(Renderer *r,
                  const ParticleSystem *ps,
                  float fps,
                  InteractMode mode,
                  const SimParams *params);

/* ウィンドウリサイズ (飽和カリングのカウント配列も作り直す) */
void render_resize(Renderer *r, int win_w, int win_h);

/* 飽和カリングの上限 (既定 PS_CULL_LIMIT_DEFAULT、0 で無効) */
void render_set_cull_limit(Renderer *r, int limit);

/* OpenGL リソース解放 */
void render_shutdown(Renderer *r);

/* ------------------------------------------------------------------ *
 *  ベンチマーク用の計測 (render_profile_enable を呼ばない限り何もしない)
 * ------------------------------------------------------------------ */
typedef struct {
    double pack_ms;    /* 位置のパック / カリング       */
    double upload_ms;  /* glBufferSubData (CPU 側)     */
    double draw_ms;    /* glDrawArrays 呼び出し (CPU 側) */
} RenderCpuTimes;

/* 計測を有効にする。GPU タイマー (GL_TIME_ELAPSED) が使えれば 1 */
int  render_profile_enable(Renderer *r);

/* 次の render_frame の GPU 計測に付ける番号 (遅れて出る結果と対応づけるため) */
void render_profile_set_tag(Renderer *r, int tag);

/* 直前の render_frame の CPU 側の所要時間 */
void render_profile_cpu(const Renderer *r, RenderCpuTimes *out);

/* 結果の出た GPU 計測を古い順に 1 つ取り出す。wait=1 なら出るまで待つ。
 * 取り出せたら 1 (*ms < 0 は無効な計測)。未読が無い / まだ出ていなければ 0 */
int  render_profile_poll_gpu(Renderer *r, int wait, int *tag, double *ms);

#endif /* RENDER_H */
