# MCU + LVGL + 高德投屏：路径显示方案调研

> 适用硬件：珠海鸿芯 HX3602C（RISC-V 192 MHz / 32 KB SRAM / 32 Mb PSRAM / MJPEG 编解码 / LCD + OSD 双图层）
> 目标界面：左侧地图卡片（深色底图 + 蓝色导航路径 + 转向提示 + 目的地 Pin + 自车箭头），右侧仪表/电量/媒体，底部导航栏。
> 结论日期：2026-11-07

---

## 0. 结论先行

“路径显示”在工程上有两种完全不同的含义，先选错会导致后面全部返工：

| 做法 | 路径由谁绘制 | 路径是什么 | 适合场景 | 开发量 | 对 HX3602C 的适配 |
|---|---|---|---|---|---|
| **A. 手机投屏（推荐先做）** | 手机端高德 | 一路 JPEG/MJPEG 像素 | 车机没有地图数据、希望效果与手机高德一致 | 低 | ★★★★★ 硬件 JPEG 解码 + 视频层，天生适合 |
| **B. 车机预装高德 RTOS 屏显 SDK** | 高德 SDK 算几何，LVGL 负责光栅化 | `draw_polyline` 等绘制指令 | 设备独立导航、可拿高德授权 | 中 | ★★★☆☆ 需 RISC-V 库、4 MB PSRAM 评估、商务授权 |
| **C. 自绘地图 + LVGL 画路径** | 自己（LVGL） | 经纬度折线 → 屏幕折线 | 离线地图、Demo、深度定制 | 高 | ★★★☆☆ 能跑，但要自己做瓦片、投影、路线、路况 |
| **D. 高德静态地图 API + LVGL 叠加** | 高德云端（`paths` 参数） | 一张带路径的 PNG/JPEG | 快速 Demo、低频刷新 | 很低 | ★★★★☆ 图片解码即可，但实时性/QPS 有限 |
| **E. 只投 TBT 导航信息** | 不画地图 | 转向箭头/距离/路名 | 低成本副屏、HUD | 低 | ★★★★★ 但只有文字和箭头，没有路径 |

**一句话建议：**

> 如果产品目标是“把手机上的高德导航投到电动车仪表”，优先走 **A（手机渲染地图/路径 → HX3602C 解码显示，LVGL 只做仪表 UI）**；
> 如果必须“设备自己导航”，且能拿到高德商务授权，走 **B（高德 RTOS 屏显 SDK + LVGL render adapter）**；
> **C 只作为没有 SDK、需要离线/定制时的兜底**，不要一上来自己造地图引擎。

关键原因：HX3602C 是一颗**视频处理 SoC**，不是高性能应用处理器。它的强项是 MJPEG 硬件解码、视频层/OSD 层 Alpha 混合、PIP、2D blit，而不是跑完整地图引擎。把“地图 + 路径”留在手机/云端渲染，MCU 只负责解码、合成和仪表 UI，是成本、效果、风险最优的解。

---

## 1. 硬件约束：HX3602C 能做什么、不能做什么

来自本地数据手册 `HX3602C_DS_CN_V1.0.1.pdf`：

| 项目 | 规格 | 对方案的影响 |
|---|---|---|
| CPU | RISC-V，最高 192 MHz，带 DSP/FPU、I/D Cache | 跑 LVGL 可以，但不要指望全屏 60 fps 软件渲染 |
| 片上内存 | 32 KB SRAM + 32 Mb PSRAM（= **4 MB**） | 4 MB 要精打细算；全屏双缓冲会吃光 |
| 显示 | 最大 480×854@60，24 bpp；RGB/MCU8080/SPI | 480×854 RGB565 单帧 820 KB |
| 视频层 | YUV420 / RGB888 / ARGB8888 | 手机投屏帧可直接进视频层 |
| OSD 层 | ARGB8888 / ARGB8565 / ARGB4444 / CLUT；全局 Alpha | LVGL 画在 OSD 层，叠在视频上 |
| 混合 | 多图层 Per-pixel Alpha、PIP 画中画 | 地图卡片可以作为 PIP 窗口，LVGL 画外框/文字 |
| 视频编解码 | **JPEG 编码/解码、MJPEG**；无 H.264 | 投屏必须用 MJPEG/JPEG，不能用 H.264 |
| JPEG 性能 | 1280×720@30、640×480@30、320×240@120（MJPEG） | 480×854@15~30 fps 足够 |
| USB | USB 2.0 HS 480 Mbps；Device 支持 UVC/MSC/UAC；Host 支持 CDC-ECM/RNDIS | 可接 WiFi 模组；也可做 UVC 设备 |
| 存储 | SDIO 3.0（SDXC/eMMC）、SPI NOR | 离线瓦片/缓存可放 SD |
| 图形加速 | bitblt、矩形填充，官方称支持 LVGL | 矩形/贴图可加速；**任意角度旋转、抗锯齿线条仍靠软件** |
| 其它 | RTC、ADC 按键、PWM 背光、PDM 音频 | 仪表所需外设齐全 |

### 1.1 4 MB PSRAM 预算（必须按最坏情况算）

| 用途 | 方案 A（视频投屏） | 方案 B（SDK 渲染） |
|---|---|---|
| LVGL 绘制缓冲 | 局部 480×80 RGB565 ≈ 77 KB（推荐） | 同左，或地图 canvas 366×300×2 ≈ 220 KB |
| 视频帧 | 地图卡片 366×300 YUV420 ≈ 165 KB（不是全屏） | — |
| OSD 帧 | 单缓冲 480×854 ARGB8565 ≈ 1.23 MB（可只用局部/CLUT） | 同左 |
| 地图瓦片/画布 | 不需要 | 512 KB~1 MB 缓存 |
| 应用/协议/字体 | 300~600 KB | 300~600 KB |
| 余量 | 约 1.5 MB | 约 1 MB |

> 结论：**不要给 LVGL 分配全屏双缓冲**。用 `LV_DISPLAY_RENDER_MODE_PARTIAL` + 1/10 屏幕大小的 draw buffer，配合视频层/地图 canvas 缓存，4 MB 才够。
> 如果 UI 必须全屏 480×854 且要 30 fps，优先把静态区域做成图片/缓存，只刷新脏矩形。

---

## 2. 成熟技术方案盘点

### 2.1 高德官方路线（最值得优先对接）

#### （1）高德智能两轮车解决方案

高德开放平台已上线“智能两轮车解决方案”，明确提供：

- **两轮车投射、投屏、预装导航**；
- 骑行箭头导航、简易路网导航、全屏导航、熄屏导航；
- 覆盖 **Android / iOS / HarmonyOS 手机投屏**，以及 **Android / Linux / RTOS 车机预装**；
- 红绿灯倒计时、车道指引、弯道会车、后方来车等安全能力。

来源：<https://lbs.amap.com/solution/two-wheeled-vehicle>
产品新闻：<https://lbs.amap.com/news/ride_com>

这与截图里的“投屏”Tab、两轮车仪表形态高度吻合。**如果这是量产项目，第一步应该是走高德商务/生态对接，而不是先自己写地图。**

#### （2）鸿蒙两轮车 SDK 的投屏架构（方案 A 的官方参考）

高德 × 鸿蒙 × 极核的“鸿蒙两轮车 SDK”公开了五个技术点，几乎就是 HX3602C 投屏方案的标准答案：

1. **位移驱动**：不是定时截图，而是位置变化才唤醒渲染，“车动图动”；
2. **ComponentSnapshot 组件级截图**：只截地图与指引区域，过滤无关 UI；
3. **精简路网模式**：隐藏冗余控件，只保留转向箭头、车道、关键路况，支持透明背景；
4. **JPEG 压缩 + 沙箱存储 + 节流**：避免高频截图造成 I/O 和耗电；
5. **静止休眠**：等红灯/停车暂停渲染。

