/**
 * @file lv_nav_path_v8.c
 * @brief LVGL 8.3 导航路径绘制实现
 */
#include "lv_nav_path_v8.h"

#include <string.h>

static lv_obj_t *make_line(lv_obj_t *parent, int32_t width, uint32_t color, lv_opa_t opa)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

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
    if(parent == NULL || max_points < 2) return NULL;

    nav_path_v8_t *p = (nav_path_v8_t *)lv_mem_alloc(sizeof(nav_path_v8_t));
    if(p == NULL) return NULL;
    memset(p, 0, sizeof(*p));

    p->pts = (lv_point_t *)lv_mem_alloc((size_t)max_points * sizeof(lv_point_t));
    if(p->pts == NULL) {
        lv_mem_free(p);
        return NULL;
    }
    p->capacity = max_points;

    p->cont = lv_obj_create(parent);
    lv_obj_remove_style_all(p->cont);
    lv_obj_set_size(p->cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(p->cont, 0, 0);
    lv_obj_clear_flag(p->cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(p->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(p->cont, LV_OPA_TRANSP, 0);

    /* 顺序即 z-order：发光 -> 描边 -> 主线 -> 高光 */
    p->glow   = make_line(p->cont, 18, 0x0A4D8F, LV_OPA_20);
    p->casing = make_line(p->cont, 13, 0x062B5A, LV_OPA_80);
    p->main_line = make_line(p->cont, 7, 0x2FA8FF, LV_OPA_COVER);
    p->core   = make_line(p->cont, 2, 0xB9F2FF, LV_OPA_50);

    nav_path_v8_set_visible(p, false);
    return p;
}

void nav_path_v8_set_points(nav_path_v8_t *p, const lv_point_t *pts, uint16_t count)
{
    if(p == NULL || p->pts == NULL || pts == NULL || count == 0) return;

    if(count > p->capacity) count = p->capacity;
    memcpy(p->pts, pts, (size_t)count * sizeof(lv_point_t));
    p->count = count;

    if(count < 2) {
        nav_path_v8_set_visible(p, false);
        return;
    }

    lv_line_set_points(p->glow, p->pts, count);
    lv_line_set_points(p->casing, p->pts, count);
    lv_line_set_points(p->main_line, p->pts, count);
    lv_line_set_points(p->core, p->pts, count);

    nav_path_v8_set_visible(p, true);
}

void nav_path_v8_set_color(nav_path_v8_t *p, lv_color_t color)
{
    if(p == NULL) return;
    lv_obj_set_style_line_color(p->main_line, color, 0);
}

void nav_path_v8_set_visible(nav_path_v8_t *p, bool visible)
{
    if(p == NULL || p->cont == NULL) return;
    if(visible) {
        lv_obj_clear_flag(p->cont, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(p->cont, LV_OBJ_FLAG_HIDDEN);
    }
}

void nav_path_v8_delete(nav_path_v8_t *p)
{
    if(p == NULL) return;
    if(p->cont != NULL) {
        lv_obj_del(p->cont);
        p->cont = NULL;
    }
    if(p->pts != NULL) {
        lv_mem_free(p->pts);
        p->pts = NULL;
    }
    lv_mem_free(p);
}
