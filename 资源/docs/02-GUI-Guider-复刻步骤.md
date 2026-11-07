# 用 GUI Guider 在 PC 上复刻这版仪表 UI

> 目标：先在电脑上把截图这版 UI 做出来（可演示、可改），不依赖 HX3602C 硬件。
> 默认按 **1400×700 设计稿坐标** 描述；如果实际屏幕是 854×480 / 800×480，见第 2 节的缩放表。
> GUI Guider 版本很关键：**先确认 HX3602C SDK 里的 LVGL 版本**，再选 GUI Guider 版本，见第 1 节。
>
> **本项目已确认：GUI Guider 1.8/1.9（LVGL 8.3）+ 1400×700 + 地图卡片分层（底图+路径+Pin+箭头）。**
> 下面所有 API 以 **LVGL 8.3** 为准；LVGL 9 的 `lv_image_*` 在 8.3 里叫 `lv_img_*`，`lv_point_precise_t` 叫 `lv_point_t`。对照表见 9.5。

---

## 1. 先定版本：GUI Guider 和 LVGL 必须和最终 SDK 对齐

| GUI Guider | 内置 LVGL | 适用 |
|---|---|---|
| v2.0.x | LVGL 9.4 | 新项目、HX3602C SDK 是 LVGL 9.x |
| v1.10.x | LVGL 9.x | 同上，保守一些 |
| v1.8.x / v1.9.x | LVGL 8.3 / 8.x | HX3602C SDK 是 LVGL 8.3（很多原厂 SDK 仍是 8.3） |

**不要用 GUI Guider v2.0 生成一套 LVGL 9.4 代码，再往 LVGL 8.3 的 SDK 里塞**：`lv_obj`/`lv_image`/draw API 都有差异，返工量很大。

**本项目已确认：GUI Guider 1.8/1.9 → LVGL 8.3。** 下面按这个版本写。

建议：

- 先问原厂/HX3602C SDK：`lvgl.h` 的 `LVGL_VERSION_MAJOR/MINOR/PATCH` 是多少；
- SDK 是 8.3 → 用 GUI Guider 1.8.x/1.9.x 做 PC 原型；
- SDK 是 9.x → 才用 GUI Guider 2.0.x；
- 如果还没定，先用 **GUI Guider 1.8.x** 做“视觉稿/交互稿”，只把它当设计工具，不直接拿生成的代码上板；等 SDK 版本确定后再迁移。

---

## 2. 分辨率与缩放

### 2.1 设计稿坐标（1400×700）

截图是 1400×700（2:1）。GUI Guider 新建工程时可以直接填 1400×700（如果版本支持自定义分辨率；不支持就用 1280×640，按比例缩放）。

### 2.2 缩放到实际屏幕

| 目标分辨率 | 等比缩放 | 内容尺寸 | 上下留白 | 说明 |
|---|---:|---:|---:|---|
| 854×480（HX3602C 横屏常见） | 0.61 | 854×427 | 上下各 26 px | 推荐先按这个做设备版 |
| 800×480 | 0.571 | 800×400 | 上下各 40 px | 7 寸屏常见 |
| 1024×600 | 0.731 | 1024×512 | 上下各 44 px | 10 寸屏 |
| 480×854（竖屏） | 不适用 | — | — | 布局要重做，不是简单缩放 |

缩放公式（以 854×480 为例）：

```
x' = round(x * 0.61)
y' = round(y * 0.61) + 26
w' = round(w * 0.61)
h' = round(h * 0.61)
```

> 建议：**先做 1400×700 的 PC 演示版**给领导看；确认后，再新建一个 854×480 工程按上表重排。不要在 1400×700 工程里直接“整体缩放”导出到 MCU，字体/线宽会糊。

---

## 3. 新建 GUI Guider 工程