来源：<https://blog.51cto.com/u_17390245/14771312>

> 对 HX3602C 的映射：手机端把“地图卡片”截成 JPEG（只截卡片区域，320×240~480×854），通过 WiFi/USB 发给 HX3602C，硬件 JPEG 解码后送视频层；LVGL 只画转向卡片、速度、电量、媒体、底部栏。**路径根本不需要 LVGL 画。**

#### （3）高德 AIoT RTOS 屏显 SDK（方案 B 的官方能力）

高德 AIoT 屏显组件以 **C 静态库**发布，面向手表/眼镜等 RTOS 设备，提供：

- 地图渲染：栅格瓦片、矢量瓦片、卫星图、POI、离线地图；
- `render_adapter`：`begin_drawing / commit_drawing / draw_polyline / draw_bitmap / draw_text / draw_color / measure_text` 等回调，**官方明确说可以对接 LVGL**；
- 导航：`awk_init_navi`、`awk_navi_data`（protobuf）、`awk_navi_set_tracking_mode`（北朝上/车头朝上/路线朝上）、`awk_navi_view`（车标、路线高亮）；
- 轨迹覆盖物：规划线、实走线、偏航线、转向点图标；
- 回调：剩余距离/时间、下一动作、车道线、红绿灯倒计时、后方来车；
- 适配层：文件、内存、网络、线程、系统时间，全部由宿主注入；
- 商务：需要高德 Key、设备激活、License/技术服务许可。

文档：<https://lbs.amap.com/api/aiot-sdk/guide/screen-display/ptos-user-manual>
概述：<https://lbs.amap.com/api/aiot-sdk/summary>

> 这是“**在 MCU 上用 LVGL 画高德路径**”最正统的路线：路径几何、转向点、路线高亮由高德 SDK 算好，LVGL 只实现 `draw_polyline` 等回调。
> 但需要确认：是否有 **RISC-V 192 MHz / 4 MB PSRAM** 的库、NAVI/TRACK_NAVI feature 是否开放、License 成本、离线瓦片授权。

**关于“MCU 版本 / ARM 版本”的重要澄清：**

| 产品 | 形态 | 架构 | 能不能跑在 HX3602C |
|---|---|---|---|
| 高德地图 App / 车机版 | Android APK | ARM（AArch64/ARMv7） | 不能，HX3602C 没有 Android/ARM |
| 高德 Android/iOS/HarmonyOS SDK | 手机端 SDK | ARM | 不能，属于手机侧 |
| 高德 AIoT RTOS 屏显 SDK | **C 静态库**，按平台/工具链编译 | 官方公开的落地平台是手表/眼镜芯片（如 SiFli Cortex-M33、杰理等），**不是“一个通用 MCU 版”** | **需要高德提供 RISC-V 构建/移植支持，不能假设 ARM 库能直接链接** |
| 高德 Web 服务 API（静态地图/路径规划） | HTTP 接口 | 与 CPU 架构无关 | 能，任何带网络的 MCU 都能请求；受 QPS/授权限制 |
| 高德瓦片数据（栅格/矢量） | 图片/数据 | 与 CPU 架构无关 | 数据本身能读；但抓取/离线缓存/商用需要授权 |

所以：

- **“高德底图”本身没有 ARM/RISC-V 之分**，它只是瓦片数据；
- **“高德地图引擎/SDK”有架构之分**，C API 可移植，但交付物是**按平台编译的静态库**；
- HX3602C 是 **RISC-V 192 MHz + 4 MB PSRAM**，不在高德公开的 RTOS 平台列表里。要走方案 B，必须让高德/原厂确认：
  1. 有没有 RISC-V 工具链构建的 `.a`；
  2. 能不能提供源码/移植包；
  3. 4 MB PSRAM 是否满足 tile cache + 绘制内存；
  4. NAVI/TRACK_NAVI feature 和 License 是否开放。

> 结论：**不要把方案 B 当成“拿来就能用”的兜底**。在拿到高德明确的 RISC-V 库/授权之前，量产主线应按方案 A（手机渲染地图/路径 → HX3602C 解码）或方案 C（自绘/离线瓦片）准备。方案 B 作为“如果能拿到授权就更优”的并行评估项。

#### （4）高德车机版广播（方案 E）

高德车机版持续发送：

- `AUTONAVI_STANDARD_BROADCAST_SEND`
- `KEY_TYPE = 10001` 为导航信息
- 字段：`ICON/NEW_ICON`（转向图标）、`CUR_ROAD_NAME`、`NEXT_ROAD_NAME`、`SEG_REMAIN_DIS`、`ROUTE_REMAIN_DIS`、`ROUTE_REMAIN_TIME`、`ETA_TEXT`、`CUR_SPEED`、`LIMITED_SPEED`、`TRAFFIC_LIGHT_NUM`、`CAMERA_DIST` 等。

参考：<https://www.awen.me/post/4a9cd354.html>
开源实现：<https://oshwhub.com/binbinchaoxun/project_chusbx>（Amap-for-ESP32）

> 只有 TBT 数据，没有地图/路径几何。适合做副屏、HUD、或给方案 A 的转向卡片提供文字数据。

#### （5）高德静态地图 API（方案 D）

`https://restapi.amap.com/v3/staticmap` 支持：

- `location`、`zoom`、`size`（最大 1024×1024）、`scale`；
- `paths`：直接在地图上画折线，格式 `weight,color,transparency,fillcolor,fillTransparency:lng,lat;lng,lat;...`；
- `traffic=1` 叠加实时路况；
- `markers/labels` 加起终点。

文档：<https://lbs.amap.com/api/webservice/guide/api/staticmaps>

> 这是最快的 Demo 路径：手机/云端请求一张“已经画好路径”的深色地图，MCU 只解码显示。缺点是刷新频率、QPS、路径样式受限，且不是真正的实时导航。

### 2.2 开源参考（方案 C 的工程证据）

| 项目 | 平台 | 关键做法 | 可借鉴点 |
|---|---|---|---|
| **X-TRACK**（FASTSHIFT） | STM32F405 168 MHz / 240×240 / LVGL 8.3 | SD 卡离线瓦片 `/z/x/y.bin`；Web Mercator；`MapConv`/`TileConv`；3×3 瓦片；`lv_line` 画轨迹；72 KB LVGL 内存池 | 低算力 MCU 上跑离线地图的完整范式；证明 168 MHz 也能流畅平移 |
| **TerraNav-STM32** | STM32F405 / 240×320 / LVGL | 九宫格瓦片；SDIO+DMA 直接读 BMP；墨卡托；`lv_async_call` 解耦 GPS | SDIO+DMA 取瓦片、线程安全更新 |
| **IceNav-v3** | ESP32-S3/P4 / 320×480 / LVGL 9.5 | 栅格瓦片 + 矢量瓦片；A* 路线；Hilbert 空间索引；3D 透视（Q16 DDA）；PPA 加速 | 矢量地图、路线规划、性能优化、PSRAM 管理 |
| **Amap-for-ESP32** | ESP32-S3 / 320×240 | Android 监听高德广播 → JSON → UDP/BLE → TFT | 导航字段协议、断线重连、中文显示 |
| **Mason's Blog** | ESP32-C6 / 1.47" | Android NotificationListener 读高德/Google Maps 通知 → BLE → LVGL | 极低成本 TBT 方案 |
| **mjpeg + LVGL** | ESP32-S3 | MJPEG 三缓冲乒乓、解码任务 + LVGL 刷新任务、20 fps | 方案 A 在 MCU 上播 MJPEG 的软件参考 |

链接：

