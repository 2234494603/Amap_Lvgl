# MCU + LVGL + 高德投屏 / 导航路径显示

本目录是方案调研 + 可复用的 LVGL 8.3/9.x 路径与瓦片模块。

## 文档

- **`docs/01-方案调研-MCU+LVGL+高德投屏与路径显示.md`**
  - 高德智能两轮车 / 鸿蒙投屏 / AIoT RTOS 屏显 SDK / 静态地图 / 车机广播 的成熟方案对比；
  - 高德 SDK 的 ARM / RISC-V 架构与授权问题；
  - HX3602C 的硬件约束与 4 MB PSRAM 预算；
  - 用 LVGL 画路径的 5 种方法（lv_line、lv_draw_line、矢量 API、Canvas 缓存、视频叠加）；
  - 截图 UI 的图层/颜色/线宽/发光/转向卡参数；
  - 实时加载周围地图瓦片的完整设计（第 10 节）。
- **`docs/02-GUI-Guider-复刻步骤.md`**
  - 在 PC 上用 **GUI Guider 1.8/1.9（LVGL 8.3）** 复刻这版 UI 的步骤；
  - 1400×700 设计稿坐标、854×480 缩放表、完整图层树；
  - 地图卡片静态/分层/动态三种做法；
  - GUI Guider 自定义代码（`lv_line` 路径）、LVGL 8.3 API 对照、字体子集、导出/上板注意事项。

## 资源

- `assets/gui_guider/`
  - **AI 切片精确版（推荐）**：
    - `map_card_ai_600x465.png`：AI 原图整张地图卡片；
    - `map_bg_clean_600x465.png`：去路径/Pin/箭头的底图；
    - `route_ai_600x465.png`：AI 路径（透明，含发光）；
    - `pin_ai_40x52.png`、`arrow_ai_56x56.png`：AI Pin / 箭头；
    - `turn_card_ai_397x124.png`、`turn_card_ai_300x100.png`：转向卡；
    - `turn_icon_ai_48x56.png`：转向箭头（透明）；
    - `speed_ticks_ai_460x260.png`：AI 速度刻度环；
    - `icon_nav_ai_64.png`、`icon_fault_ai_64.png`、`icon_assistant_ai_64.png`、`icon_cast_ai_64.png`、`icon_settings_ai_64.png`：底部导航图标；
    - `icon_seat_ai_48.png`：座椅图标；
    - `album_art_ai_48.png`：AI 专辑封面；
    - `preview_map_card_ai_600x465.png`：AI 切片合成预览。
  - 另外：`screen_bg_1400x700.png`（屏幕背景）、`battery_grad_350x24.png`（电量渐变条）。
- `assets/ai_pages_1400x700/`：AI 7 个画板按 1400×700 渲染的整页参考图（page2=导航、page3=故障、page4=助手、page5=投屏、page6=设置）。
- `tools/slice_ai_map_assets.py`：从 AI 渲染的地图卡片切出精确分层资源。
- `tools/slice_ai_ui_assets.py`：从 AI 第 2 页切出底部导航/座椅/刻度等小图标。

> PC 原型可以用大图；上 HX3602C 前必须换成小图/瓦片/动态绘制，否则 Flash/PSRAM 放不下。

## 代码

### `src/lv_nav_path/`

LVGL 9.3+ 导航路径绘制：

- 路径点管理、累计弧长、Catmull-Rom 平滑、按距离取点/航向；
- 发光 → 描边 → 渐变主线 → 中心高光 → 已走过路径；
- 纯 C，不依赖 ThorVG，适合 HX3602C 这类 MCU。

最小用法：

```c
static lv_point_precise_t raw[64];
static lv_point_precise_t smooth[256];
static float cum[256];
static nav_path_t path;

nav_path_init(&path, smooth, cum, 256);
nav_path_set(&path, raw, raw_count);

nav_path_style_t style;
nav_path_style_init(&style);

/* 在卡片的 LV_EVENT_DRAW_MAIN 中 */
nav_path_draw(lv_event_get_layer(e), &path, &style, 0.45f);
```

> 路径点是屏幕绝对坐标；卡片移动时请加偏移，或构建局部坐标后在绘制时平移。

### `src/lv_nav_path_v8/`

LVGL 8.3 版本的路径绘制（GUI Guider 1.8/1.9 用）：

- 内部用 4 条 `lv_line` 叠加：发光 / 描边 / 主线 / 高光；
- `nav_path_v8_create()` / `set_points()` / `delete()`；
- 纯 LVGL 8.3 API，不依赖 LVGL 9 的 draw/vector API。

```c
static lv_point_t pts[] = { {325,448}, {330,392}, /* ... */ };

s_route = nav_path_v8_create(guider_ui.screen_cont_map, 128);
nav_path_v8_set_points(s_route, pts, sizeof(pts) / sizeof(pts[0]));
```

### `src/nav_tilemap/`

实时地图瓦片调度（纯 C，不依赖 LVGL）：

- 视口计算、`z/x/y` 范围、中心优先的环形加载顺序；
- 固定槽位缓存 + LRU + 世代号；
- `request_cb` 由应用实现（SD 卡 / HTTP / 手机下发）；
- `commit/fail` 完成回调；
- `screen_offset()` 直接给出 LVGL 贴图坐标。

```c
static nav_tilemap_t tm;
nav_tilemap_init(&tm, 128, 366, 300, 30, 3);
tm.release_cb = tile_free;

nav_tilemap_set_view(&tm, car_x, car_y, ahead_x, ahead_y, 17);
nav_tilemap_update(&tm, tile_request, NULL);

/* 加载线程完成后 */
nav_tilemap_commit(&tm, z, tx, ty, rgb565_buf);

/* UI 线程 */
int32_t x, y;
nav_tilemap_screen_offset(&tm, tx, ty, &x, &y);
lv_obj_set_pos(img, x, y);
```

## 推荐落地顺序

1. **方案 A（先做）**：手机高德渲染地图/路径 → JPEG/MJPEG → HX3602C 硬件解码 → 视频层；LVGL 只做仪表 UI。
2. **方案 B（并行申请）**：高德 AIoT RTOS 屏显 SDK + LVGL render adapter；需要高德授权和 RISC-V 库。
3. **方案 C（兜底）**：离线/自绘瓦片 + `nav_tilemap` + `lv_nav_path`；注意地图数据版权。

详见文档第 0 节和第 10 节。
