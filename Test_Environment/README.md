# 开源地图路线规划测试环境

该目录模拟手机导航软件：负责加载地图、导入用户路线、调用 OSRM 规划，并把转换后的路线和导航进度动态同步到 MCU+LVGL 副屏。

当前版本是纯 HTML、CSS、JavaScript 工程，不需要 npm，不需要编译。浏览器首次打开时需要联网加载地图渲染库、地图瓦片和路线规划结果。

## 当前运行状态

- 本地地址：<http://127.0.0.1:8080>
- 当前桥接服务器进程 PID：`18296`
- 工程目录：`C:\Users\Administrator\Documents\Adobe\GitHub\Test_Environment`
- 当前地图渲染：MapLibre GL JS 6.3.0
- 当前地图数据：OpenFreeMap / OpenStreetMap
- 当前路线规划：OSRM 公共测试服务
- 构建方式：无构建、无编译
- LVGL 桥接接口：`POST /api/route`
- LVGL 桥接数据：`bridge/latest_route.bin`

PID 只对应本次启动。如果服务器重新启动，PID 会发生变化。

当前 LVGL 端使用固定三维地图图片，因此桥接首先验证“路线形状和导航进度镜像”。任意手机路线不会自动与固定背景道路完全重合。若要求两端地图道路完全一致，还需要由手机同步 456×248 地图快照，或让 MCU 使用同一套地图数据、投影、中心、缩放和旋转参数。

## 已实现功能

- 加载和切换多种开源矢量底图；
- 支持 Liberty、Bright、Positron、Dark 四种地图样式；
- 地图缩放、旋转、倾斜、比例尺显示；
- 点击地图选择起点和终点；
- 起终点标记可拖动调整；
- 一键交换起点和终点；
- 使用 OSRM 规划真实道路驾车路线；
- 显示路线距离、预计时间和逐向导航步骤；
- 显示道路名称和各路段距离；
- 导入用户 GeoJSON 路线；
- 导入用户 GPX track 和 route；
- 同时显示“用户轨迹”和“规划路线”，用于结果对比；
- 内置成都示例轨迹；
- 地图样式切换后自动恢复规划路线和用户路线。

## 页面预览入口

服务器运行时，在浏览器打开：

```text
http://127.0.0.1:8080
```

也可以使用：

```text
http://localhost:8080
```

不要直接双击 `index.html` 使用 `file://` 打开。浏览器对 ES Module 和本地资源有限制，应通过 HTTP 静态服务器访问。

## 启动方法

在 PowerShell 中执行：

```powershell
cd C:\Users\Administrator\Documents\Adobe\GitHub\Test_Environment
.\start-server.ps1
```

指定其他端口：

```powershell
.\start-server.ps1 -Port 8090
```

启动脚本会优先使用 Windows Python Launcher `py`，找不到时再使用 `python`。

也可以手动启动：

```powershell
cd C:\Users\Administrator\Documents\Adobe\GitHub\Test_Environment
py -m http.server 8080 --bind 127.0.0.1
```

## 停止服务器

停止本次已经启动的进程：

```powershell
Stop-Process -Id 18296
```

如果服务器重新启动过，应先查询实际 PID：

```powershell
Get-NetTCPConnection -LocalPort 8080 -State Listen |
    Select-Object -ExpandProperty OwningProcess
```

然后停止查询到的进程。

## 使用步骤

### 规划道路路线

1. 点击左侧“起点”。
2. 在地图上点击起点位置。
3. 点击左侧“终点”。
4. 在地图上点击终点位置。
5. 点击“开始规划”。
6. 页面会显示规划路线、总距离、预计时间和转向步骤。
7. 可以拖动 A、B 标记调整位置，然后重新规划。

### 加载成都示例

1. 点击“成都示例”。
2. 页面会显示一条橙色用户轨迹，并自动设置起终点。
3. 点击“开始规划”。
4. 蓝色 OSRM 规划路线会和橙色用户轨迹同时显示，便于比较路线差异。

### 导入用户路线

1. 点击“导入 GeoJSON / GPX”。
2. 选择用户路线文件。
3. 页面读取路线并自动缩放到路线范围。
4. 导入路线使用橙色虚线显示。

支持的扩展名：

- `.geojson`
- `.json`
- `.gpx`

支持的 GeoJSON 几何类型：

- `LineString`
- `MultiLineString`
- `Feature`
- `FeatureCollection`

支持的 GPX 内容：

- `trkseg` / `trkpt`
- `rte` / `rtept`

## 地图样式

