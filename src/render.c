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
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  GLSL シェーダソース (文字列埋め込み)                                 */
/* ------------------------------------------------------------------ */
static const char *VERT_SRC =
    "#version 330 core\n"
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
    "#version 330 core\n"
    "in  vec4 v_color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec2  c    = gl_PointCoord - 0.5;\n"
    "    float dist = dot(c, c);\n"
    "    if (dist > 0.25) discard;\n"       /* 円形クリッピング */
    "    float fade = 1.0 - dist * 4.0;\n"  /* 中心が明るいグラデーション */
    "    frag_color = vec4(v_color.rgb * fade, v_color.a * fade);\n"
    "}\n";

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
    /* GLEW 初期化 */
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "[render] glewInit: %s\n", glewGetErrorString(err));
        return NULL;
    }

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
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE); /* gl_PointSize をシェーダで制御 */
    glEnable(GL_POINT_SMOOTH);              /* (compatibility 用フォールバック) */

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
    glClear(GL_COLOR_BUFFER_BIT);

    /* ---- CPU→GPU 転送 ---- */
    int n = ps_pack_gpu(ps, r->staging, MAX_PARTICLES);

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    /* glBufferSubData: リアロケなし、既存バッファへの部分更新 */
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(n * sizeof(GpuVertex)),
                    r->staging);

    /* ---- 描画 ---- */
    glUseProgram(r->prog);
    glUniform2f(r->u_resolution, (float)r->win_w, (float)r->win_h);
    glUniform1f(r->u_point_size, params->point_size);

    glBindVertexArray(r->vao);
    glDrawArrays(GL_POINTS, 0, n);       /* 1回の API コールで全粒子描画 */
    glBindVertexArray(0);

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
    free(r->staging);
    free(r);
}
