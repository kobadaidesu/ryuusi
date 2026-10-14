/*  sim_bench.c  –  SDL / OpenGL なしで ps_update + ps_pack_gpu だけを回すマイクロベンチ
 *
 *  アプリの計測モード (src/bench.c) と同じシナリオ・同じ固定 dt・同じ乱数の種で
 *  物理だけを回す。ウィンドウを開かないので数秒で終わり、ループの書き換えを
 *  すぐに試せる。ネイティブ (make sim-bench) と wasm + Node (make sim-bench-wasm)
 *  の両方でビルドできる。チェックサムはアプリの計測モードと一致するはず。
 *
 *  使い方:
 *    ./bin/sim_bench [--label=name] [--runs=N] [--out=path.json]
 *    node bin/sim_bench.js [同上]
 */

#include "bench_core.h"
#include "particle.h"
#include "sim_params.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include("build_info.h")
#    include "build_info.h"
#  endif
#endif
#ifndef BUILD_SHA
#  define BUILD_SHA   "unknown"
#  define BUILD_DIRTY 0
#endif

/* app.c と同じ */
#define RIPPLE_SPEED 700.0f

/* app.c の main_loop_iter と同じ順で 1 フレーム進める。ms[] に段階ごとの時間 */
static void step(ParticleSystem *ps, GpuVertex *staging, const SimParams *params,
                 RippleWave *ripples, int *ripple_count, const BenchInput *in,
                 double *update_ms, double *pack_ms)
{
    const float dt = BENCH_DT;

    if (in->do_ripple && *ripple_count < MAX_RIPPLES) {
        ripples[*ripple_count].cx         = in->mouse_x;
        ripples[*ripple_count].cy         = in->mouse_y;
        ripples[*ripple_count].radius     = 0.0f;
        ripples[*ripple_count].max_radius = 1500.0f;
        (*ripple_count)++;
    }
    for (int ri = 0; ri < *ripple_count; ) {
        ripples[ri].radius += RIPPLE_SPEED * dt;
        if (ripples[ri].radius > ripples[ri].max_radius)
            ripples[ri] = ripples[--(*ripple_count)];
        else
            ri++;
    }

    double t0 = bench_now_ms();
    ps_update(ps, dt, in->mouse_x, in->mouse_y, in->mode, in->mode_active,
              in->timestop_on, in->gather_on, ripples, *ripple_count, params);
    double t1 = bench_now_ms();
    ps_pack_gpu(ps, staging, MAX_PARTICLES);
    double t2 = bench_now_ms();

    *update_ms = t1 - t0;
    *pack_ms   = t2 - t1;
}

int main(int argc, char **argv)
{
    const char *label = "sim";
    const char *out   = NULL;
    int runs = 1;
    for (int i = 1; i < argc; i++) {
        if      (strncmp(argv[i], "--label=", 8) == 0) label = argv[i] + 8;
        else if (strncmp(argv[i], "--out=", 6) == 0)   out   = argv[i] + 6;
        else if (strncmp(argv[i], "--runs=", 7) == 0)  runs  = atoi(argv[i] + 7);
        else { fprintf(stderr, "unknown option: %s\n", argv[i]); return 1; }
    }
    if (runs < 1) runs = 1;

    ParticleSystem *ps = (ParticleSystem*)malloc(sizeof(ParticleSystem));
    GpuVertex *staging = (GpuVertex*)malloc(MAX_PARTICLES * sizeof(GpuVertex));
    BenchData *data    = bench_data_new(runs);
    if (!ps || !staging || !data) { fprintf(stderr, "out of memory\n"); return 1; }

    SimParams params = SIM_PARAMS_DEFAULT;
    params.particle_count = BENCH_PARTICLES;

    for (int run = 0; run < runs; run++) {
        /* app.c の bench_reset_state と同じ初期化 */
        RippleWave ripples[MAX_RIPPLES];
        int ripple_count = 0;
        ps_init(ps, BENCH_W, BENCH_H, &params);
        ps_seed(BENCH_SEED);
        ps_reset(ps, &params);

        BenchInput in;
        double upd, pack;
        for (int f = 0; f < BENCH_WARMUP; f++) {
            bench_scenario_input(-1, f, &in);
            step(ps, staging, &params, ripples, &ripple_count, &in, &upd, &pack);
        }

        for (int sc = 0; sc < bench_scenario_count(); sc++) {
            const BenchScenario *s = bench_scenario(sc);
            double total = 0.0;
            for (int f = 0; f < s->frames; f++) {
                bench_scenario_input(sc, f, &in);
                step(ps, staging, &params, ripples, &ripple_count, &in, &upd, &pack);
                bench_data_set(data, run, sc, BS_UPDATE, f, upd);
                bench_data_set(data, run, sc, BS_PACK,   f, pack);
                bench_data_set(data, run, sc, BS_FRAME,  f, upd + pack);
                total += upd + pack;
            }
            BenchChecksum cs;
            bench_checksum(ps, &cs);
            bench_data_set_checksum(data, run, sc, &cs);
            /* fps は物理 + パックだけの処理能力 (描画を含まない) */
            bench_data_set_wall(data, run, sc, total * 1e-3);
        }
    }

    BenchMeta meta;
    meta.label      = label;
#ifdef __EMSCRIPTEN__
    meta.platform   = "wasm-sim";
#else
    meta.platform   = "native-sim";
#endif
    meta.build      = BUILD_SHA;
    meta.dirty      = BUILD_DIRTY;
    meta.width      = BENCH_W;
    meta.height     = BENCH_H;
    meta.gpu_timer  = 0;
    meta.note       = "sim_bench: ps_update + ps_pack_gpu のみ (描画なし)。frame = update + pack、fps はその処理能力";
    meta.stage_mask = BENCH_STAGE_BIT(BS_UPDATE) | BENCH_STAGE_BIT(BS_PACK)
                    | BENCH_STAGE_BIT(BS_FRAME);

    char *md = bench_data_markdown(data, &meta);
    printf("%s\n", md ? md : "");
    free(md);

    if (out) {
        char *json = bench_data_json(data, &meta);
        FILE *f = fopen(out, "w");
        if (!f || !json) { fprintf(stderr, "cannot write %s\n", out); free(json); return 1; }
        fputs(json, f);
        fclose(f);
        free(json);
        printf("wrote %s\n", out);
    }

    bench_data_free(data);
    free(staging);
    free(ps);
    return 0;
}
