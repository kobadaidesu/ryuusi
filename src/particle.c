#include "particle.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  内部ユーティリティ                                                  */
/* ------------------------------------------------------------------ */

/* 高速 xorshift32 乱数 (stdlib rand より 3 倍以上速い) */
static uint32_t g_seed = 12345678u;
static inline float frand(void)
{
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return (float)(g_seed & 0x7FFFFFFFu) * (1.0f / 2147483648.0f);
}
static inline float frand_range(float lo, float hi)
{
    return lo + frand() * (hi - lo);
}

/* HSV -> RGB (h:0..1, s:0..1, v:0..1) */
static inline void hsv2rgb(float h, float s, float v,
                            float *r, float *g, float *b)
{
    if (s <= 0.0f) { *r = *g = *b = v; return; }
    float hh = h * 6.0f;
    int   i  = (int)hh;
    float ff = hh - (float)i;
    float p  = v * (1.0f - s);
    float q  = v * (1.0f - s * ff);
    float t  = v * (1.0f - s * (1.0f - ff));
    switch (i % 6) {
        case 0: *r=v; *g=t; *b=p; break;
        case 1: *r=q; *g=v; *b=p; break;
        case 2: *r=p; *g=v; *b=t; break;
        case 3: *r=p; *g=q; *b=v; break;
        case 4: *r=t; *g=p; *b=v; break;
        default:*r=v; *g=p; *b=q; break;
    }
}

/* ------------------------------------------------------------------ */
/*  ps_init                                                             */
/* ------------------------------------------------------------------ */
void ps_init(ParticleSystem *ps, int win_w, int win_h, const SimParams *params)
{
    memset(ps, 0, sizeof(*ps));
    ps->win_w = win_w;
    ps->win_h = win_h;

    float bw = (params->sim_bounds_w > 0.0f) ? params->sim_bounds_w : (float)win_w;
    float bh = (params->sim_bounds_h > 0.0f) ? params->sim_bounds_h : (float)win_h;
    float bx = ((float)win_w - bw) * 0.5f;
    float by = ((float)win_h - bh) * 0.5f;

    int init_count = params->particle_count;
    if (init_count < 1) init_count = 1;
    if (init_count > MAX_PARTICLES) init_count = MAX_PARTICLES;
    for (int i = 0; i < init_count; i++) {
        ps->x[i]    = bx + frand() * bw;
        ps->y[i]    = by + frand() * bh;
        ps->vx[i]   = frand_range(-50.0f, 50.0f);
        ps->vy[i]   = frand_range(-50.0f, 50.0f);
        ps->life[i] = 1.0f;
        ps->hue[i]  = frand_range(params->hue_min, params->hue_max);
        ps->mass[i] = frand_range(0.5f, 2.0f);
        ps->count++;
    }
}

/* ------------------------------------------------------------------ */
/*  ps_spawn  –  count 個の粒子を (cx,cy) 付近に生成                   */
/* ------------------------------------------------------------------ */
void ps_spawn(ParticleSystem *ps, float cx, float cy, int count,
              const SimParams *params)
{
    int added = 0;
    int cap = params->particle_count;
    if (cap < 1) cap = 1;
    if (cap > MAX_PARTICLES) cap = MAX_PARTICLES;
    while (added < count && ps->count < cap) {
        int i = ps->count;

        float angle = frand() * 6.2831853f;
        float speed = frand_range(20.0f, 200.0f);
        float radius= frand_range(0.0f, 40.0f);

        ps->x[i]    = cx + cosf(angle) * radius;
        ps->y[i]    = cy + sinf(angle) * radius;
        ps->vx[i]   = cosf(angle) * speed * frand();
        ps->vy[i]   = sinf(angle) * speed * frand() - 30.0f;
        ps->life[i] = 1.0f;
        ps->hue[i]  = frand_range(params->hue_min, params->hue_max);
        ps->mass[i] = frand_range(0.5f, 2.0f);

        ps->count++;
        added++;
    }
}

