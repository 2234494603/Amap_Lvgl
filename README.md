明白，已经按“手机导航模拟器 → MCU+LVGL 副屏”重新改造。

当前链路：

```text
Test_Environment 模拟手机导航
        ↓ OSRM 规划
投影、抽稀到最多 64 点
        ↓ POST /api/route
bridge_server.py
        ↓ latest_route.bin
GaoDe_Map Windows Simulator
        ↓ 每 250 ms
动态路线 + 车头进度
```

使用方法：

1. 打开 **http://127.0.0.1:8080**
2. 在手机模拟器页面选择起点、终点并规划。
3. 路线会自动发送到 LVGL 桥接文件。
4. 运行你那边的 `GaoDe_Map` Simulator。
5. 点击“开始模拟导航”，LVGL 车头会跟随进度更新。

新增内容：

- [手机同步界面](C:/Users/Administrator/Documents/Adobe/GitHub/Test_Environment/index.html:80)
- [路线投影、抽稀和发送逻辑](C:/Users/Administrator/Documents/Adobe/GitHub/Test_Environment/app.js:361)
- [桥接服务器](C:/Users/Administrator/Documents/Adobe/GitHub/Test_Environment/bridge_server.py)
- [LVGL 接收器](C:/Users/Administrator/Documents/Adobe/GitHub/GUI_Guider/GaoDe_Map/custom/phone_nav_bridge.c)
- [完整架构说明](C:/Users/Administrator/Documents/Adobe/GitHub/README.md:190)

桥接服务器当前 PID 是 `28432`。JSON 到二进制数据包已经实测通过；遵照要求，没有编译 LVGL 工程。

目前属于“路线示意镜像”：路线和进度是真实动态的，但 LVGL 使用固定地图背景，所以任意路线不一定贴合背景道路。该限制和后续地图快照同步方案也已经写入 [README.md](C:/Users/Administrator/Documents/Adobe/GitHub/README.md:254)。

# Amap LVGL 开源地图导航工程

本仓库包含两个相互配合的部分：

| 目录 | 作用 |
| --- | --- |
| `GUI_Guider/GaoDe_Map` | GUI Guider 1.10 / LVGL 8.3 的 480×272 导航界面 |
| `Test_Environment` | 模拟手机导航软件，负责地图、规划、路线转换和副屏同步 |

整体目标是先在浏览器中加载真实地图、导入用户路线并完成路径规划，再把规划结果转换成 MCU/LVGL 能够绘制的局部像素点。

## 当前状态

- 测试页面：<http://127.0.0.1:8080>
- 当前桥接服务器 PID：`28432`
- 测试环境不需要 npm，不需要编译；
- GUI Guider 目标工程：`GUI_Guider/GaoDe_Map/GaoDe_Map.guiguider`；
- LVGL 版本：8.3；
- 屏幕尺寸：480×272；
- 地图卡片区域：456×248；
- 路线容量：最多 64 个 `lv_point_t` 点；
- 当前导航底图和卡片位于 `GaoDe_Map/custom/`；
- 手机模拟器通过 `/api/route` 发布路线；
- Windows LVGL Simulator 每 250 ms 读取一次桥接数据；
- 当前没有执行 LVGL 工程编译。

PID 只对应本次启动，服务器重新启动后会变化。

## 测试环境已实现功能

- MapLibre GL JS 矢量地图渲染；
- OpenFreeMap / OpenStreetMap 地图数据；
- Liberty、Bright、Positron、Dark 四种地图样式；
- 地图缩放、旋转、倾斜和比例尺；
- 点击地图选择起点和终点；
- 起终点标记拖动；
- 起终点交换；
- OSRM 驾车路线规划；
- 距离、预计时间和逐向导航步骤；
- GeoJSON 用户路线导入；
- GPX track / route 导入；
- 用户轨迹和规划路线同时显示；
- 成都示例路线。
- 自动把经纬度路线投影到 456×248；
- Douglas-Peucker 抽稀到最多 64 点；
- 一键把路线同步到 LVGL 副屏；
- 导航进度滑块；
- 自动模拟导航并同步车头进度；
- 显示手机到 LVGL 桥接连接状态。