1. `Create Project` → `Simulator`（先不选具体板卡）；
2. 工程名：`ev_dash`；
3. 分辨率：`1400×700`（或按第 2 节目标分辨率）；
4. 颜色深度：`16 bit`（RGB565，和 HX3602C 一致；不要用 32 bit 做最终稿，颜色会偏）；
5. LVGL 版本：按第 1 节选择；
6. 创建后先把默认 screen 的背景改成 `#05070D`。

---

## 4. 资源准备

已生成一套 PNG，在：

```
assets/gui_guider/
```

> **切图来源**：`电动车智能显示屏修图-Q.ai` 第 2 页（导航主界面），用 PyMuPDF 以 12× 渲染成 3742×1906，再用 `tools/slice_ai_map_assets.py` / `tools/slice_ai_ui_assets.py` 切出。不是从 1400×700 截图切的，边缘和文字更清晰。

**优先使用从 `.ai` 切出来的精确版：**

| 文件 | 用途 | 建议导入格式 |
|---|---|---|
| `map_card_ai_600x465.png` | **AI 原图整张地图卡片**（最准，最快出效果） | True color，RGB565 |
| `map_bg_clean_600x465.png` | 去掉路径/Pin/箭头/转向卡的底图（分层用） | True color，RGB565 |
| `route_ai_600x465.png` | AI 路径（透明，含发光/渐变） | True color with alpha |
| `pin_ai_40x52.png` | AI 目的地 Pin（透明） | True color with alpha |
| `arrow_ai_56x56.png` | AI 自车箭头（透明） | True color with alpha |
| `turn_card_ai_397x124.png` | AI 转向卡（按 AI 卡片比例） | True color with alpha |
| `turn_card_ai_300x100.png` | 转向卡（按截图比例，可选） | True color with alpha |
| `turn_icon_ai_48x56.png` | 转向白色箭头（透明，做可编辑卡片时用） | True color with alpha |
| `speed_ticks_ai_460x260.png` | AI 速度刻度环（透明） | True color with alpha |
| `icon_nav_ai_64.png` | 底部“导航”图标（白） | True color with alpha |
| `icon_fault_ai_64.png` | 底部“故障”图标（蓝） | True color with alpha |
| `icon_assistant_ai_64.png` | 底部“助手”图标（蓝） | True color with alpha |
| `icon_cast_ai_64.png` | 底部“投屏”图标（蓝） | True color with alpha |
| `icon_settings_ai_64.png` | 底部“设置”图标（蓝） | True color with alpha |
| `icon_seat_ai_48.png` | “打开座舱”座椅图标（白） | True color with alpha |
| `album_art_ai_48.png` | AI 专辑封面 | True color |
| `preview_map_card_ai_600x465.png` | AI 切片合成预览（不进工程） | — |

> `assets/ai_pages_1400x700/` 是 AI 7 个画板按 1400×700 渲染的整页参考图：
> page2=导航、page3=故障、page4=助手、page5=投屏、page6=设置、page0=开机 Hi、page1=首页。
> 做其他页面时可以直接参考。

### 4.1 重新切图 / 改尺寸

```powershell
# 从 .ai 第 2 页重新渲染并切图（脚本会自动渲染，不需要中间文件）
python tools/slice_ai_map_assets.py
python tools/slice_ai_ui_assets.py
```

- 改分辨率：改脚本里的 `BOX_*` 裁剪框和输出尺寸；
- 要 **1:1 干净底图**：在 Illustrator 里隐藏“路径 / Pin / 箭头 / 转向卡”四个对象，单独导出一张 600×465 PNG，替换 `map_bg_clean_600x465.png`；
- AI 卡片原始比例约 600×498，如果不想轻微压扁，把 `cont_map` 高度改成 498，内部坐标按比例微调。

> **PC 原型可以大胆用大图；上板前必须换掉**：
> - `screen_bg` 1400×700 RGB565 ≈ 1.96 MB，不能进 MCU Flash；
> - `map_bg_clean` 600×465 ≈ 156 KB，也偏大；
> - `route` 600×465 RGBA ≈ 1.1 MB，太大。
>
> 设备版建议：背景用纯色/小尺寸渐变；地图用 128×128 瓦片；路径用 `lv_line`/`lv_canvas` 动态画。见第 9 节。
>
> `map_bg_clean_600x465.png` 是去路径/Pin/箭头的底图；路径/Pin/箭头/转向卡会重新盖上去。要 100% 干净底图，请在 AI 里隐藏对应对象单独导出。