- X-TRACK：<https://github.com/FASTSHIFT/X-TRACK>
- X-TRACK 地图系统解析：<https://deepwiki.com/FASTSHIFT/X-TRACK/5-map-system>
- TerraNav：<https://github.com/Lelechacc/TerraNav-STM32>
- IceNav-v3：<https://github.com/jgauchia/IceNav-v3>
- Amap-for-ESP32：<https://oshwhub.com/binbinchaoxun/project_chusbx>
- Mason's Blog：<https://maisonsmd.dev/blog/google-maps-on-esp32>
- mjpeg + LVGL：<https://gitee.com/vi-iot/mjpeg>

### 2.3 方案对比（按本项目权重）

评分 1~5，5 最好：

| 维度 | A 手机投屏 | B 高德 RTOS SDK | C 自绘瓦片 | D 静态地图 | E TBT 广播 |
|---|---|---|---|---|---|
| 路径/地图效果 | 5（与手机一致） | 5 | 3~4 | 4 | 1 |
| 实时路况/红绿灯 | 5 | 5 | 1 | 2 | 3 |
| 开发量（越少越好） | 5 | 3 | 1 | 5 | 5 |
| CPU/内存压力 | 5（硬解） | 3 | 2 | 4 | 5 |
| 离线能力 | 1（依赖手机） | 4 | 5 | 1 | 5 |
| 数据/授权风险 | 3（手机权限） | 2（高德授权） | 2（地图数据版权） | 4 | 4 |
| 与 HX3602C 匹配 | 5 | 3 | 3 | 4 | 5 |
| **建议** | **P0 先做** | **P1 并行申请** | P2 兜底 | POC | 辅助 |

---

## 3. 用 LVGL 画路径：到底怎么画

### 3.1 先做决策：路径是“像素”还是“几何”

- **像素路径**（方案 A/D）：路径已经在 JPEG/PNG 里。LVGL 只负责显示图片/视频、画外框、转向卡片、车标。**这是最省算力的，也是截图效果最容易复刻的。**
- **几何路径**（方案 B/C）：路径是一组经纬度/屏幕点。LVGL 需要把折线光栅化。下面 3.2~3.5 讲的就是这部分。

> 如果最终选 A，本章的 3.2~3.6 只需要了解，不必全部实现；如果选 B/C，3.2~3.5 是核心。

### 3.2 方法一：`lv_line`（LVGL 8/9 都可用，最简单）

`lv_line` 支持 `line_width`、`line_color`、`line_rounded`，把点数组交给它即可：

```c
static lv_point_precise_t pts[] = {
    {20, 250}, {60, 210}, {95, 215}, {120, 160},
    {160, 150}, {190, 90}, {230, 80}
};

static lv_style_t style;
lv_style_init(&style);
lv_style_set_line_width(&style, 6);
lv_style_set_line_color(&style, lv_color_hex(0x2FA8FF));
lv_style_set_line_rounded(&style, true);

lv_obj_t *line = lv_line_create(card);
lv_line_set_points(line, pts, sizeof(pts) / sizeof(pts[0]));
lv_obj_add_style(line, &style, 0);
```

**优点**：零学习成本，LVGL 8/9 通用，X-TRACK 量产验证。
**缺点**：

- 粗线在**拐角处没有圆角 join**，90° 急弯会出现缺口/尖角；把路径做密、角度做小可以缓解；
- 渐变、发光、虚线（非水平/垂直）支持有限；
- 点很多时，X-TRACK 用多个 `lv_line` 分段（`lv_poly_line`）来控制单次重绘量；
- 每个 `lv_line` 是一个对象，几百个点会影响布局/刷新。

**适用**：路径 100~300 点、线宽 1~6 px、对拐角要求不苛刻。截图里的路径是平滑曲线，`lv_line` + 平滑采样基本够用。

### 3.3 方法二：自定义绘制 + `lv_draw_line` 的 `points` 数组（推荐，LVGL 9.3+）

在 `LV_EVENT_DRAW_MAIN` 里直接拿到 `lv_layer_t`，用一条 draw task 画整条折线：

```c
static void card_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t c;
    lv_obj_get_coords(obj, &c);

    /* 路径点必须是屏幕绝对坐标；也可以在这里加 c.x1/c.y1 偏移 */
    static lv_point_precise_t pts[] = {
        {40, 300}, {80, 260}, {120, 265}, {150, 210},
        {200, 200}, {230, 140}, {280, 130}
    };

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.points     = pts;
    dsc.point_cnt  = sizeof(pts) / sizeof(pts[0]);
    dsc.color      = lv_color_hex(0x2FA8FF);
    dsc.width      = 6;
    dsc.opa        = LV_OPA_COVER;
    dsc.round_start = 1;
    dsc.round_end   = 1;
    lv_draw_line(layer, &dsc);
}
```

**优点**：

- 一条 draw task 画完整条线，比每段一个 `lv_draw_line` 少很多任务；
- 支持线宽、圆头、透明度；
- 不依赖 ThorVG，纯 C，适合资源紧张的 MCU；
- 可以叠多遍做“发光/描边/高光”。

**缺点**：

- **没有圆角 join**：急弯仍可能有缺口。解决办法：
  1. 路径先做 Catmull-Rom/贝塞尔平滑，点距 6~10 px，拐角自然变小；
  2. 对角度大于 30° 的拐点，补一个半径 = 线宽/2 的圆（`lv_draw_rect` + `LV_RADIUS_CIRCLE`）；
  3. 用“多遍描边”掩盖：外层宽、内层窄，视觉上更圆润。
- 不支持沿路径的渐变；要渐变就按段插值颜色（见 3.4）。
- `points` 版本内部会 `lv_malloc` 拷贝一份点数组（约 `count * 8` 字节），每帧多次调用要注意内存碎片；路径不长（<300 点）没问题。

### 3.4 方法三：LVGL 矢量 API（效果最好，但要 ThorVG）

LVGL 9.2+ 提供 `lv_draw_vector`，支持 `line_to / quad_to / cubic_to`、`stroke_width`、`cap/join`、虚线、线性/径向渐变：

```c
lv_draw_vector_dsc_t *dsc = lv_draw_vector_dsc_create(layer);
lv_vector_path_t *path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);

lv_fpoint_t p0 = {40, 300};
lv_vector_path_move_to(path, &p0);
/* 用 cubic_to / quad_to 画平滑曲线 */
lv_fpoint_t c1 = {80, 240}, c2 = {120, 280}, p1 = {150, 210};
lv_vector_path_cubic_to(path, &c1, &c2, &p1);
/* ... 继续追加 ... */

lv_draw_vector_dsc_set_stroke_color(dsc, lv_color_hex(0x2FA8FF));
lv_draw_vector_dsc_set_stroke_opa(dsc, LV_OPA_COVER);
lv_draw_vector_dsc_set_stroke_width(dsc, 6.0f);
lv_draw_vector_dsc_set_stroke_cap(dsc, LV_VECTOR_STROKE_CAP_ROUND);
lv_draw_vector_dsc_set_stroke_join(dsc, LV_VECTOR_STROKE_JOIN_ROUND);

/* 沿路径渐变（截图里的蓝→青） */
lv_grad_stop_t stops[2];
stops[0].color = lv_color_hex(0x0B63D8); stops[0].opa = LV_OPA_COVER; stops[0].frac = 0;
stops[1].color = lv_color_hex(0x35C8FF); stops[1].opa = LV_OPA_COVER; stops[1].frac = 255;
lv_draw_vector_dsc_set_stroke_linear_gradient(dsc, 40, 300, 280, 130);
lv_draw_vector_dsc_set_stroke_gradient_color_stops(dsc, stops, 2);

lv_draw_vector_dsc_add_path(dsc, path);
lv_draw_vector(dsc);
lv_vector_path_delete(path);
lv_draw_vector_dsc_delete(dsc);
```

**优点**：圆角 join、渐变、虚线、贝塞尔一次搞定，最接近设计稿。
**缺点**：

