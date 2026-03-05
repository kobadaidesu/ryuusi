#ifndef PARTICLE_H
#define PARTICLE_H

#include <stdint.h>
#include "sim_params.h"

/* ------------------------------------------------------------------ *
 *  定数
 * ------------------------------------------------------------------ */
#define MAX_PARTICLES  300000
#define SPAWN_PER_FRAME    300   /* 毎フレームのデフォルト生成数     */
#define MAX_RIPPLES     32       /* 同時に存在できる波紋の最大数     */
/* ------------------------------------------------------------------ *
 *  波紋 (app.c で管理、ps_update に const ポインタで渡す)
 * ------------------------------------------------------------------ */
typedef struct {
    float cx, cy;        /* 発生中心              */
    float radius;        /* 現在の半径 (px)       */
    float max_radius;    /* この半径を超えたら消滅 */
} RippleWave;

/* ------------------------------------------------------------------ *
 *  インタラクションモード
 * ------------------------------------------------------------------ */
typedef enum {
    MODE_NONE      = 0,
    MODE_ATTRACT   = 1,   /* 引力          */
    MODE_REPEL     = 2,   /* 斥力          */
    MODE_EXPLODE   = 3,   /* 爆発          */
    MODE_VORTEX    = 4,   /* 渦            */
    MODE_SPAWN     = 5,   /* 生成          */
    MODE_TIMESTOP  = 6,   /* タイムストップ */
} InteractMode;

/* ------------------------------------------------------------------ *
 *  SoA レイアウトの粒子システム
 *
 *  連続メモリ・SoA 配置によりキャッシュ効率を最大化する。
 *  アクティブ粒子は [0, count) に集約し、
 *  削除は末尾要素と入れ替えて O(1) を保つ。
 * ------------------------------------------------------------------ */
typedef struct {
    /* --- 位置 --- */
    float x[MAX_PARTICLES];
    float y[MAX_PARTICLES];

    /* --- 速度 --- */
    float vx[MAX_PARTICLES];
    float vy[MAX_PARTICLES];

    /* --- 寿命 (0=死、1=生まれたて。毎フレーム減衰) --- */
    float life[MAX_PARTICLES];

    /* --- 基本色相 (0..1、粒子ごとに固有) --- */
    float hue[MAX_PARTICLES];

    /* --- 質量(速度応答の係数) --- */
    float mass[MAX_PARTICLES];

    /* --- アクティブ粒子数 --- */
    int count;

    /* --- ウィンドウサイズ (境界反射用) --- */
    int win_w;
    int win_h;

} ParticleSystem;

/* ------------------------------------------------------------------ *
 *  GPU 転送用パックド頂点
 *  粒子1つ = 6 float = 24 bytes
 * ------------------------------------------------------------------ */
typedef struct {
    float x, y;
    float r, g, b, a;
} GpuVertex;

/* ------------------------------------------------------------------ *
 *  API
 * ------------------------------------------------------------------ */
void ps_init(ParticleSystem *ps, int win_w, int win_h, const SimParams *params);
void ps_spawn(ParticleSystem *ps, float cx, float cy, int count, const SimParams *params);
void ps_update(ParticleSystem *ps,
               float dt,
               float mx, float my,             /* マウス位置 (スクリーン座標) */
               InteractMode mode,
               int   mode_active,              /* ボタン押下中か              */
               int   timestop_on,              /* タイムストップ ON/OFF       */
               int   gather_on,               /* G キー全体引力 ON/OFF       */
               const RippleWave *ripples,      /* 現在アクティブな波紋        */
               int   ripple_count,
               const SimParams *params);       /* 実行時パラメータ           */
void ps_reset(ParticleSystem *ps, const SimParams *params);
/* 位置・速度はそのままで色相だけ一括更新 */
void ps_recolor(ParticleSystem *ps, const SimParams *params);

/* GPU バッファへパック (render.c から呼ぶ) */
int  ps_pack_gpu(const ParticleSystem *ps,
                 GpuVertex *out,         /* MAX_PARTICLES 確保済み前提 */
                 int max_out);


#endif /* PARTICLE_H */