| 页面选项 | OpenFreeMap 样式地址 | 用途 |
| --- | --- | --- |
| Liberty | `https://tiles.openfreemap.org/styles/liberty` | 通用道路地图 |
| Bright | `https://tiles.openfreemap.org/styles/bright` | 明亮、信息密度较高 |
| Positron | `https://tiles.openfreemap.org/styles/positron` | 浅色简洁背景 |
| Dark | `https://tiles.openfreemap.org/styles/dark` | 深色导航界面 |

地图样式根地址在 `app.js` 中配置：

```javascript
const STYLE_ROOT = 'https://tiles.openfreemap.org/styles';
```

## 路线规划接口

当前使用 OSRM 公共演示服务：

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

关键参数：

- `overview=full`：返回完整路线几何；
- `geometries=geojson`：使用 GeoJSON 坐标格式；
- `steps=true`：返回逐向导航步骤；
- `alternatives=false`：当前只取一条推荐路线。

公共服务只适合开发验证。产品化时应部署自己的 OSRM 服务，并将 `OSRM_ROOT` 替换成本地或自有服务器地址。

## 已完成的验证

本次使用成都示例坐标进行了真实 OSRM 请求，结果如下：

| 检查项 | 结果 |
| --- | --- |
| OSRM 返回状态 | `Ok` |
| 路线距离 | 约 `4472 m` |
| 预计时间 | 约 `416 s` |
| 完整路线几何点 | `115` 个 |
| 示例 GeoJSON 类型 | `FeatureCollection` |
| 示例路线点 | `7` 个 |
| 本地首页 HTTP 状态 | `200` |
| MapLibre CDN | HTTP `200` |
| 四种 OpenFreeMap 样式 | 全部 HTTP `200` |

以上验证没有执行任何编译。

## 工程目录

```text
Test_Environment/
├─ index.html                 页面结构和控制面板
├─ styles.css                页面、地图和路线视觉样式
├─ app.js                    地图加载、选点、规划和文件解析逻辑
├─ start-server.ps1          本地静态服务器启动脚本
├─ README.md                 当前说明文档
├─ 这里是测试环境.txt          原测试目录标记文件
└─ data/
   └─ sample-route.geojson   成都用户轨迹示例
```

## 代码结构

### `index.html`

负责地图容器、底图选择、起终点按钮、路线规划按钮、用户路线文件选择，以及路线距离、时间和步骤展示。

### `styles.css`

负责深色导航界面、响应式布局、路线图例、起终点标记，以及规划路线和用户路线的视觉区分。

### `app.js`

负责 MapLibre 初始化、OpenFreeMap 样式切换、起终点交互、OSRM 请求、GeoJSON 图层、GPX 解析、路线范围计算、导航步骤中文化和示例路线加载。

## 路线颜色约定

- 蓝色发光实线：OSRM 规划路线；
- 橙色虚线：用户导入路线或示例轨迹；
- 绿色 A 标记：起点；
- 蓝色 B 标记：终点。

## 开源项目和许可证

| 项目 | 用途 | 许可证 / 数据许可 | GitHub / 官方地址 |
| --- | --- | --- | --- |
| MapLibre GL JS | 浏览器 WebGL 矢量地图渲染 | BSD-3-Clause | https://github.com/maplibre/maplibre-gl-js |
| OpenFreeMap | OpenStreetMap 矢量瓦片和地图样式 | 开源项目 | https://github.com/hyperknot/openfreemap |
| OpenFreeMap Styles | Bright、Liberty 等地图样式 | 以项目仓库说明为准 | https://github.com/hyperknot/openfreemap-styles |
| OpenStreetMap | 道路和地理地图数据 | ODbL | https://www.openstreetmap.org/copyright |
| OSRM Backend | 道路路线规划 | BSD-2-Clause | https://github.com/Project-OSRM/osrm-backend |

MapLibre GL JS 当前通过以下固定版本加载：

```text
https://unpkg.com/maplibre-gl@6.3.0/dist/maplibre-gl.mjs
https://unpkg.com/maplibre-gl@6.3.0/dist/maplibre-gl.css
```

固定版本可以减少上游版本变化导致的兼容性问题。

## 联网请求说明

页面运行时会访问：

- `unpkg.com`：加载 MapLibre GL JS 和 CSS；
- `tiles.openfreemap.org`：加载地图样式、字体、图标和地图瓦片；
- `router.project-osrm.org`：提交起终点坐标并获取规划路线。

