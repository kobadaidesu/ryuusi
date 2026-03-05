#ifndef GUI_H
#define GUI_H

/* C から呼べる Dear ImGui ラッパー
 * 実装は gui.cpp (C++) にある。
 */

#include "sim_params.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SDL_Window;
typedef void *SDL_GLContext;

/* ImGui 初期化 (SDL_GL_CreateContext 後に呼ぶ) */
void gui_init(struct SDL_Window *window, SDL_GLContext gl_ctx);

/* フレーム開始 (SDL イベント処理前に呼ぶ) */
void gui_new_frame(void);

/* SDL イベントを ImGui に渡す (SDL_PollEvent の直後に呼ぶ)
 * 戻り値: ImGui がイベントを消費した場合 1 */
int gui_process_event(void *sdl_event);

/* GUI ウィジェット描画 + ImGui レンダリング
 *   hud_visible    : HUD オーバーレイを表示するか (P キー)
 *   params_visible : Params ウィンドウを表示するか (Tab キー)
 *   戻り値ビットフラグ:
 *     bit0 (1): 粒子を全部再配置 (Respawn)
 *     bit1 (2): 色相だけ即時更新 (Recolor) */
int gui_render(SimParams *params, int hud_visible, int params_visible,
               float fps, int count, int mode, int timestop_on);

/* ImGui 後始末 */
void gui_shutdown(void);

/* ImGui がマウス/キーボードを使用中か */
int gui_wants_mouse(void);
int gui_wants_keyboard(void);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
