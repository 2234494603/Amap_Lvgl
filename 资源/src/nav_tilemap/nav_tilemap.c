/**
 * @file nav_tilemap.c
 * @brief 实时地图瓦片调度器实现
 */
#include "nav_tilemap.h"

#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* 小工具                                                                     */
/* -------------------------------------------------------------------------- */

static inline int32_t nav_abs32(int32_t v)
{
    return v < 0 ? -v : v;
}

static inline int32_t nav_max32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static inline double nav_view_center_x(const nav_tilemap_t *tm)
{
    return tm->center_x + tm->look_ahead_x;
}

static inline double nav_view_center_y(const nav_tilemap_t *tm)
{
    return tm->center_y + tm->look_ahead_y;
}

static inline int32_t nav_tiles_per_axis(uint8_t z)
{
    if(z >= 22) return 1 << 22;
    return 1 << z;
}

static bool nav_slot_visible(const nav_tilemap_t *tm, const nav_tile_slot_t *s)
{
    if(s->state != NAV_TILE_READY && s->state != NAV_TILE_LOADING) return false;
    if(s->z != tm->zoom) return false;
    return nav_tilemap_visible(tm, s->tx, s->ty);
}

/** 找空槽；没有空槽就淘汰“不可见 + 最久未用”的 READY/FAILED 槽。 */
static nav_tile_slot_t *nav_alloc_slot(nav_tilemap_t *tm)
{
    for(uint16_t i = 0; i < tm->slot_count; i++) {
        if(tm->slots[i].state == NAV_TILE_EMPTY) {
            return &tm->slots[i];
        }
    }

    nav_tile_slot_t *victim = NULL;
    for(uint16_t i = 0; i < tm->slot_count; i++) {
        nav_tile_slot_t *s = &tm->slots[i];
        if(s->state == NAV_TILE_LOADING) continue;       /* 在途，不能动 */
        if(nav_slot_visible(tm, s)) continue;            /* 当前可见，不能动 */
        if(victim == NULL || s->last_used < victim->last_used) {
            victim = s;
        }
    }

    if(victim != NULL) {
        if(victim->pixels != NULL && tm->release_cb != NULL) {
            tm->release_cb(victim->pixels);
        }
        memset(victim, 0, sizeof(*victim));
        victim->state = NAV_TILE_EMPTY;
    }

    return victim;
}

/* -------------------------------------------------------------------------- */
/* 初始化 / 清理                                                              */
/* -------------------------------------------------------------------------- */

void nav_tilemap_init(nav_tilemap_t *tm,
                      int32_t tile_size,
                      int32_t view_w,
                      int32_t view_h,
                      uint16_t slot_count,
                      uint8_t max_inflight)
{
    if(tm == NULL) return;
    memset(tm, 0, sizeof(*tm));

    if(tile_size <= 0) tile_size = 128;
    if(view_w <= 0) view_w = 320;
    if(view_h <= 0) view_h = 240;
    if(slot_count == 0) slot_count = 1;
    if(slot_count > NAV_TILEMAP_MAX_SLOTS) slot_count = NAV_TILEMAP_MAX_SLOTS;
    if(max_inflight == 0) max_inflight = 1;
    if(max_inflight > 8) max_inflight = 8;

    tm->tile_size    = tile_size;
    tm->view_w       = view_w;
    tm->view_h       = view_h;
    tm->slot_count   = slot_count;
    tm->max_inflight = max_inflight;
    tm->margin       = 1;
    tm->generation   = 1;
    tm->tick         = 1;
}

void nav_tilemap_clear(nav_tilemap_t *tm)
{
    if(tm == NULL) return;

    for(uint16_t i = 0; i < tm->slot_count; i++) {
        nav_tile_slot_t *s = &tm->slots[i];
        if(s->pixels != NULL && tm->release_cb != NULL) {
            tm->release_cb(s->pixels);
        }
        memset(s, 0, sizeof(*s));
        s->state = NAV_TILE_EMPTY;
    }

    tm->inflight = 0;
    tm->generation++;
}

void nav_tilemap_set_view(nav_tilemap_t *tm,
                          double center_x,
                          double center_y,
                          double ahead_x,
                          double ahead_y,
                          uint8_t zoom)
{
    if(tm == NULL) return;

    if(zoom > 22) zoom = 22;

    if(tm->zoom != zoom) {
        tm->zoom = zoom;
        /* 缩放级别变了，旧瓦片的 key 不再匹配；直接清空，避免错位。
         * 如果要做“旧图缩放占位”，应在 clear 前把旧槽位复制到单独的占位缓存。 */
        nav_tilemap_clear(tm);
    }

    tm->center_x     = center_x;
    tm->center_y     = center_y;
    tm->look_ahead_x = ahead_x;
    tm->look_ahead_y = ahead_y;
}

