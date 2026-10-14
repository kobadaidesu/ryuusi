#include "particle.h"
#include <float.h>
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

void ps_seed(uint32_t seed)
{
    g_seed = seed ? seed : 12345678u; /* xorshift は 0 から抜けられない */
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

/* 粒子 i の描画色 (RGBA8) を色相と寿命から作る。
 * 寿命に応じて明度を変える: 誕生 → 明るい, 消える → 暗い。alpha = 寿命。
 * 寿命は今は常に 1 (LIFE_DECAY = 0) なので、色は生成・再配置・色替えのときだけ変わる。
 * 寿命が変わる経路を作ったら、そこでもこの関数で色を作り直し color_version を上げること */
static inline uint8_t unorm8(float v)
{
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return (uint8_t)(v * 255.0f + 0.5f);
}

static void update_colors(ParticleSystem *ps, int from, int to)
{
    for (int i = from; i < to; i++) {
        float life = ps->life[i];
        float val  = life * 1.4f;
        if (val > 1.0f) val = 1.0f;
        float r, g, b;
        hsv2rgb(ps->hue[i], 0.8f, val, &r, &g, &b);
        ps->color[i][0] = unorm8(r);
        ps->color[i][1] = unorm8(g);
        ps->color[i][2] = unorm8(b);
        ps->color[i][3] = unorm8(life);
    }
    if (to > from) ps->color_version++;
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
        ps->inv_mass[i] = 1.0f / frand_range(0.5f, 2.0f);
        ps->count++;
    }
    update_colors(ps, 0, ps->count);
}

/* ------------------------------------------------------------------ */
/*  ps_spawn  –  count 個の粒子を (cx,cy) 付近に生成                   */
/* ------------------------------------------------------------------ */
void ps_spawn(ParticleSystem *ps, float cx, float cy, int count,
              const SimParams *params)
{
    int added = 0;
    int first = ps->count;
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
        ps->inv_mass[i] = 1.0f / frand_range(0.5f, 2.0f);

        ps->count++;
        added++;
    }
    update_colors(ps, first, ps->count);
}

/* ------------------------------------------------------------------ */
/*  ps_update  –  メインループから毎フレーム呼ぶ                        */
/* ------------------------------------------------------------------ */
/* 寿命の減り方 (1 秒あたり)。0 なので粒子は寿命では消えない。
 * 0 より大きくすると ps_update が ps_remove_dead で消えた粒子を詰める */
#define LIFE_DECAY 0.0f

/* 寿命が尽きた粒子を末尾と入れ替えて削除する O(1)。
 * 主ループから外してあるのは、ループの途中で要素数が変わると
 * 自動ベクトル化ができなくなるため */
static void ps_remove_dead(ParticleSystem *ps)
{
    int i = 0;
    while (i < ps->count) {
        if (ps->life[i] > 0.0f) { i++; continue; }
        int last = ps->count - 1;
        ps->x[i]        = ps->x[last];
        ps->y[i]        = ps->y[last];
        ps->vx[i]       = ps->vx[last];
        ps->vy[i]       = ps->vy[last];
        ps->life[i]     = ps->life[last];
        ps->hue[i]      = ps->hue[last];
        ps->inv_mass[i] = ps->inv_mass[last];
        memcpy(ps->color[i], ps->color[last], 4);
        ps->count--;
    }
    ps->color_version++;
}

/* f と上限 cap の小さい方。fminf と違って NaN を考えないので比較 1 回 (SIMD の min) になる */
static inline float min_cap(float f, float cap)
{
    return f < cap ? f : cap;
}

