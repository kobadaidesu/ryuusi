#include "input.h"
#include "gui.h"
#include <string.h>

void input_init(InputState *in)
{
    memset(in, 0, sizeof(*in));
}

void input_process_ex(InputState *in, int block_mouse, int block_keyboard)
{
    /* フレーム単位フラグをリセット */
    in->do_reset       = 0;
    in->do_fullscreen  = 0;
    in->do_toggle_gui    = 0;
    in->do_toggle_params = 0;
    in->do_ripple        = 0;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        /* ImGui にイベントを転送 */
        gui_process_event(&ev);

        switch (ev.type) {
            case SDL_QUIT:
                in->should_quit = 1;
                break;

            case SDL_KEYDOWN:
                if (block_keyboard) break;
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        in->should_quit = 1;
                        break;
                    case SDLK_r:
                        in->do_reset = 1;
                        break;
                    case SDLK_f:
                        in->do_fullscreen = 1;
                        break;
                    case SDLK_t:
                        in->timestop_on = !in->timestop_on; /* トグル */
                        break;
                    case SDLK_p:
                        in->do_toggle_gui = 1;
                        break;
                    case SDLK_TAB:
                        in->do_toggle_params = 1;
                        break;
                    case SDLK_b:
                        in->do_ripple = 1;
                        break;
                    default:
                        break;
                }
                break;

            case SDL_WINDOWEVENT:
                break;

            default:
                break;
        }
    }

    /* ---- 継続的な入力状態の取得 ---- */

    /* マウス座標 */
    int mx, my;
    Uint32 btns = SDL_GetMouseState(&mx, &my);
    in->mouse_x = (float)mx;
    in->mouse_y = (float)my;

    if (block_mouse) {
        in->mode        = MODE_NONE;
        in->mode_active = 0;
        return;
    }

    /* キーボード */
    const Uint8 *ks = SDL_GetKeyboardState(NULL);

    int left   = (btns & SDL_BUTTON_LMASK)  != 0;
    int right  = (btns & SDL_BUTTON_RMASK)  != 0;
    int middle = (btns & SDL_BUTTON_MMASK)  != 0;
    int key_e  = ks[SDL_SCANCODE_E];
    int key_v  = ks[SDL_SCANCODE_V];
    int key_s  = ks[SDL_SCANCODE_S];
    in->gather_on = ks[SDL_SCANCODE_G];

    /* 優先順位: EXPLODE > VORTEX > REPEL > ATTRACT > SPAWN */
    if (key_e) {
        in->mode        = MODE_EXPLODE;
        in->mode_active = 1;
    } else if (key_v) {
        in->mode        = MODE_VORTEX;
        in->mode_active = 1;
    } else if (right) {
        in->mode        = MODE_REPEL;
        in->mode_active = 1;
    } else if (left) {
        in->mode        = MODE_ATTRACT;
        in->mode_active = 1;
    } else if (middle || key_s) {
        in->mode        = MODE_SPAWN;
        in->mode_active = 1;
    } else {
        in->mode        = MODE_NONE;
        in->mode_active = 0;
    }
}