## 启动测试环境

在 PowerShell 中执行：

```powershell
cd C:\Users\Administrator\Documents\Adobe\GitHub\Test_Environment
.\start-server.ps1
```

打开：

```text
http://127.0.0.1:8080
```

不要直接用 `file://` 打开 `index.html`，浏览器会限制 ES Module 和本地资源加载。

指定其他端口：

```powershell
.\start-server.ps1 -Port 8090
```

停止本次桥接服务器：

```powershell
Stop-Process -Id 28432
```

如果 PID 已变化，可查询 8080 端口：

```powershell
Get-NetTCPConnection -LocalPort 8080 -State Listen |
    Select-Object -ExpandProperty OwningProcess
```

## 测试页面操作

### 规划路线

1. 点击“起点”，再点击地图；
2. 点击“终点”，再点击地图；
3. 点击“开始规划”；
4. 页面显示规划路线、总距离、预计时间和导航步骤；
5. 拖动 A、B 标记可以调整起终点并重新规划。

### 对比用户轨迹

1. 点击“成都示例”，或导入 GeoJSON / GPX；
2. 橙色虚线表示用户轨迹；
3. 点击“开始规划”；
4. 蓝色发光线表示 OSRM 规划结果；
5. 两条路线会同时显示，便于比较。

支持的文件：

- `.geojson`
- `.json`
- `.gpx`

支持的 GeoJSON 几何：

- `LineString`
- `MultiLineString`
- `Feature`
- `FeatureCollection`

支持的 GPX 节点：

- `trkseg` / `trkpt`
- `rte` / `rtept`

## 开源地图和路线规划来源

| 项目 | 用途 | 许可证 / 数据许可 | 地址 |
| --- | --- | --- | --- |
| MapLibre GL JS | WebGL 矢量地图渲染 | BSD-3-Clause | https://github.com/maplibre/maplibre-gl-js |
| OpenFreeMap | OpenStreetMap 矢量瓦片 | 开源项目 | https://github.com/hyperknot/openfreemap |
| OpenFreeMap Styles | 地图视觉样式 | 以仓库说明为准 | https://github.com/hyperknot/openfreemap-styles |
| OpenStreetMap | 地图和道路数据 | ODbL | https://www.openstreetmap.org/copyright |
| OSRM Backend | 道路路线规划 | BSD-2-Clause | https://github.com/Project-OSRM/osrm-backend |

MapLibre 当前固定为 6.3.0：

```text
https://unpkg.com/maplibre-gl@6.3.0/dist/maplibre-gl.mjs
https://unpkg.com/maplibre-gl@6.3.0/dist/maplibre-gl.css
```

OpenFreeMap 样式：

```text
https://tiles.openfreemap.org/styles/liberty
https://tiles.openfreemap.org/styles/bright
https://tiles.openfreemap.org/styles/positron
https://tiles.openfreemap.org/styles/dark
```

## OSRM 路线接口

当前测试地址：

```javascript
const OSRM_ROOT = 'https://router.project-osrm.org';
```

请求形式：

```text
GET /route/v1/driving/{起点经度},{起点纬度};{终点经度},{终点纬度}
    ?overview=full
    &geometries=geojson
    &steps=true
    &alternatives=false
```

返回数据中需要关注：

| JSON 字段 | 用途 |
| --- | --- |
| `routes[0].geometry.coordinates` | 完整经纬度路线点 |
| `routes[0].distance` | 总距离，单位米 |
| `routes[0].duration` | 预计时间，单位秒 |
| `routes[0].legs[].steps[]` | 转向、道路名和分段距离 |

成都示例实测结果：

| 检查项 | 结果 |
| --- | --- |
| OSRM 状态 | `Ok` |
| 路线距离 | 约 4472 m |
| 预计时间 | 约 416 s |
| 原始路线几何点 | 115 个 |
| 示例用户轨迹点 | 7 个 |
| 本地首页 | HTTP 200 |
| 四种地图样式 | 全部 HTTP 200 |