/* ------------------------------------------------------------------ */
/*  ps_update  –  メインループから毎フレーム呼ぶ                        */
/* ------------------------------------------------------------------ */
void ps_update(ParticleSystem *ps,
               float dt,
               float mx, float my,
               InteractMode mode,
               int   mode_active,
               int   timestop_on,
               int   gather_on,
               const RippleWave *ripples,
               int   ripple_count,
               const SimParams *params)
{
    const float GRAVITY      = params->gravity;
    const float DAMPING      = params->damping;
    const float ATTRACT_STR  = params->attract_str;
    const float TIMESTOP_STR = params->timestop_str;
    const float REPEL_STR    = params->repel_str;
    const float VORTEX_STR   = params->vortex_str;
    const float EXPLODE_STR  = params->explode_str;
    const float LIFE_DECAY   = 0.0f;
    const float WIN_W = (float)ps->win_w;
    const float WIN_H = (float)ps->win_h;

    /* シミュレーション境界 (0 = ウィンドウ全体) */
    const float BW = (params->sim_bounds_w > 0.0f) ? params->sim_bounds_w : WIN_W;
    const float BH = (params->sim_bounds_h > 0.0f) ? params->sim_bounds_h : WIN_H;
    const float BX = (WIN_W - BW) * 0.5f;
    const float BY = (WIN_H - BH) * 0.5f;

    int i = 0;
    while (i < ps->count) {
        /* ---- 寿命 ---- */
        ps->life[i] -= LIFE_DECAY * dt;
        if (ps->life[i] <= 0.0f) {
            /* 末尾と入れ替えて即削除 O(1) */
            int last = ps->count - 1;
            ps->x[i]    = ps->x[last];
            ps->y[i]    = ps->y[last];
            ps->vx[i]   = ps->vx[last];
            ps->vy[i]   = ps->vy[last];
            ps->life[i] = ps->life[last];
            ps->hue[i]  = ps->hue[last];
            ps->mass[i] = ps->mass[last];
            ps->count--;
            continue; /* 同インデックスを再処理 */
        }

        /* ---- 重力 ---- */
        ps->vy[i] += GRAVITY * dt;

        /* ---- 風 (定常加速度) ---- */
        ps->vx[i] += params->wind_x * dt;
        ps->vy[i] += params->wind_y * dt;

        /* ---- 波紋 (リング状の衝撃波) ---- */
        for (int r = 0; r < ripple_count; r++) {
            float rdx   = ps->x[i] - ripples[r].cx;
            float rdy   = ps->y[i] - ripples[r].cy;
            float rdist = sqrtf(rdx * rdx + rdy * rdy) + 0.1f;
            float ring_w = 80.0f;
            float diff  = fabsf(rdist - ripples[r].radius);
            if (diff < ring_w) {
                float frac = 1.0f - diff / ring_w;
                float rf   = params->ripple_str * frac * frac / (rdist * ps->mass[i]);
                ps->vx[i] += (rdx / rdist) * rf * dt;
                ps->vy[i] += (rdy / rdist) * rf * dt;
            }
        }

        /* ---- 常時カーソル引力 (クリック不要) ---- */
        {
            float dx = mx - ps->x[i];
            float dy = my - ps->y[i];
            float dist2 = dx * dx + dy * dy + 100.0f;
            float inv_dist = 1.0f / sqrtf(dist2);
            float f = params->passive_attract * inv_dist * inv_dist / ps->mass[i];
            ps->vx[i] += dx * inv_dist * f * dt;
            ps->vy[i] += dy * inv_dist * f * dt;
        }

        /* ---- G キー: 位置を直接カーソルへ補間 (一瞬で集まる) ---- */
        if (gather_on) {
            /* lerp = 1 - exp(-speed * dt)  →  speed が大きいほど即座に到着 */
            float lerp = 1.0f - expf(-params->gather_str * dt);
            ps->x[i]  += (mx - ps->x[i]) * lerp;
            ps->y[i]  += (my - ps->y[i]) * lerp;
            /* 速度も殺してオーバーシュートを防ぐ */
            ps->vx[i] *= 0.05f;
            ps->vy[i] *= 0.05f;
        }

        /* ---- タイムストップ (トグル、他モードと重複可) ---- */
        if (timestop_on) {
            ps->vx[i] *= params->timestop_damping;
            ps->vy[i] *= params->timestop_damping;
            if (mode_active && mode == MODE_ATTRACT) {
                float dx = mx - ps->x[i];
                float dy = my - ps->y[i];
                float dist2 = dx * dx + dy * dy + 100.0f;
                float inv_dist = 1.0f / sqrtf(dist2);
                float f = TIMESTOP_STR * inv_dist * inv_dist / ps->mass[i];
                ps->vx[i] += dx * inv_dist * f * dt;
                ps->vy[i] += dy * inv_dist * f * dt;
            }
        }

        /* ---- マウスインタラクション ---- */
        if (mode_active && mode != MODE_NONE && mode != MODE_SPAWN) {
            float dx = mx - ps->x[i];
            float dy = my - ps->y[i];
            float dist2 = dx * dx + dy * dy + 100.0f; /* 0 除算防止 */
            float inv_dist = 1.0f / sqrtf(dist2);
            float inv_dist2 = inv_dist * inv_dist;

            switch (mode) {
                case MODE_ATTRACT: {
                    float range = params->attract_range;
                    if (range <= 0.0f || dist2 <= range * range) {
                        float f = ATTRACT_STR * inv_dist2 / ps->mass[i];
                        ps->vx[i] += dx * inv_dist * f * dt;
                        ps->vy[i] += dy * inv_dist * f * dt;
                    }
                    break;
                }
                case MODE_REPEL: {
                    float f = REPEL_STR * inv_dist2 / ps->mass[i];
                    ps->vx[i] -= dx * inv_dist * f * dt;
                    ps->vy[i] -= dy * inv_dist * f * dt;
                    break;
                }
                case MODE_EXPLODE: {
                    /* 近い粒子ほど強く吹き飛ばす (一度だけ) */
                    float f = EXPLODE_STR * inv_dist2 / ps->mass[i];
                    f = fminf(f, params->explode_cap);
                    ps->vx[i] -= dx * inv_dist * f * dt;
                    ps->vy[i] -= dy * inv_dist * f * dt;
                    ps->life[i] = fminf(ps->life[i] + 0.1f, 1.0f);
                    break;
                }
                case MODE_VORTEX: {
                    /* 接線方向に力を加えて渦を作る */
                    float tx = -dy * inv_dist;
                    float ty =  dx * inv_dist;
                    float f  = VORTEX_STR * inv_dist2 / ps->mass[i];
                    f = fminf(f, params->vortex_cap);
                    ps->vx[i] += tx * f * dt;
                    ps->vy[i] += ty * f * dt;
                    /* 渦の目: 中心から外向きに反発 (粒子が入れないゾーン) */
                    float fr = VORTEX_STR * inv_dist2 / ps->mass[i];
                    fr = fminf(fr, params->vortex_cap);
                    ps->vx[i] -= dx * inv_dist * fr * dt;
                    ps->vy[i] -= dy * inv_dist * fr * dt;
                    break;
                }
                default: break;
            }
        }

        /* ---- 速度制限 ---- */
        float spd2 = ps->vx[i] * ps->vx[i] + ps->vy[i] * ps->vy[i];
        float spd_lim = params->speed_limit;
        if (spd2 > spd_lim * spd_lim) {
            float inv_spd = spd_lim / sqrtf(spd2);
            ps->vx[i] *= inv_spd;
            ps->vy[i] *= inv_spd;
        }

        /* ---- 位置更新 ---- */
        ps->x[i] += ps->vx[i] * dt;
        ps->y[i] += ps->vy[i] * dt;

        /* ---- 減衰 ---- */
        ps->vx[i] *= DAMPING;
        ps->vy[i] *= DAMPING;

        /* ---- 境界処理: 壁で反射 ---- */
        if (ps->x[i] < BX)      { ps->x[i] = BX;      ps->vx[i] =  fabsf(ps->vx[i]) * params->bounce; }
        if (ps->x[i] > BX + BW) { ps->x[i] = BX + BW; ps->vx[i] = -fabsf(ps->vx[i]) * params->bounce; }
        if (ps->y[i] < BY)      { ps->y[i] = BY;       ps->vy[i] =  fabsf(ps->vy[i]) * params->bounce; }
        if (ps->y[i] > BY + BH) { ps->y[i] = BY + BH;  ps->vy[i] = -fabsf(ps->vy[i]) * params->bounce; }

        i++;
    }

    /* ---- マウス位置でスポーン (MODE_SPAWN) ---- */
    if (mode_active && mode == MODE_SPAWN) {
        ps_spawn(ps, mx, my, SPAWN_PER_FRAME, params);
    }

    /* ---- 常時リスポーン: 粒子が減ったら補充 ---- */
    int target = params->particle_count;
    if (target < 1) target = 1;
    if (target > MAX_PARTICLES) target = MAX_PARTICLES;
    /* 超過分を末尾から削除 */
    if (ps->count > target) ps->count = target;
    if (ps->count < target * 2 / 3) {
        ps_spawn(ps,
                 BX + frand() * BW,
                 BY + frand() * BH,
                 SPAWN_PER_FRAME * 5,
                 params);
    }
}

