/*  render.c  –  SDL2 + OpenGL 3.3 Core パーティクル一括描画
 *
 *  設計ポイント:
 *  - 頂点 1 つ = (x, y, r, g, b, a) の 6 float (24 bytes) インターリーブ
 *  - VBO は起動時に MAX_PARTICLES 分をあらかじめ確保 (glBufferData NULL)
 *    毎フレームは glBufferSubData だけで済む (リアロケなし)
 *  - 加算合成 (GL_ONE, GL_ONE) で発光感を演出
 *  - gl_PointSize をシェーダで制御し、円形ディスカードで丸いスプライト
 */

#include "render.h"
#include "sim_params.h"
#include "bench_core.h"  /* bench_now_ms */
#ifdef __EMSCRIPTEN__
#  include <GLES3/gl3.h>
#  include <emscripten/html5.h>
/* EXT_disjoint_timer_query_webgl2 の定数 (GLES3 のヘッダには無い) */
#  define GL_TIME_ELAPSED     0x88BF
#  define GL_GPU_DISJOINT_EXT 0x8FBB
#else
#  include <GL/glew.h>
#endif
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  GLSL シェーダソース (文字列埋め込み)                                 */
/* ------------------------------------------------------------------ */
static const char *VERT_SRC =
#ifdef __EMSCRIPTEN__
    "#version 300 es\n"
    "precision highp float;\n"
#else
    "#version 330 core\n"
#endif
    "layout(location = 0) in vec2  a_pos;\n"
    "layout(location = 1) in vec4  a_color;\n"
    "uniform vec2  u_resolution;\n"
    "uniform float u_point_size;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    vec2 ndc = (a_pos / u_resolution) * 2.0 - 1.0;\n"
    "    ndc.y    = -ndc.y;\n"              /* Y 軸反転 (スクリーン座標系) */
    "    gl_Position  = vec4(ndc, 0.0, 1.0);\n"
    "    gl_PointSize = u_point_size;\n"
    "    v_color = a_color;\n"
    "}\n";

static const char *FRAG_SRC =
#ifdef __EMSCRIPTEN__
    "#version 300 es\n"
    "precision mediump float;\n"
#else
    "#version 330 core\n"
#endif
    "in  vec4 v_color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec2  c    = gl_PointCoord - 0.5;\n"
    "    float dist = dot(c, c);\n"
    "    if (dist > 0.25) discard;\n"       /* 円形クリッピング */
    "    float fade = 1.0 - dist * 4.0;\n"  /* 中心が明るいグラデーション */
    "    frag_color = vec4(v_color.rgb * fade, v_color.a * fade);\n"
    "}\n";

/* GPU 計測の結果は数フレーム遅れて読む (待つとストールするため) */
#define PROF_QUERIES 8

/* ------------------------------------------------------------------ */
/*  レンダラ内部構造体                                                   */
/* ------------------------------------------------------------------ */
struct Renderer {
    GLuint prog;        /* パーティクル シェーダプログラム */
    GLuint vao;
    GLuint vbo;
    GLint  u_resolution;
    GLint  u_point_size;
    GpuVertex *staging; /* CPU ステージングバッファ */

    int win_w, win_h;

    /* ---- ベンチマーク用の計測 (render_profile_enable 後のみ) ---- */
    int    prof;                       /* 計測中か                     */
    int    prof_gpu;                   /* GPU タイマーが使えるか         */
    int    prof_tag;                   /* 次の GPU 計測に付ける番号      */
    RenderCpuTimes prof_cpu;           /* 直前のフレームの CPU 側の時間  */
    GLuint prof_q[PROF_QUERIES];       /* GL_TIME_ELAPSED のクエリ (リング) */
    int    prof_q_tag[PROF_QUERIES];
    int    prof_head;                  /* 次に読むクエリ                 */
    int    prof_pending;               /* 結果をまだ読んでいない数        */
};