公共 OSRM 服务只适合测试。产品化时需要自建 OSRM，并修改 `Test_Environment/app.js` 中的 `OSRM_ROOT`。

## 已实现的手机导航到 LVGL 动态桥接

当前实际链路：

```text
Test_Environment 手机导航模拟器
    ├─ MapLibre 显示真实地图
    ├─ OSRM 规划道路路线
    ├─ Web Mercator 投影到 456×248
    ├─ Douglas-Peucker 抽稀到 ≤64 点
    └─ POST /api/route
                ↓
bridge_server.py
    ├─ 校验坐标、点数和进度
    ├─ 原子写入 latest_route.json
    └─ 原子写入 latest_route.bin
                ↓ 每 250 ms
phone_nav_bridge.c（Windows LVGL Simulator）
    ├─ 新路线：gaode_map_set_route()
    └─ 新进度：gaode_map_set_progress()
                ↓
GaoDe_Map MCU+LVGL 副屏动态刷新
```

桥接相关文件：

```text
Test_Environment/bridge_server.py
Test_Environment/bridge/latest_route.json
Test_Environment/bridge/latest_route.bin
GUI_Guider/GaoDe_Map/custom/phone_nav_bridge.c
GUI_Guider/GaoDe_Map/custom/phone_nav_bridge.h
```

浏览器规划成功后会自动发送一次完整路线。也可以点击“发送当前规划路线”。“导航进度”滑块和“开始模拟导航”会继续发布同一路线的新进度，LVGL 无需重复创建路线，只更新车头位置。

桥接二进制包使用小端格式：

| 偏移 | 大小 | 字段 |
| --- | --- | --- |
| 0 | 4 | 魔数 `GDRT` |
| 4 | 1 | 协议版本，当前为 1 |
| 5 | 1 | flags |
| 6 | 2 | 点数，2..64 |
| 8 | 4 | route revision |
| 12 | 4 | sequence |
| 16 | 4 | 总距离，米 |
| 20 | 4 | 预计时间，秒 |
| 24 | 2 | 导航进度，0..1000 |
| 26 | 2 | 保留字段 |
| 28 | N×4 | `uint16 x + uint16 y` 点列 |

Windows Simulator 默认读取：

```text
C:/Users/Administrator/Documents/Adobe/GitHub/Test_Environment/bridge/latest_route.bin
```

可以通过环境变量覆盖：

```powershell
$env:GAODE_PHONE_ROUTE_FILE = 'D:\nav\latest_route.bin'
```

### 当前桥接属于“示意路线镜像”

`GaoDe_Map` 当前使用固定三维地图图片作为背景。手机可以动态发送任意路线形状和导航进度，但任意真实路线不会自动与这张固定图片中的道路完全重合。

当前模式适合验证：

- 手机规划结果能否传到 MCU 副屏；
- 路线形状、终点和车头进度能否动态更新；
- 重新规划时能否替换旧路线；
- UART/BLE/Wi-Fi 数据协议。

如果产品要求手机和 MCU 显示完全一致的道路，需选择以下方案之一：

1. **手机同步地图快照**：手机按 MCU 视口渲染 456×248 地图，压缩后连同路线发送；
2. **MCU 使用同一地图数据**：MCU 读取相同区域的离线瓦片或矢量道路，并采用同一投影、中心、缩放和旋转；
3. **只显示路线示意图**：保留装饰性暗色背景，只保证路线形状和导航信息准确，不宣称背景道路对应真实位置。

当前代码采用第 3 种方式。它的传输量最小，也最适合作为 MCU 通信和 LVGL 动态路线的第一阶段验证。

# 如何接入 GaoDe_Map.guiguider

## 1. 先理解 `.guiguider` 和运行时代码的分工

`GaoDe_Map.guiguider` 是 GUI Guider 的界面工程文件，它负责页面、尺寸、资源和代码生成配置。

