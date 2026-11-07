#ifndef PHONE_NAV_BRIDGE_H
#define PHONE_NAV_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the desktop phone-navigation bridge.
 *
 * On Windows Simulator builds this polls the atomic binary packet produced by
 * Test_Environment/bridge_server.py. Other targets receive harmless no-op
 * implementations and can feed their UART/BLE/Wi-Fi decoder directly into
 * gaode_map_set_route() and gaode_map_set_progress().
 */
void phone_nav_bridge_init(void);
void phone_nav_bridge_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