导入方法：GUI Guider 左侧 `Resource` → `Image` → `Add` → 选择 PNG → `Color format` 选 `True color` 或 `True color with alpha` → `Apply`。同名资源不要重复导入。

---

## 5. 图层树（按 z-order 从下到上）

```
screen (1400×700, bg #05070D)
├─ img_screen_bg           0,0 1400×700
├─ cont_top                0,0 1400×90
│  ├─ lbl_time             55,25  170×70   "10:30"
│  ├─ lbl_date             225,55 220×30   "5月20日 星期二"
│  └─ lbl_mode             1180,35 190×28  "蓝牙 / 运动模式"
├─ cont_map                45,120 600×465  radius 20, border 1 #1E3A5F, clip corner
│  ├─ img_map_bg           0,0 600×465   ← map_bg_clean_600x465.png
│  ├─ img_route            0,0 600×465   ← route_ai_600x465.png
│  ├─ img_pin              506,58 40×52  ← pin_ai_40x52.png（尖端落在路径终点）
│  ├─ img_arrow            286,435 56×56 ← arrow_ai_56x56.png（底部被卡片裁掉）
│  └─ img_turn             20,6 397×124  ← turn_card_ai_397x124.png
│     （如果要可编辑文字，把 img_turn 换成 cont_turn + img_turn_icon + lbl_turn）
│     ├─ img_turn_icon     28,15 58×85   ← turn_icon_ai_48x56.png 放大
│     └─ lbl_turn          110,20 260×80  "500米后 进入\n软件园三号路"
├─ line_divider            700,110 1×475  #1F2937
- img_speed_ticks 放 speed_ticks_ai_460x260.png（AI 切片）；
├─ lbl_speed               850,155 280×140 "03"      font 120
├─ lbl_speed_unit          1130,255 100×30 "KM/H"    font 24
├─ btn_eco                 840,320 80×36  "ECO"
├─ btn_d                  980,320 80×36  "D"
├─ btn_s                 1120,320 80×36  "S"
├─ lbl_battery             790,390 80×30  "100%"
├─ bar_battery             870,392 350×24
├─ lbl_charge              1235,388 40×32  ⚡
├─ btn_seat                780,450 220×100
│  ├─ img_seat_icon        20,30 36×36
│  └─ lbl_seat             65,38 140×30 "打开座舱"
├─ cont_media              1060,450 220×100
│  ├─ img_album            15,26 48×48
│  ├─ lbl_song             75,22 130×24 "竞气"
│  ├─ lbl_artist           75,50 130×20 "陈奕迅/Eason"
│  ├─ lbl_prev             75,68 28×28
│  ├─ lbl_play             120,66 30×30
│  └─ lbl_next             165,68 28×28
└─ cont_nav                0,610 1400×80
   ├─ nav_item_0           120,5 120×70 "导航"
   ├─ nav_item_1           380,5 120×70 "故障"
   ├─ nav_item_2           640,5 120×70 "助手"   ← 截图红箭头指向这个，做选中态
   ├─ nav_item_3           900,5 120×70 "投屏"
   └─ nav_item_4           1160,5 120×70 "设置"
```

> `cont_map` 一定要开 **Clip corner**（圆角裁剪），否则路径/箭头会溢出卡片。
> `img_route`、`img_pin`、`img_arrow` 是透明 PNG，导入时选 **True color with alpha**。

---

## 6. 样式与字体

### 6.1 颜色

