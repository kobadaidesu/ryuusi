/*  bench.c  –  計測モード (シナリオの進行 / 段階ごとの記録 / 結果の出力)
 *
 *  進み方 (1 回分):
 *    ウォームアップ BENCH_WARMUP フレーム (捨てる) → シナリオ 0..N-1
 *  を --bench-runs 回くり返す。各回の頭で app.c に状態の初期化を頼む
 *  (BENCH_FRAME_RESET)。GPU の計測結果は数フレーム遅れて届くので、
 *  フレームごとの番号 (tag) で元のフレームに書き戻す。
 */

#include "bench.h"
#include "gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif

/* make が生成する (git の short sha と、src などに未コミットの変更があるか) */
#if defined(__has_include)
#  if __has_include("build_info.h")
#    include "build_info.h"
#  endif
#endif
#ifndef BUILD_SHA
#  define BUILD_SHA   "unknown"
#  define BUILD_DIRTY 0
#endif

/* ------------------------------------------------------------------ */
/*  設定と進行状態                                                      */
/* ------------------------------------------------------------------ */
static struct {
    int         enabled;
    int         runs;
    const char *out_path;
    const char *label;
    const char *report_url;
} g_cfg = { 0, 1, NULL, "bench", NULL };

static struct {
    Renderer  *renderer;
    BenchData *data;
    int        gpu_timer;
    int        win_w, win_h;

    int        run;            /* 何回目か                          */
    int        scenario;       /* -1 = ウォームアップ               */
    int        frame;          /* シナリオ内のフレーム番号          */
    int        need_reset;     /* 次のフレームで状態を初期化する    */
    int        finished;       /* 全回が終わった                    */
    int        reported;       /* 結果を出力した                    */

    double     scenario_t0;    /* シナリオ開始時刻 ms (FPS 用)      */
    int        wall_run, wall_scenario; /* 実時間を確定待ちのシナリオ (-1 = なし) */

    char       progress[160];
    char      *result_md;      /* 結果の Markdown (GUI に表示)      */
} g_b;

int bench_configure(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--bench") == 0) {
            g_cfg.enabled = 1;
        } else if (strncmp(a, "--bench-out=", 12) == 0) {
            g_cfg.out_path = a + 12;
        } else if (strncmp(a, "--bench-runs=", 13) == 0) {
            g_cfg.runs = atoi(a + 13);
            if (g_cfg.runs < 1) {
                fprintf(stderr, "--bench-runs は 1 以上: %s\n", a);
                return -1;
            }
        } else if (strncmp(a, "--bench-label=", 14) == 0) {
            g_cfg.label = a + 14;
        } else if (strncmp(a, "--bench-report=", 15) == 0) {
            g_cfg.report_url = a + 15;
        } else if (strncmp(a, "--bench", 7) == 0) {
            fprintf(stderr, "unknown option: %s\n", a);
            return -1;
        }
        /* それ以外 (macOS の -psn_ など) は無視 */
    }
    return 0;
}

int bench_enabled(void)
{
    return g_cfg.enabled;
}

void bench_start(Renderer *r, int win_w, int win_h)
{
    memset(&g_b, 0, sizeof(g_b));
    g_b.renderer   = r;
    g_b.data       = bench_data_new(g_cfg.runs);
    g_b.gpu_timer  = render_profile_enable(r);
    g_b.win_w      = win_w;
    g_b.win_h      = win_h;
    g_b.scenario   = -1;
    g_b.need_reset = 1;
    g_b.wall_run   = -1;

    if (!g_b.data) {
        fprintf(stderr, "[bench] out of memory\n");
        g_b.finished = g_b.reported = 1;
        return;
    }
    if (win_w != BENCH_W || win_h != BENCH_H)
        fprintf(stderr, "[bench] warning: window is %dx%d (expected %dx%d)\n",
                win_w, win_h, BENCH_W, BENCH_H);
    printf("[bench] runs=%d gpu_timer=%s\n", g_cfg.runs, g_b.gpu_timer ? "yes" : "no");
}