/* -------------------------------------------------------------------------- */
/* 查询 / 坐标                                                                */
/* -------------------------------------------------------------------------- */

nav_tile_slot_t *nav_tilemap_find(nav_tilemap_t *tm,
                                  uint8_t z,
                                  int32_t tx,
                                  int32_t ty)
{
    if(tm == NULL) return NULL;

    for(uint16_t i = 0; i < tm->slot_count; i++) {
        nav_tile_slot_t *s = &tm->slots[i];
        if(s->state != NAV_TILE_EMPTY &&
           s->z == z && s->tx == tx && s->ty == ty) {
            return s;
        }
    }
    return NULL;
}

void nav_tilemap_screen_offset(const nav_tilemap_t *tm,
                               int32_t tx,
                               int32_t ty,
                               int32_t *x,
                               int32_t *y)
{
    if(tm == NULL) {
        if(x) *x = 0;
        if(y) *y = 0;
        return;
    }

    const double left = nav_view_center_x(tm) - tm->view_w * 0.5;
    const double top  = nav_view_center_y(tm) - tm->view_h * 0.5;

    if(x) *x = (int32_t)floor((double)tx * tm->tile_size - left + 0.5);
    if(y) *y = (int32_t)floor((double)ty * tm->tile_size - top + 0.5);
}

int32_t nav_tilemap_world_to_screen_x(const nav_tilemap_t *tm, double wx)
{
    if(tm == NULL) return 0;
    return (int32_t)floor(wx - nav_view_center_x(tm) + tm->view_w * 0.5 + 0.5);
}

int32_t nav_tilemap_world_to_screen_y(const nav_tilemap_t *tm, double wy)
{
    if(tm == NULL) return 0;
    return (int32_t)floor(wy - nav_view_center_y(tm) + tm->view_h * 0.5 + 0.5);
}

bool nav_tilemap_visible(const nav_tilemap_t *tm, int32_t tx, int32_t ty)
{
    if(tm == NULL) return false;

    int32_t x, y;
    nav_tilemap_screen_offset(tm, tx, ty, &x, &y);

    return (x + tm->tile_size > 0) &&
           (x < tm->view_w) &&
           (y + tm->tile_size > 0) &&
           (y < tm->view_h);
}

uint16_t nav_tilemap_ready_count(const nav_tilemap_t *tm)
{
    if(tm == NULL) return 0;
    uint16_t n = 0;
    for(uint16_t i = 0; i < tm->slot_count; i++) {
        if(tm->slots[i].state == NAV_TILE_READY) n++;
    }
    return n;
}

void nav_tilemap_tick(nav_tilemap_t *tm)
{
    if(tm != NULL) tm->tick++;
}

/* -------------------------------------------------------------------------- */
/* 核心：缺块计算 + 优先级                                                     */
/* -------------------------------------------------------------------------- */

typedef struct {
    int32_t tx;
    int32_t ty;
    int32_t ring;   /* 离中心瓦片的切比雪夫距离 */
    int32_t d2;     /* 距中心瓦片的平方距离，用于同 ring 内排序 */
} nav_candidate_t;

static void nav_sort_candidates(nav_candidate_t *c, uint16_t n)
{
    /* n 很小（<96），插入排序足够，且无额外内存 */
    for(uint16_t i = 1; i < n; i++) {
        nav_candidate_t key = c[i];
        int32_t j = (int32_t)i - 1;
        while(j >= 0 &&
              (c[j].ring > key.ring ||
               (c[j].ring == key.ring && c[j].d2 > key.d2))) {
            c[j + 1] = c[j];
            j--;
        }
        c[j + 1] = key;
    }
}