| 名称 | 值 | 用途 |
|---|---|---|
| bg | `#05070D` | 屏幕底色 |
| card | `#0A0E18` | 卡片 |
| card_border | `#1E2A3A` | 卡片描边 |
| text_primary | `#F2F7FF` | 主文字 |
| text_secondary | `#8A94A6` | 副文字 |
| accent | `#2FA8FF` | 高亮/选中/路径 |
| accent_2 | `#35C8FF` | 路径渐变终点 |
| success | `#48E0B0` | 电量渐变终点 |
| danger | `#FF5C5C` | 故障/超速 |

### 6.2 字体

| 用途 | 字体 | 字号 | 说明 |
|---|---|---|---|
| 时间/速度 | Bebas Neue / DIN Condensed / 阿里巴巴普惠体 Bold | 64 / 120 | 数字要窄、粗；只生成数字 + `:` + `%` + `KMH` |
| 日期/导航文字 | 思源黑体 / 阿里巴巴普惠体 | 24 / 20 | 中文子集 |
| 按钮/底部栏 | 同上 | 18 | 中文子集 |
| 副文字 | 同上 | 14 | 中文子集 |

**中文字体子集**：在 GUI Guider 的字体工具里选择 TTF，把下面这段文字粘到“Symbols/Range”里，只生成用到的字：

```
0123456789:%KMH
5月20日星期二蓝牙运动模式米后进入软件园三号路打开座舱竞气陈奕迅导航故障助手投屏设置
```

> 如果 GUI Guider 只生成 ASCII，中文会显示方块。检查字体是否包含 CJK，并确认 UTF-8 输入。
> 不要给整个中文字库（几 MB），HX3602C 的 Flash 吃不消。

---

## 7. 地图卡片：先做静态，再做动态

### 7.1 最快方案：整张图占位（30 分钟）

1. 把 `map_card_ai_600x465.png` 导入（AI 原图，最准）；
2. 在 `cont_map` 里放一个 `img_map_full`，0,0，600×465；
3. 这样路径、Pin、箭头、转向卡都已经是图里的，UI 看起来 100% 像设计稿；
4. 适合先给领导看整体效果，不适合后续改文字/路线。

### 7.2 推荐方案：分层（1~2 小时）

1. `img_map_bg` 放 `map_bg_clean_600x465.png`（AI 底图去路径版）；
2. `img_route` 放 `route_ai_600x465.png`，0,0，600×465；
3. `img_pin` 放 `pin_ai_40x52.png`，506,58；
4. `img_arrow` 放 `arrow_ai_56x56.png`，286,435；
5. `img_turn` 放 `turn_card_ai_397x124.png`，20,6；如果要改文字/图标，用 `cont_turn` + `turn_icon_ai_48x56.png` + `lbl_turn`；
6. 需要动路径时，用第 9 节的 `lv_line`/`lv_canvas` 替换 `img_route`。

**Pin 对齐**：Pin 图片的尖端应该落在路径终点。AI 切片版直接用 pin_ai_40x52.png + 位置 506,58。
**箭头旋转（LVGL 8.3）**：

```c
/* 图片默认朝上；航向 0°=向上，向右=90°，单位 0.1° */
lv_img_set_pivot(img_arrow, 28, 28);   /* 旋转中心：图片中心 */
lv_img_set_angle(img_arrow, heading_deg * 10);
```

> LVGL 8.3 用 `lv_img_set_angle()`，不是 LVGL 9 的 `lv_image_set_rotation()`。
> 旋转图片建议导入为 `True color with alpha`；如果只有调色板格式，旋转可能不生效。
> GUI Guider 里可以先不转，代码里再改。

### 7.3 转向卡片

- `cont_turn`：300×100，radius 16，bg `#0B0F18`，opa 约 220，shadow 12/80；
- 图标：用 Font Awesome 的 `turn-right`，或直接放一张 52×52 PNG；
- 文本：`500米后 进入\n软件园三号路`，行距 4~6，左对齐；
- 文字超出时用 `LV_LABEL_LONG_MODE_DOTS` 或缩小字号；道路名可能很长，预留 2 行。

---

## 8. 速度、电量、媒体、底部栏