导入的 GeoJSON 和 GPX 文件仅在浏览器本地解析，当前代码不会主动上传用户文件。起点和终点坐标会发送给 OSRM 服务用于规划路线。

## 当前限制

- 当前需要互联网连接；
- 当前只接入 OSRM `driving` 驾车配置；
- 公共 OSRM 服务不保证生产级可用性和请求配额；
- 当前没有地址搜索和逆地理编码；
- 当前没有实时交通、道路封闭或高德交通数据；
- 当前导航步骤是测试级中文转换，不是完整自然语言导航引擎；
- 当前用户路线只显示，不会自动参与 OSRM 路径代价计算；
- 当前没有持久化保存用户路线；
- 中国境内互联网环境可能影响海外地图和路线服务访问速度。

## 产品化和离线部署建议

### 阶段一：浏览器联网验证

使用当前工程验证地图视觉、起终点交互、用户路线格式、路线规划结果和 LVGL 所需的数据字段。

### 阶段二：本地地图和本地 OSRM

- 下载所需地区的 OpenStreetMap PBF 数据；
- 部署自有 OSRM Backend；
- 准备本地 MBTiles 或 PMTiles；
- 使用本地瓦片服务器；
- 把 `STYLE_ROOT` 和 `OSRM_ROOT` 指向本地服务；
- 固定地图数据版本，保证测试可重复。

不要直接在普通开发机上部署整套全球 OpenFreeMap 数据。全球数据需要大量磁盘和内存，应先选择一个城市或省份做离线测试。

### 阶段三：接入 LVGL 导航工程

推荐数据流：

```text
用户起终点
    ↓
本地或远程 OSRM
    ↓
GeoJSON 经纬度路线
    ↓
坐标投影和地图视口裁剪
    ↓
路线点抽稀
    ↓
LVGL 局部像素坐标
    ↓
lv_nav_path_v8_set_points()
```

具体步骤：

1. 从 OSRM `routes[0].geometry.coordinates` 取得 `[经度, 纬度]` 点列；
2. 根据当前地图边界将经纬度转换成地图像素坐标；
3. 使用 Douglas-Peucker 等算法抽稀路线；
4. 将点数限制在 MCU 可接受范围；
5. 裁剪超出屏幕的路线段；
6. 转换为 LVGL `lv_point_t` 数组；
7. 调用 `lv_nav_path_v8_set_points()` 更新动态路线；
8. 使用 `routes[0].legs[].steps[]` 更新转向图标、道路名称和剩余距离；
9. 根据车辆 GPS 位置更新车头图标和路线进度；
10. 偏离规划路线时重新请求 OSRM 并替换路线点列。

## 建议输出给 MCU 的精简格式

浏览器或上位机不应直接把完整 OSRM JSON 发送给 MCU。建议先转换成精简数据：

```json
{
  "route_id": 1,
  "distance_m": 4472,
  "duration_s": 416,
  "points": [[228, 247], [240, 151], [254, 112], [326, 91], [373, 51]],
  "maneuver": {
    "type": "turn-right",
    "distance_m": 500,
    "road": "软件园三号路"
  }
}
```

其中 `points` 已经是目标屏幕上的局部像素坐标，MCU 无需再处理地理投影。

## 常见问题

### 页面打开后地图空白

检查是否通过 HTTP 地址打开、能否访问 `unpkg.com` 和 `tiles.openfreemap.org`、浏览器是否支持 WebGL，以及浏览器 Console 是否有 CORS 或网络错误。

### 点击规划后失败

检查起终点是否位于可通行道路附近、是否能够访问 `router.project-osrm.org`、公共服务是否限流，或者改用本地 OSRM。

### 8080 端口被占用

```powershell
.\start-server.ps1 -Port 8090
```

然后访问 `http://127.0.0.1:8090`。

### 导入文件后没有路线

确认 GeoJSON 包含 `LineString` 或 `MultiLineString`，或 GPX 包含至少两个有效的 `trkpt` / `rtept` 坐标点。

## 下一步开发方向

- 增加途经点和多点路线规划；
- 增加步行、自行车和货车路线配置；
- 增加地址搜索和逆地理编码；
- 增加路线偏离检测和自动重新规划；
- 增加 GPX 时间、高程、速度数据展示；
- 增加路线编辑、保存和导出；
- 增加本地 PMTiles / MBTiles；
- 增加自建 OSRM Docker 配置；
- 增加 OSRM 路线到 LVGL `lv_point_t` 的转换工具；
- 将当前测试结果接入 `GaoDe_Map` LVGL 工程。