/*
 * 主ループは自動ベクトル化 (SIMD) できる形にしてある:
 *  - フレーム内で一定の条件 (モード、タイムストップ、G) と係数はループの外で決め、
 *    ループの中では係数を掛けるだけにする (使わない力は係数 0 で足しても値は変わらない)
 *  - 粒子ごとの条件 (力の上限、範囲、速度制限、壁) は分岐ではなく選択 (a ? b : c) で書く
 *  - 配列は restrict のローカルポインタで触り、1 粒子の値はローカル変数に読んでから最後に書く
 *  - 波紋は「波紋ごとに全粒子を回す」別のループにする (内側の可変長ループがあると
 *    外側のループをベクトル化できないため)
 */
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
    const float DAMPING = params->damping;
    const float WIN_W = (float)ps->win_w;
    const float WIN_H = (float)ps->win_h;

    /* シミュレーション境界 (0 = ウィンドウ全体) */
    const float BW = (params->sim_bounds_w > 0.0f) ? params->sim_bounds_w : WIN_W;
    const float BH = (params->sim_bounds_h > 0.0f) ? params->sim_bounds_h : WIN_H;
    const float BX = (WIN_W - BW) * 0.5f;
    const float BY = (WIN_H - BH) * 0.5f;

    /* ---- 寿命 (LIFE_DECAY = 0 なので今は何もしない) ---- */
    if (LIFE_DECAY > 0.0f) {
        for (int i = 0; i < ps->count; i++)
            ps->life[i] -= LIFE_DECAY * dt;
        ps_remove_dead(ps);
    }

    const int n = ps->count;
    float       *restrict X    = ps->x;
    float       *restrict Y    = ps->y;
    float       *restrict VX   = ps->vx;
    float       *restrict VY   = ps->vy;
    const float *restrict IM   = ps->inv_mass;

    /* ---- 波紋 (リング状の衝撃波): 波紋ごとに全粒子へ ----
     *  輪の幅 80px の中だけ、輪からの近さ frac の 2 乗に比例して外向きに押す。
     *  輪の外は frac = 0 になるので分岐はいらない */
    {
        const float RING_W     = 80.0f;
        const float INV_RING_W = 1.0f / RING_W;
        const float k          = params->ripple_str * dt;
        for (int r = 0; r < ripple_count; r++) {
            const float cx  = ripples[r].cx;
            const float cy  = ripples[r].cy;
            const float rad = ripples[r].radius;
            for (int i = 0; i < n; i++) {
                float rdx   = X[i] - cx;
                float rdy   = Y[i] - cy;
                float rdist = sqrtf(rdx * rdx + rdy * rdy) + 0.1f;
                float inv_r = 1.0f / rdist;
                float frac  = 1.0f - fabsf(rdist - rad) * INV_RING_W;
                frac = frac > 0.0f ? frac : 0.0f;
                /* 方向 (rdx / rdist) × 強さ (str × frac² / rdist / mass) × dt */
                float rf = k * frac * frac * inv_r * inv_r * IM[i];
                VX[i] += rdx * rf;
                VY[i] += rdy * rf;
            }
        }
    }

    /* ---- フレーム内で一定の係数 ---- */
    /* 重力と風 (定常加速度) */
    const float acc_x = params->wind_x * dt;
    const float acc_y = params->gravity * dt + params->wind_y * dt;
    /* 常時カーソル引力 (クリック不要) */
    const float passive_k = params->passive_attract * dt;
    /* G キー: 位置を直接カーソルへ補間。lerp = 1 - exp(-speed * dt) はフレームで一定 */
    const float gather_lerp = gather_on ? 1.0f - expf(-params->gather_str * dt) : 0.0f;
    /* タイムストップ: 速度を減衰。左クリック中は専用の強い引力 */
    const float ts_damp = timestop_on ? params->timestop_damping : 1.0f;
    const float ts_k    = (timestop_on && mode_active && mode == MODE_ATTRACT)
                        ? params->timestop_str * dt : 0.0f;

    /* マウスのモード: 中心へ向かう成分 (rad、引力 +1 / 斥力 -1) と接線成分 (tan、渦) の向き、
     * 強さ k、力の上限 cap、効く範囲 (距離の 2 乗) range2 */
    float m_k = 0.0f, m_cap = FLT_MAX, m_rad = 0.0f, m_tan = 0.0f, m_range2 = FLT_MAX;
    int   m_explode = 0;
    if (mode_active) {
        switch (mode) {
            case MODE_ATTRACT:
                m_k = params->attract_str;  m_rad = 1.0f;
                if (params->attract_range > 0.0f)
                    m_range2 = params->attract_range * params->attract_range;
                break;
            case MODE_REPEL:
                m_k = params->repel_str;    m_rad = -1.0f;
                break;
            case MODE_EXPLODE:
                /* 近い粒子ほど強く吹き飛ばす */
                m_k = params->explode_str;  m_rad = -1.0f; m_cap = params->explode_cap;
                m_explode = 1;
                break;
            case MODE_VORTEX:
                /* 接線方向に回し、同じ強さで外向きにも押す (渦の目: 粒子が入れないゾーン) */
                m_k = params->vortex_str;   m_rad = -1.0f; m_tan = 1.0f;
                m_cap = params->vortex_cap;
                break;
            default:
                break;
        }
    }

    const float spd_lim  = params->speed_limit;
    const float spd_lim2 = spd_lim * spd_lim;
    const float bounce   = params->bounce;
    const float BX1 = BX + BW;
    const float BY1 = BY + BH;

    for (int i = 0; i < n; i++) {
        float x  = X[i],  y  = Y[i];
        float vx = VX[i], vy = VY[i];
        const float im = IM[i];

        /* ---- 重力・風 ---- */
        vx += acc_x;
        vy += acc_y;

        /* ---- カーソルへの距離 (以下の力で共有する) ---- */
        float dx  = mx - x;
        float dy  = my - y;
        float d2  = dx * dx + dy * dy + 100.0f;   /* 0 除算防止 */
        float id  = 1.0f / sqrtf(d2);             /* 1 / 距離   */
        float id2 = id * id;

        /* ---- 常時カーソル引力 ---- */
        {
            float f = passive_k * id2 * im;
            vx += dx * id * f;
            vy += dy * id * f;
        }

        /* ---- G キー: 位置をカーソルへ寄せ、速度も殺してオーバーシュートを防ぐ ---- */
        if (gather_on) {
            x  += (mx - x) * gather_lerp;
            y  += (my - y) * gather_lerp;
            vx *= 0.05f;
            vy *= 0.05f;
            /* 位置が動いたので、以下の力のために距離を測り直す */
            dx  = mx - x;
            dy  = my - y;
            d2  = dx * dx + dy * dy + 100.0f;
            id  = 1.0f / sqrtf(d2);
            id2 = id * id;
        }

        /* ---- タイムストップ (トグル、他モードと重複可) ---- */
        vx *= ts_damp;
        vy *= ts_damp;
        {
            float f = ts_k * id2 * im;
            vx += dx * id * f;
            vy += dy * id * f;
        }

        /* ---- マウスインタラクション ---- */
        {
            float f = min_cap(m_k * id2 * im, m_cap);
            f = d2 <= m_range2 ? f : 0.0f;         /* 引力の範囲外 */
            f *= dt;
            vx += (m_rad * dx - m_tan * dy) * id * f;
            vy += (m_rad * dy + m_tan * dx) * id * f;
        }

        /* ---- 速度制限 ---- */
        float spd2  = vx * vx + vy * vy;
        float scale = spd2 > spd_lim2 ? spd_lim / sqrtf(spd2) : 1.0f;
        vx *= scale;
        vy *= scale;

        /* ---- 位置更新 ---- */
        x += vx * dt;
        y += vy * dt;

        /* ---- 減衰 ---- */
        vx *= DAMPING;
        vy *= DAMPING;

        /* ---- 境界処理: 壁で反射 ---- */
        int hit;
        hit = x < BX;  x = hit ? BX  : x; vx = hit ?  fabsf(vx) * bounce : vx;
        hit = x > BX1; x = hit ? BX1 : x; vx = hit ? -fabsf(vx) * bounce : vx;
        hit = y < BY;  y = hit ? BY  : y; vy = hit ?  fabsf(vy) * bounce : vy;
        hit = y > BY1; y = hit ? BY1 : y; vy = hit ? -fabsf(vy) * bounce : vy;

        X[i]  = x;  Y[i]  = y;
        VX[i] = vx; VY[i] = vy;
    }

    /* ---- 爆発は寿命も少し回復させる (今は寿命が減らないので見た目は変わらない) ---- */
    if (m_explode) {
        float *restrict L = ps->life;
        for (int i = 0; i < n; i++)
            L[i] = min_cap(L[i] + 0.1f, 1.0f);
    }

    /* ---- 寿命が変わる経路 (LIFE_DECAY > 0) では、明るさと alpha も変わるので色を作り直す。
     *      今は寿命が常に 1 なので通らない (爆発の回復も 1 で頭打ち) ---- */
    if (LIFE_DECAY > 0.0f)
        update_colors(ps, 0, ps->count);

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
        ps->inv_mass[i] = 1.0f / frand_range(0.5f, 2.0f);
        ps->count++;
    }
    update_colors(ps, 0, ps->count);
}