### 8.1 速度刻度

两种做法：

**A. 图片刻度 + 数字标签（推荐，最稳）**

- img_speed_ticks 放 speed_ticks_ai_460x260.png（AI 切片）；
- `lbl_speed` 叠在中间，字体 120；
- 蓝色激活段已经烘在刻度图里（左侧 4 根蓝刻度）；做动态时再用 `lv_arc` 覆盖。

**B. `lv_arc` 动态**

- `lv_arc_create`，尺寸 400×400，位置 840,55；
- `lv_arc_set_bg_angles(arc, 150, 390)`；
- `lv_arc_set_angles(arc, 150, 150 + value/80*240)`；
- `LV_PART_MAIN` 弧宽 8、颜色 `#1F2937`；
- `LV_PART_INDICATOR` 弧宽 8、颜色 `#2FA8FF`；
- 去掉 knob：`lv_obj_remove_style(arc, NULL, LV_PART_KNOB)`。

> 不要同时用“刻度图里的蓝刻度”和“lv_arc”，会重影。二选一。

**C. `lv_meter`（LVGL 8.3 可用）**

LVGL 8.3 自带 `lv_meter`，可以同时画刻度、彩色分段和指示弧，适合速度表。GUI Guider 1.8 的控件栏里如果能看到 Meter，直接拖；看不到就用代码：

```c
lv_obj_t *meter = lv_meter_create(scr);
lv_obj_set_size(meter, 460, 260);
lv_obj_set_pos(meter, 810, 55);
lv_obj_set_style_bg_opa(meter, LV_OPA_TRANSP, 0);
lv_obj_set_style_border_width(meter, 0, 0);

lv_meter_scale_t *scale = lv_meter_add_scale(meter);
lv_meter_set_scale_ticks(meter, scale, 49, 2, 13, lv_color_hex(0x4B5563));
lv_meter_set_scale_major_ticks(meter, scale, 4, 4, 26, lv_color_hex(0x4B5563), 0);
lv_meter_set_scale_range(meter, scale, 0, 80, 240, 150);

lv_meter_indicator_t *arc = lv_meter_add_arc(meter, scale, 8, lv_color_hex(0x2FA8FF), 0);
lv_meter_set_indicator_start_value(meter, arc, 0);
lv_meter_set_indicator_end_value(meter, arc, 3);
```

> 如果 GUI Guider 1.8 没有 Meter 控件，代码创建也可以；它比 `lv_arc` 更适合“刻度+蓝弧”的效果。

### 8.2 电量

- 原型：`bar_battery` 用 `battery_grad_350x24.png` 直接当图片；`lbl_battery` 写 `100%`。
- 动态：`lv_bar` + `lv_bar_set_value(bar, 100, LV_ANIM_ON)`；indicator 用纯色 `#2FA8FF`，或代码里设 `bg_grad`。
- 闪电：`LV_SYMBOL_CHARGE` 或 24×24 PNG。

### 8.3 座椅/媒体

- 座椅：`lv_button` 220×100，radius 16，border `#2A3446`，`bg_opa 0`；图标用 `icon_seat_ai_48.png` + 文字。
- 媒体：容器 + `album_art_ai_48.png` + 两行文字 + 三个按钮；按钮用 `LV_SYMBOL_PREV/PLAY/NEXT`。
- 点击事件：GUI Guider 的 `Events` 面板 → `Clicked` → 设置 `Play` 图标在 `LV_SYMBOL_PLAY/PAUSE` 之间切换。

### 8.4 底部导航

- 每个 `nav_item` 是 120×70 的透明容器，里面图标 32×32 + 文字 18；
- 图标直接用 AI 切片：`icon_nav_ai_64.png`、`icon_fault_ai_64.png`、`icon_assistant_ai_64.png`、`icon_cast_ai_64.png`、`icon_settings_ai_64.png`（缩到 32×32 或 40×40）；
- 选中态：图标/文字变 `#2FA8FF`，可加一条 3px 顶部指示条；导航图标是白色，选中时换成蓝色 tint 或叠一层蓝色；
- 用 `LV_STATE_CHECKED` + `LV_OBJ_FLAG_CHECKABLE` 做互斥（代码里用 `lv_group` 或手动清其它项）；
- 截图里“助手”是选中态（红箭头指向它），所以默认选中第 3 项。