- 软件后端依赖 **ThorVG（C++）**，`LV_USE_VECTOR_GRAPHIC` + `LV_USE_THORVG_INTERNAL`；
- Flash/RAM 占用明显高于 `lv_line`，C++ 运行时/异常处理要确认工具链；
- HX3602C 的 SDK 是否带 ThorVG、是否允许 C++ 编译，需要向原厂确认；
- 复杂路径的 tessellation 可能吃栈，社区有 800×480 ESP32-S3 需要把 LVGL 任务栈加到 16 KB 的案例。

**建议**：如果 HX3602C 工具链能编 ThorVG，且路径需要渐变/圆角，优先矢量 API；否则用 3.3 的多遍描边方案。

### 3.5 方法四：Canvas 缓存（性能最好）

把“底图 + 路径 + 目的地 Pin”画进一块 `lv_canvas`（RGB565），只在路线/缩放/位置变化时重绘；自车箭头用独立 `lv_image` 叠加，每帧只移动/旋转箭头。

```c
/* 366×300 RGB565 ≈ 220 KB，放 PSRAM */
LV_DRAW_BUF_DEFINE_STATIC(map_buf, 366, 300, LV_COLOR_FORMAT_RGB565);
LV_DRAW_BUF_INIT_STATIC(map_buf);

lv_obj_t *canvas = lv_canvas_create(card);
lv_canvas_set_draw_buf(canvas, &map_buf);

lv_layer_t layer;
lv_canvas_init_layer(canvas, &layer);

/* 1. 底图（静态图片/瓦片） */
/* 2. 路径（lv_draw_line / vector） */
/* 3. 目的地 Pin */

lv_canvas_finish_layer(canvas, &layer);
```

**优点**：

- 路径只画一次，后续刷新只动箭头/文字，CPU 压力最小；
- 适合 HX3602C 这种 192 MHz 的 MCU；
- 可以用 2D blit 加速贴图。

**缺点**：

- 需要额外 220 KB（卡片）~820 KB（全屏）PSRAM；
- 地图平移/缩放时要重绘，重绘期间可能掉帧；
- 圆角裁剪需要额外处理（父容器 `clip_corner` 或离屏 mask）。

### 3.6 方法五：视频/图片叠加（方案 A 的核心）

路径已经是 JPEG 像素，LVGL 不需要画：

- 手机端只截“地图卡片”区域（ComponentSnapshot / MediaProjection crop）；
- JPEG 编码（q=60~80，10~15 fps）；
- HX3602C 硬件 JPEG 解码 → 视频层/PIP 窗口；
- LVGL 在 OSD 层画转向卡片、速度、电量、媒体、底部栏；
- 硬件 Alpha 混合，CPU 几乎不参与地图刷新。

这就是截图 UI 最合理的实现方式：**地图卡片是视频窗口，不是 LVGL 画布。**

### 3.7 选型建议

| 场景 | 推荐 |
|---|---|
| 量产、手机高德投屏 | **3.6 视频叠加**（路径不进 LVGL） |
| 设备独立导航、有高德 RTOS SDK | **3.5 Canvas + 3.3/3.4 画 SDK 的 polyline** |
| 离线 Demo、无 SDK | **3.2 lv_line** 或 **3.3 lv_draw_line** |
| 路径要渐变/圆角/虚线、工具链支持 C++ | **3.4 矢量 API** |
| 低端 MCU、路径固定/变化慢 | **3.5 Canvas 缓存** |

---

## 4. 复刻截图视觉效果（LVGL 绘制时）

截图里的地图卡片大约 600×490（设计稿 1400×700），换算到 480×854 横屏约 **366×300**。推荐的图层顺序：

```
卡片圆角背景 (#070B14, radius 20)
  ├─ 地图底图（深色栅格/道路/建筑，或视频）
  ├─ 暗化遮罩 (black, 45%~65% opacity)  ← 让路径跳出来
  ├─ 路径发光层  width 16~20, #0A4D8F, opa 15%~25%
  ├─ 路径描边层  width 10~12, #062B5A, opa 80%
  ├─ 路径主线    width 6~8,  渐变 #0B63D8 → #35C8FF
  ├─ 路径高光    width 2,    #B9F2FF, opa 50%~70%
  ├─ 已走过路径  width 6~8,  #2A3A50（覆盖主线）
  ├─ 目的地 Pin  (32×40, 底部锚点)
  ├─ 自车箭头    (28×28, 按航向旋转, 带白色描边/阴影)
  ├─ 转向提示卡  (radius 16, bg #0B0F18 85%, 阴影)
  └─ 卡片边框    (1 px, #1E3A5F)
```

### 4.1 关键参数

- **底图**：深蓝黑 `#070B14`；道路 `#111827` / `#1F2937`；建筑块 `#0D1526`；水系 `#06283D`。
- **路径渐变**：从自车（深蓝 `#0B63D8`）到终点（青 `#35C8FF`），沿路径方向插值，不是整屏横向。
- **发光**：LVGL 没有高斯模糊，用 2~3 遍“宽线 + 低透明度”模拟；不要用大阴影，性能差。
- **圆角**：`lv_obj_set_style_radius(card, 20, 0)` + `lv_obj_set_style_clip_corner(card, true, 0)`；自定义绘制不受圆角裁剪，路径点要留在卡片内，最后再画圆角边框。
- **转向卡**：`radius 16`、`bg_opa 220`、`shadow_width 12`、`shadow_opa 80`、`shadow_ofs_y 4`；图标 36×36；文字“500米后 进入 / 软件园三号路”分两行。
- **自车箭头**：建议用 PNG/ARGB 图片 + `lv_image_set_rotation()`，比用三角形画更好看；旋转角度 = `atan2(dy, dx)` 转成 LVGL 的 0.1° 单位（注意图片默认朝向）。
- **目的地 Pin**：图片底部中心对齐路径终点；加 2~4 px 阴影。
- **已走过路径**：用深灰覆盖，透明度 200~230；不要再用发光，否则视觉上会“发亮”。

### 4.2 路径平滑（让 `lv_line` 也像设计稿）

输入的路径通常是稀疏折线（每 50~200 m 一个点），直接画会有折角。推荐：

1. **投影**：GCJ-02 经纬度 → Web Mercator 世界像素 → 屏幕坐标（见 4.3）。
2. **简化**：Douglas-Peucker，容差 1~2 px，去掉冗余点。
3. **平滑**：Catmull-Rom 样条采样，每段插 6~10 个点，点距约 6~10 px。
4. **裁剪**：只保留卡片外扩 20 px 内的段，减少绘制量。
5. **限点**：卡片内 100~250 点足够；超过就抽稀。

Catmull-Rom 公式（`p0,p1,p2,p3` 为连续四点，`t∈[0,1]`）：

```
x(t) = 0.5 * ((2*p1.x) + (-p0.x + p2.x)*t
       + (2*p0.x - 5*p1.x + 4*p2.x - p3.x)*t²
       + (-p0.x + 3*p1.x - 3*p2.x + p3.x)*t³)
```

### 4.3 坐标换算（方案 C 必须）

```
Web Mercator:
  worldSize = 256 << zoom
  x = (lon + 180) / 360 * worldSize
  y = (1 - ln(tan(lat*π/180) + 1/cos(lat*π/180)) / π) / 2 * worldSize

屏幕:
  sx = (x - centerX) * scale + screenW/2
  sy = (y - centerY) * scale + screenH/2
```

中国境内高德使用 **GCJ-02**；如果输入是 WGS-84（GPS），先做 WGS-84 → GCJ-02 偏移。X-TRACK 的 `MapConv`/`GPS_Transform` 可直接参考。

### 4.4 自车位置与航向