浏览器地图、HTTP 请求和 OSRM JSON 不能直接写进 `.guiguider` 文件。动态导航功能应放在：

```text
GUI_Guider/GaoDe_Map/custom/
```

当前运行时入口已经存在：

```c
custom_init(&guider_ui);
```

Windows Simulator 的调用位置：

```text
GUI_Guider/GaoDe_Map/lvgl-simulator/main.c
```

自定义代码构建入口：

```text
GUI_Guider/GaoDe_Map/custom/custom.mk
```

GUI Guider 重新生成时可能覆盖 `generated/` 和部分 `main.c`。业务逻辑应保留在 `custom/`，不要长期修改生成文件。

## 2. 当前可以直接调用的接口

接口头文件：

```text
GUI_Guider/GaoDe_Map/custom/custom.h
```

初始化导航界面：

```c
void custom_init(lv_ui *ui);
```

替换路线：

```c
bool gaode_map_set_route(const lv_point_t *points, uint16_t count);
```

停止内置动画：

```c
void gaode_map_set_demo_enabled(bool enabled);
```

路线点坐标要求：

- 原点是地图卡片左上角，不是整个 480×272 屏幕左上角；
- 合法范围为 `x = 0..455`、`y = 0..247`；
- 点数至少 2 个，最多 64 个；
- `gaode_map_set_route()` 会复制点数组，调用完成后临时数组可以复用；
- 必须从 LVGL/UI 线程调用。

## 3. 最简单的静态路线接入

先把一条规划结果转换成 `lv_point_t` 数组：

```c
#include "custom.h"

static const lv_point_t route_from_osrm[] = {
    {228, 247},
    {231, 224},
    {240, 151},
    {254, 112},
    {285, 101},
    {326, 91},
    {365, 80},
    {399, 68},
    {411, 60},
    {373, 51}
};

static void load_user_route(void)
{
    gaode_map_set_demo_enabled(false);
    gaode_map_set_route(
        route_from_osrm,
        (uint16_t)(sizeof(route_from_osrm) / sizeof(route_from_osrm[0]))
    );
}
```

调用顺序：

```c
setup_ui(&guider_ui);
custom_init(&guider_ui);
load_user_route();
```

如果工程使用的是已有 `main.c`，只需要在现有 `custom_init(&guider_ui)` 后调用 `load_user_route()`。

## 4. 为什么不能直接传 OSRM 经纬度

OSRM 返回的是：

```json
[104.0527, 30.5747]
```

LVGL 路线接口需要的是：

```c
{228, 247}
```

因此必须执行：

```text
经纬度
  → Web Mercator 投影
  → 计算路线范围
  → 等比例缩放到 456×248
  → 留出边距
  → 路线抽稀
  → lv_point_t
```

## 5. 经纬度转换到地图像素

先进行 Web Mercator 归一化：

```javascript
function project(lng, lat) {
  const x = (lng + 180) / 360;
  const sin = Math.sin(lat * Math.PI / 180);
  const y = 0.5 - Math.log((1 + sin) / (1 - sin)) / (4 * Math.PI);
  return [x, y];
}
```

然后将整条路线等比例放入 456×248 地图区域：

```javascript
function routeToLvglPoints(coordinates) {
  const width = 456;
  const height = 248;
  const padding = 12;
  const projected = coordinates.map(([lng, lat]) => project(lng, lat));

  const xs = projected.map((p) => p[0]);
  const ys = projected.map((p) => p[1]);
  const minX = Math.min(...xs);
  const maxX = Math.max(...xs);
  const minY = Math.min(...ys);
  const maxY = Math.max(...ys);

  const scale = Math.min(
    (width - padding * 2) / Math.max(maxX - minX, Number.EPSILON),
    (height - padding * 2) / Math.max(maxY - minY, Number.EPSILON)
  );

  const contentWidth = (maxX - minX) * scale;
  const contentHeight = (maxY - minY) * scale;
  const offsetX = (width - contentWidth) / 2;
  const offsetY = (height - contentHeight) / 2;

  return projected.map(([x, y]) => [
    Math.round(offsetX + (x - minX) * scale),
    Math.round(offsetY + (y - minY) * scale)
  ]);
}
```