void nav_tilemap_update(nav_tilemap_t *tm,
                        nav_tile_request_cb request_cb,
                        void *user_data)
{
    if(tm == NULL || request_cb == NULL || tm->slot_count == 0) return;
    if(tm->inflight >= tm->max_inflight) return;

    nav_tilemap_tick(tm);

    const int32_t T = tm->tile_size;
    const int32_t world_tiles = nav_tiles_per_axis(tm->zoom);
    const double cx = nav_view_center_x(tm);
    const double cy = nav_view_center_y(tm);

    const int32_t cx_tile = (int32_t)floor(cx / (double)T);
    const int32_t cy_tile = (int32_t)floor(cy / (double)T);

    int32_t tx0 = (int32_t)floor((cx - tm->view_w * 0.5) / (double)T) - tm->margin;
    int32_t tx1 = (int32_t)floor((cx + tm->view_w * 0.5 - 1.0) / (double)T) + tm->margin;
    int32_t ty0 = (int32_t)floor((cy - tm->view_h * 0.5) / (double)T) - tm->margin;
    int32_t ty1 = (int32_t)floor((cy + tm->view_h * 0.5 - 1.0) / (double)T) + tm->margin;

    if(tx0 < 0) tx0 = 0;
    if(ty0 < 0) ty0 = 0;
    if(tx1 >= world_tiles) tx1 = world_tiles - 1;
    if(ty1 >= world_tiles) ty1 = world_tiles - 1;
    if(tx1 < tx0 || ty1 < ty0) return;

    nav_candidate_t cand[NAV_TILEMAP_MAX_CANDIDATES];
    uint16_t nc = 0;

    for(int32_t ty = ty0; ty <= ty1 && nc < NAV_TILEMAP_MAX_CANDIDATES; ty++) {
        for(int32_t tx = tx0; tx <= tx1 && nc < NAV_TILEMAP_MAX_CANDIDATES; tx++) {
            int32_t dx = tx - cx_tile;
            int32_t dy = ty - cy_tile;
            cand[nc].tx   = tx;
            cand[nc].ty   = ty;
            cand[nc].ring = nav_max32(nav_abs32(dx), nav_abs32(dy));
            cand[nc].d2   = dx * dx + dy * dy;
            nc++;
        }
    }

    nav_sort_candidates(cand, nc);

    for(uint16_t i = 0; i < nc; i++) {
        if(tm->inflight >= tm->max_inflight) break;

        const int32_t tx = cand[i].tx;
        const int32_t ty = cand[i].ty;

        nav_tile_slot_t *s = nav_tilemap_find(tm, tm->zoom, tx, ty);

        if(s != NULL) {
            if(s->state == NAV_TILE_FAILED &&
               (tm->tick - s->last_used) > 5) {
                /* 隔几次重试，避免坏 URL/坏卡无限打队列 */
                if(s->pixels != NULL && tm->release_cb != NULL) {
                    tm->release_cb(s->pixels);
                    s->pixels = NULL;
                }
                s->state = NAV_TILE_EMPTY;
                /* 重新走分配流程 */
            }
            else {
                continue;   /* LOADING / READY / 刚失败，跳过 */
            }
        }

        if(s == NULL) {
            s = nav_alloc_slot(tm);
            if(s == NULL) break;  /* 全是在途/可见，等下一轮 */
        }

        s->tx         = tx;
        s->ty         = ty;
        s->z          = tm->zoom;
        s->state      = NAV_TILE_LOADING;
        s->last_used  = tm->tick;
        s->generation = tm->generation;
        s->pixels     = NULL;

        tm->inflight++;
        request_cb(tm, tm->zoom, tx, ty, (uint8_t)cand[i].ring, user_data);
    }
}

/* -------------------------------------------------------------------------- */
/* 加载完成 / 失败                                                            */
/* -------------------------------------------------------------------------- */

bool nav_tilemap_commit(nav_tilemap_t *tm,
                        uint8_t z,
                        int32_t tx,
                        int32_t ty,
                        void *pixels)
{
    if(tm == NULL) return false;

    /* 缩放已切换：旧世代的图直接丢弃，避免“放大后冒出旧图” */
    if(z != tm->zoom) {
        if(pixels != NULL && tm->release_cb != NULL) {
            tm->release_cb(pixels);
        }
        return false;
    }

    nav_tile_slot_t *s = nav_tilemap_find(tm, z, tx, ty);
    if(s == NULL) {
        s = nav_alloc_slot(tm);
        if(s == NULL) return false;
    }

    if(s->state == NAV_TILE_LOADING && tm->inflight > 0) {
        tm->inflight--;
    }

    if(s->pixels != NULL && s->pixels != pixels && tm->release_cb != NULL) {
        tm->release_cb(s->pixels);
    }

    s->tx         = tx;
    s->ty         = ty;
    s->z          = z;
    s->state      = NAV_TILE_READY;
    s->pixels     = pixels;
    s->last_used  = tm->tick;
    s->generation = tm->generation;

    return true;
}

void nav_tilemap_fail(nav_tilemap_t *tm,
                      uint8_t z,
                      int32_t tx,
                      int32_t ty)
{
    if(tm == NULL) return;

    nav_tile_slot_t *s = nav_tilemap_find(tm, z, tx, ty);
    if(s == NULL) return;

    if(s->state == NAV_TILE_LOADING && tm->inflight > 0) {
        tm->inflight--;
    }

    s->state     = NAV_TILE_FAILED;
    s->last_used = tm->tick;
}