- 把 GPS 点投影到路径上：遍历相邻线段，做“点到线段投影”，取距离最小的一段；
- 得到段内比例 `t`，插值位置和航向；
- 视觉上用 `lv_anim` 以 30 fps 插值，数据更新 5~10 Hz 即可；
- 偏航超过阈值（如 30 m）时重新吸附/重算路线。

### 4.5 性能注意事项

1. **只刷脏区**：`lv_obj_invalidate_area()`，不要整屏 `lv_obj_invalidate()`。
2. **静态层缓存**：底图、已完成路径、目的地 Pin 画进 canvas；每帧只动箭头和转向文字。
3. **避免全屏 Alpha**：视频层/OSD 层混合是硬件做的；LVGL 内部少用全屏半透明遮罩，暗化遮罩可以画在底图里。
4. **路径重绘频率**：路线不变就不重绘；GPS 更新 5~10 Hz，箭头插值 30 fps。
5. **PSRAM 对齐**：draw buffer 32 字节对齐，DMA 才能高效；HX3602C 的 2D blit 接口要向原厂确认。
6. **LVGL 配置**：`LV_COLOR_DEPTH 16`、`LV_USE_OS`、`LV_MEM_SIZE`、`LV_DRAW_BUF_STRIDE_ALIGN`；如果用矢量 API，再开 `LV_USE_MATRIX`、`LV_USE_FLOAT`、`LV_USE_VECTOR_GRAPHIC`、`LV_USE_THORVG_INTERNAL`。
7. **不要做 3D 透视**：HX3602C 只有 0/90/180/270 旋转和镜像，没有任意角度透视变换；软件旋转 480×854 代价很高。截图那种 3D 视角建议留在手机端（方案 A）。MCU 端推荐**北朝上 + 旋转车标**（X-TRACK 做法），或预生成 8/16 个方向的瓦片。

---

## 5. 推荐架构

### 5.1 方案 A：手机投屏 + LVGL OSD（P0）

```
┌────────────── 手机端 ──────────────┐
│ 高德地图/导航（车机版或 SDK）        │
│  · 组件级截图/裁剪地图卡片           │
│  · 位移驱动、节流 10~15 fps          │
│  · JPEG 编码 q=60~80                │
│  · 转向/路名/距离（广播或 SDK 回调）  │
└──────────────┬─────────────────────┘
               │ WiFi TCP/UDP 或 USB
┌──────────────▼─────────────────────┐
│ HX3602C                             │
│  WiFi/USB → JPEG HW decode          │
│  → 视频层/PIP（366×300 地图卡片）    │
│  → LCD                              │
│  LVGL OSD：转向卡、速度、电量、媒体、 │
│            底部导航、圆角边框         │
│  硬件 Alpha 混合                     │
└─────────────────────────────────────┘
```

**协议建议**：

- 视频帧：`magic(4) + seq(4) + w(2) + h(2) + len(4) + JPEG data + CRC32`；
- 控制/导航：JSON 或二进制，含 `turn_icon`、`distance`、`road`、`next_road`、`eta`、`speed`、`limit`、`battery`；
- 断线重连、帧丢弃策略（只保留最新帧）、看门狗（10 s 无帧显示“等待连接”）。

**带宽/延迟估算**：

- 366×300 JPEG q70 ≈ 15~30 KB/帧；15 fps ≈ 1.8~3.6 Mbps；WiFi 2.4G 足够，BLE 不够（除非 2~5 fps + 320×240）。
- 延迟：截图 30~60 ms + 编码 20~40 ms + 网络 30~80 ms + 解码 20~30 ms + 显示 16 ms ≈ **120~220 ms**，对地图卡片可接受，对 AR-HUD 不够。
- CPU：JPEG 解码走硬件，LVGL 只刷小区域；192 MHz 足够。

**风险**：

- Android/iOS 屏幕捕获权限、后台限制、发热/耗电；
- 高德界面是否允许被捕获（一般非 secure surface，但系统/ROM 可能限制）；
- WiFi 2.4G 与蓝牙共存、车机热点客户端隔离；
- JPEG 文字边缘有块效应，转向卡文字建议 LVGL 原生绘制，不要压进视频。

### 5.2 方案 B：高德 RTOS SDK + LVGL render adapter（P1）

```
高德 RTOS SDK（C 静态库）
  ├─ 地图瓦片/矢量/POI
  ├─ 路线规划/导航数据（protobuf）
  ├─ 轨迹覆盖物（规划线/实走线/偏航/转向点）
  └─ render_adapter ──► LVGL
                         ├─ begin_drawing → lv_canvas_init_layer
                         ├─ draw_polyline → lv_draw_line(points)
                         ├─ draw_bitmap   → lv_draw_image
                         ├─ draw_text     → lv_draw_label
                         ├─ draw_color    → lv_draw_rect
                         └─ commit_drawing→ lv_canvas_finish_layer
```

`render_adapter` 的 LVGL 骨架（按高德头文件字段名调整）：

```c
static lv_layer_t s_map_layer;
static lv_obj_t *s_map_canvas;

static void adp_begin(uint32_t map_id, awk_render_context_t st)
{
    lv_canvas_init_layer(s_map_canvas, &s_map_layer);
}

static void adp_polyline(uint32_t map_id, awk_point_t *pts, uint32_t n,
                         const awk_paint_style_t *style)
{
    if(n < 2) return;
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.points = (lv_point_precise_t *)pts; /* 若类型不同，做一次转换 */
    d.point_cnt = (int32_t)n;
    d.color = lv_color_hex(style->color & 0xFFFFFF);
    d.opa = (style->color >> 24) & 0xFF;
    d.width = style->width;
    d.round_start = 1;
    d.round_end = 1;
    lv_draw_line(&s_map_layer, &d);
}

static void adp_commit(uint32_t map_id)
{
    lv_canvas_finish_layer(s_map_canvas, &s_map_layer);
}
```

**需要确认**：

- 高德是否提供 HX3602C（RISC-V 192 MHz / 4 MB PSRAM）可用的静态库；官方公开的 RTOS 落地平台主要是 **ARM Cortex-M（如 SiFli SF32LB52）**、杰理等手表/眼镜芯片，**没有公开的 HX3602C RISC-V 构建**；
- 如果不能提供 RISC-V 库，是否可提供源码/移植包（C API 可移植，但底层网络/文件/内存/线程 adapter 要自己实现）；
- `NAVI` / `TRACK_NAVI` / `TILE` / `POI` feature 是否开放；
- License/激活/离线瓦片授权；
- 4 MB PSRAM 是否满足 SDK 的 tile cache + 绘制内存；
- `awk_point_t` 与 `lv_point_precise_t` 的布局/精度是否兼容（不兼容就拷贝转换）。

> 如果高德只给 ARM 库、不给 RISC-V 移植，**方案 B 在 HX3602C 上直接不可行**；此时只能走方案 A/C，或换一颗高德已支持的 ARM 平台（这会推翻当前硬件选型，需谨慎评估）。

### 5.3 方案 C：自绘瓦片 + LVGL（P2 兜底）

```
SD 卡 / SPI Flash
  └─ /MAP/{z}/{x}/{y}.bin (RGB565 256×256)
        │
        ▼
  MapConv（GCJ02/Web Mercator）→ TileConv（3×3 视口）
        │
        ▼
  lv_img 3×3 拼接 + 偏移 → 地图卡片
        │
  Route（高德路径规划 API / GPX）
        │
        ▼
  RDP 简化 → Catmull-Rom 平滑 → 裁剪
        │
        ▼
  lv_line / lv_draw_line / vector → 路径
```

**不要踩的坑**：

- 高德/百度瓦片不能随意离线缓存/商用，需授权；OSM 有 ODbL 署名要求；
- 自己做路线规划、实时路况、红绿灯，工作量远超预期；
- 256×256 RGB565 瓦片 128 KB/张，3×3 = 1.15 MB；建议 128×128 或 JPEG 瓦片 + 硬解；
- 没有 GPU 时，地图平移要尽量用“整像素偏移 + 预取”，避免每帧重绘全部瓦片。

