#ifndef RENDER_H
#define RENDER_H

#include "particle.h"

/* OpenGL コンテキスト設定用定数 */
#define GL_MAJOR_VERSION  3
#define GL_MINOR_VERSION  3

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
 *   2) ps_pack_gpu → VBO 転送 → glDrawArrays
 *   3) FPS テキスト(デバッグ用; 必要なら ImGui で置換可)     */
void render_frame(Renderer *r,
                  const ParticleSystem *ps,
                  float fps,
                  InteractMode mode,
                  const SimParams *params);

/* ウィンドウリサイズ */
void render_resize(Renderer *r, int win_w, int win_h);

/* OpenGL リソース解放 */
void render_shutdown(Renderer *r);

#endif /* RENDER_H */
