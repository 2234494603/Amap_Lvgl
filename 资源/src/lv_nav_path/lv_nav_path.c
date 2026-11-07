/**
 * @file lv_nav_path.c
 * @brief LVGL 9 导航路径绘制工具实现
 *
 * 绘制顺序：
 *   发光 -> 深色描边 -> 渐变主线 -> 中心高光 -> 已走过路径
 *
 * 设计取舍：
 *   - 不依赖 ThorVG，纯 C，适合 HX3602C 这类 192 MHz RISC-V；
 *   - 用 lv_draw_line_dsc_t.points 一次提交整条折线，减少 draw task；
 *   - 渐变按弧长重采样成 N 段（默认 48 段），每段一个 draw task；
 *   - 已走过路径用深灰覆盖，而不是把路径拆成两套几何。
 */
#include "lv_nav_path.h"

#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* 内部工具                                                                   */
/* -------------------------------------------------------------------------- */

static inline float nav_absf(float v)
{
    return v < 0.0f ? -v : v;
}

static void nav_path_rebuild_metrics(nav_path_t *path)
{
    if(path->count == 0) {
        path->length = 0.0f;
        path->bbox.x1 = path->bbox.y1 = 0;
        path->bbox.x2 = path->bbox.y2 = 0;
        return;
    }

    path->cum[0] = 0.0f;
    path->bbox.x1 = path->bbox.x2 = (lv_coord_t)path->pts[0].x;
    path->bbox.y1 = path->bbox.y2 = (lv_coord_t)path->pts[0].y;

    for(uint16_t i = 1; i < path->count; i++) {
        float dx = (float)path->pts[i].x - (float)path->pts[i - 1].x;
        float dy = (float)path->pts[i].y - (float)path->pts[i - 1].y;
        path->cum[i] = path->cum[i - 1] + sqrtf(dx * dx + dy * dy);

        lv_coord_t x = (lv_coord_t)path->pts[i].x;
        lv_coord_t y = (lv_coord_t)path->pts[i].y;
        if(x < path->bbox.x1) path->bbox.x1 = x;
        if(x > path->bbox.x2) path->bbox.x2 = x;
        if(y < path->bbox.y1) path->bbox.y1 = y;
        if(y > path->bbox.y2) path->bbox.y2 = y;
    }

    path->length = path->cum[path->count - 1];
}

static void nav_draw_segment(lv_layer_t *layer,
                             lv_point_precise_t a,
                             lv_point_precise_t b,
                             lv_color_t color,
                             lv_opa_t opa,
                             int32_t width);

static void nav_draw_polyline(lv_layer_t *layer,
                              const lv_point_precise_t *pts,
                              uint16_t count,
                              lv_color_t color,
                              lv_opa_t opa,
                              int32_t width)
{
    if(count < 2 || width <= 0 || opa <= LV_OPA_MIN) return;

#if NAV_PATH_USE_POINTS
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.points      = (lv_point_precise_t *)pts; /* lv_draw_line 内部会拷贝 */
    dsc.point_cnt   = (int32_t)count;
    dsc.color       = color;
    dsc.opa         = opa;
    dsc.width       = width;
    dsc.round_start = 1;
    dsc.round_end   = 1;
    lv_draw_line(layer, &dsc);
#else
    for(uint16_t i = 0; i + 1 < count; i++) {
        nav_draw_segment(layer, pts[i], pts[i + 1], color, opa, width);
    }
#endif
}

static void nav_draw_segment(lv_layer_t *layer,
                             lv_point_precise_t a,
                             lv_point_precise_t b,
                             lv_color_t color,
                             lv_opa_t opa,
                             int32_t width)
{
    if(width <= 0 || opa <= LV_OPA_MIN) return;

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.p1.x        = a.x;
    dsc.p1.y        = a.y;
    dsc.p2.x        = b.x;
    dsc.p2.y        = b.y;
    dsc.color       = color;
    dsc.opa         = opa;
    dsc.width       = width;
    dsc.round_start = 1;
    dsc.round_end   = 1;
    lv_draw_line(layer, &dsc);
}

/** 找到弧长 dist 所在线段：返回段索引 i 和段内比例 t。 */
static void nav_locate(const nav_path_t *path, float dist, uint16_t *idx, float *t)
{
    if(path->count < 2) {
        *idx = 0;
        *t = 0.0f;
        return;
    }

    if(dist < 0.0f) dist = 0.0f;
    if(dist > path->length) dist = path->length;

    uint16_t i = 0;
    while(i + 2 < path->count && path->cum[i + 1] < dist) {
        i++;
    }

    float seg = path->cum[i + 1] - path->cum[i];
    *idx = i;
    *t = (seg > 0.0001f) ? (dist - path->cum[i]) / seg : 0.0f;
    if(*t < 0.0f) *t = 0.0f;
    if(*t > 1.0f) *t = 1.0f;
}

/* -------------------------------------------------------------------------- */
/* 公共 API                                                                   */
/* -------------------------------------------------------------------------- */

