/*  bench_core.c  –  ベンチマークの共通部分 (シナリオ / チェックサム / 集計 / 出力)
 *
 *  SDL / OpenGL には依存しない。アプリの計測モード (bench.c) と
 *  SDL なしのマイクロベンチ (bench/sim_bench.c) の両方から使う。
 */

/* clock_gettime を -std=c99 で使うため */
#define _POSIX_C_SOURCE 200112L

#include "bench_core.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#elif defined(_WIN32)
#  include <windows.h>
#else
#  include <time.h>
#endif

const char *const bench_stage_names[BS_COUNT] = {
    "update", "pack", "upload", "draw", "gui", "present", "frame", "gpu"
};

double bench_now_ms(void)
{
#if defined(__EMSCRIPTEN__)
    return emscripten_get_now();
#elif defined(_WIN32)
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
#endif
}

/* ------------------------------------------------------------------ */
/*  シナリオ                                                            */
/*  状態は前のシナリオから引き継ぐ (gather_release は gather_hold の直後) */
/* ------------------------------------------------------------------ */
enum {
    SC_IDLE = 0,
    SC_ATTRACT,
    SC_GATHER_HOLD,
    SC_GATHER_RELEASE,
    SC_GATHER_RELEASE_SPREAD,
    SC_VORTEX,
    SC_EXPLODE,
    SC_RIPPLE,
    SC_COUNT
};

static const BenchScenario g_scenarios[SC_COUNT] = {
    { "idle",                  300 },
    { "attract",               300 },
    { "gather_hold",           180 },
    { "gather_release",         60 },
    { "gather_release_spread", 120 },
    { "vortex",                300 },
    { "explode",                60 },
    { "ripple",                300 },
};

int bench_scenario_count(void)
{
    return SC_COUNT;
}

const BenchScenario *bench_scenario(int index)
{
    if (index < 0 || index >= SC_COUNT) return NULL;
    return &g_scenarios[index];
}

void bench_scenario_input(int scenario, int frame, BenchInput *out)
{
    memset(out, 0, sizeof(*out));
    /* カーソルは常に中央 (常時カーソル引力もここへ向かう) */
    out->mouse_x = BENCH_W * 0.5f;
    out->mouse_y = BENCH_H * 0.5f;
    out->mode    = MODE_NONE;

    switch (scenario) {
        case SC_ATTRACT:            /* 左クリック引力 */
            out->mode        = MODE_ATTRACT;
            out->mode_active = 1;
            break;
        case SC_GATHER_HOLD:        /* G を押し続ける */
            out->gather_on = 1;
            break;
        case SC_GATHER_RELEASE_SPREAD:
            /* 中央のままだと全粒子が 1 点に重なって力がゼロになり広がらない。
             * 開始時にカーソルを右へ動かして広がりを測る */
            out->mouse_x = 900.0f;
            break;
        case SC_VORTEX:             /* V を押し続ける */
            out->mode        = MODE_VORTEX;
            out->mode_active = 1;
            break;
        case SC_EXPLODE:            /* E を 1 回 (最初の 1 フレームだけ押す) */
            if (frame == 0) {
                out->mode        = MODE_EXPLODE;
                out->mode_active = 1;
            }
            break;
        case SC_RIPPLE:             /* B を 30 フレームごとに */
            out->do_ripple = (frame % 30 == 0);
            break;
        default:                    /* idle / ウォームアップ / G を離した直後 (中央のまま = 1 点に重なった最悪ケース) */
            break;
    }
}

/* ------------------------------------------------------------------ */
/*  チェックサム                                                        */
/* ------------------------------------------------------------------ */
void bench_checksum(const ParticleSystem *ps, BenchChecksum *out)
{
    double sx = 0.0, sy = 0.0, svx = 0.0, svy = 0.0;
    for (int i = 0; i < ps->count; i++) {
        sx  += (double)ps->x[i];
        sy  += (double)ps->y[i];
        svx += (double)ps->vx[i];
        svy += (double)ps->vy[i];
    }
    out->count  = ps->count;
    out->sum_x  = sx;
    out->sum_y  = sy;
    out->sum_vx = svx;
    out->sum_vy = svy;
}

/* ------------------------------------------------------------------ */
/*  計測値の蓄積                                                        */
/* ------------------------------------------------------------------ */
struct BenchData {
    int            runs;
    double        *ms;        /* [run][scenario][stage][frame]  */
    double        *wall;      /* [run][scenario] 秒            */
    BenchChecksum *checksum;  /* [run][scenario] 終了時        */
    BenchChecksum *early;     /* [run][scenario] BENCH_EARLY_FRAME フレーム後 */
};

static size_t ms_index(int run, int sc, int stage, int frame)
{
    return (((size_t)run * SC_COUNT + (size_t)sc) * BS_COUNT + (size_t)stage)
           * BENCH_MAX_FRAMES + (size_t)frame;
}