---

## 6. 落地计划（建议）

| 阶段 | 时间 | 交付 | 验收 |
|---|---|---|---|
| **P0 视觉/绘制验证** | 1 周 | PC LVGL 模拟器 + 截图复刻（路径/箭头/Pin/转向卡） | 480×854 下路径平滑、无缺口、帧率 ≥ 20 fps（PC） |
| **P1 方案 A PoC** | 2~3 周 | 手机 JPEG 发送端 + HX3602C 解码 + 视频层 + LVGL OSD | 端到端延迟 < 250 ms；15 fps；CPU 余量 > 30% |
| **P2 方案 A 产品化** | 4~6 周 | 重连/看门狗/休眠/错误 UI/OTA/热管理 | 连续 2 h 稳定；断网 10 s 内恢复 |
| **P3 方案 B 评估** | 并行 2~4 周 | 高德 SDK 申请、RISC-V 库验证、render adapter | `awk_init`/`map_create`/瓦片下载/导航回调跑通 |
| **P4 选型** | — | A/B 对比报告 | 成本、效果、授权、风险四项打分 |

> 建议 P0/P1 与 P3 并行：P0 先把“路径长什么样”定死，P1 验证视频链路，P3 谈授权/拿库。不要等 SDK 再开始 UI。

---

## 7. 待确认问题（向原厂/高德/硬件确认）

1. **HX3602C SDK**：LVGL 版本（8.x/9.x）、2D 加速接口、视频层格式/PIP/Alpha 配置、PSRAM 带宽、JPEG 解码最大尺寸/帧率、是否支持 C++/ThorVG。
2. **高德 RTOS SDK**：是否有 RISC-V 192 MHz 构建；NAVI/TRACK_NAVI 是否可用；License 价格/激活方式；离线瓦片授权；最小 RAM/Flash。
3. **两轮车方案**：手机投屏 SDK（Android/iOS/HarmonyOS）的接入方式、ComponentSnapshot 是否开放、是否要求高德 App 版本/包名。
4. **屏幕**：实际分辨率/朝向/PPI/阳光下可读性；是否需要触摸；UI 是否要 3D 视角。
5. **产品/合规**：骑行中显示视频是否影响安全/法规；地图数据版权；手机权限与隐私。
6. **成本**：WiFi 模组、SD 卡、PSRAM 是否够；是否要 LTE/蓝牙；OTA 分区。

---

## 8. 参考资料

### 高德官方

- 智能两轮车解决方案：<https://lbs.amap.com/solution/two-wheeled-vehicle>
- 两轮车方案上线新闻：<https://lbs.amap.com/news/ride_com>
- 鸿蒙两轮车 SDK 技术解读：<https://blog.51cto.com/u_17390245/14771312>
- AIoT RTOS 屏显 SDK：<https://lbs.amap.com/api/aiot-sdk/guide/screen-display/ptos-user-manual>
- AIoT SDK 概述：<https://lbs.amap.com/api/aiot-sdk/summary>
- 静态地图 API：<https://lbs.amap.com/api/webservice/guide/api/staticmaps>
- 高德车机版广播解析：<https://www.awen.me/post/4a9cd354.html>

### 开源项目

- X-TRACK：<https://github.com/FASTSHIFT/X-TRACK>
- X-TRACK 地图系统：<https://deepwiki.com/FASTSHIFT/X-TRACK/5-map-system>
- TerraNav-STM32：<https://github.com/Lelechacc/TerraNav-STM32>
- IceNav-v3：<https://github.com/jgauchia/IceNav-v3>
- Amap-for-ESP32：<https://oshwhub.com/binbinchaoxun/project_chusbx>
- Mason's Blog（ESP32 导航）：<https://maisonsmd.dev/blog/google-maps-on-esp32>
- MJPEG + LVGL：<https://gitee.com/vi-iot/mjpeg>

### LVGL 文档

- Line：<https://lvgl.io/docs/open/widgets/line>
- Line draw descriptor：<https://lvgl.io/docs/open/main-modules/draw/draw_descriptors>
- Vector graphics：<https://lvgl.io/docs/open/main-modules/draw/draw_units>
- Canvas：<https://lvgl.io/docs/open/widgets/canvas>
- Image rotation/transform：<https://lvgl.io/docs/open/widgets/image>

### 本地资料

- `HX3602C_DS_CN_V1.0.1.pdf`（视频处理芯片技术规格书）
- `主要提示.png`（目标 UI 设计稿）

---

## 9. 附：本仓库代码

- `src/lv_nav_path/lv_nav_path.h` / `.c`
  - 路径点管理、累计长度、Catmull-Rom 平滑、按距离取点/航向；
  - LVGL 9 多遍描边绘制（发光/描边/渐变主线/高光/已走过路径）；
  - 可直接在 `LV_EVENT_DRAW_MAIN` 中调用。
- 代码默认使用 **LVGL 9.3+** 的 `lv_draw_line_dsc_t.points`；LVGL 8 请参考文件内注释降级为 `lv_line`。
- 示例坐标是屏幕绝对坐标；实际项目请根据卡片位置加偏移，或把路径构建成卡片局部坐标后在绘制时平移。

---

## 10. 实时加载周围地图瓦片（“路线框图”怎么做）

你圈出的红框是“车辆周围的地图底图”。它和蓝色路径是两套数据，必须分开处理：

- **地图底图 = 瓦片（图片）**：按 `z/x/y` 切片，车走到哪里，就加载周围一圈；
- **导航路径 = 一条全局折线**：按经纬度/世界坐标保存，投影到屏幕后一次性画在瓦片上面，**不需要按瓦片切分，也不需要每块瓦片重复画**；
- **自车箭头 / 目的地 Pin = 独立图片**：位置由 GPS/路径计算，叠在最上层。

这样做的最大好处：车移动时只需要“换瓦片”，路径几何不变，不会出现“路径跟着瓦片重新加载而闪烁”的问题。

### 10.1 先定坐标系：世界像素 + 瓦片号

以 Web Mercator、缩放级别 `z`、瓦片边长 `T`（推荐 128 或 256）为例：

```
worldSize(z) = T * 2^z                 // 整个世界在 z 级的像素宽高
worldX(lon)  = (lon + 180) / 360 * worldSize
worldY(lat)  = (1 - ln(tan(lat*π/180) + 1/cos(lat*π/180)) / π) / 2 * worldSize

tileX = floor(worldX / T)
tileY = floor(worldY / T)
```

屏幕坐标：

```
screenX = worldX - (centerWorldX - viewW/2)
screenY = worldY - (centerWorldY - viewH/2)
```

其中 `centerWorld` 不是简单的 GPS 点，建议加“前视偏移”：

```
centerWorld = carWorld + velocity * lookAheadSec   // 例如 1.5~2.5 s
```

这样车不会总在卡片正中间，前方能看到更多路。

> 中国境内用 GCJ-02。如果 GPS 输出 WGS-84，先做 WGS-84 → GCJ-02，再投墨卡托。X-TRACK 的 `GPS_Transform` / `MapConv` 可以直接参考。

### 10.2 视口需要哪些瓦片

设地图卡片 `viewW × viewH`（截图约 366×300），当前中心世界像素 `(cx, cy)`：

```
left   = cx - viewW/2
top    = cy - viewH/2
right  = cx + viewW/2
bottom = cy + viewH/2

tx0 = floor(left  / T) - margin
tx1 = floor(right / T) + margin
ty0 = floor(top   / T) - margin
ty1 = floor(bottom/ T) + margin
```

`margin` 建议 1：多加载一圈，车移动时不会看到空白。

以 366×300、T=256、margin=1 为例：

