/*  app.c  –  初期化 / メインループ / 後始末
 *
 *  デルタタイム計算:
 *   SDL_GetPerformanceCounter を使いナノ秒精度で dt を計算する。
 *   dt は 0.001〜0.05 秒に clamp して物理が暴発しないようにする。
 *
 *  VSync:
 *   SDL_GL_SetSwapInterval(1) → VSync ON
 *   SDL_GL_SetSwapInterval(0) → VSync OFF (デフォルト)
 */

#include "app.h"
#include "particle.h"
#include "render.h"
#include "input.h"
#include "gui.h"
#include "sim_params.h"

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  include <GLES3/gl3.h>
#else
#  include <GL/glew.h>
#endif
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  定数                                                                */
/* ------------------------------------------------------------------ */
#define WINDOW_TITLE  "ParticleSim  [P: Params GUI]"
#define WIN_W         1280
#define WIN_H          720
#define VSYNC_ON       0

/* ------------------------------------------------------------------ */
/*  グローバルアプリ状態                                                 */
/* ------------------------------------------------------------------ */
static SDL_Window   *g_window   = NULL;
static SDL_GLContext g_glctx    = NULL;
static Renderer     *g_renderer = NULL;

static ParticleSystem g_ps;
static InputState     g_input;
static SimParams      g_params = SIM_PARAMS_DEFAULT;
static int            g_gui_visible    = 1;  /* HUD (P キー) */
static int            g_params_visible = 0;  /* Params ウィンドウ (Tab キー) */

#define RIPPLE_SPEED 700.0f
static RippleWave g_ripples[MAX_RIPPLES];
static int        g_ripple_count = 0;