---

## 9. 动态路径：在 GUI Guider 里加自定义代码

GUI Guider 的“拖拽”没有“导航路径”控件，需要写一点 C。推荐顺序：

### 9.1 先用 `lv_line`（LVGL 8.3 写法）

在 `custom.c` 里（GUI Guider 的 Custom Code 区域）。LVGL 8.3 用 `lv_point_t`，不是 9.x 的 `lv_point_precise_t`：

```c
#include "custom.h"
#include "gui_guider.h"
#include "lvgl.h"

static lv_point_t s_pts[] = {
    {325, 448}, {330, 392}, {345, 342}, {370, 300},
    {410, 272}, {455, 254}, {500, 228}, {525, 198}, {520, 168}
};

static lv_obj_t *make_line(lv_obj_t *parent, uint32_t color, lv_opa_t opa, int32_t width)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);

    lv_line_set_points(line, s_pts, sizeof(s_pts) / sizeof(s_pts[0]));
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_opa(line, opa, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, 0);
    return line;
}

void custom_init(void)
{
    /* 对象名以 gui_guider.h 里生成的为准 */
    lv_obj_t *map = guider_ui.screen_cont_map;

    /* 创建顺序就是 z-order：发光 -> 描边 -> 主线 -> 高光 */
    make_line(map, 0x0A4D8F, LV_OPA_20, 18);
    make_line(map, 0x062B5A, LV_OPA_80, 13);
    make_line(map, 0x2FA8FF, LV_OPA_COVER, 7);
    make_line(map, 0xB9F2FF, LV_OPA_50, 2);
}
```

> 路径点要相对 `cont_map` 左上角；`lv_line` 不会复制点数组，所以这里用 `static`，不要用局部数组。

或者直接用本仓库的 LVGL 8.3 模块（推荐，省得手动建 4 条线）：

```c
#include "lv_nav_path_v8.h"

static nav_path_v8_t *s_route;

void custom_init(void)
{
    static lv_point_t pts[] = {
        {325, 448}, {330, 392}, {345, 342}, {370, 300},
        {410, 272}, {455, 254}, {500, 228}, {525, 198}, {520, 168}
    };

    s_route = nav_path_v8_create(guider_ui.screen_cont_map, 128);
    nav_path_v8_set_points(s_route, pts, sizeof(pts) / sizeof(pts[0]));
}

/* 页面销毁时调用 */
void custom_deinit(void)
{
    nav_path_v8_delete(s_route);
    s_route = NULL;
}
```

> GUI Guider 重新生成代码时，`custom.c` 一般不会被覆盖；但 `gui_guider.c`/`setup_scr_*.c` 会。**所有自定义代码放 `custom.c` 或单独文件**，不要改生成文件。

### 9.2 用 `lv_canvas`（要渐变/发光）

- 在 `custom.c` 里创建 `lv_canvas`，尺寸 600×465；
- 用 `lv_canvas_init_layer` + `lv_draw_line` 逐段画（LVGL 8/9 API 不同）；
- 每段颜色用 `lv_color_mix()` 插值；
- 画完 `lv_canvas_finish_layer`；
- 路径不变时只画一次，车动时只移动箭头。

### 9.3 用本仓库的 `lv_nav_path`？先看清版本

- `src/lv_nav_path` 是 **LVGL 9.3+** 的版本（`lv_draw_line_dsc_t.points`、`lv_event_get_layer` 等），**不要直接用于 GUI Guider 1.8/1.9（LVGL 8.3）**；
- LVGL 8.3 用 `src/lv_nav_path_v8`：
  - `nav_path_v8_create(parent, max_points)`；
  - `nav_path_v8_set_points()`；
  - `nav_path_v8_delete()`；
  - 内部用 4 条 `lv_line` 做发光/描边/主线/高光，纯 LVGL 8.3 API。