```
可见 2×2 ~ 3×3 块，加一圈后约 4×4 = 16 块
```

以 T=128 为例：

```
可见 4×5 = 20 块，加一圈后约 5×6 = 30 块
```

**HX3602C 的 4 MB PSRAM 建议优先 T=128**：

| 瓦片尺寸 | 单块 RGB565 | 16 块 | 30 块 | 评价 |
|---|---:|---:|---:|---|
| 256×256 | 128 KB | 2.0 MB | 3.75 MB | 16 块已经很紧，30 块放不下 |
| 128×128 | 32 KB | 512 KB | 960 KB | **推荐**，还能留出 OSD/视频/应用内存 |
| 64×64 | 8 KB | 128 KB | 240 KB | 对象太多，draw task 多，不推荐 |

> 如果一定要 256×256，建议只缓存 9 块可见 + 3~4 块预取，并且不要同时保留全屏 ARGB OSD。

### 10.3 加载顺序：从中心向外，再预取前方

不要按 `for y` / `for x` 顺序加载，否则车在移动时可能先加载到屏幕边缘。推荐“环形优先级”：

```
优先级 0：当前中心瓦片
优先级 1：上下左右 4 块
优先级 2：四个角
优先级 3：外圈预取（尤其车头方向）
```

伪代码：

```c
int cx_tile = floor(cx / T);
int cy_tile = floor(cy / T);

for (int ring = 0; ring <= max_ring; ring++) {
    for (int dy = -ring; dy <= ring; dy++) {
        for (int dx = -ring; dx <= ring; dx++) {
            if (ring > 0 && abs(dx) != ring && abs(dy) != ring) continue;
            int tx = cx_tile + dx;
            int ty = cy_tile + dy;
            if (!tile_in_view(tx, ty)) continue;
            tile_request(tx, ty, ring);   // ring 越小优先级越高
        }
    }
}
```

预取方向：

- 用最近 2 个 GPS 点算航向 `heading`；
- 在 `heading` 方向多取 1~2 块（例如 `dx = cos(heading)*2`、`dy = sin(heading)*2`）；
- 高速/低缩放时把 `margin` 加到 2。

### 10.4 缓存：固定槽位 + LRU + 状态机

不要在每帧 `malloc/free` 瓦片。用固定槽位：

```c
typedef enum { EMPTY, LOADING, READY, FAILED } tile_state_t;

typedef struct {
    int32_t tx, ty;
    uint8_t z;
    uint8_t state;
    uint32_t last_used;
    uint32_t generation;
    void *pixels;          // RGB565 buffer / lv_draw_buf_t*
} tile_slot_t;
```

规则：

1. 按 `(z, tx, ty)` 查找；
2. `EMPTY` 直接用；
3. 没有空槽，淘汰 `READY/FAILED` 中 `last_used` 最旧、且**不在当前可见范围**的；
4. `LOADING` 的槽不能淘汰；
5. 当前可见的 `READY` 槽不能淘汰；
6. 换缩放级别时 `generation++`，旧世代的加载结果直接丢弃（避免“放大后突然冒出旧图”）。

### 10.5 异步流水线：UI 线程永远不阻塞

```
GPS 5~10 Hz
   │
   ▼
计算视口/缺块 ──► 请求队列（优先级） ──► 加载任务
   │                                      │
   │                                      ├─ SD 卡：SDIO + DMA 读 .bin
   │                                      ├─ 网络：HTTP GET / 手机下发
   │                                      └─ JPEG：硬件解码 → RGB565
   │                                      │
   ▼                                      ▼
LVGL 只读 READY 槽位  ◄── 完成队列 ◄── 写槽位 + 标记 READY
```

关键点：

- **UI 线程只做“查表 + 贴图”**，绝不在 LVGL 任务里读 SD/等网络；
- 加载任务用独立线程/RTOS task，优先级低于显示/输入；
- 同时最多 2~4 个 in-flight 请求，避免 SDIO/WiFi 被单个方向占满；
- 完成后用 `lv_async_call()` 或消息队列通知 UI，不要直接跨线程碰 LVGL；
- 失败重试 2 次 + 退避；仍失败就保持占位块，不要卡住队列；
- 看门狗：单次读/解码不要超过 100~200 ms，超时切块。

### 10.6 LVGL 怎么贴：两种方式

#### 方式 A：每个瓦片一个 `lv_image`（推荐）

- 预创建 `slot_count` 个 `lv_image` 对象，和槽位一一绑定；
- `READY` 时 `lv_image_set_src(img, slot->pixels)`；
- 位置 = `nav_tilemap_screen_offset(tx, ty)`；
- 只改位置/源，不反复创建/删除对象；
- 瓦片之间用整数像素对齐，避免 1 px 缝；
- 父容器开 `clip_corner`，圆角由卡片负责；
- 可选：新瓦片 `image_opa` 从 0 动画到 255，减少“跳变”。

```c
for (uint16_t i = 0; i < tm->slot_count; i++) {
    tile_slot_t *s = &tm->slots[i];
    lv_obj_t *img = imgs[i];

    if (s->state != READY) {
        lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
        continue;
    }

    int32_t x, y;
    nav_tilemap_screen_offset(tm, s->tx, s->ty, &x, &y);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(img, x, y);
    lv_image_set_src(img, s->pixels);
}
```

#### 方式 B：一张大 Canvas，手动 blit

- `lv_canvas` 366×300 RGB565 ≈ 220 KB；
- 每块瓦片就绪时，只把该瓦片区域 `memcpy` 到 canvas 对应位置；
- 优点：只有一个对象，LVGL 合成简单；
- 缺点：多一次内存拷贝（30 块 × 32 KB = 960 KB/全屏刷新），CPU 压力更大；
- 适合瓦片不频繁变化、或需要做整体滤镜/暗化的情况。

> 截图里的“暗化 + 低对比度”建议直接烘焙进瓦片（预生成深色样式），不要在 LVGL 里叠全屏黑色半透明矩形，否则每帧都要 Alpha 混合，192 MHz 的 CPU 会吃不消。

### 10.7 车动时怎么“跟随”

不要每帧把地图容器按小数像素平移，容易抖动/模糊。推荐：

1. 用浮点世界坐标算屏幕偏移；
2. 渲染时取整（`floor`/`round`）；
3. 设置一个**死区**：车移动 < 6~10 px 不重排瓦片，只移动自车箭头；
4. 超过死区再整体平移容器/瓦片；
5. 瓦片边界跨过时，才加载新的一行/一列，并回收离开视口的槽位；
6. 自车箭头单独用 `lv_image`，每帧按航向旋转，不受瓦片加载影响。

这样 30 km/h、zoom 17（约 1.2 m/px）时，10 px ≈ 12 m，大约 1.4 s 才需要动一次地图；瓦片 256 px ≈ 300 m，约 36 s 才换一块，实时压力很小。

### 10.8 缩放/旋转/3D

- **缩放**：`z` 变化时立即用旧瓦片临时缩放（父级 2× / 子级 0.5×）占位，再异步加载新 z 的瓦片；不要白屏。
- **旋转**：HX3602C 只有 0/90/180/270 和镜像，没有任意角度旋转。北朝上 + 旋转车标是成本最低的方案；车头朝上要么手机端做（方案 A），要么预生成 8/16 个方向的瓦片（存储×8/×16，不推荐）。
- **3D 透视**：不要在 MCU 上做。截图那种 3D 视角建议留在手机端渲染（方案 A）。

### 10.9 瓦片从哪来

