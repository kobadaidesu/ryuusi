#ifndef SIM_PARAMS_H
#define SIM_PARAMS_H

typedef struct {
    /* --- 引力 / 斥力 --- */
    float attract_str;       /* 左クリック引力          (default: 6000000) */
    float attract_range;     /* 左クリック引力の有効範囲 px (0=無制限, default: 0) */
    float repel_str;         /* 右クリック斥力          (default: 180000)  */
    float passive_attract;   /* 常時カーソル引力        (default: 80000)   */
    float gather_str;        /* G キー収束速度 (lerp)   (default: 30)      */

    /* --- タイムストップ --- */
    float timestop_str;      /* タイムストップ中左クリック引力 (default: 9000000) */
    float timestop_damping;  /* タイムストップ減衰率           (default: 0.65)    */

    /* --- 渦 / 爆発 --- */
    float vortex_str;        /* 渦の強さ                (default: 2000000) */
    float vortex_cap;        /* 渦の力上限              (default: 30000)   */
    float explode_str;       /* 爆発の強さ              (default: 2000000) */
    float explode_cap;       /* 爆発の力上限            (default: 20000)   */

    /* --- 物理全般 --- */
    float gravity;           /* 重力 (下向き)           (default: 0)       */
    float damping;           /* 速度減衰係数             (default: 0.999)   */
    float speed_limit;       /* 最大速度 (px/s)         (default: 1600)    */

    /* --- 描画 --- */
    float point_size;        /* 点サイズ (px)           (default: 3.0)     */

    /* --- 色相範囲 (HSV の H、0..1) --- */
    float hue_min;           /* 色相の最小値            (default: 0.33)    */
    float hue_max;           /* 色相の最大値            (default: 0.67)    */

    /* --- シミュレーション境界 (0 = ウィンドウ全体) --- */
    float sim_bounds_w;      /* プレイエリア幅 px       (default: 0)       */
    float sim_bounds_h;      /* プレイエリア高 px       (default: 0)       */

    /* --- 風 (全粒子に加わる定常加速度) --- */
    float wind_x;            /* 風 X 成分 px/s²        (default: 0)       */
    float wind_y;            /* 風 Y 成分 px/s²        (default: 0)       */

    /* --- 波紋 --- */
    float ripple_str;        /* 波紋の衝撃力            (default: 3000000) */

    /* --- 粒子数 --- */
    int particle_count;      /* アクティブ粒子数上限    (default: 300000)  */

    /* --- 壁反発 --- */
    float bounce;            /* 壁反発係数              (default: 0.6)     */
} SimParams;

#define SIM_PARAMS_DEFAULT { \
    6000000.0f,  /* attract_str       */ \
    0.0f,        /* attract_range     */ \
    180000.0f,   /* repel_str         */ \
    80000.0f,    /* passive_attract   */ \
    30.0f,       /* gather_str        */ \
    9000000.0f,  /* timestop_str      */ \
    0.65f,       /* timestop_damping  */ \
    2000000.0f,  /* vortex_str        */ \
    30000.0f,    /* vortex_cap        */ \
    2000000.0f,  /* explode_str       */ \
    20000.0f,    /* explode_cap       */ \
    0.0f,        /* gravity           */ \
    0.999f,      /* damping           */ \
    1600.0f,     /* speed_limit       */ \
    3.0f,        /* point_size        */ \
    1.00f,       /* hue_min           */ \
    1.00f,       /* hue_max           */ \
    0.0f,        /* sim_bounds_w      */ \
    0.0f,        /* sim_bounds_h      */ \
    0.0f,        /* wind_x            */ \
    0.0f,        /* wind_y            */ \
    3000000.0f,  /* ripple_str        */ \
    300000,      /* particle_count    */ \
    0.6f,        /* bounce            */ \
}

#endif /* SIM_PARAMS_H */