必须保持横纵比例一致，不能分别拉伸 X 和 Y，否则道路转弯角度会失真。

## 6. 把 115 个路线点减少到 64 个以内

当前 `ROUTE_CAPACITY` 是 64，而成都实测路线有 115 个点，因此必须抽稀。

推荐使用 Douglas-Peucker 算法，并在像素坐标转换后执行：

1. 初始容差建议 1.5～3 像素；
2. 如果点数仍超过 64，逐步提高容差；
3. 必须保留首点、终点和明显拐点；
4. 最终检查所有坐标都在 456×248 范围内。

不要简单地只取每 N 个点。均匀跳点可能删除关键转弯。

## 7. 当前模拟桥接和真实 MCU 接入架构

当前 Windows Simulator 使用 `latest_route.bin` 模拟手机向副屏发送数据。真实 MCU 上不应使用文件轮询，而是让手机、上位机或导航服务完成地图和 OSRM 计算：

```text
手机 / 上位机 / 本地服务器
    ├─ 加载 OpenStreetMap
    ├─ 请求 OSRM
    ├─ 经纬度投影
    ├─ Douglas-Peucker 抽稀
    └─ 生成最多 64 个局部像素点
                ↓
       UART / USB / BLE / Wi-Fi
                ↓
MCU 接收路线数据
                ↓
gaode_map_set_route()
                ↓
LVGL 刷新发光路线、终点和车头
```

这样 MCU 不需要解析完整地图瓦片，也不需要处理庞大的 OSRM JSON。

## 8. 推荐发送给 MCU 的数据

JSON 测试格式：

```json
{
  "route_id": 1,
  "distance_m": 4472,
  "duration_s": 416,
  "point_count": 10,
  "points": [[228,247], [231,224], [240,151], [254,112], [285,101], [326,91], [365,80], [399,68], [411,60], [373,51]],
  "maneuver": {
    "type": "turn-right",
    "distance_m": 500,
    "road": "软件园三号路"
  }
}
```

正式 MCU 通信建议使用二进制帧，减少内存和带宽：

```text
帧头 + route_id + point_count + x/y 点列 + distance + maneuver + CRC
```

456×248 坐标可以使用 16 位整数。若只考虑该尺寸，也可以设计更紧凑的编码。

## 9. MCU 收到路线后的示例

```c
#define MAX_ROUTE_POINTS 64

static lv_point_t received_points[MAX_ROUTE_POINTS];
static uint16_t received_count;

void navigation_route_received(const uint16_t *xy, uint16_t count)
{
    uint16_t i;

    if(count < 2 || count > MAX_ROUTE_POINTS) return;

    for(i = 0; i < count; i++) {
        uint16_t x = xy[i * 2];
        uint16_t y = xy[i * 2 + 1];
        if(x >= 456 || y >= 248) return;
        received_points[i].x = (lv_coord_t)x;
        received_points[i].y = (lv_coord_t)y;
    }

    received_count = count;

    /* 切换到 LVGL/UI 线程后调用。 */
    gaode_map_set_demo_enabled(false);
    gaode_map_set_route(received_points, received_count);
}
```

串口、网络或 BLE 回调通常不在 LVGL 线程中，不要直接在中断或后台线程操作 LVGL。应发送事件或消息，让 LVGL 任务执行最后两行。

## 10. 动态重新规划

推荐流程：

1. MCU 或手机持续获取车辆位置；
2. 判断车辆到当前路线的距离；
3. 偏离超过阈值时重新请求 OSRM；
4. 上位机重新投影并抽稀；
5. 发送新的路线点；
6. LVGL 调用 `gaode_map_set_route()` 替换旧路线；
7. 车头位置和终点标记同步更新。

## 11. 转向卡片的当前限制