- 如果一定要在 8.3 上做渐变，用 `lv_canvas` 逐段画（见 9.2），不要用 9.x 的 vector API。

### 9.4 自定义代码怎么被调用

GUI Guider 生成的 `setup_scr_screen()` 末尾通常会调用 `custom_init()`；如果没有，在 `main()` 初始化 UI 之后手动调一次：

```c
setup_ui(&guider_ui);
custom_init();
```

或者在 `events_init.c` 的 `events_init()` 里调用。以你生成的工程为准。

### 9.5 LVGL 8.3 API 对照（GUI Guider 1.8/1.9 必看）

| LVGL 9.x 写法 | LVGL 8.3 写法 | 说明 |
|---|---|---|
| `lv_image_create()` | `lv_img_create()` | 控件名不同 |
| `lv_image_set_src()` | `lv_img_set_src()` | |
| `lv_image_set_rotation(obj, angle)` | `lv_img_set_angle(obj, angle)` + `lv_img_set_pivot()` | 单位 0.1° |
| `lv_point_precise_t` | `lv_point_t` | 8.3 是 int16 坐标 |
| `lv_obj_delete()` | `lv_obj_del()` | |
| `lv_obj_set_style_image_opa()` | `lv_obj_set_style_img_opa()` | |
| `lv_canvas_set_draw_buf()` | `lv_canvas_set_buffer()` | |
| `lv_draw_line(layer, &dsc)` | `lv_draw_line(&layer, &dsc, &p1, &p2)` | 8.3 是逐段接口 |
| `lv_scale` | `lv_meter` | 8.3 用 Meter；9.x 用 Scale |
| `lv_obj_set_style_line_width/color/opa/rounded` | 同名 | 8.3 支持 |
| `lv_line_set_points(line, pts, n)` | 同名，但 `pts` 是 `lv_point_t*` | |

> 图片旋转/缩放建议导入为 `True color with alpha`；调色板图片在 8.3 下旋转可能不生效。

### 9.6 第一版执行顺序（按你确认的方案）

1. GUI Guider 新建 **1400×700** 工程，LVGL **8.3**，颜色深度 16 bit；
2. 导入 `screen_bg_1400x700.png`、`map_bg_clean_600x465.png`、`route_ai_600x465.png`、`pin_ai_40x52.png`、`arrow_ai_56x56.png`、`turn_card_ai_397x124.png`、`speed_ticks_ai_460x260.png`、`battery_grad_350x24.png`、`album_art_ai_48.png`；
3. 按第 5 节图层树，先放 `cont_map`（45,120,600×465）→ `img_map_bg` → `img_route` → `img_pin`（506,58）→ `img_arrow`（286,435）→ `img_turn`（20,6）；
4. 再做 `cont_turn`、右侧速度/电量/座椅/媒体、底部导航；
5. `Run Simulator`，先对比整体比例、圆角、颜色；路径/Pin/箭头位置用“微调”对齐；
6. 这一版 **路径是 PNG，不需要 custom code**；等 UI 定稿后再把 `img_route` 换成 `lv_nav_path_v8` 的动态路径（第 9.1 节）。

**对象命名**：GUI Guider 生成的头文件里对象名可能是 `screen_img_1` 这种默认名。建议在 GUI Guider 里给关键对象改成 `cont_map`、`img_route`、`img_pin`、`img_arrow`，这样 `custom.c` 里好引用；改完重新生成代码，确认 `gui_guider.h` 里的名字一致。

---

## 10. 交互与动画