/* ------------------------------------------------------------------ */
/*  内部: シェーダコンパイル                                             */
/* ------------------------------------------------------------------ */
static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), NULL, buf);
        fprintf(stderr, "[render] shader compile error:\n%s\n", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint create_program(void)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   VERT_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAG_SRC);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(prog, sizeof(buf), NULL, buf);
        fprintf(stderr, "[render] program link error:\n%s\n", buf);
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

/* ------------------------------------------------------------------ */
/*  render_init                                                         */
/* ------------------------------------------------------------------ */
Renderer *render_init(int win_w, int win_h)
{
#ifndef __EMSCRIPTEN__
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "[render] glewInit: %s\n", glewGetErrorString(err));
        return NULL;
    }
#endif

    Renderer *r = (Renderer*)calloc(1, sizeof(Renderer));
    if (!r) return NULL;

    r->win_w = win_w;
    r->win_h = win_h;

    /* ---- シェーダ ---- */
    r->prog = create_program();
    if (!r->prog) { free(r); return NULL; }

    r->u_resolution = glGetUniformLocation(r->prog, "u_resolution");
    r->u_point_size = glGetUniformLocation(r->prog, "u_point_size");

    /* ---- VAO / VBO ---- */
    glGenVertexArrays(1, &r->vao);
    glGenBuffers(1, &r->vbo);

    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);

    /* MAX_PARTICLES 分のバッファをあらかじめ確保 (data=NULL → 予約のみ) */
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(MAX_PARTICLES * sizeof(GpuVertex)),
                 NULL,
                 GL_DYNAMIC_DRAW);

    /* attribute 0: vec2 position (offset 0) */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GpuVertex),
                          (void*)offsetof(GpuVertex, x));

    /* attribute 1: vec4 color (offset 8 bytes) */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          sizeof(GpuVertex),
                          (void*)offsetof(GpuVertex, r));

    glBindVertexArray(0);

    /* ---- ステージングバッファ ---- */
    r->staging = (GpuVertex*)malloc(MAX_PARTICLES * sizeof(GpuVertex));
    if (!r->staging) {
        fprintf(stderr, "[render] staging malloc failed\n");
        free(r);
        return NULL;
    }

    /* ---- OpenGL 基本設定 ---- */
#ifndef __EMSCRIPTEN__
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

    /* 加算合成: 重なるほど明るく = 発光エフェクト */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glDisable(GL_DEPTH_TEST);

    glViewport(0, 0, win_w, win_h);
    glClearColor(0.02f, 0.02f, 0.06f, 1.0f); /* ほぼ黒の深夜ブルー */

    fprintf(stdout, "[render] OpenGL %s | GLSL %s\n",
            glGetString(GL_VERSION),
            glGetString(GL_SHADING_LANGUAGE_VERSION));

    return r;
}

/* ------------------------------------------------------------------ */
/*  render_frame                                                        */
/* ------------------------------------------------------------------ */
void render_frame(Renderer *r,
                  const ParticleSystem *ps,
                  float fps,
                  InteractMode mode,
                  const SimParams *params)
{
    /* ベンチマーク計測: リングに空きがあれば GPU 計測を始める */
    int q = -1;
    if (r->prof && r->prof_gpu && r->prof_pending < PROF_QUERIES) {
        q = (r->prof_head + r->prof_pending) % PROF_QUERIES;
        r->prof_q_tag[q] = r->prof_tag;
        glBeginQuery(GL_TIME_ELAPSED, r->prof_q[q]);
    }
    double t0 = r->prof ? bench_now_ms() : 0.0;

    glClear(GL_COLOR_BUFFER_BIT);

    /* ---- CPU→GPU 転送 ---- */
    int n = ps_pack_gpu(ps, r->staging, MAX_PARTICLES);
    double t1 = r->prof ? bench_now_ms() : 0.0;

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    /* glBufferSubData: リアロケなし、既存バッファへの部分更新 */
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(n * sizeof(GpuVertex)),
                    r->staging);
    double t2 = r->prof ? bench_now_ms() : 0.0;

    /* ---- 描画 ---- */
    glUseProgram(r->prog);
    glUniform2f(r->u_resolution, (float)r->win_w, (float)r->win_h);
    glUniform1f(r->u_point_size, params->point_size);

    glBindVertexArray(r->vao);
    glDrawArrays(GL_POINTS, 0, n);       /* 1回の API コールで全粒子描画 */
    glBindVertexArray(0);

    if (r->prof) {
        double t3 = bench_now_ms();
        r->prof_cpu.pack_ms   = t1 - t0;
        r->prof_cpu.upload_ms = t2 - t1;
        r->prof_cpu.draw_ms   = t3 - t2;
        if (q >= 0) {
            glEndQuery(GL_TIME_ELAPSED);
            r->prof_pending++;
        }
    }

    (void)fps;
    (void)mode;
}