/* ------------------------------------------------------------------ */
/*  ps_recolor  –  位置・速度はそのままで色相だけ一括更新               */
/* ------------------------------------------------------------------ */
void ps_recolor(ParticleSystem *ps, const SimParams *params)
{
    for (int i = 0; i < ps->count; i++) {
        ps->hue[i] = frand_range(params->hue_min, params->hue_max);
    }
    update_colors(ps, 0, ps->count);
}

/* ------------------------------------------------------------------ */
/*  描画用のパック                                                       */
/* ------------------------------------------------------------------ */

/* 飽和カリングの準備 (カウントを 0 に戻す)。無効なら 0 を返す */
static int cull_begin(const PackCull *cull)
{
    if (!cull || !cull->counts || cull->limit <= 0 || cull->w <= 0 || cull->h <= 0)
        return 0;
    memset(cull->counts, 0, (size_t)cull->w * (size_t)cull->h * sizeof(uint16_t));
    return 1;
}

/* 中心画素のカウントが上限未満なら数えて 1 (描く)、上限に達していたら 0 (描かない) */
static inline int cull_keep(const PackCull *cull, unsigned limit, float x, float y)
{
    const int w = cull->w, h = cull->h;
    int px = (int)x;
    int py = (int)y;
    if (px < 0) px = 0; else if (px >= w) px = w - 1;
    if (py < 0) py = 0; else if (py >= h) py = h - 1;
    uint16_t *c = &cull->counts[(size_t)py * (size_t)w + (size_t)px];
    if (*c >= limit) return 0;   /* この画素はもう飽和している */
    (*c)++;
    return 1;
}

static unsigned cull_limit(const PackCull *cull)
{
    return cull->limit > 65535 ? 65535u : (unsigned)cull->limit;
}

int ps_pack_positions(const ParticleSystem *ps, float *pos, uint8_t (*col)[4],
                      const PackCull *cull)
{
    const int n = ps->count;
    if (!cull_begin(cull)) {
        for (int i = 0; i < n; i++) {
            pos[2 * i]     = ps->x[i];
            pos[2 * i + 1] = ps->y[i];
        }
        return n;
    }
    const unsigned limit = cull_limit(cull);
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (!cull_keep(cull, limit, ps->x[i], ps->y[i])) continue;
        pos[2 * m]     = ps->x[i];
        pos[2 * m + 1] = ps->y[i];
        memcpy(col[m], ps->color[i], 4);
        m++;
    }
    return m;
}

int ps_cull_indices(const ParticleSystem *ps, uint32_t *idx, const PackCull *cull)
{
    const int n = ps->count;
    if (!cull_begin(cull)) return n;
    const unsigned limit = cull_limit(cull);
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (!cull_keep(cull, limit, ps->x[i], ps->y[i])) continue;
        idx[m++] = (uint32_t)i;
    }
    return m;
}
