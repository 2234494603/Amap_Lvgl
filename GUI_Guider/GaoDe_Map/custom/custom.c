/**
 * Dynamic LVGL 8.3 navigation demo for the 480x272 GaoDe_Map project.
 *
 * The visual structure is created here instead of editing generated files so
 * GUI Guider can regenerate the project without overwriting the route logic.
 */
#include "custom.h"
#include "lv_nav_path_v8.h"

#include <stdlib.h>

#define MAP_W               456
#define MAP_H               248
#define ROUTE_CAPACITY      64
#define DEMO_PERIOD_MS      50
#define DEMO_STEP           5

static lv_obj_t *s_map;
static lv_obj_t *s_vehicle_outer;
static lv_obj_t *s_vehicle_inner;
static lv_obj_t *s_vehicle_nose;
static lv_obj_t *s_pin_outer;
static lv_obj_t *s_pin_inner;
static lv_obj_t *s_distance_label;
static lv_obj_t *s_road_label;
static lv_timer_t *s_demo_timer;
static nav_path_v8_t *s_route;
static lv_point_t s_nose_points[2];
static uint16_t s_progress;
static uint8_t s_route_variant;
static bool s_demo_enabled = true;

static const lv_point_t s_route_a[] = {
    {244, 232}, {246, 212}, {252, 191}, {258, 168},
    {276, 151}, {306, 138}, {338, 122}, {370, 107},
    {404, 91}, {421, 76}, {420, 57}
};

static const lv_point_t s_route_b[] = {
    {244, 232}, {242, 210}, {239, 188}, {245, 164},
    {265, 146}, {296, 141}, {326, 132}, {350, 112},
    {374, 95}, {405, 82}, {420, 57}
};

static const lv_point_t s_road_h1[] = {{-20, 89}, {476, 88}};
static const lv_point_t s_road_h2[] = {{-20, 159}, {476, 160}};
static const lv_point_t s_road_h3[] = {{-20, 218}, {476, 218}};
static const lv_point_t s_road_v1[] = {{103, -20}, {102, 268}};
static const lv_point_t s_road_v2[] = {{211, -20}, {214, 268}};
static const lv_point_t s_road_v3[] = {{343, -20}, {339, 268}};
static const lv_point_t s_road_d1[] = {{-20, 203}, {92, 72}, {154, -20}};
static const lv_point_t s_road_d2[] = {{278, 268}, {291, 178}, {343, 92}, {476, 29}};

