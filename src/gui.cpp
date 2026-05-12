/*  gui.cpp  –  Dear ImGui ラッパー (C++ / SDL2+OpenGL3 バックエンド)
 *
 *  C コードから extern "C" 関数として呼べるように薄くラップする。
 */

#include "gui.h"
#include "sim_params.h"
#include "particle.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_opengl3.h"

#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#  include <GLES3/gl3.h>
#else
#  include <GL/glew.h>
#endif
#include <cstdio>

/* ------------------------------------------------------------------ */
/*  プリセットスロット (10 個、バイナリファイルで永続化)                */
/* ------------------------------------------------------------------ */
#define NUM_PRESETS 10
static SimParams g_presets[NUM_PRESETS];
static bool      g_preset_used[NUM_PRESETS];
static bool      g_presets_loaded = false;

static void preset_path(int i, char *buf, int bufsz)
{
    snprintf(buf, bufsz, "preset_%02d.bin", i + 1);
}

static void presets_load_all(void)
{
    if (g_presets_loaded) return;
    g_presets_loaded = true;

    /* スロット 0 をデフォルト (Rainbow) で初期化 */
    static const SimParams DEF_PRESET = SIM_PARAMS_DEFAULT;
    g_presets[0]    = DEF_PRESET;
    g_preset_used[0] = true;

    /* ファイルがあれば上書き読み込み */
    for (int i = 0; i < NUM_PRESETS; i++) {
        char path[32]; preset_path(i, path, sizeof(path));
        FILE *f = fopen(path, "rb");
        if (f) {
            if (fread(&g_presets[i], sizeof(SimParams), 1, f) == 1)
                g_preset_used[i] = true;
            fclose(f);
        }
    }
}

static void preset_save(int i, const SimParams *p)
{
    g_presets[i] = *p;
    g_preset_used[i] = true;
    char path[32]; preset_path(i, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(p, sizeof(SimParams), 1, f); fclose(f); }
}

extern "C" {

void gui_init(SDL_Window *window, void *gl_ctx)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().Alpha = 0.92f;

    /* 日本語フォントを読み込む (Emscripten では仮想FSにフォントがないためスキップ) */
#ifndef __EMSCRIPTEN__
    static const ImWchar jp_ranges[] = {
        0x0020, 0x00FF,   /* ASCII + Latin */
        0x3000, 0x30FF,   /* ひらがな・カタカナ */
        0x4E00, 0x9FFF,   /* CJK 統合漢字 */
        0xFF00, 0xFFEF,   /* 全角英数 */
        0,
    };
    const char *font_paths[] = {
        "/mnt/c/Windows/Fonts/meiryo.ttc",
        "/mnt/c/Windows/Fonts/msgothic.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        nullptr
    };
    bool loaded = false;
    for (int k = 0; font_paths[k] && !loaded; k++) {
        ImFont *f = io.Fonts->AddFontFromFileTTF(font_paths[k], 16.0f, nullptr, jp_ranges);
        if (f) loaded = true;
    }
    if (!loaded)
#endif
    {
        io.Fonts->AddFontDefault();
        io.FontGlobalScale = 1.2f;
    }

    /* スタイル調整 */
    ImGuiStyle &st = ImGui::GetStyle();
    st.Alpha             = 1.0f;
    st.WindowPadding     = ImVec2(12, 10);
    st.FramePadding      = ImVec2(6, 4);
    st.ItemSpacing       = ImVec2(8, 6);
    st.ScrollbarSize     = 12.0f;
    st.WindowBorderSize  = 1.0f;
    st.FrameBorderSize   = 0.5f;
    st.WindowRounding    = 6.0f;
    st.FrameRounding     = 4.0f;
    st.GrabRounding      = 4.0f;

    ImVec4 *c = st.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.07f, 0.07f, 0.12f, 0.97f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.10f, 0.10f, 0.20f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.15f, 0.15f, 0.35f, 1.00f);
    c[ImGuiCol_Header]          = ImVec4(0.20f, 0.30f, 0.55f, 0.85f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.28f, 0.38f, 0.65f, 0.90f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.14f, 0.22f, 0.48f, 1.00f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.14f, 0.14f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.20f, 0.20f, 0.30f, 1.00f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.45f, 0.60f, 0.95f, 1.00f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.55f, 0.70f, 1.00f, 1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.20f, 0.28f, 0.52f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.28f, 0.38f, 0.65f, 1.00f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.14f, 0.20f, 0.42f, 1.00f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.55f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_Separator]       = ImVec4(0.30f, 0.30f, 0.50f, 0.80f);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