void nav_path_style_init(nav_path_style_t *style)
{
    if(style == NULL) return;
    memset(style, 0, sizeof(*style));

    /* 外层发光：深蓝，低透明度，宽 */
    style->glow_color  = lv_color_hex(0x0A4D8F);
    style->glow_opa    = 40;
    style->glow_width  = 18;

    /* 深色描边 */
    style->casing_color = lv_color_hex(0x062B5A);
    style->casing_opa   = 200;
    style->casing_width = 11;

    /* 主线渐变：深蓝 -> 青 */
    style->color_start = lv_color_hex(0x0B63D8);
    style->color_end   = lv_color_hex(0x35C8FF);
    style->width       = 7;

    /* 中心高光 */
    style->core_color = lv_color_hex(0xB9F2FF);
    style->core_opa   = 150;
    style->core_width = 2;

    /* 已走过路径 */
    style->traveled_color = lv_color_hex(0x2A3A50);
    style->traveled_opa   = 220;

    style->gradient           = true;
    style->max_gradient_steps = 48;
}

void nav_path_init(nav_path_t *path,
                   lv_point_precise_t *pt_buf,
                   float *cum_buf,
                   uint16_t capacity)
{
    if(path == NULL) return;
    memset(path, 0, sizeof(*path));
    path->pts      = pt_buf;
    path->cum      = cum_buf;
    path->capacity = (pt_buf != NULL && cum_buf != NULL) ? capacity : 0;
}

uint16_t nav_path_set(nav_path_t *path,
                      const lv_point_precise_t *pts,
                      uint16_t count)
{
    if(path == NULL || path->pts == NULL || path->cum == NULL ||
       path->capacity == 0 || pts == NULL || count == 0) {
        if(path != NULL) {
            path->count = 0;
            path->length = 0.0f;
        }
        return 0;
    }

    if(count > path->capacity) count = path->capacity;

    /* 支持 src 与 dst 是同一块缓存（平滑函数会用到） */
    if(pts != path->pts) {
        memmove(path->pts, pts, (size_t)count * sizeof(lv_point_precise_t));
    }

    path->count = count;
    nav_path_rebuild_metrics(path);
    return count;
}