static void clear_widget(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_panel(lv_obj_t *parent, int16_t x, int16_t y,
                              int16_t width, int16_t height, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    clear_widget(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static lv_obj_t *create_road(lv_obj_t *parent, const lv_point_t *points,
                             uint16_t count, int16_t width, uint32_t color)
{
    lv_obj_t *line = lv_line_create(parent);
    clear_widget(line);
    lv_obj_set_size(line, LV_PCT(100), LV_PCT(100));
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

static void create_map_blocks(lv_obj_t *parent)
{
    static const lv_area_t blocks[] = {
        {14, 16, 86, 66}, {119, 14, 195, 72}, {229, 17, 321, 70}, {359, 14, 442, 69},
        {18, 99, 82, 145}, {119, 103, 194, 148}, {232, 93, 319, 151}, {361, 99, 441, 150},
        {15, 171, 87, 207}, {119, 171, 196, 211}, {230, 169, 325, 210}, {359, 171, 442, 207},
        {17, 229, 88, 246}, {118, 224, 195, 246}, {355, 225, 442, 246}
    };
    uint16_t i;

    for(i = 0; i < (uint16_t)(sizeof(blocks) / sizeof(blocks[0])); i++) {
        lv_obj_t *block = create_panel(parent, blocks[i].x1, blocks[i].y1,
                                       lv_area_get_width(&blocks[i]),
                                       lv_area_get_height(&blocks[i]), 0x101522);
        lv_obj_set_style_radius(block, 2, 0);
        lv_obj_set_style_bg_grad_color(block, lv_color_hex(0x080C15), 0);
        lv_obj_set_style_bg_grad_dir(block, LV_GRAD_DIR_VER, 0);
    }
}

static void create_map_roads(lv_obj_t *parent)
{
    create_road(parent, s_road_h1, 2, 5, 0x1B2230);
    create_road(parent, s_road_h2, 2, 6, 0x1A2230);
    create_road(parent, s_road_h3, 2, 5, 0x171F2C);
    create_road(parent, s_road_v1, 2, 5, 0x1B2230);
    create_road(parent, s_road_v2, 2, 5, 0x171F2B);
    create_road(parent, s_road_v3, 2, 6, 0x1B2332);
    create_road(parent, s_road_d1, 3, 7, 0x1A2230);
    create_road(parent, s_road_d2, 4, 6, 0x1D2635);
}

static lv_obj_t *create_round_marker(lv_obj_t *parent, int16_t size, uint32_t color)
{
    lv_obj_t *obj = create_panel(parent, 0, 0, size, size, color);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    return obj;
}

static void set_pin_position(const lv_point_t *point)
{
    if(point == NULL || s_pin_outer == NULL) return;
    lv_obj_set_pos(s_pin_outer, point->x - 13, point->y - 13);
    lv_obj_set_pos(s_pin_inner, point->x - 5, point->y - 5);
}

static uint16_t approx_distance(const lv_point_t *a, const lv_point_t *b)
{
    uint16_t dx = (uint16_t)abs((int)b->x - (int)a->x);
    uint16_t dy = (uint16_t)abs((int)b->y - (int)a->y);
    uint16_t max = dx > dy ? dx : dy;
    uint16_t min = dx > dy ? dy : dx;
    return (uint16_t)(max + (min >> 1));
}

static void update_vehicle(uint16_t progress)
{
    uint32_t total = 0;
    uint32_t target;
    uint32_t walked = 0;
    uint16_t i;
    lv_point_t p;
    lv_point_t a;
    lv_point_t b;
    int32_t dx;
    int32_t dy;
    int32_t scale;

    if(s_route == NULL || s_route->count < 2) return;
    for(i = 1; i < s_route->count; i++) total += approx_distance(&s_route->pts[i - 1], &s_route->pts[i]);
    if(total == 0) return;

    target = (total * progress) / 1000U;
    a = s_route->pts[0];
    b = s_route->pts[1];
    p = a;

    for(i = 1; i < s_route->count; i++) {
        uint16_t segment = approx_distance(&s_route->pts[i - 1], &s_route->pts[i]);
        a = s_route->pts[i - 1];
        b = s_route->pts[i];
        if(target <= walked + segment || i == s_route->count - 1) {
            uint32_t local = target > walked ? target - walked : 0;
            if(segment == 0) segment = 1;
            p.x = (lv_coord_t)(a.x + ((int32_t)(b.x - a.x) * (int32_t)local) / segment);
            p.y = (lv_coord_t)(a.y + ((int32_t)(b.y - a.y) * (int32_t)local) / segment);
            break;
        }
        walked += segment;
    }

    lv_obj_set_pos(s_vehicle_outer, p.x - 12, p.y - 12);
    lv_obj_set_pos(s_vehicle_inner, p.x - 5, p.y - 5);

    dx = (int32_t)b.x - a.x;
    dy = (int32_t)b.y - a.y;
    scale = abs((int)dx) > abs((int)dy) ? abs((int)dx) : abs((int)dy);
    if(scale == 0) scale = 1;
    s_nose_points[0] = p;
    s_nose_points[1].x = (lv_coord_t)(p.x + (dx * 15) / scale);
    s_nose_points[1].y = (lv_coord_t)(p.y + (dy * 15) / scale);
    lv_line_set_points(s_vehicle_nose, s_nose_points, 2);
}

bool gaode_map_set_route(const lv_point_t *points, uint16_t count)
{
    if(s_route == NULL || points == NULL || count < 2 || count > ROUTE_CAPACITY) return false;
    nav_path_v8_set_points(s_route, points, count);
    set_pin_position(&s_route->pts[s_route->count - 1]);
    s_progress = 0;
    update_vehicle(0);
    return true;
}

void gaode_map_set_navigation(const char *distance_text, const char *road_text)
{
    if(s_distance_label != NULL && distance_text != NULL) lv_label_set_text(s_distance_label, distance_text);
    if(s_road_label != NULL && road_text != NULL) lv_label_set_text(s_road_label, road_text);
}

void gaode_map_set_demo_enabled(bool enabled)
{
    s_demo_enabled = enabled;
}

static void demo_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if(!s_demo_enabled || s_route == NULL) return;

    if(s_progress + DEMO_STEP > 1000U) {
        s_route_variant ^= 1U;
        if(s_route_variant == 0U) {
            gaode_map_set_route(s_route_a, (uint16_t)(sizeof(s_route_a) / sizeof(s_route_a[0])));
            gaode_map_set_navigation("500 m  ENTER", "SOFTWARE PARK 3RD RD");
        }
        else {
            gaode_map_set_route(s_route_b, (uint16_t)(sizeof(s_route_b) / sizeof(s_route_b[0])));
            gaode_map_set_navigation("320 m  KEEP RIGHT", "SCIENCE AVENUE");
        }
    }
    else {
        s_progress = (uint16_t)(s_progress + DEMO_STEP);
        update_vehicle(s_progress);
    }
}

static void create_turn_card(lv_obj_t *parent)
{
    lv_obj_t *card = create_panel(parent, 16, 14, 252, 72, 0x0B0F19);
    lv_obj_t *icon;
    lv_obj_t *accent;

    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x1C2637), 0);
    lv_obj_set_style_shadow_width(card, 16, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);

    accent = create_panel(card, 0, 0, 4, 72, 0x00AFFF);
    lv_obj_set_style_radius(accent, 2, 0);

    icon = lv_label_create(card);
    clear_widget(icon);
    lv_label_set_text(icon, LV_SYMBOL_RIGHT);
    lv_obj_set_pos(icon, 18, 17);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);

    s_distance_label = lv_label_create(card);
    clear_widget(s_distance_label);
    lv_label_set_text(s_distance_label, "500 m  ENTER");
    lv_obj_set_pos(s_distance_label, 62, 10);
    lv_obj_set_style_text_color(s_distance_label, lv_color_hex(0xE8EDF5), 0);
    lv_obj_set_style_text_font(s_distance_label, &lv_font_montserrat_14, 0);

    s_road_label = lv_label_create(card);
    clear_widget(s_road_label);
    lv_obj_set_width(s_road_label, 178);
    lv_label_set_long_mode(s_road_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_road_label, "SOFTWARE PARK 3RD RD");
    lv_obj_set_pos(s_road_label, 62, 37);
    lv_obj_set_style_text_color(s_road_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_road_label, &lv_font_montserrat_14, 0);
}

