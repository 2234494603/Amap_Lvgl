/**
 * @file lv_nav_path_v8.h
 * @brief Lightweight glowing navigation path for LVGL 8.3.
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

nav_path_v8_t *nav_path_v8_create(lv_obj_t *parent, uint16_t max_points);
void nav_path_v8_set_points(nav_path_v8_t *path, const lv_point_t *points, uint16_t count);
void nav_path_v8_set_color(nav_path_v8_t *path, lv_color_t color);
void nav_path_v8_set_visible(nav_path_v8_t *path, bool visible);
void nav_path_v8_delete(nav_path_v8_t *path);

#ifdef __cplusplus
}
#endif

#endif
