#ifndef __CUSTOM_H_
#define __CUSTOM_H_

#include "gui_guider.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create the 480x272 navigation demo on the GUI Guider screen. */
void custom_init(lv_ui *ui);

/** Stop timers and remove objects created by custom_init(). */
void custom_deinit(void);

/**
 * Replace the active navigation route.
 * Points are local to the 456x248 map card. The data is copied internally.
 */
bool gaode_map_set_route(const lv_point_t *points, uint16_t count);

/** Update the maneuver text shown in the top-left card. */
void gaode_map_set_navigation(const char *distance_text, const char *road_text);

/** Enable or disable the built-in route/re-route animation. */
void gaode_map_set_demo_enabled(bool enabled);

#ifdef __cplusplus
}
#endif

#endif
