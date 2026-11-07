/**
 * @file nav_tilemap.h
 * @brief 实时地图瓦片调度器（纯 C，不依赖 LVGL）
 *
 * 解决什么问题：
 *   - 车移动时，计算当前视口需要哪些 z/x/y 瓦片；
 *   - 按“中心优先、外圈预取”的顺序发出加载请求；
 *   - 固定槽位缓存 + LRU 淘汰 + 世代号，避免每帧 malloc/free；
 *   - 给 LVGL 提供每块瓦片的屏幕坐标。
 *
 * 不负责：
 *   - SD/HTTP 读取、JPEG 解码（由 request_cb 的实现在工作线程完成）；
 *   - lv_image 的创建/贴图（由 UI 层根据 slot 状态完成）。
 *
 * 典型用法：
 *   nav_tilemap_init(&tm, 128, 366, 300, 30, 3);
 *   tm.release_cb = tile_free;
 *   nav_tilemap_set_view(&tm, car_x, car_y, ahead_x, ahead_y, 17);
 *   nav_tilemap_update(&tm, tile_request, NULL);
 *   // 工作线程完成后：nav_tilemap_commit(&tm, z, tx, ty, rgb565_buf);
 *   // UI：nav_tilemap_screen_offset(&tm, tx, ty, &x, &y); lv_obj_set_pos(img, x, y);
 */
#ifndef NAV_TILEMAP_H
#define NAV_TILEMAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NAV_TILEMAP_MAX_SLOTS
#define NAV_TILEMAP_MAX_SLOTS 40
#endif

/* 视口内候选瓦片上限。128 px 瓦片 + 366×300 视口约 30 块，64 足够；
 * 降低这个值可以减小 nav_tilemap_update() 的栈占用。 */
#ifndef NAV_TILEMAP_MAX_CANDIDATES
#define NAV_TILEMAP_MAX_CANDIDATES 64
#endif

typedef enum {
    NAV_TILE_EMPTY = 0,
    NAV_TILE_LOADING,
    NAV_TILE_READY,
    NAV_TILE_FAILED
} nav_tile_state_t;

typedef struct {
    int32_t tx;
    int32_t ty;
    uint8_t z;
    uint8_t state;          /**< nav_tile_state_t */
    uint32_t last_used;     /**< LRU 时间戳 */
    uint32_t generation;    /**< 缩放世代号，旧世代结果可丢弃 */
    void *pixels;           /**< RGB565 缓冲 / lv_draw_buf_t*，由应用管理 */
} nav_tile_slot_t;

typedef struct nav_tilemap_s nav_tilemap_t;

struct nav_tilemap_s {
    int32_t tile_size;      /**< 128 或 256 */
    int32_t view_w;
    int32_t view_h;

    double center_x;        /**< 自车世界像素（当前 zoom） */
    double center_y;
    double look_ahead_x;    /**< 前视偏移（世界像素） */
    double look_ahead_y;

    uint8_t zoom;
    uint8_t margin;         /**< 视口外多加载几圈，默认 1 */
    uint8_t max_inflight;   /**< 同时最多几个加载请求 */
    uint8_t inflight;
    uint16_t slot_count;
    uint32_t generation;
    uint32_t tick;

    nav_tile_slot_t slots[NAV_TILEMAP_MAX_SLOTS];

    /** 淘汰/清空槽位时释放像素缓冲；可为 NULL */
    void (*release_cb)(void *pixels);
};

/**
 * 瓦片加载请求回调。
 * 注意：该回调在调用 nav_tilemap_update() 的线程执行；实现里只应入队，
 *       不要在这里同步读 SD/HTTP，否则会阻塞 UI。
 * @param priority 0=中心，1=内圈，2=外圈……
 */
typedef void (*nav_tile_request_cb)(nav_tilemap_t *tm,
                                    uint8_t z,
                                    int32_t tx,
                                    int32_t ty,
                                    uint8_t priority,
                                    void *user_data);

void nav_tilemap_init(nav_tilemap_t *tm,
                      int32_t tile_size,
                      int32_t view_w,
                      int32_t view_h,
                      uint16_t slot_count,
                      uint8_t max_inflight);

/** 清空所有槽位（会调用 release_cb），generation++。 */
void nav_tilemap_clear(nav_tilemap_t *tm);

/**
 * 更新视口。zoom 变化时会清空缓存并 generation++。
 * @param center_x/center_y  自车世界像素
 * @param ahead_x/ahead_y    前视偏移（世界像素），可传 0
 */
void nav_tilemap_set_view(nav_tilemap_t *tm,
                          double center_x,
                          double center_y,
                          double ahead_x,
                          double ahead_y,
                          uint8_t zoom);

/**
 * 计算缺块并按优先级发出请求。每帧/每次 GPS 更新调用一次即可。
 * 若 inflight 已达 max_inflight，会停止发新请求。
 */
void nav_tilemap_update(nav_tilemap_t *tm,
                        nav_tile_request_cb request_cb,
                        void *user_data);

/** 按 key 查找槽位（忽略状态）。 */
nav_tile_slot_t *nav_tilemap_find(nav_tilemap_t *tm,
                                  uint8_t z,
                                  int32_t tx,
                                  int32_t ty);

/**
 * 加载完成：写入像素并标记 READY。
 * 如果该 key 没有 LOADING 槽，会尝试分配/淘汰一个。
 * @return true 成功；false 没有可用槽位
 */
bool nav_tilemap_commit(nav_tilemap_t *tm,
                        uint8_t z,
                        int32_t tx,
                        int32_t ty,
                        void *pixels);

/** 加载失败：标记 FAILED，释放 inflight。 */
void nav_tilemap_fail(nav_tilemap_t *tm,
                      uint8_t z,
                      int32_t tx,
                      int32_t ty);

/**
 * 瓦片左上角的屏幕坐标（当前 zoom）。
 * 像素取整，避免瓦片之间出现 1 px 缝。
 */
void nav_tilemap_screen_offset(const nav_tilemap_t *tm,
                               int32_t tx,
                               int32_t ty,
                               int32_t *x,
                               int32_t *y);

/** 世界像素 -> 屏幕坐标（当前 zoom）。 */
int32_t nav_tilemap_world_to_screen_x(const nav_tilemap_t *tm, double wx);
int32_t nav_tilemap_world_to_screen_y(const nav_tilemap_t *tm, double wy);

/** 该瓦片是否与当前视口相交（只对当前 zoom 有意义）。 */
bool nav_tilemap_visible(const nav_tilemap_t *tm, int32_t tx, int32_t ty);

/** 当前 READY 的瓦片数量。 */
uint16_t nav_tilemap_ready_count(const nav_tilemap_t *tm);

/** 递增 tick（LRU 时间戳）。update() 会自动调用。 */
void nav_tilemap_tick(nav_tilemap_t *tm);

#ifdef __cplusplus
}
#endif

#endif /* NAV_TILEMAP_H */