#ifdef __EMSCRIPTEN__
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    ImGui_ImplOpenGL3_Init("#version 330 core");
#endif
}

void gui_new_frame(void)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

int gui_process_event(void *sdl_event)
{
    return ImGui_ImplSDL2_ProcessEvent((SDL_Event *)sdl_event) ? 1 : 0;
}

/* HSV -> ImVec4 (プレビュー用、s=0.9, v=1.0) */
static ImVec4 hue_to_color(float h)
{
    float r, g, b;
    float hh = h * 6.0f;
    int   i  = (int)hh % 6;
    float f  = hh - (int)hh;
    float p  = 0.1f;
    float q  = 1.0f - 0.9f * f;
    float t  = 0.1f + 0.9f * f;
    switch (i) {
        case 0: r=1.0f; g=t;    b=p;    break;
        case 1: r=q;    g=1.0f; b=p;    break;
        case 2: r=p;    g=1.0f; b=t;    break;
        case 3: r=p;    g=q;    b=1.0f; break;
        case 4: r=t;    g=p;    b=1.0f; break;
        default:r=1.0f; g=p;    b=q;    break;
    }
    return ImVec4(r, g, b, 1.0f);
}

int gui_render(SimParams *p, int hud_visible, int params_visible,
               float fps, int count, int mode, int timestop_on)
{
    int result = 0; /* bit0=respawn, bit1=recolor */

    /* ---- HUD オーバーレイ (P キー) ---- */
    if (hud_visible) {
        static const char *mode_jp[] = {
            "なし", "引力", "斥力", "爆発", "渦", "生成", "タイムストップ"
        };
        const char *mname = (mode >= 0 && mode <= 6) ? mode_jp[mode] : "?";
        ImGuiIO &hio = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(hio.DisplaySize.x * 0.5f, 8.0f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::Begin("##hud", nullptr,
            ImGuiWindowFlags_NoDecoration  | ImGuiWindowFlags_NoInputs   |
            ImGuiWindowFlags_NoNav         | ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing);
        if (timestop_on)
            ImGui::Text("%d粒子  %.0f FPS  モード: %s + タイムストップ", count, fps, mname);
        else
            ImGui::Text("%d粒子  %.0f FPS  モード: %s", count, fps, mname);
        ImGui::End();
    }

    if (params_visible) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(390.0f, 750.0f), ImGuiCond_FirstUseEver);

        /* フラグなし = タイトルバーでドラッグ移動 + 端をドラッグでリサイズ可能 */
        ImGui::Begin("Params  [Tab: toggle]", nullptr, 0);

        /* デフォルト値参照用 (各セクションの部分リセットに使う) */
        static const SimParams DEF = SIM_PARAMS_DEFAULT;

        /* ---- Color Presets ---- */
        if (ImGui::CollapsingHeader("Color Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
            struct Preset { const char *name; float lo, hi; };
            static const Preset presets[] = {
                { "Green~Blue",  0.33f, 0.67f },
                { "Fire",        0.00f, 0.12f },
                { "Gold",        0.06f, 0.17f },
                { "Lava",        0.00f, 0.06f },
                { "Sunset",      0.02f, 0.10f },
                { "Sakura",      0.88f, 1.00f },
                { "Neon",        0.75f, 0.92f },
                { "Purple",      0.70f, 0.85f },
                { "Ocean",       0.50f, 0.65f },
                { "Glacier",     0.54f, 0.62f },
                { "Mint",        0.35f, 0.50f },
                { "Aurora",      0.40f, 0.80f },
                { "Rainbow",     0.00f, 1.00f },
                { "Warm",        0.00f, 0.20f },
                { "Cool",        0.50f, 0.80f },
                { "Rose",        0.82f, 0.97f },
            };
            static const int N = (int)(sizeof(presets) / sizeof(presets[0]));

            float btn_w = 110.0f;
            float win_w = ImGui::GetContentRegionAvail().x;
            int cols = (int)(win_w / (btn_w + 22.0f));
            if (cols < 1) cols = 1;

            for (int k = 0; k < N; k++) {
                float mid = (presets[k].lo + presets[k].hi) * 0.5f;
                ImVec4 col = hue_to_color(mid);
                ImGui::PushID(k);
                ImGui::ColorButton("##c", col,
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                    ImVec2(14, 14));
                ImGui::SameLine();
                if (ImGui::Button(presets[k].name, ImVec2(btn_w, 0.0f))) {
                    p->hue_min = presets[k].lo;
                    p->hue_max = presets[k].hi;
                    result |= 2;
                }
                ImGui::PopID();
                if ((k % cols) < (cols - 1)) ImGui::SameLine();
            }

            ImGui::Spacing();
            ImGui::SliderFloat("Hue min", &p->hue_min, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Hue max", &p->hue_max, 0.0f, 1.0f, "%.2f");
            if (ImGui::Button("Recolor now", ImVec2(-FLT_MIN, 0.0f))) { result |= 2; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##color")) {
                p->hue_min = DEF.hue_min; p->hue_max = DEF.hue_max; result |= 2;
            }
        }

        /* ---- Sim Area (境界壁) ---- */
        if (ImGui::CollapsingHeader("Sim Area  [wall]", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiIO &io2 = ImGui::GetIO();
            float dw = io2.DisplaySize.x;
            float dh = io2.DisplaySize.y;

            ImGui::SliderFloat("Width  px", &p->sim_bounds_w, 0.0f, dw > 0 ? dw : 3840.0f, "%.0f");
            ImGui::SliderFloat("Height px", &p->sim_bounds_h, 0.0f, dh > 0 ? dh : 2160.0f, "%.0f");

            if (ImGui::Button("Full window (no wall)", ImVec2(-FLT_MIN, 0.0f))) {
                p->sim_bounds_w = 0.0f; p->sim_bounds_h = 0.0f;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##bounds")) {
                p->sim_bounds_w = DEF.sim_bounds_w; p->sim_bounds_h = DEF.sim_bounds_h;
            }

            /* 境界ボックスをプレビュー描画 */
            if (p->sim_bounds_w > 0.0f || p->sim_bounds_h > 0.0f) {
                float bw = (p->sim_bounds_w > 0.0f) ? p->sim_bounds_w : dw;
                float bh = (p->sim_bounds_h > 0.0f) ? p->sim_bounds_h : dh;
                float bx = (dw - bw) * 0.5f;
                float by = (dh - bh) * 0.5f;
                ImDrawList *dl = ImGui::GetBackgroundDrawList();
                dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
                            IM_COL32(100, 220, 255, 180), 0.0f, 0, 2.0f);
            }
        }

        /* ---- Attract / Repel ---- */
        if (ImGui::CollapsingHeader("Attract / Repel", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Attract (LMB)",  &p->attract_str,     100000.0f, 30000000.0f, "%.0f");
            ImGui::SliderFloat("Attract range (0=inf)", &p->attract_range, 0.0f, 2000.0f, "%.0f px");
            ImGui::SliderFloat("Repel (RMB)",    &p->repel_str,       10000.0f,  2000000.0f,  "%.0f");
            ImGui::SliderFloat("Passive attract",&p->passive_attract,  0.0f,      500000.0f,  "%.0f");
            ImGui::SliderFloat("Gather speed (G)", &p->gather_str, 1.0f, 200.0f, "%.1f");
            if (ImGui::SmallButton("Reset##attract")) {
                p->attract_str    = DEF.attract_str;
                p->attract_range  = DEF.attract_range;
                p->repel_str      = DEF.repel_str;
                p->passive_attract= DEF.passive_attract;
                p->gather_str     = DEF.gather_str;
            }
        }

        /* ---- Time Stop (T key) ---- */
        if (ImGui::CollapsingHeader("Time Stop (T key)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("TS Attract",  &p->timestop_str,     100000.0f, 30000000.0f, "%.0f");
            ImGui::SliderFloat("TS Damping",  &p->timestop_damping, 0.10f,     0.99f,       "%.2f");
            if (ImGui::SmallButton("Reset##ts")) {
                p->timestop_str     = DEF.timestop_str;
                p->timestop_damping = DEF.timestop_damping;
            }
        }

        /* ---- Vortex (V) / Explode (E) ---- */
        if (ImGui::CollapsingHeader("Vortex (V) / Explode (E)")) {
            ImGui::SliderFloat("Vortex str",   &p->vortex_str,  100000.0f, 10000000.0f, "%.0f");
            ImGui::SliderFloat("Vortex cap",   &p->vortex_cap,  1000.0f,   200000.0f,   "%.0f");
            ImGui::SliderFloat("Explode str",  &p->explode_str, 100000.0f, 10000000.0f, "%.0f");
            ImGui::SliderFloat("Explode cap",  &p->explode_cap, 1000.0f,   200000.0f,   "%.0f");
            if (ImGui::SmallButton("Reset##vortex")) {
                p->vortex_str  = DEF.vortex_str;  p->vortex_cap  = DEF.vortex_cap;
                p->explode_str = DEF.explode_str; p->explode_cap = DEF.explode_cap;
            }
        }

        /* ---- Ripple (B key) ---- */
        if (ImGui::CollapsingHeader("Ripple  [B key]")) {
            ImGui::SliderFloat("Ripple str", &p->ripple_str, 100000.0f, 20000000.0f, "%.0f");
            if (ImGui::SmallButton("Reset##ripple")) { p->ripple_str = DEF.ripple_str; }
        }

        /* ---- Physics ---- */
        if (ImGui::CollapsingHeader("Physics")) {
            ImGui::SliderFloat("Gravity",     &p->gravity,     -500.0f, 500.0f,  "%.1f");
            ImGui::SliderFloat("Damping",     &p->damping,     0.90f,   1.0f,    "%.4f");
            ImGui::SliderFloat("Speed limit", &p->speed_limit, 200.0f,  5000.0f, "%.0f px/s");
            ImGui::SliderFloat("Bounce",      &p->bounce,      0.0f,    1.0f,    "%.2f");
            if (ImGui::SmallButton("Reset##physics")) {
                p->gravity     = DEF.gravity;
                p->damping     = DEF.damping;
                p->speed_limit = DEF.speed_limit;
                p->bounce      = DEF.bounce;
            }
        }

        /* ---- Particles ---- */
        if (ImGui::CollapsingHeader("Particle of number", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("Count", &p->particle_count, 1000, MAX_PARTICLES, "%d");
            if (ImGui::Button("Apply (Respawn)", ImVec2(-FLT_MIN, 0.0f))) result |= 1;
        }

        /* ---- Presets ---- */
        if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
            presets_load_all();

            /* Save 行 */
            ImGui::Text("Save :");
            for (int i = 0; i < NUM_PRESETS; i++) {
                ImGui::SameLine();
                ImGui::PushID(i);
                char lbl[4]; snprintf(lbl, sizeof(lbl), "%d", i + 1);
                if (ImGui::SmallButton(lbl)) preset_save(i, p);
                ImGui::PopID();
            }

            /* Load 行 (空スロットは薄く表示) */
            ImGui::Text("Load :");
            for (int i = 0; i < NUM_PRESETS; i++) {
                ImGui::SameLine();
                ImGui::PushID(100 + i);
                char lbl[4]; snprintf(lbl, sizeof(lbl), "%d", i + 1);
                if (!g_preset_used[i]) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                        ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                }
                if (ImGui::SmallButton(lbl) && g_preset_used[i]) {
                    *p = g_presets[i];
                    result |= 2; /* Recolor */
                }
                if (!g_preset_used[i]) ImGui::PopStyleColor(2);
                ImGui::PopID();
            }
        }

        /* ---- Rendering ---- */
        if (ImGui::CollapsingHeader("Point Size")) {
            ImGui::SliderFloat("Point size", &p->point_size, 1.0f, 10.0f, "%.1f px");
            if (ImGui::SmallButton("Reset##render")) { p->point_size = DEF.point_size; }
        }

        ImGui::Separator();
        if (ImGui::Button("Reset ALL to defaults", ImVec2(-1.0f, 0.0f))) {
            SimParams def = SIM_PARAMS_DEFAULT;
            *p = def;
            result |= 2;
        }
        if (ImGui::Button("Respawn particles (R)", ImVec2(-1.0f, 0.0f))) {
            result |= 1;
        }

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return result;
}

void gui_shutdown(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

int gui_wants_mouse(void)
{
    return ImGui::GetIO().WantCaptureMouse ? 1 : 0;
}

int gui_wants_keyboard(void)
{
    return ImGui::GetIO().WantCaptureKeyboard ? 1 : 0;
}

} /* extern "C" */