当前顶部中文转向卡片是为了贴近参考图而使用的静态图片。

虽然 `custom.h` 中保留了：

```c
void gaode_map_set_navigation(const char *distance_text, const char *road_text);
```

但当前实现暂时不会修改图片内的文字。要显示实时道路名和距离，需要下一步将卡片拆成：

- 半透明卡片背景；
- 独立转向图标；
- 距离标签；
- 道路名称标签；
- 中文字体资源。

不要直接在静态卡片图片上叠加第二套文字，否则会出现重影。

## 12. GUI Guider 中能看到什么

GUI Guider 编辑器中主要看到静态预览背景。动态路线由 `custom_init()` 在运行时创建，因此只有 Simulator 或真实设备运行时才会出现和运动。

正确流程是：

```text
打开 GaoDe_Map.guiguider
    ↓
生成 GUI Guider 基础代码
    ↓
保留 custom/ 目录
    ↓
运行 Simulator 或设备程序
    ↓
custom_init() 创建动态路线
    ↓
gaode_map_set_route() 接收规划结果
```

## Test_Environment 目录

```text
Test_Environment/
├─ index.html
├─ styles.css
├─ app.js
├─ start-server.ps1
├─ bridge_server.py
├─ bridge/
│  ├─ latest_route.json
│  └─ latest_route.bin
├─ README.md
├─ 这里是测试环境.txt
└─ data/
   └─ sample-route.geojson
```

## GaoDe_Map 关键文件

```text
GUI_Guider/GaoDe_Map/
├─ GaoDe_Map.guiguider
├─ custom/
│  ├─ custom.c
│  ├─ custom.h
│  ├─ custom.mk
│  ├─ lv_nav_path_v8.c
│  ├─ lv_nav_path_v8.h
│  ├─ phone_nav_bridge.c
│  ├─ phone_nav_bridge.h
│  └─ gaode_*.c
├─ generated/
├─ import/image/
└─ lvgl-simulator/
   ├─ main.c
   └─ Makefile
```

## 联网和数据说明

测试页面会访问：

- `unpkg.com`：MapLibre JS/CSS；
- `tiles.openfreemap.org`：地图样式、字体、图标和瓦片；
- `router.project-osrm.org`：路线规划。

GeoJSON 和 GPX 文件只在浏览器本地解析，当前不会上传文件本体。起终点坐标会发送给 OSRM。

## 当前限制

- 测试页面需要互联网；
- 当前只使用 OSRM `driving`；
- 没有地址搜索和逆地理编码；
- 没有实时交通和道路封闭数据；
- 公共服务不适合产品环境；
- 用户轨迹当前用于显示和比较，不参与路径代价计算；
- GUI Guider 顶部导航文字当前是静态图片；
- OSRM 经纬度不能直接传给 LVGL；
- 路线必须抽稀到 64 点以内；
- 动态 LVGL API 必须从 UI 线程调用。

## 离线和产品化建议

1. 先下载一个城市或省份的 OpenStreetMap PBF；
2. 自建 OSRM Backend；
3. 使用 MBTiles 或 PMTiles 保存地图；
4. 使用本地瓦片服务器；
5. 将 `STYLE_ROOT` 和 `OSRM_ROOT` 改成本地地址；
6. 固定地图数据版本；
7. 由上位机将路线转换成 MCU 像素点；
8. 通过 UART/BLE/Wi-Fi 发送给 LVGL 设备。

不要直接在普通开发机部署全球 OpenFreeMap 数据。全球数据需要大量磁盘和内存，应先使用城市级数据验证。

## 下一步

- 将 Windows 文件桥替换为真实 UART/BLE/Wi-Fi 传输；
- 为二进制帧增加 CRC、确认和重传机制；
- 输出 `.h` 路线文件作为离线调试方式；
- 增加途经点；
- 增加步行、自行车路线；
- 增加路线偏离检测；
- 将静态转向卡片改为可更新的中文 LVGL 组件；
- 接入串口、BLE 或 Wi-Fi 实时路线更新。