| 来源 | 优点 | 缺点 | 适用 |
|---|---|---|---|
| SD 卡预置 `.bin`（RGB565） | 无网络、解码零成本、最稳定 | 占存储；更新地图麻烦 | 离线/量产预置 |
| SD 卡预置 JPEG 瓦片 | 存储小 3~5 倍 | 需要硬件 JPEG 解码；解码有耗时 | 存储紧张的离线方案 |
| 手机通过 WiFi 下发 | 地图新、可实时路况 | 依赖手机；带宽/延迟 | 方案 A 的变种 |
| 高德 RTOS SDK 在线瓦片 | 数据权威、POI/路况全 | 需要授权/激活/网络 | 方案 B |
| 自建/OSM 瓦片服务 | 可控、可定制深色样式 | 数据/合规/运维 | Demo/特定区域 |

**重要合规提醒**：高德/百度等商业地图瓦片不能随意抓取、离线缓存或二次分发；量产必须走高德官方 SDK/授权，或使用有明确许可的 OSM/自建数据。

### 10.10 HX3602C 上的实现要点

1. **瓦片格式**：优先 RGB565 `.bin`，LVGL 可直接 `lv_image_set_src`，无解码；文件头 8~16 字节（宽、高、格式、CRC）。
2. **SDIO + DMA**：4-bit、50 MHz，理论 ~25 MB/s；128 KB 瓦片约 5 ms，30 块约 150 ms，可接受。
3. **对齐**：瓦片缓冲 32 字节对齐（Cache line/DMA），否则 PSRAM 读写会变慢或出错。
4. **JPEG 瓦片**：用硬件 JPEG 解码到 RGB565；解码任务和 LVGL 分离，避免阻塞。
5. **内存**：T=128 时 30 块 ≈ 960 KB；加上 LVGL 局部缓冲 77 KB、OSD 1.23 MB、视频 165 KB，总计约 2.4 MB，4 MB 够用。
6. **不要全屏重绘**：只 invalidate 新瓦片/移动区域；静态瓦片不重绘。
7. **预热**：开机/进入导航时预加载当前视口 + 一圈；之后只补差量。
8. **降级**：SD 卡拔出/网络断开时保留最后一张图 + 占位块 + “地图加载中”提示，不要黑屏。

### 10.11 一帧的完整顺序

```
1. GPS/IMU 更新（5~10 Hz）
2. 更新 carWorld、heading、lookAhead
3. 计算视口 tile 范围
4. 缺块 -> 入队（中心优先，前方预取）
5. 已就绪瓦片 -> 更新 lv_image 位置/源
6. 投影 route polyline -> 裁剪 -> nav_path_draw()
7. 更新自车箭头位置/角度
8. 更新转向卡/速度/电量（仅变化时）
9. lv_timer_handler() 刷脏区
```

> 注意：第 6 步的路径绘制频率可以低于瓦片更新；路线不变时，只有视口平移才需要重绘。可以把“瓦片 + 路径 + Pin”缓存到 canvas，每帧只动箭头。

### 10.12 本仓库的瓦片调度模块

`src/nav_tilemap/nav_tilemap.h` / `.c` 提供了一个不依赖 LVGL 的纯 C 调度器：

- 视口计算、瓦片范围、环形优先级；
- 固定槽位 + LRU + 世代号；
- `request_cb` 由应用实现（SD/HTTP/手机）；
- `commit/fail` 完成回调；
- `screen_offset()` 直接给出 LVGL 贴图坐标。

用法：

```c
static nav_tilemap_t tm;
nav_tilemap_init(&tm, 128, 366, 300, 30, 3);
tm.release_cb = tile_free;          // 淘汰时释放像素缓冲

/* GPS 更新 */
nav_tilemap_set_view(&tm, car_x, car_y, ahead_x, ahead_y, zoom);
nav_tilemap_update(&tm, tile_request, NULL);

/* 加载任务完成后 */
nav_tilemap_commit(&tm, z, tx, ty, rgb565_buf);

/* LVGL 绘制 */
int32_t x, y;
nav_tilemap_screen_offset(&tm, tx, ty, &x, &y);
lv_obj_set_pos(img, x, y);
```

完整实现见代码文件；报告里不再重复。

### 10.13 先确认：你要的是“装饰性骨架图”还是“真实地图”

截图里的底图其实更像**设计稿画的深色骨架图**（道路块 + 建筑块），不一定是真实高德地图。两者实现成本差一个数量级：

| 类型 | 长什么样 | 数据来源 | 实现 | 适用 |
|---|---|---|---|---|
| **A. 装饰性骨架图** | 深色道路/建筑块，不要求与真实道路一致 | 设计一张/几张无缝背景图 | 平移背景图 + 画路径 + 动车标 | Demo、固定园区、成本优先 |
| **B. 真实栅格瓦片** | 真实道路/建筑/POI | 高德 RTOS SDK / 离线瓦片 / 手机下发 | 第 10.1~10.12 的瓦片调度 | 量产导航 |
| **C. 矢量路网** | 只有道路线，没有 POI/文字 | OSM/自建矢量瓦片 | 每块瓦片存折线，LVGL 画线 | 小存储、可缩放、风格化 |

**如果截图只是 UI 设计，底图不要求真实**，不要做瓦片调度：  
- 让设计出一张 **2× 视口大小**的深色无缝背景（例如 732×600）；
- 用两个 `lv_image` 横向/纵向拼接，车动时反向平移，越界后交换；
- 路径是设计好的曲线/或按 GPS 投影的折线；
- 车标按航向旋转。
这样 CPU/内存几乎为零，效果和截图一致。

**只有当你需要“车走到哪里，地图真的变到哪里”**，才需要下面的瓦片调度。

### 10.14 骨架图实时滚动的最简实现

```c
/* 背景图 2 倍视口宽，横向无缝 */
lv_obj_t *bg1 = lv_image_create(card);
lv_obj_t *bg2 = lv_image_create(card);
lv_image_set_src(bg1, &bg_map);
lv_image_set_src(bg2, &bg_map);

/* 车向右移动 10 px，背景向左移动 10 px */
int32_t offset = (int32_t)fmod(car_world_x, bg_width);
lv_obj_set_pos(bg1, -offset, 0);
lv_obj_set_pos(bg2, bg_width - offset, 0);

/* 路径固定/按 GPS 投影，画在背景上 */
nav_path_draw(layer, &route, &style, progress);
```

要点：

- 背景图要**左右/上下可无缝拼接**（边缘像素一致），否则会看到接缝；
- 平移用整数像素，避免模糊；
- 只 invalidate 移动区域，或把背景放到独立 canvas；
- 如果车只在固定园区跑，可以直接用一张大图（比如 2048×1024），不用循环拼接。

### 10.15 矢量路网瓦片的做法（可选）

如果既要“真实道路”，又要省存储/省流量：

1. 离线把 OSM/高德数据转成自定义矢量瓦片：每块 `z/x/y` 存若干条道路折线；
2. 二进制格式建议：
   ```
   header: magic, version, z, x, y, feature_count
   feature: class(u8), width(u8), point_count(u16),
            points: delta-encoded int16 x/y（相对瓦片左上角）
   ```
3. 加载后按 bbox 裁剪，只保留视口内线段；
4. 按道路等级合并同色线段，用 `lv_draw_line_dsc_t.points` 一次画一类；
5. 不要画 POI 文字/路名，MCU 上文字排版和碰撞检测很贵；
6. 缩放时按 `z` 换瓦片；相邻级别可以做 2×/0.5× 占位。

优点：一块瓦片几 KB~几十 KB，放大不糊；缺点：需要自己做数据转换、裁剪、LOD、道路样式。

---

### 10.16 一句话总结

- **只要截图效果、不要真实地图**：无缝骨架图平移，别做瓦片；
- **要真实地图、手机能参与**：手机渲染整张地图卡片（方案 A），MCU 只解码；
- **要真实地图、设备独立**：高德 RTOS SDK / 离线栅格瓦片 + `nav_tilemap`；
- **路径永远是一条全局折线**，不要按瓦片切分；瓦片只负责底图。
