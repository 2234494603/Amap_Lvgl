# GaoDe Map dynamic navigation integration

This GUI Guider 1.10 project targets LVGL 8.3.10 at 480x272 and 16-bit color.
The editable project entry is `GaoDe_Map.guiguider`; the runtime navigation UI
is deliberately kept in `custom/` so GUI Guider code generation does not
overwrite it.

## Files

- `custom/lv_nav_path_v8.c/.h`: four-layer glowing route renderer for LVGL 8.3.
- `custom/custom.c/.h`: map layout, animated vehicle, destination marker,
  maneuver card, demo route and re-route behavior.
- `generated/`: minimal GUI Guider screen scaffold for the previously empty
  project. GUI Guider may regenerate these files.

## Supplying a real route

Route points use map-card-local coordinates: x=0..455, y=0..247.
The route data is copied, so the caller may reuse its input buffer.

```c
static const lv_point_t route[] = {
    {244, 232}, {250, 190}, {276, 151},
    {338, 122}, {404, 91}, {420, 57}
};

gaode_map_set_demo_enabled(false);
gaode_map_set_route(route, sizeof(route) / sizeof(route[0]));
gaode_map_set_navigation("500 m  ENTER", "SOFTWARE PARK 3RD RD");
```

Call the three functions above from the LVGL/UI thread after `custom_init()`.
If route data arrives from another task, copy it into a message/queue and apply
it in the UI task; LVGL objects must not be updated concurrently.

No compile or simulator run was performed while integrating this module.