/* ------------------------------------------------------------------ */
/*  ps_reset  –  全粒子を削除して再配置                                 */
/* ------------------------------------------------------------------ */
void ps_reset(ParticleSystem *ps, const SimParams *params)
{
    float ww = (float)ps->win_w;
    float wh = (float)ps->win_h;
    float bw = (params->sim_bounds_w > 0.0f) ? params->sim_bounds_w : ww;
    float bh = (params->sim_bounds_h > 0.0f) ? params->sim_bounds_h : wh;
    float bx = (ww - bw) * 0.5f;
    float by = (wh - bh) * 0.5f;
    int target = params->particle_count;
    if (target < 1) target = 1;
    if (target > MAX_PARTICLES) target = MAX_PARTICLES;
    ps->count = 0;
    for (int i = 0; i < target; i++) {
        ps->x[i]    = bx + frand() * bw;
        ps->y[i]    = by + frand() * bh;
        ps->vx[i]   = frand_range(-50.0f, 50.0f);
        ps->vy[i]   = frand_range(-50.0f, 50.0f);
        ps->life[i] = 1.0f;
        ps->hue[i]  = frand_range(params->hue_min, params->hue_max);
        ps->mass[i] = frand_range(0.5f, 2.0f);
        ps->count++;
    }
}

/* ------------------------------------------------------------------ */
/*  ps_recolor  –  位置・速度はそのままで色相だけ一括更新               */
/* ------------------------------------------------------------------ */
void ps_recolor(ParticleSystem *ps, const SimParams *params)
{
    for (int i = 0; i < ps->count; i++) {
        ps->hue[i] = frand_range(params->hue_min, params->hue_max);
    }
}

/* ------------------------------------------------------------------ */
/*  ps_pack_gpu  –  SoA → インターリーブ GPU 頂点へ変換                 */
/*  戻り値: 書き込んだ頂点数                                             */
/* ------------------------------------------------------------------ */
int ps_pack_gpu(const ParticleSystem *ps, GpuVertex *out, int max_out)
{
    int n = ps->count;
    if (n > max_out) n = max_out;

    for (int i = 0; i < n; i++) {
        float life = ps->life[i];
        /* 寿命に応じて彩度・明度を変化させる: 誕生 → 明るい, 消える → 暗い */
        float sat  = 0.8f;
        float val  = life * 1.4f;
        if (val > 1.0f) val = 1.0f;

        float r, g, b;
        hsv2rgb(ps->hue[i], sat, val, &r, &g, &b);

        out[i].x = ps->x[i];
        out[i].y = ps->y[i];
        out[i].r = r;
        out[i].g = g;
        out[i].b = b;
        out[i].a = life;
    }
    return n;
}

