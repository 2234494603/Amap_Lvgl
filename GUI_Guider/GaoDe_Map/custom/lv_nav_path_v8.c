/**
 * @file lv_nav_path_v8.c
 * @brief LVGL 8.3 implementation using four stacked lv_line objects.
 */
#include "lv_nav_path_v8.h"

#include <string.h>

static lv_obj_t *create_line(lv_obj_t *parent, int32_t width, uint32_t color, lv_opa_t opa)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_opa(line, opa, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    return line;
}

nav_path_v8_t *nav_path_v8_create(lv_obj_t *parent, uint16_t max_points)
{
    nav_path_v8_t *path;

    if(parent == NULL || max_points < 2) return NULL;

    path = (nav_path_v8_t *)lv_mem_alloc(sizeof(*path));
    if(path == NULL) return NULL;
    memset(path, 0, sizeof(*path));

    path->pts = (lv_point_t *)lv_mem_alloc((size_t)max_points * sizeof(lv_point_t));
    if(path->pts == NULL) {
        lv_mem_free(path);
        return NULL;
    }
    path->capacity = max_points;

    path->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(path->cont);
    lv_obj_set_size(path->cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(path->cont, 0, 0);
    lv_obj_clear_flag(path->cont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(path->cont, LV_OPA_TRANSP, 0);

    path->glow = create_line(path->cont, 16, 0x0067A8, LV_OPA_30);
    path->casing = create_line(path->cont, 11, 0x063A69, LV_OPA_COVER);
    path->main_line = create_line(path->cont, 7, 0x05B8FF, LV_OPA_COVER);
    path->core = create_line(path->cont, 2, 0xC8F7FF, LV_OPA_70);
    nav_path_v8_set_visible(path, false);
    return path;
}

void nav_path_v8_set_points(nav_path_v8_t *path, const lv_point_t *points, uint16_t count)
{
    if(path == NULL || path->pts == NULL || points == NULL) return;
    if(count > path->capacity) count = path->capacity;

    path->count = count;
    if(count < 2) {
        nav_path_v8_set_visible(path, false);
        return;
    }

    memcpy(path->pts, points, (size_t)count * sizeof(lv_point_t));
    lv_line_set_points(path->glow, path->pts, count);
    lv_line_set_points(path->casing, path->pts, count);
    lv_line_set_points(path->main_line, path->pts, count);
    lv_line_set_points(path->core, path->pts, count);
    nav_path_v8_set_visible(path, true);
}

void nav_path_v8_set_color(nav_path_v8_t *path, lv_color_t color)
{
    if(path != NULL) lv_obj_set_style_line_color(path->main_line, color, 0);
}

void nav_path_v8_set_visible(nav_path_v8_t *path, bool visible)
{
    if(path == NULL || path->cont == NULL) return;
    if(visible) lv_obj_clear_flag(path->cont, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(path->cont, LV_OBJ_FLAG_HIDDEN);
}

void nav_path_v8_delete(nav_path_v8_t *path)
{
    if(path == NULL) return;
    if(path->cont != NULL) lv_obj_del(path->cont);
    if(path->pts != NULL) lv_mem_free(path->pts);
    lv_mem_free(path);
}
