#include "phone_nav_bridge.h"

#include "custom.h"

#if defined(_WIN32)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHONE_ROUTE_HEADER_SIZE 28U
#define PHONE_ROUTE_MAX_POINTS  64U
#define PHONE_ROUTE_POLL_MS     250U
#define PHONE_ROUTE_MAX_SIZE    (PHONE_ROUTE_HEADER_SIZE + PHONE_ROUTE_MAX_POINTS * 4U)

static const char *const s_default_route_path =
    "C:/Users/Administrator/Documents/Adobe/GitHub/Test_Environment/bridge/latest_route.bin";

static lv_timer_t *s_poll_timer;
static uint32_t s_last_sequence = 0xffffffffU;
static uint32_t s_last_revision = 0xffffffffU;

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static const char *route_file_path(void)
{
    const char *override_path = getenv("GAODE_PHONE_ROUTE_FILE");
    return (override_path != NULL && override_path[0] != '\0')
               ? override_path
               : s_default_route_path;
}

static void poll_phone_route(lv_timer_t *timer)
{
    uint8_t packet[PHONE_ROUTE_MAX_SIZE];
    lv_point_t points[PHONE_ROUTE_MAX_POINTS];
    FILE *file;
    size_t size;
    uint16_t point_count;
    uint16_t progress;
    uint32_t revision;
    uint32_t sequence;
    uint16_t i;

    LV_UNUSED(timer);
    file = fopen(route_file_path(), "rb");
    if(file == NULL) return;
    size = fread(packet, 1U, sizeof(packet), file);
    fclose(file);

    if(size < PHONE_ROUTE_HEADER_SIZE) return;
    if(memcmp(packet, "GDRT", 4U) != 0 || packet[4] != 1U) return;

    point_count = read_u16_le(&packet[6]);
    revision = read_u32_le(&packet[8]);
    sequence = read_u32_le(&packet[12]);
    progress = read_u16_le(&packet[24]);

    if(point_count < 2U || point_count > PHONE_ROUTE_MAX_POINTS) return;
    if(size != PHONE_ROUTE_HEADER_SIZE + (size_t)point_count * 4U) return;
    if(sequence == s_last_sequence) return;

    for(i = 0U; i < point_count; i++) {
        uint16_t x = read_u16_le(&packet[PHONE_ROUTE_HEADER_SIZE + (size_t)i * 4U]);
        uint16_t y = read_u16_le(&packet[PHONE_ROUTE_HEADER_SIZE + (size_t)i * 4U + 2U]);
        if(x >= 456U || y >= 248U) return;
        points[i].x = (lv_coord_t)x;
        points[i].y = (lv_coord_t)y;
    }

    gaode_map_set_demo_enabled(false);
    if(revision != s_last_revision) {
        if(!gaode_map_set_route(points, point_count)) return;
        s_last_revision = revision;
    }
    gaode_map_set_progress(progress);
    s_last_sequence = sequence;
}

void phone_nav_bridge_init(void)
{
    if(s_poll_timer != NULL) return;
    s_last_sequence = 0xffffffffU;
    s_last_revision = 0xffffffffU;
    s_poll_timer = lv_timer_create(poll_phone_route, PHONE_ROUTE_POLL_MS, NULL);
}

void phone_nav_bridge_deinit(void)
{
    if(s_poll_timer != NULL) {
        lv_timer_del(s_poll_timer);
        s_poll_timer = NULL;
    }
}

#else

void phone_nav_bridge_init(void)
{
}

void phone_nav_bridge_deinit(void)
{
}

#endif