uint16_t nav_path_smooth_catmull(nav_path_t *dst,
                                 const nav_path_t *src,
                                 uint8_t subdiv)
{
    if(dst == NULL || src == NULL || dst->pts == NULL || dst->cum == NULL ||
       dst->capacity < 2 || src->count < 2 || subdiv < 1) {
        return 0;
    }

    /* 根据 dst 容量自动降低插值数，避免溢出 */
    uint32_t needed = (uint32_t)(src->count - 1) * subdiv + 1;
    while(needed > dst->capacity && subdiv > 1) {
        subdiv--;
        needed = (uint32_t)(src->count - 1) * subdiv + 1;
    }
    if(needed > dst->capacity) return 0;

    uint16_t out = 0;
    for(uint16_t i = 0; i + 1 < src->count; i++) {
        const lv_point_precise_t p0 = src->pts[(i > 0) ? (i - 1) : i];
        const lv_point_precise_t p1 = src->pts[i];
        const lv_point_precise_t p2 = src->pts[i + 1];
        const lv_point_precise_t p3 = src->pts[(i + 2 < src->count) ? (i + 2) : (i + 1)];

        for(uint8_t s = 0; s < subdiv; s++) {
            float t  = (float)s / (float)subdiv;
            float t2 = t * t;
            float t3 = t2 * t;

            float x = 0.5f * ((2.0f * p1.x) +
                              (-p0.x + p2.x) * t +
                              (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                              (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
            float y = 0.5f * ((2.0f * p1.y) +
                              (-p0.y + p2.y) * t +
                              (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                              (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);

            dst->pts[out].x = (lv_value_precise_t)(x + (x >= 0.0f ? 0.5f : -0.5f));
            dst->pts[out].y = (lv_value_precise_t)(y + (y >= 0.0f ? 0.5f : -0.5f));
            out++;
        }
    }

    dst->pts[out++] = src->pts[src->count - 1];
    nav_path_set(dst, dst->pts, out);
    return out;
}

bool nav_path_point_at(const nav_path_t *path,
                       float dist,
                       lv_point_precise_t *pt,
                       float *angle_deg)
{
    if(path == NULL || pt == NULL || path->count < 2 || path->length <= 0.0f) {
        return false;
    }

    if(dist < 0.0f) dist = 0.0f;
    if(dist > path->length) dist = path->length;

    uint16_t i;
    float t;
    nav_locate(path, dist, &i, &t);

    const lv_point_precise_t a = path->pts[i];
    const lv_point_precise_t b = path->pts[i + 1];
    float dx = (float)b.x - (float)a.x;
    float dy = (float)b.y - (float)a.y;

    pt->x = (lv_value_precise_t)((float)a.x + dx * t);
    pt->y = (lv_value_precise_t)((float)a.y + dy * t);

    if(angle_deg != NULL) {
        float angle = atan2f(dy, dx) * 57.295779513f;
        if(angle < 0.0f) angle += 360.0f;
        *angle_deg = angle;
    }

    return true;
}

void nav_path_draw(lv_layer_t *layer,
                   const nav_path_t *path,
                   const nav_path_style_t *style,
                   float progress)
{
    if(layer == NULL || path == NULL || style == NULL || path->count < 2) {
        return;
    }

    /* 1. 发光 */
    if(style->glow_width > 0 && style->glow_opa > LV_OPA_MIN) {
        nav_draw_polyline(layer, path->pts, path->count,
                          style->glow_color, style->glow_opa, style->glow_width);
    }

    /* 2. 深色描边 */
    if(style->casing_width > 0 && style->casing_opa > LV_OPA_MIN) {
        nav_draw_polyline(layer, path->pts, path->count,
                          style->casing_color, style->casing_opa, style->casing_width);
    }

    /* 3. 渐变主线 */
    if(style->width > 0) {
        if(style->gradient) {
            uint16_t steps = style->max_gradient_steps;
            if(steps < 2) steps = 2;
            if(steps > 64) steps = 64;

            /* 段数不多时逐段插值，最贴合原始折线 */
            if((uint16_t)(path->count - 1) <= steps) {
                uint16_t segs = path->count - 1;
                for(uint16_t i = 0; i < segs; i++) {
                    uint8_t mix = (uint8_t)((uint32_t)i * 255u / (segs > 0 ? segs : 1));
                    lv_color_t c = lv_color_mix(style->color_start, style->color_end, mix);
                    nav_draw_segment(layer, path->pts[i], path->pts[i + 1],
                                     c, LV_OPA_COVER, style->width);
                }
            }
            else {
                /* 段数很多时按弧长重采样，控制 draw task 数量 */
                lv_point_precise_t a, b;
                for(uint16_t s = 0; s < steps; s++) {
                    float d0 = path->length * (float)s / (float)steps;
                    float d1 = path->length * (float)(s + 1) / (float)steps;
                    if(!nav_path_point_at(path, d0, &a, NULL)) break;
                    if(!nav_path_point_at(path, d1, &b, NULL)) break;

                    uint8_t mix = (uint8_t)(((uint32_t)s * 2u + 1u) * 255u / (steps * 2u));
                    lv_color_t c = lv_color_mix(style->color_start, style->color_end, mix);
                    nav_draw_segment(layer, a, b, c, LV_OPA_COVER, style->width);
                }
            }
        }
        else {
            nav_draw_polyline(layer, path->pts, path->count,
                              style->color_end, LV_OPA_COVER, style->width);
        }
    }

    /* 4. 中心高光 */
    if(style->core_width > 0 && style->core_opa > LV_OPA_MIN) {
        nav_draw_polyline(layer, path->pts, path->count,
                          style->core_color, style->core_opa, style->core_width);
    }

    /* 5. 已走过路径：深灰覆盖 */
    if(progress > 0.0f && style->traveled_opa > LV_OPA_MIN) {
        if(progress > 1.0f) progress = 1.0f;
        float d = path->length * progress;
        if(d > 0.5f) {
            uint16_t idx;
            float t;
            nav_locate(path, d, &idx, &t);

            lv_point_precise_t cut;
            nav_path_point_at(path, d, &cut, NULL);

            /* 先覆盖掉发光/描边，再覆盖主线 */
            if(idx + 1 >= 2) {
                nav_draw_polyline(layer, path->pts, idx + 1,
                                  style->traveled_color, style->traveled_opa,
                                  style->casing_width + 2);
            }
            if(t > 0.001f) {
                nav_draw_segment(layer, path->pts[idx], cut,
                                 style->traveled_color, style->traveled_opa,
                                 style->casing_width + 2);
            }

            if(idx + 1 >= 2) {
                nav_draw_polyline(layer, path->pts, idx + 1,
                                  style->traveled_color, style->traveled_opa,
                                  style->width);
            }
            if(t > 0.001f) {
                nav_draw_segment(layer, path->pts[idx], cut,
                                 style->traveled_color, style->traveled_opa,
                                 style->width);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* LVGL 8.x 降级说明                                                          */
/* -------------------------------------------------------------------------- */
/*
 * LVGL 8.x 没有 lv_draw_line_dsc_t.points / lv_draw_line(layer, ...)。
 * 可选做法：
 *   1. 用 lv_line 控件：
 *        lv_obj_t *line = lv_line_create(card);
 *        lv_line_set_points(line, pts, count);
 *        lv_obj_set_style_line_width(line, 7, 0);
 *        lv_obj_set_style_line_color(line, lv_color_hex(0x2FA8FF), 0);
 *        lv_obj_set_style_line_rounded(line, true, 0);
 *      粗线拐角没有圆角 join，建议先做 Catmull-Rom 平滑。
 *   2. 用 lv_canvas + lv_draw_line(&draw_ctx, &dsc, &p1, &p2) 逐段画：
 *        lv_canvas_init_layer(canvas, &layer);  // v8 是 lv_canvas_init_layer
 *        for each segment: lv_draw_line(&layer, &dsc, &p1, &p2);
 *        lv_canvas_finish_layer(canvas, &layer);
 *   3. 渐变：逐段 lv_color_mix() 计算颜色。
 *   4. 发光：同一折线用更宽、更低透明度的样式重复画 2~3 遍。
 */
