#ifndef INPUT_H
#define INPUT_H

#include "particle.h"   /* InteractMode */
#include <SDL2/SDL.h>

/* ------------------------------------------------------------------ *
 *  入力状態スナップショット (毎フレーム更新)
 *
 *  キーバインド:
 *   マウス左           引力 (ATTRACT)
 *   マウス右           斥力 (REPEL)
 *   マウス中           生成 (SPAWN)
 *   E キー             爆発 (EXPLODE)  ← 長押し中のみ
 *   V キー             渦   (VORTEX)   ← 長押し中のみ
 *   B キー             波紋 (Ripple)   ← 押した瞬間に発生
 *   T キー             タイムストップ切り替え
 *   P キー             HUD 表示切り替え
 *   Tab キー           パラメータ GUI 表示切り替え
 *   R キー             リセット
 *   F キー             フルスクリーン切り替え
 *   Esc / Q            終了
 * ------------------------------------------------------------------ */
typedef struct {
    float         mouse_x, mouse_y;  /* スクリーン座標                */
    InteractMode  mode;              /* 現在の有効モード              */
    int           mode_active;       /* モードが有効か                */
    int           timestop_on;       /* タイムストップ ON/OFF (トグル) */
    int           gather_on;         /* G キー全体引力 (長押し)        */
    int           do_reset;          /* リセット要求フラグ (1フレーム) */
    int           do_fullscreen;     /* フルスクリーン要求フラグ       */
    int           do_toggle_gui;     /* HUD 表示切り替えフラグ (P)   */
    int           do_toggle_params;  /* Params 表示切り替えフラグ (Tab) */
    int           do_ripple;         /* B キー: 波紋発生 (1フレーム)  */
    int           should_quit;       /* 終了フラグ                    */
} InputState;

/* ------------------------------------------------------------------ *
 *  API
 * ------------------------------------------------------------------ */
void input_init(InputState *in);

/* SDL イベントキューを全部消費してスナップショットを更新する。
 *   block_mouse    : ImGui がマウスを使用中のとき 1 → マウスモードを無効化
 *   block_keyboard : ImGui がキーボードを使用中のとき 1 → キー入力を無効化
 * ※ 各 SDL イベントは ImGui にも転送する。 */
void input_process_ex(InputState *in, int block_mouse, int block_keyboard);

#endif /* INPUT_H */