/* ------------------------------------------------------------------ */
/*  GPU の結果: tag = (run, scenario, frame) を 1 つの整数にしたもの      */
/* ------------------------------------------------------------------ */
static int make_tag(int run, int sc, int frame)
{
    if (sc < 0) return -1; /* ウォームアップ */
    return (run * bench_scenario_count() + sc) * BENCH_MAX_FRAMES + frame;
}

static void store_gpu(int tag, double ms)
{
    if (tag < 0) return;
    int frame = tag % BENCH_MAX_FRAMES;
    int rest  = tag / BENCH_MAX_FRAMES;
    int sc    = rest % bench_scenario_count();
    int run   = rest / bench_scenario_count();
    bench_data_set(g_b.data, run, sc, BS_GPU, frame, ms);
}

static void poll_gpu(int wait)
{
    int tag;
    double ms;
    while (render_profile_poll_gpu(g_b.renderer, wait, &tag, &ms))
        store_gpu(tag, ms);
}

/* ------------------------------------------------------------------ */
/*  結果の出力                                                          */
/* ------------------------------------------------------------------ */
#ifdef __EMSCRIPTEN__
/* 画面 (キャンバスの下) に表と JSON を出し、report があれば POST する。
 * window.benchResult にも JSON を置く */
EM_JS(void, bench_js_publish, (const char *json, const char *md, const char *report), {
    var j = UTF8ToString(json);
    var m = UTF8ToString(md);
    var url = report ? UTF8ToString(report) : "";
    try { window.benchResult = JSON.parse(j); } catch (e) { window.benchResult = j; }
    var pre = document.getElementById("bench-result");
    if (!pre) {
        pre = document.createElement("pre");
        pre.id = "bench-result";
        pre.style.cssText = "margin:12px;font:12px/1.4 ui-monospace,monospace;color:#ddd;white-space:pre-wrap;";
        document.body.appendChild(pre);
    }
    var status = document.createElement("div");
    status.id = "bench-status";
    status.textContent = url ? ("POST " + url + " ...") : "report が無いので POST しない";
    pre.textContent = m + "\n" + j;
    pre.parentNode.insertBefore(status, pre);
    document.title = "BENCH DONE";
    if (url) {
        fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: j })
            .then(function (r) { status.textContent = "POST " + url + " -> " + r.status; })
            .catch(function (e) { status.textContent = "POST " + url + " failed: " + e; });
    }
});
#endif

static void bench_report(void)
{
    poll_gpu(1); /* 計測はもう終わったので待ってよい */

    const char *note;
#ifdef __EMSCRIPTEN__
    note = g_b.gpu_timer
        ? "fps は requestAnimationFrame に縛られる。主指標は段階ごとの ms。present はブラウザでは実質 0"
        : "gpu: EXT_disjoint_timer_query_webgl2 が使えないため推定不可。fps は requestAnimationFrame に縛られる。主指標は段階ごとの ms";
#else
    note = g_b.gpu_timer
        ? "frame はフレーム内の CPU 側の仕事の合計 (present を含む)。fps はフレーム数 / 実時間"
        : "gpu: タイマークエリが使えないため推定不可";
#endif

    BenchMeta meta;
    meta.label      = g_cfg.label;
#ifdef __EMSCRIPTEN__
    meta.platform   = "wasm";
#else
    meta.platform   = "native";
#endif
    meta.build      = BUILD_SHA;
    meta.dirty      = BUILD_DIRTY;
    meta.width      = g_b.win_w;
    meta.height     = g_b.win_h;
    meta.gpu_timer  = g_b.gpu_timer;
    meta.note       = note;
    meta.stage_mask = (1u << BS_COUNT) - 1u;

    char *json = bench_data_json(g_b.data, &meta);
    char *md   = bench_data_markdown(g_b.data, &meta);

    printf("\n%s\n", md ? md : "");
    fflush(stdout);
#ifdef __EMSCRIPTEN__
    bench_js_publish(json ? json : "{}", md ? md : "", g_cfg.report_url);
#else
    if (g_cfg.out_path && json) {
        FILE *f = fopen(g_cfg.out_path, "w");
        if (f) {
            fputs(json, f);
            fclose(f);
            printf("[bench] wrote %s\n", g_cfg.out_path);
        } else {
            fprintf(stderr, "[bench] cannot write %s\n", g_cfg.out_path);
        }
    }
#endif
    free(json);
    g_b.result_md = md;  /* GUI に表示し続けるので解放しない */
    gui_set_bench_text(g_b.result_md);
}

