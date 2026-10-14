#ifndef BENCH_CORE_H
#define BENCH_CORE_H

/* ------------------------------------------------------------------ *
 *  ベンチマークの共通部分 (SDL / OpenGL に依存しない)
 *
 *  アプリの計測モード (bench.c) と、SDL なしで物理だけを回す
 *  マイクロベンチ (bench/sim_bench.c) の両方から使う。
 *   - シナリオの定義と、フレームごとの入力
 *   - 位置のチェックサム
 *   - 段階ごとの所要時間の蓄積、p50 / p95 の集計、JSON / Markdown 出力
 * ------------------------------------------------------------------ */

#include "particle.h"

/* 再現性のための固定値 */
#define BENCH_W          1280
#define BENCH_H           720
#define BENCH_DT         (1.0f / 60.0f)  /* 実時間を使わず常にこの dt */
#define BENCH_SEED       12345678u       /* 計測開始時の乱数の種 */
#define BENCH_PARTICLES  300000
#define BENCH_WARMUP     60              /* 各回の最初に捨てるフレーム数 */
#define BENCH_MAX_FRAMES 300             /* 1 シナリオの最大フレーム数 */
#define BENCH_EARLY_FRAME 10             /* 早期チェックサムを取るフレーム数 (カオスで誤差が育つ前) */

/* 計測する段階 */
typedef enum {
    BS_UPDATE = 0,  /* ps_update                         */
    BS_PACK,        /* ps_pack_gpu                       */
    BS_UPLOAD,      /* glBufferSubData (CPU 側)          */
    BS_DRAW,        /* glDrawArrays の呼び出し (CPU 側)   */
    BS_GUI,         /* gui_render                        */
    BS_PRESENT,     /* SDL_GL_SwapWindow                 */
    BS_FRAME,       /* フレーム全体 (CPU 側の仕事の合計)   */
    BS_GPU,         /* GPU タイマークエリ                 */
    BS_COUNT
} BenchStage;

#define BENCH_STAGE_BIT(s) (1u << (s))

extern const char *const bench_stage_names[BS_COUNT];

/* 計測用の時刻 (ms)。ブラウザ版 SDL の SDL_GetPerformanceCounter は 1 ms 刻みなので、
 * 段階ごとの計測にはこちらを使う (ブラウザでは performance.now()) */
double bench_now_ms(void);

/* シナリオ */
typedef struct {
    const char *name;
    int         frames;
} BenchScenario;

int                  bench_scenario_count(void);
const BenchScenario *bench_scenario(int index);

/* InputState 相当の値。シナリオがフレームごとに直接与える */
typedef struct {
    float        mouse_x, mouse_y;
    InteractMode mode;
    int          mode_active;
    int          gather_on;
    int          timestop_on;
    int          do_ripple;
} BenchInput;

/* scenario = -1 はウォームアップ (idle と同じ入力) */
void bench_scenario_input(int scenario, int frame, BenchInput *out);

/* 位置のチェックサム: 全粒子の x, y, vx, vy を double で合計。
 * 各シナリオの BENCH_EARLY_FRAME フレーム後 (早期) と終了時の 2 回取る */
typedef struct {
    int    count;
    double sum_x, sum_y, sum_vx, sum_vy;
} BenchChecksum;

void bench_checksum(const ParticleSystem *ps, BenchChecksum *out);

/* ------------------------------------------------------------------ *
 *  計測値の蓄積と出力
 * ------------------------------------------------------------------ */
typedef struct BenchData BenchData;

typedef struct {
    const char *label;
    const char *platform;    /* "native" / "wasm" / "native-sim" など */
    const char *build;       /* git short sha                          */
    int         dirty;
    int         width, height;
    int         gpu_timer;   /* GPU タイマーが使えたか                   */
    const char *note;        /* 補足 (NULL 可)                          */
    unsigned    stage_mask;  /* 出力する段階 (BENCH_STAGE_BIT の OR)     */
    int         cull_limit;  /* 飽和カリングの上限 (0 = 無効)            */
    const char *gpu_renderer; /* GPU の名前 (NULL 可)。ブラウザでは WebGL の
                               * UNMASKED_RENDERER。SwiftShader ならソフトウェア描画 */
} BenchMeta;

BenchData *bench_data_new(int runs);
void       bench_data_free(BenchData *d);
int        bench_data_runs(const BenchData *d);

/* ms に負の値を渡すと欠測 (GPU の結果が取れなかったなど)。
 * -ffast-math では NaN の判定が消えるので NaN は使わない */
#define BENCH_MISSING (-1.0)
void bench_data_set(BenchData *d, int run, int scenario, BenchStage stage,
                    int frame, double ms);
void bench_data_set_wall(BenchData *d, int run, int scenario, double seconds);
void bench_data_set_checksum(BenchData *d, int run, int scenario,
                             const BenchChecksum *cs);
void bench_data_set_checksum_early(BenchData *d, int run, int scenario,
                                   const BenchChecksum *cs);

/* 複数回の結果は「回ごとの p50 / p95 の中央値」にまとめる。
 * 戻り値は malloc した文字列 (呼び出し側で free) */
char *bench_data_json(const BenchData *d, const BenchMeta *meta);
char *bench_data_markdown(const BenchData *d, const BenchMeta *meta);

#endif /* BENCH_CORE_H */
