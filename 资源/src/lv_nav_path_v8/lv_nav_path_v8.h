/**
 * @file lv_nav_path_v8.h
 * @brief LVGL 8.3 导航路径绘制（GUI Guider 1.8/1.9 用）
 *
 * 思路：用 4 个叠加的 lv_line 做“发光 + 描边 + 主线 + 高光”。
 * 优点：不依赖 lv_canvas/draw API，LVGL 8.3 直接可用；
 * 缺点：单色，没有沿路径渐变；需要渐变时按段拆多条线，或用 lv_canvas。
 *
 * 用法：
 *   nav_path_v8_t *p = nav_path_v8_create(guider_ui.screen_cont_map, 128);
 *   nav_path_v8_set_points(p, pts, count);
 *   // 删除页面时：nav_path_v8_delete(p);
 *
 * 注意：lv_line 不复制点数组，本模块内部保存一份，调用者可以释放临时数组。
 */
#ifndef LV_NAV_PATH_V8_H
#define LV_NAV_PATH_V8_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *glow;
    lv_obj_t *casing;
    lv_obj_t *main_line;
    lv_obj_t *core;

    lv_point_t *pts;
    uint16_t capacity;
    uint16_t count;
} nav_path_v8_t;

/**
 * 创建路径图层。
 * @param parent     地图卡片容器
 * @param max_points 最大点数
 */
nav_path_v8_t *nav_path_v8_create(lv_obj_t *parent, uint16_t max_points);

/** 设置路径点（相对 parent 左上角）。 */
void nav_path_v8_set_points(nav_path_v8_t *p, const lv_point_t *pts, uint16_t count);

/** 设置主线颜色（默认 #2FA8FF）。 */
void nav_path_v8_set_color(nav_path_v8_t *p, lv_color_t color);

/** 显示/隐藏整条路径。 */
void nav_path_v8_set_visible(nav_path_v8_t *p, bool visible);

/** 删除并释放。 */
void nav_path_v8_delete(nav_path_v8_t *p);

#ifdef __cplusplus
}
#endif

#endif /* LV_NAV_PATH_V8_H */