/* ------------------------------------------------------------------ */
/*  フレームの頭 / 終わり                                                */
/* ------------------------------------------------------------------ */
BenchFrameKind bench_begin_frame(BenchInput *in)
{
    double now = bench_now_ms();

    /* 前のフレームで終わったシナリオの実時間を確定 (次のフレームの頭まで = 1 フレーム分の間隔を含む) */
    if (g_b.wall_run >= 0) {
        bench_data_set_wall(g_b.data, g_b.wall_run, g_b.wall_scenario,
                            (now - g_b.scenario_t0) * 1e-3);
        g_b.wall_run = -1;
    }

    if (g_b.finished) {
        if (!g_b.reported) {
            g_b.reported = 1;
            bench_report();
        }
        bench_scenario_input(-1, 0, in);
        return BENCH_FRAME_DONE;
    }

    if (g_b.scenario >= 0 && g_b.frame == 0)
        g_b.scenario_t0 = now;

    bench_scenario_input(g_b.scenario, g_b.frame, in);
    render_profile_set_tag(g_b.renderer, make_tag(g_b.run, g_b.scenario, g_b.frame));

    const char *name = g_b.scenario >= 0 ? bench_scenario(g_b.scenario)->name : "warmup";
    int frames = g_b.scenario >= 0 ? bench_scenario(g_b.scenario)->frames : BENCH_WARMUP;
    snprintf(g_b.progress, sizeof(g_b.progress),
             "BENCH  run %d/%d  %s  %d/%d", g_b.run + 1, g_cfg.runs, name, g_b.frame, frames);
    gui_set_bench_text(g_b.progress);

    if (g_b.need_reset) {
        g_b.need_reset = 0;
        return BENCH_FRAME_RESET;
    }
    return BENCH_FRAME_RUN;
}

void bench_end_frame(const double stage_ms[BS_COUNT], const ParticleSystem *ps)
{
    if (g_b.finished) return;

    poll_gpu(0);

    int sc = g_b.scenario;
    if (sc >= 0) {
        RenderCpuTimes rt;
        render_profile_cpu(g_b.renderer, &rt);
        BenchData *d = g_b.data;
        int f = g_b.frame;
        bench_data_set(d, g_b.run, sc, BS_UPDATE,  f, stage_ms[BS_UPDATE]);
        bench_data_set(d, g_b.run, sc, BS_PACK,    f, rt.pack_ms);
        bench_data_set(d, g_b.run, sc, BS_UPLOAD,  f, rt.upload_ms);
        bench_data_set(d, g_b.run, sc, BS_DRAW,    f, rt.draw_ms);
        bench_data_set(d, g_b.run, sc, BS_GUI,     f, stage_ms[BS_GUI]);
        bench_data_set(d, g_b.run, sc, BS_PRESENT, f, stage_ms[BS_PRESENT]);
        bench_data_set(d, g_b.run, sc, BS_FRAME,   f, stage_ms[BS_FRAME]);
    }

    /* 次のフレームへ */
    int frames = sc >= 0 ? bench_scenario(sc)->frames : BENCH_WARMUP;
    ++g_b.frame;
    if (sc >= 0 && g_b.frame == BENCH_EARLY_FRAME) {
        BenchChecksum cs;
        bench_checksum(ps, &cs);
        bench_data_set_checksum_early(g_b.data, g_b.run, sc, &cs);
    }
    if (g_b.frame < frames) return;

    if (sc >= 0) {
        BenchChecksum cs;
        bench_checksum(ps, &cs);
        bench_data_set_checksum(g_b.data, g_b.run, sc, &cs);
        g_b.wall_run      = g_b.run;
        g_b.wall_scenario = sc;
    }
    g_b.frame = 0;
    g_b.scenario++;
    if (g_b.scenario >= bench_scenario_count()) {
        g_b.scenario   = -1;
        g_b.need_reset = 1;
        if (++g_b.run >= g_cfg.runs)
            g_b.finished = 1;
    }
}