| 功能 | GUI Guider 做法 |
|---|---|
| 底部导航切换 | 每个 `nav_item` 的 `Clicked` 事件 → 改样式/切 screen |
| 速度变化 | `lv_timer` 每 100ms 更新 `lbl_speed` 文本 + `lv_arc_set_value` |
| 电量 | `lv_bar_set_value(bar, value, LV_ANIM_ON)` |
| 播放/暂停 | 点击 `lbl_play` → 切换 `LV_SYMBOL_PLAY/PAUSE` |
| 路径进度 | 定时器更新 `progress`，重绘 `lv_line` 或 `lv_canvas` |
| 车标旋转 | `lv_img_set_pivot(img_arrow, 28, 28); lv_img_set_angle(img_arrow, angle * 10);` |
| 开机动画 | GUI Guider 的 Animations：透明度/位移/缩放 |

> PC 演示建议加一个“演示模式”定时器：速度 0→3→18→36 循环，电量 100→80，让领导看到动态效果；不要只放静态图。

---

## 11. 运行与导出

1. 点 `Run Simulator`，先在 PC 上看 1400×700 效果；
2. 切到 16 bit 颜色检查渐变/透明度是否可接受；
3. 检查中文是否有方块、文字是否溢出、圆角是否裁剪；
4. `Generate Code` → 生成 `generated/`；
5. 设备版：新建目标分辨率工程（854×480），按第 2 节缩放重排；不要直接缩放 1400×700 工程；
6. 上 HX3602C 时：
   - 确认 SDK 的 LVGL 版本与 GUI Guider 一致；
   - 把 `generated/` + `custom/` 加进工程；
   - 提供 `lv_tick`、显示 flush、输入（如有触摸）；
   - 把大图换成小图/动态绘制（见第 4 节）。

---

## 12. 验收清单

- [ ] 1400×700 下整体布局与截图一致（地图卡片/速度/电量/底部栏位置）；
- [ ] 地图卡片圆角裁剪正确，路径/Pin/箭头不溢出；
- [ ] 中文无方块，道路名 2 行不溢出；
- [ ] 16 bit 下渐变没有明显色带；
- [ ] 速度/电量/播放/导航有动态演示；
- [ ] 854×480 设备版重排后，字号/线宽仍然清晰；
- [ ] 设备版资源总大小 < Flash 预算（背景/地图/路径不要直接搬大图）。

---

## 13. 常见问题

**Q1：GUI Guider 里中文显示方块？**
A：字体没包含 CJK。用 TTF（思源黑体/阿里巴巴普惠体）生成子集，把用到的汉字粘进 Symbols；确认 UTF-8。

**Q2：图片导入后变黑/透明不对？**
A：带透明的 PNG 选 `True color with alpha`；背景图选 `True color`；16 bit 工程不要用 32 bit 的渐变。

**Q3：路径拐角有缺口？**
A：`lv_line` 的粗线没有圆角 join。把路径做 Catmull-Rom 平滑、点距 6~10 px；或在拐点补圆；或用 `lv_canvas` 逐段画 + 圆头。

**Q4：圆角卡片里的图片还是直角？**
A：父容器开 `Clip corner`，并确认子对象没有 `OVERFLOW_VISIBLE`。

**Q5：GUI Guider 重新生成后我的代码没了？**
A：自定义代码放 `custom.c` / 独立 `.c`，不要改 `generated/` 下的文件。

**Q6：v2.0 生成的代码能在 HX3602C 的 LVGL 8.3 SDK 里用吗？**
A：不能直接编译。先确认 SDK 的 LVGL 版本；8.3 就用 GUI Guider 1.8.x 重做，或只把 v2.0 当视觉稿。

---

## 14. 下一步

1. 确认两件事：**实际屏幕分辨率**、**HX3602C SDK 的 LVGL 版本**；
2. 用 `assets/gui_guider/` 里的图，按第 5 节图层树在 GUI Guider 里搭第一版；
3. 先做 `map_card_ai` 整图占位，30 分钟出效果；再换分层；
4. 需要动态路径时，把第 9.1 节的 `custom.c` 贴进去；
5. 把工程发我（或截图 + 对象名），我可以按你的 GUI Guider 版本把 `custom.c`、样式和事件补完整。