BenchData *bench_data_new(int runs)
{
    if (runs < 1) runs = 1;
    BenchData *d = (BenchData*)calloc(1, sizeof(BenchData));
    if (!d) return NULL;
    size_t n_ms = (size_t)runs * SC_COUNT * BS_COUNT * BENCH_MAX_FRAMES;
    d->runs     = runs;
    d->ms       = (double*)malloc(n_ms * sizeof(double));
    d->wall     = (double*)calloc((size_t)runs * SC_COUNT, sizeof(double));
    d->checksum = (BenchChecksum*)calloc((size_t)runs * SC_COUNT, sizeof(BenchChecksum));
    d->early    = (BenchChecksum*)calloc((size_t)runs * SC_COUNT, sizeof(BenchChecksum));
    if (!d->ms || !d->wall || !d->checksum || !d->early) {
        bench_data_free(d);
        return NULL;
    }
    for (size_t i = 0; i < n_ms; i++) d->ms[i] = BENCH_MISSING;
    return d;
}

void bench_data_free(BenchData *d)
{
    if (!d) return;
    free(d->ms);
    free(d->wall);
    free(d->checksum);
    free(d->early);
    free(d);
}

int bench_data_runs(const BenchData *d)
{
    return d->runs;
}

void bench_data_set(BenchData *d, int run, int scenario, BenchStage stage,
                    int frame, double ms)
{
    if (run < 0 || run >= d->runs || scenario < 0 || scenario >= SC_COUNT) return;
    if ((int)stage < 0 || stage >= BS_COUNT) return;
    if (frame < 0 || frame >= g_scenarios[scenario].frames) return;
    d->ms[ms_index(run, scenario, stage, frame)] = ms;
}

void bench_data_set_wall(BenchData *d, int run, int scenario, double seconds)
{
    if (run < 0 || run >= d->runs || scenario < 0 || scenario >= SC_COUNT) return;
    d->wall[run * SC_COUNT + scenario] = seconds;
}

void bench_data_set_checksum(BenchData *d, int run, int scenario,
                             const BenchChecksum *cs)
{
    if (run < 0 || run >= d->runs || scenario < 0 || scenario >= SC_COUNT) return;
    d->checksum[run * SC_COUNT + scenario] = *cs;
}

void bench_data_set_checksum_early(BenchData *d, int run, int scenario,
                                   const BenchChecksum *cs)
{
    if (run < 0 || run >= d->runs || scenario < 0 || scenario >= SC_COUNT) return;
    d->early[run * SC_COUNT + scenario] = *cs;
}

/* ------------------------------------------------------------------ */
/*  集計                                                                */
/* ------------------------------------------------------------------ */
static int cmp_double(const void *a, const void *b)
{
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}

/* ソート済み配列の分位点 (線形補間、numpy の既定と同じ) */
static double quantile_sorted(const double *v, int n, double q)
{
    if (n <= 0) return BENCH_MISSING;
    double pos = q * (double)(n - 1);
    int    lo  = (int)pos;
    if (lo >= n - 1) return v[n - 1];
    double fr = pos - (double)lo;
    return v[lo] + (v[lo + 1] - v[lo]) * fr;
}

/* 欠測を除いて並べ替え、件数を返す */
static int collect_sorted(const double *src, int n, double *dst)
{
    int m = 0;
    for (int i = 0; i < n; i++)
        if (src[i] >= 0.0) dst[m++] = src[i];
    qsort(dst, (size_t)m, sizeof(double), cmp_double);
    return m;
}

/* シナリオ × 段階の p50 / p95 (回ごとの値の中央値)。値が無ければ 0 を返す */
static int stage_stats(const BenchData *d, int sc, int stage,
                       double *p50, double *p95)
{
    double buf[BENCH_MAX_FRAMES];
    double *r50 = (double*)malloc((size_t)d->runs * sizeof(double));
    double *r95 = (double*)malloc((size_t)d->runs * sizeof(double));
    int frames = g_scenarios[sc].frames;
    int nr = 0;
    for (int run = 0; run < d->runs; run++) {
        int m = collect_sorted(&d->ms[ms_index(run, sc, stage, 0)], frames, buf);
        if (m == 0) continue;
        r50[nr] = quantile_sorted(buf, m, 0.50);
        r95[nr] = quantile_sorted(buf, m, 0.95);
        nr++;
    }
    if (nr > 0) {
        qsort(r50, (size_t)nr, sizeof(double), cmp_double);
        qsort(r95, (size_t)nr, sizeof(double), cmp_double);
        *p50 = quantile_sorted(r50, nr, 0.5);
        *p95 = quantile_sorted(r95, nr, 0.5);
    }
    free(r50);
    free(r95);
    return nr > 0;
}

