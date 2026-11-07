/**
 * @file lv_nav_path.h
 * @brief LVGL 9 导航路径绘制工具（发光/描边/渐变/已走过路径）
 *
 * 目标：在 320x240 ~ 480x854 的 MCU 屏幕上，用尽可能少的 draw task
 * 画出接近手机导航的粗路径效果。
 *
 * 依赖：LVGL 9.3+（lv_draw_line_dsc_t.points / point_cnt）
 *       LVGL 8.x 请参考 .c 文件末尾的降级说明。
 *
 * 用法：
 *   1. 准备两块静态数组：点缓存和累计长度缓存；
 *   2. nav_path_init() -> nav_path_set() -> nav_path_smooth_catmull()；
 *   3. 在卡片的 LV_EVENT_DRAW_MAIN 中调用 nav_path_draw()。
 *
 * 注意：路径点使用“屏幕绝对坐标”。如果卡片会移动，建议构建局部坐标，
 *       绘制时由调用者加偏移（或每次移动后平移点数组）。
 */
#ifndef LV_NAV_PATH_H
#define LV_NAV_PATH_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 单条路径最大点数。卡片内 100~250 点足够。 */
#ifndef LV_NAV_PATH_MAX_POINTS
#define LV_NAV_PATH_MAX_POINTS 256
#endif

/**
 * 1: 用 lv_draw_line_dsc_t.points 一次提交整条折线（LVGL 9.3+，推荐）
 * 0: 逐段提交，避免 points 版本的临时 lv_malloc（内存很紧时用）
 */
#ifndef NAV_PATH_USE_POINTS
#define NAV_PATH_USE_POINTS 1
#endif

typedef struct {
    lv_point_precise_t *pts;   /**< 点缓存，由调用者提供 */
    float *cum;                /**< 累计弧长缓存，长度与 pts 相同 */
    uint16_t count;            /**< 当前点数 */
    uint16_t capacity;         /**< 缓存容量 */
    float length;              /**< 总弧长（像素） */
    lv_area_t bbox;            /**< 包围盒 */
} nav_path_t;

typedef struct {
    /* 外层发光：建议 2~3 遍不同宽度/透明度叠加，这里只保留一遍主发光 */
    lv_color_t glow_color;
    lv_opa_t   glow_opa;
    int32_t    glow_width;

    /* 深色描边：让路径在浅色底图上也能看清 */
    lv_color_t casing_color;
    lv_opa_t   casing_opa;
    int32_t    casing_width;

    /* 主线：起点色 -> 终点色（沿路径方向插值） */
    lv_color_t color_start;
    lv_color_t color_end;
    int32_t    width;

    /* 中心高光：一条细的浅色线，模拟玻璃/霓虹质感 */
    lv_color_t core_color;
    lv_opa_t   core_opa;
    int32_t    core_width;

    /* 已走过路径：用深灰覆盖，弱化 */
    lv_color_t traveled_color;
    lv_opa_t   traveled_opa;

    bool       gradient;            /**< true: 按段插值颜色；false: 单色 */
    uint16_t   max_gradient_steps;  /**< 渐变最多拆成多少段，建议 24~64 */
} nav_path_style_t;

/** 填入一套接近设计稿的默认样式。 */
void nav_path_style_init(nav_path_style_t *style);

/**
 * 初始化路径对象。
 * @param path      路径对象
 * @param pt_buf    点缓存，容量至少 capacity
 * @param cum_buf   累计长度缓存，容量至少 capacity
 * @param capacity  容量
 */
void nav_path_init(nav_path_t *path,
                   lv_point_precise_t *pt_buf,
                   float *cum_buf,
                   uint16_t capacity);

/**
 * 设置路径点（会复制进 pt_buf），并计算累计弧长/包围盒。
 * @return 实际写入点数
 */
uint16_t nav_path_set(nav_path_t *path,
                      const lv_point_precise_t *pts,
                      uint16_t count);

/**
 * Catmull-Rom 平滑。dst 需要自己提供缓存（建议容量为 src 的 6~10 倍）。
 * @param dst     输出路径
 * @param src     输入路径
 * @param subdiv  每段插值数，建议 6~10
 * @return 输出点数；0 表示失败/容量不足
 */
uint16_t nav_path_smooth_catmull(nav_path_t *dst,
                                 const nav_path_t *src,
                                 uint8_t subdiv);

/**
 * 按弧长取路径上的点与航向。
 * @param dist       距起点的距离（像素），会被 clamp 到 [0, length]
 * @param pt         输出点
 * @param angle_deg  输出航向角，0=向右，顺时针为正，范围 [0,360)
 * @return true 成功
 */
bool nav_path_point_at(const nav_path_t *path,
                       float dist,
                       lv_point_precise_t *pt,
                       float *angle_deg);

/**
 * 在 LV_EVENT_DRAW_MAIN 中绘制路径。
 * @param layer     lv_event_get_layer(e)
 * @param path      路径
 * @param style     样式
 * @param progress  0.0~1.0，已走过比例；<0 或 >1 会被 clamp
 */
void nav_path_draw(lv_layer_t *layer,
                   const nav_path_t *path,
                   const nav_path_style_t *style,
                   float progress);

#ifdef __cplusplus
}
#endif

#endif /* LV_NAV_PATH_H */