/* ------------------------------------------------------------------ */
/*  render_resize                                                       */
/* ------------------------------------------------------------------ */
void render_resize(Renderer *r, int win_w, int win_h)
{
    r->win_w = win_w;
    r->win_h = win_h;
    glViewport(0, 0, win_w, win_h);
}

/* ------------------------------------------------------------------ */
/*  render_shutdown                                                     */
/* ------------------------------------------------------------------ */
void render_shutdown(Renderer *r)
{
    if (!r) return;
    glDeleteVertexArrays(1, &r->vao);
    glDeleteBuffers(1, &r->vbo);
    glDeleteProgram(r->prog);
    if (r->prof_gpu) glDeleteQueries(PROF_QUERIES, r->prof_q);
    free(r->staging);
    free(r);
}

/* ------------------------------------------------------------------ */
/*  ベンチマーク用の計測                                                 */
/* ------------------------------------------------------------------ */
int render_profile_enable(Renderer *r)
{
    r->prof = 1;
#ifdef __EMSCRIPTEN__
    /* WebGL2 では拡張を有効にできたときだけ使える (Safari などは非対応) */
    r->prof_gpu = emscripten_webgl_enable_extension(
        emscripten_webgl_get_current_context(),
        "EXT_disjoint_timer_query_webgl2") ? 1 : 0;
#else
    /* GL 3.3 Core ではタイマークエリは標準機能 */
    r->prof_gpu = (GLEW_VERSION_3_3 || GLEW_ARB_timer_query) ? 1 : 0;
#endif
    if (r->prof_gpu) {
        glGenQueries(PROF_QUERIES, r->prof_q);
        /* 生成直後のエラーなどで使えないなら諦める */
        if (glGetError() != GL_NO_ERROR) r->prof_gpu = 0;
    }
    r->prof_head    = 0;
    r->prof_pending = 0;
    return r->prof_gpu;
}

void render_profile_set_tag(Renderer *r, int tag)
{
    r->prof_tag = tag;
}

void render_profile_cpu(const Renderer *r, RenderCpuTimes *out)
{
    *out = r->prof_cpu;
}

int render_profile_poll_gpu(Renderer *r, int wait, int *tag, double *ms)
{
    if (!r->prof_gpu || r->prof_pending == 0) return 0;

    GLuint id = r->prof_q[r->prof_head];
    GLuint available = 0;
    glGetQueryObjectuiv(id, GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available && !wait) return 0;

    double ns;
#ifdef __EMSCRIPTEN__
    /* WebGL では 32 bit で受ける (4 秒までなので十分) */
    GLuint v = 0;
    glGetQueryObjectuiv(id, GL_QUERY_RESULT, &v);
    ns = (double)v;
    /* 計測中に GPU の状態が飛んだ (disjoint) なら値は信用できない */
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    if (disjoint) ns = -1.0;
#else
    GLuint64 v = 0;
    glGetQueryObjectui64v(id, GL_QUERY_RESULT, &v);
    ns = (double)v;
#endif

    *tag = r->prof_q_tag[r->prof_head];
    *ms  = ns >= 0.0 ? ns * 1e-6 : -1.0;
    r->prof_head = (r->prof_head + 1) % PROF_QUERIES;
    r->prof_pending--;
    return 1;
}