/* FPS = フレーム数 ÷ 実時間 (回ごとの値の中央値) */
static double scenario_fps(const BenchData *d, int sc)
{
    double *v = (double*)malloc((size_t)d->runs * sizeof(double));
    int n = 0;
    for (int run = 0; run < d->runs; run++) {
        double w = d->wall[run * SC_COUNT + sc];
        if (w > 0.0) v[n++] = (double)g_scenarios[sc].frames / w;
    }
    qsort(v, (size_t)n, sizeof(double), cmp_double);
    double fps = quantile_sorted(v, n, 0.5);
    free(v);
    return fps;
}

static int checksum_equal(const BenchChecksum *a, const BenchChecksum *b)
{
    return a->count == b->count && a->sum_x == b->sum_x && a->sum_y == b->sum_y
        && a->sum_vx == b->sum_vx && a->sum_vy == b->sum_vy;
}

/* 全回でチェックサム (早期・終了時とも) が 1 回目と完全に一致したか */
static int checksum_stable(const BenchData *d, int sc)
{
    for (int run = 1; run < d->runs; run++) {
        if (!checksum_equal(&d->checksum[sc], &d->checksum[run * SC_COUNT + sc])
            || !checksum_equal(&d->early[sc], &d->early[run * SC_COUNT + sc]))
            return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/*  文字列バッファ                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    char  *buf;
    size_t len, cap;
} StrBuf;

static void sb_printf(StrBuf *s, const char *fmt, ...)
{
    for (;;) {
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(s->buf ? s->buf + s->len : NULL,
                          s->buf ? s->cap - s->len : 0, fmt, ap);
        va_end(ap);
        if (n < 0) return;
        if (s->buf && s->len + (size_t)n < s->cap) {
            s->len += (size_t)n;
            return;
        }
        size_t cap = s->cap ? s->cap : 4096;
        while (cap <= s->len + (size_t)n) cap *= 2;
        char *nb = (char*)realloc(s->buf, cap);
        if (!nb) return;
        s->buf = nb;
        s->cap = cap;
    }
}

/* JSON 文字列 (ラベル等は英数字前提なので " と \ だけ逃がす) */
static void sb_json_str(StrBuf *s, const char *str)
{
    sb_printf(s, "\"");
    for (const char *p = str ? str : ""; *p; p++) {
        if (*p == '"' || *p == '\\') sb_printf(s, "\\%c", *p);
        else if ((unsigned char)*p < 0x20) sb_printf(s, " ");
        else sb_printf(s, "%c", *p);
    }
    sb_printf(s, "\"");
}

static int stage_enabled(const BenchMeta *meta, int stage)
{
    return (meta->stage_mask & BENCH_STAGE_BIT(stage)) != 0;
}

/* ------------------------------------------------------------------ */
/*  JSON                                                                */
/* ------------------------------------------------------------------ */
char *bench_data_json(const BenchData *d, const BenchMeta *meta)
{
    StrBuf s = { NULL, 0, 0 };
    sb_printf(&s, "{\n  \"label\": ");
    sb_json_str(&s, meta->label);
    sb_printf(&s, ", \"platform\": ");
    sb_json_str(&s, meta->platform);
    sb_printf(&s, ", \"build\": ");
    sb_json_str(&s, meta->build);
    sb_printf(&s, ", \"dirty\": %s,\n", meta->dirty ? "true" : "false");
    sb_printf(&s, "  \"particles\": %d, \"size\": [%d, %d], \"dt\": %.6f,\n",
              BENCH_PARTICLES, meta->width, meta->height, (double)BENCH_DT);
    sb_printf(&s, "  \"runs\": %d, \"warmup_frames\": %d, \"gpu_timer\": %s,\n",
              d->runs, BENCH_WARMUP, meta->gpu_timer ? "true" : "false");
    sb_printf(&s, "  \"cull_limit\": %d,\n", meta->cull_limit);
    if (meta->gpu_renderer) {
        sb_printf(&s, "  \"gpu_renderer\": ");
        sb_json_str(&s, meta->gpu_renderer);
        sb_printf(&s, ",\n");
    }
    if (meta->note) {
        sb_printf(&s, "  \"note\": ");
        sb_json_str(&s, meta->note);
        sb_printf(&s, ",\n");
    }
    sb_printf(&s, "  \"scenarios\": [\n");

    for (int sc = 0; sc < SC_COUNT; sc++) {
        double fps = scenario_fps(d, sc);
        sb_printf(&s, "    { \"name\": \"%s\", \"frames\": %d, \"fps\": ",
                  g_scenarios[sc].name, g_scenarios[sc].frames);
        if (fps >= 0.0) sb_printf(&s, "%.2f", fps);
        else            sb_printf(&s, "null");
        sb_printf(&s, ",\n      \"ms\": {");

        int first = 1;
        for (int st = 0; st < BS_COUNT; st++) {
            if (!stage_enabled(meta, st)) continue;
            double p50, p95;
            sb_printf(&s, "%s\n        \"%s\": ", first ? "" : ",", bench_stage_names[st]);
            first = 0;
            if (stage_stats(d, sc, st, &p50, &p95))
                sb_printf(&s, "{ \"p50\": %.4f, \"p95\": %.4f }", p50, p95);
            else
                sb_printf(&s, "null");
        }
        sb_printf(&s, "\n      },\n");

        const BenchChecksum *ce = &d->early[sc];
        sb_printf(&s, "      \"checksum_early\": { \"frame\": %d, \"count\": %d, \"sum_x\": %.10e, "
                      "\"sum_y\": %.10e, \"sum_vx\": %.10e, \"sum_vy\": %.10e },\n",
                  BENCH_EARLY_FRAME, ce->count, ce->sum_x, ce->sum_y, ce->sum_vx, ce->sum_vy);
        const BenchChecksum *cs = &d->checksum[sc];
        sb_printf(&s, "      \"checksum\": { \"count\": %d, \"sum_x\": %.10e, "
                      "\"sum_y\": %.10e, \"sum_vx\": %.10e, \"sum_vy\": %.10e },\n",
                  cs->count, cs->sum_x, cs->sum_y, cs->sum_vx, cs->sum_vy);
        sb_printf(&s, "      \"checksum_stable\": %s }%s\n",
                  checksum_stable(d, sc) ? "true" : "false",
                  sc + 1 < SC_COUNT ? "," : "");
    }
    sb_printf(&s, "  ]\n}\n");
    return s.buf;
}

/* ------------------------------------------------------------------ */
/*  Markdown                                                            */
/* ------------------------------------------------------------------ */
char *bench_data_markdown(const BenchData *d, const BenchMeta *meta)
{
    StrBuf s = { NULL, 0, 0 };
    sb_printf(&s, "### %s (%s, build %s%s, %d run%s, %dx%d, cull %d)\n\n",
              meta->label, meta->platform, meta->build, meta->dirty ? "-dirty" : "",
              d->runs, d->runs > 1 ? "s" : "", meta->width, meta->height, meta->cull_limit);
    if (meta->gpu_renderer)
        sb_printf(&s, "GPU: %s\n\n", meta->gpu_renderer);
    sb_printf(&s, "ms は p50 / p95%s\n\n",
              meta->gpu_timer || !stage_enabled(meta, BS_GPU) ? ""
                                                             : "。gpu はタイマーが使えず推定不可");

    sb_printf(&s, "| scenario | fps |");
    for (int st = 0; st < BS_COUNT; st++)
        if (stage_enabled(meta, st)) sb_printf(&s, " %s |", bench_stage_names[st]);
    sb_printf(&s, "\n|---|---:|");
    for (int st = 0; st < BS_COUNT; st++)
        if (stage_enabled(meta, st)) sb_printf(&s, "---:|");
    sb_printf(&s, "\n");

    for (int sc = 0; sc < SC_COUNT; sc++) {
        double fps = scenario_fps(d, sc);
        sb_printf(&s, "| %s | ", g_scenarios[sc].name);
        if (fps >= 0.0) sb_printf(&s, "%.1f |", fps);
        else            sb_printf(&s, "- |");
        for (int st = 0; st < BS_COUNT; st++) {
            if (!stage_enabled(meta, st)) continue;
            double p50, p95;
            if (stage_stats(d, sc, st, &p50, &p95))
                sb_printf(&s, " %.2f / %.2f |", p50, p95);
            else
                sb_printf(&s, " - |");
        }
        sb_printf(&s, "\n");
    }

    sb_printf(&s, "\nチェックサム (早期 = 開始から %d フレーム後、終了 = シナリオの最後)\n\n"
                  "| scenario | count | early sum_x | early sum_y | end sum_x | end sum_y | end sum_vx | end sum_vy | stable |\n"
                  "|---|---:|---:|---:|---:|---:|---:|---:|---|\n", BENCH_EARLY_FRAME);
    for (int sc = 0; sc < SC_COUNT; sc++) {
        const BenchChecksum *ce = &d->early[sc];
        const BenchChecksum *cs = &d->checksum[sc];
        sb_printf(&s, "| %s | %d | %.6e | %.6e | %.6e | %.6e | %.6e | %.6e | %s |\n",
                  g_scenarios[sc].name, cs->count, ce->sum_x, ce->sum_y,
                  cs->sum_x, cs->sum_y, cs->sum_vx, cs->sum_vy,
                  checksum_stable(d, sc) ? "yes" : "NO");
    }
    return s.buf;
}