/* ------------------------------------------------------------------ */
/*  フルスクリーン切り替え                                               */
/* ------------------------------------------------------------------ */
static void toggle_fullscreen(void)
{
    static int fs = 0;
    fs = !fs;
    SDL_SetWindowFullscreen(g_window,
                            fs ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

/* ------------------------------------------------------------------ */
/*  app_init                                                            */
/* ------------------------------------------------------------------ */
int app_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    g_window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    g_glctx = SDL_GL_CreateContext(g_window);
    if (!g_glctx) {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }

    SDL_GL_SetSwapInterval(VSYNC_ON);

    g_renderer = render_init(WIN_W, WIN_H);
    if (!g_renderer) {
        fprintf(stderr, "render_init failed\n");
        SDL_GL_DeleteContext(g_glctx);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return -1;
    }

    gui_init(g_window, g_glctx);

    ps_init(&g_ps, WIN_W, WIN_H, &g_params);
    input_init(&g_input);

    printf("Particle slots: %d\n", MAX_PARTICLES);
    printf("Initial count : %d\n", g_ps.count);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  メインループ状態 (Emscripten コールバックで共有)                    */
/* ------------------------------------------------------------------ */
static Uint64 g_freq;
static Uint64 g_prev;
static float  g_fps        = 0.0f;
static int    g_frame_count = 0;
static Uint64 g_fps_timer;

static void main_loop_iter(void)
{
    gui_new_frame();

    input_process_ex(&g_input,
                     gui_wants_mouse(),
                     gui_wants_keyboard());

    if (g_input.do_toggle_gui)    g_gui_visible    = !g_gui_visible;
    if (g_input.do_toggle_params) g_params_visible = !g_params_visible;

    Uint64 now = SDL_GetPerformanceCounter();
    float  dt  = (float)(now - g_prev) / (float)g_freq;
    g_prev = now;
    if (dt > 0.05f)  dt = 0.05f;
    if (dt < 0.001f) dt = 0.001f;

    if (g_input.do_reset)
        ps_reset(&g_ps, &g_params);

    if (g_input.do_ripple && g_ripple_count < MAX_RIPPLES) {
        g_ripples[g_ripple_count].cx         = g_input.mouse_x;
        g_ripples[g_ripple_count].cy         = g_input.mouse_y;
        g_ripples[g_ripple_count].radius     = 0.0f;
        g_ripples[g_ripple_count].max_radius = 1500.0f;
        g_ripple_count++;
    }

    for (int ri = 0; ri < g_ripple_count; ) {
        g_ripples[ri].radius += RIPPLE_SPEED * dt;
        if (g_ripples[ri].radius > g_ripples[ri].max_radius)
            g_ripples[ri] = g_ripples[--g_ripple_count];
        else
            ri++;
    }

    if (g_input.do_fullscreen) {
        toggle_fullscreen();
        int w, h;
        SDL_GetWindowSize(g_window, &w, &h);
        render_resize(g_renderer, w, h);
        g_ps.win_w = w;
        g_ps.win_h = h;
        ps_reset(&g_ps, &g_params);
    }

    {
        int w, h;
        SDL_GetWindowSize(g_window, &w, &h);
        if (w != g_ps.win_w || h != g_ps.win_h) {
            render_resize(g_renderer, w, h);
            g_ps.win_w = w;
            g_ps.win_h = h;
        }
    }

    ps_update(&g_ps, dt,
              g_input.mouse_x, g_input.mouse_y,
              g_input.mode, g_input.mode_active,
              g_input.timestop_on,
              g_input.gather_on,
              g_ripples, g_ripple_count,
              &g_params);

    render_frame(g_renderer, &g_ps, g_fps, g_input.mode, &g_params);

    {
        int gr = gui_render(&g_params, g_gui_visible, g_params_visible,
                            g_fps, g_ps.count, (int)g_input.mode,
                            g_input.timestop_on);
        if (gr & 1) ps_reset(&g_ps, &g_params);
        if (gr & 2) ps_recolor(&g_ps, &g_params);
    }

    SDL_GL_SwapWindow(g_window);

    g_frame_count++;
    Uint64 elapsed = now - g_fps_timer;
    if (elapsed >= g_freq) {
        g_fps = (float)g_frame_count * (float)g_freq / (float)elapsed;
        g_frame_count = 0;
        g_fps_timer   = now;

#ifndef __EMSCRIPTEN__
        const char *mode_names[] = {
            "NONE", "ATTRACT", "REPEL", "EXPLODE", "VORTEX", "SPAWN", "TIMESTOP"
        };
        char title_buf[256];
        snprintf(title_buf, sizeof(title_buf),
                 "ParticleSim | %d pts | %.0f FPS | %s%s"
                 " | LMB:Attract RMB:Repel E:Explode V:Vortex"
                 " S:Spawn T:TimeStop[%s] B:Ripple R:Reset F:Full P:GUI",
                 g_ps.count, g_fps, mode_names[g_input.mode],
                 g_input.timestop_on ? "+TS" : "",
                 g_input.timestop_on ? "ON" : "OFF");
        SDL_SetWindowTitle(g_window, title_buf);
#endif
    }

#ifdef __EMSCRIPTEN__
    if (g_input.should_quit) {
        app_shutdown();
        emscripten_cancel_main_loop();
    }
#endif
}

/* ------------------------------------------------------------------ */
/*  app_run  –  メインループ                                            */
/* ------------------------------------------------------------------ */
void app_run(void)
{
    g_freq        = SDL_GetPerformanceFrequency();
    g_prev        = SDL_GetPerformanceCounter();
    g_fps_timer   = g_prev;
    g_fps         = 0.0f;
    g_frame_count = 0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop_iter, 0, 1);
#else
    while (!g_input.should_quit)
        main_loop_iter();
#endif
}

/* ------------------------------------------------------------------ */
/*  app_shutdown                                                        */
/* ------------------------------------------------------------------ */
void app_shutdown(void)
{
    gui_shutdown();
    render_shutdown(g_renderer);
    SDL_GL_DeleteContext(g_glctx);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}