static void create_status_chip(lv_obj_t *parent)
{
    lv_obj_t *chip = create_panel(parent, 369, 14, 69, 28, 0x0A111D);
    lv_obj_t *label = lv_label_create(chip);
    lv_obj_set_style_radius(chip, 14, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, lv_color_hex(0x15344D), 0);
    clear_widget(label);
    lv_label_set_text(label, "NAV LIVE");
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x54D4FF), 0);
}

void custom_init(lv_ui *ui)
{
    lv_obj_t *screen;

    if(ui == NULL || ui->screen == NULL) return;
    custom_deinit();
    screen = ui->screen;
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x03050A), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    s_map = create_panel(screen, 12, 12, MAP_W, MAP_H, 0x070B13);
    lv_obj_set_style_radius(s_map, 22, 0);
    lv_obj_set_style_clip_corner(s_map, true, 0);
    lv_obj_set_style_border_width(s_map, 1, 0);
    lv_obj_set_style_border_color(s_map, lv_color_hex(0x172338), 0);
    lv_obj_set_style_bg_grad_color(s_map, lv_color_hex(0x03060C), 0);
    lv_obj_set_style_bg_grad_dir(s_map, LV_GRAD_DIR_VER, 0);

    create_map_blocks(s_map);
    create_map_roads(s_map);

    s_route = nav_path_v8_create(s_map, ROUTE_CAPACITY);
    if(s_route == NULL) return;
    nav_path_v8_set_points(s_route, s_route_a,
                           (uint16_t)(sizeof(s_route_a) / sizeof(s_route_a[0])));

    s_vehicle_nose = create_road(s_map, s_nose_points, 2, 5, 0xD8FAFF);
    s_vehicle_outer = create_round_marker(s_map, 24, 0x1769FF);
    lv_obj_set_style_border_width(s_vehicle_outer, 3, 0);
    lv_obj_set_style_border_color(s_vehicle_outer, lv_color_hex(0xDDF8FF), 0);
    lv_obj_set_style_shadow_width(s_vehicle_outer, 12, 0);
    lv_obj_set_style_shadow_color(s_vehicle_outer, lv_color_hex(0x008CFF), 0);
    lv_obj_set_style_shadow_opa(s_vehicle_outer, LV_OPA_50, 0);
    s_vehicle_inner = create_round_marker(s_map, 10, 0xF4FCFF);

    s_pin_outer = create_round_marker(s_map, 26, 0x00AFFF);
    lv_obj_set_style_border_width(s_pin_outer, 4, 0);
    lv_obj_set_style_border_color(s_pin_outer, lv_color_hex(0x072B4F), 0);
    lv_obj_set_style_shadow_width(s_pin_outer, 10, 0);
    lv_obj_set_style_shadow_color(s_pin_outer, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_shadow_opa(s_pin_outer, LV_OPA_50, 0);
    s_pin_inner = create_round_marker(s_map, 10, 0x06101D);
    set_pin_position(&s_route->pts[s_route->count - 1]);
    update_vehicle(0);

    create_turn_card(s_map);
    create_status_chip(s_map);

    s_demo_timer = lv_timer_create(demo_timer_cb, DEMO_PERIOD_MS, NULL);
}

void custom_deinit(void)
{
    if(s_demo_timer != NULL) {
        lv_timer_del(s_demo_timer);
        s_demo_timer = NULL;
    }
    if(s_route != NULL) {
        nav_path_v8_delete(s_route);
        s_route = NULL;
    }
    if(s_map != NULL) {
        lv_obj_del(s_map);
        s_map = NULL;
    }
    s_vehicle_outer = NULL;
    s_vehicle_inner = NULL;
    s_vehicle_nose = NULL;
    s_pin_outer = NULL;
    s_pin_inner = NULL;
    s_distance_label = NULL;
    s_road_label = NULL;
    s_progress = 0;
    s_route_variant = 0;
}
