/**
 * Dynamic LVGL 8.3 navigation demo for the 480x272 GaoDe_Map project.
 *
 * The visual structure is created here instead of editing generated files so
 * GUI Guider can regenerate the project without overwriting the route logic.
 */
#include "custom.h"
#include "lv_nav_path_v8.h"

#include <stdlib.h>

LV_IMG_DECLARE(gaode_map_bg_3d);
LV_IMG_DECLARE(gaode_vehicle_arrow);
LV_IMG_DECLARE(gaode_destination_pin);

#define MAP_W               456
#define MAP_H               248
#define ROUTE_CAPACITY      64
#define DEMO_PERIOD_MS      50
#define DEMO_STEP           5

static lv_obj_t *s_map;
static lv_obj_t *s_vehicle_img;
static lv_obj_t *s_pin_img;
static lv_obj_t *s_distance_label;
static lv_obj_t *s_road_label;
static lv_timer_t *s_demo_timer;
static nav_path_v8_t *s_route;
static uint16_t s_progress;
static uint8_t s_route_variant;
static bool s_demo_enabled = true;

static const lv_point_t s_route_a[] = {
    {244, 232}, {248, 208}, {253, 183}, {262, 160},
    {286, 145}, {318, 135}, {350, 122}, {380, 109},
    {405, 93}, {415, 75}, {397, 60}, {376, 51}
};

static const lv_point_t s_route_b[] = {
    {244, 232}, {239, 208}, {235, 183}, {242, 158},
    {268, 142}, {302, 136}, {332, 119}, {356, 99},
    {382, 84}, {405, 72}, {395, 58}, {376, 51}
};

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

static void set_pin_position(const lv_point_t *point)
{
    if(point == NULL || s_pin_img == NULL) return;
    lv_obj_set_pos(s_pin_img, point->x - 15, point->y - 39);
}

static int16_t heading_angle(int32_t dx, int32_t dy)
{
    int32_t ax = abs((int)dx);
    int32_t ay = abs((int)dy);

    if(ay * 2 < ax) return dx >= 0 ? 900 : 2700;
    if(ax * 2 < ay) return dy >= 0 ? 1800 : 0;
    if(dx >= 0 && dy < 0) return 450;
    if(dx >= 0 && dy >= 0) return 1350;
    if(dx < 0 && dy >= 0) return 2250;
    return 3150;
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

    dx = (int32_t)b.x - a.x;
    dy = (int32_t)b.y - a.y;
    lv_obj_set_pos(s_vehicle_img, p.x - 21, p.y - 21);
    lv_img_set_angle(s_vehicle_img, heading_angle(dx, dy));
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
    lv_obj_t *card = create_panel(parent, 16, 14, 300, 72, 0x0B0F19);
    lv_obj_t *icon;

    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x1C2637), 0);
    lv_obj_set_style_shadow_width(card, 16, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);

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
    lv_obj_set_width(s_road_label, 226);
    lv_label_set_long_mode(s_road_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_road_label, "SOFTWARE PARK 3RD RD");
    lv_obj_set_pos(s_road_label, 62, 37);
    lv_obj_set_style_text_color(s_road_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_road_label, &lv_font_montserrat_14, 0);
}

void custom_init(lv_ui *ui)
{
    lv_obj_t *screen;
    lv_obj_t *map_background;

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

    map_background = lv_img_create(s_map);
    clear_widget(map_background);
    lv_img_set_src(map_background, &gaode_map_bg_3d);
    lv_obj_set_pos(map_background, 0, 0);

    s_route = nav_path_v8_create(s_map, ROUTE_CAPACITY);
    if(s_route == NULL) return;
    nav_path_v8_set_points(s_route, s_route_a,
                           (uint16_t)(sizeof(s_route_a) / sizeof(s_route_a[0])));

    s_pin_img = lv_img_create(s_map);
    clear_widget(s_pin_img);
    lv_img_set_src(s_pin_img, &gaode_destination_pin);

    s_vehicle_img = lv_img_create(s_map);
    clear_widget(s_vehicle_img);
    lv_img_set_src(s_vehicle_img, &gaode_vehicle_arrow);
    lv_img_set_pivot(s_vehicle_img, 21, 21);
    set_pin_position(&s_route->pts[s_route->count - 1]);
    update_vehicle(0);

    create_turn_card(s_map);

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
    s_vehicle_img = NULL;
    s_pin_img = NULL;
    s_distance_label = NULL;
    s_road_label = NULL;
    s_progress = 0;
    s_route_variant = 0;
}
