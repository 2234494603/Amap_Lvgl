import * as maplibregl from 'https://unpkg.com/maplibre-gl@6.3.0/dist/maplibre-gl.mjs';

const STYLE_ROOT = 'https://tiles.openfreemap.org/styles';
const OSRM_ROOT = 'https://router.project-osrm.org';
const EMPTY_COLLECTION = { type: 'FeatureCollection', features: [] };

const styles = {
  liberty: `${STYLE_ROOT}/liberty`,
  bright: `${STYLE_ROOT}/bright`,
  positron: `${STYLE_ROOT}/positron`,
  dark: `${STYLE_ROOT}/dark`
};

const sampleRoute = {
  type: 'FeatureCollection',
  features: [{
    type: 'Feature',
    properties: { name: '成都城区用户轨迹示例' },
    geometry: {
      type: 'LineString',
      coordinates: [
        [104.0527, 30.5747], [104.0565, 30.5770], [104.0619, 30.5764],
        [104.0668, 30.5738], [104.0705, 30.5715], [104.0753, 30.5698],
        [104.0796, 30.5668]
      ]
    }
  }]
};

const state = {
  start: null,
  end: null,
  pickMode: null,
  planned: EMPTY_COLLECTION,
  imported: EMPTY_COLLECTION,
  startMarker: null,
  endMarker: null,
  lastRoute: null,
  lvglPoints: [],
  routeRevision: 1,
  sequence: 0,
  progress: 0,
  navigationTimer: null,
  syncBusy: false
};

const els = Object.fromEntries([
  'map-style', 'pick-start', 'pick-end', 'plan-route', 'swap-route', 'clear-route',
  'load-sample', 'route-file', 'start-label', 'end-label', 'status', 'map-tip',
  'route-summary', 'distance', 'duration', 'steps', 'send-route', 'nav-progress',
  'progress-label', 'toggle-navigation', 'check-bridge', 'phone-status', 'bridge-indicator'
].map((id) => [id.replaceAll('-', '_'), document.getElementById(id)]));

const map = new maplibregl.Map({
  container: 'map',
  style: styles.liberty,
  center: [104.0668, 30.5728],
  zoom: 12.6,
  pitch: 38,
  bearing: -11,
  attributionControl: true
});

map.addControl(new maplibregl.NavigationControl({ visualizePitch: true }), 'top-right');
map.addControl(new maplibregl.ScaleControl({ unit: 'metric' }), 'bottom-left');

map.on('load', restoreOverlays);
map.on('style.load', restoreOverlays);
map.on('click', (event) => {
  if (!state.pickMode) return;
  setPoint(state.pickMode, [event.lngLat.lng, event.lngLat.lat]);
  setPickMode(null);
});

els.map_style.addEventListener('change', () => {
  map.setStyle(styles[els.map_style.value]);
  setStatus(`正在切换 ${els.map_style.selectedOptions[0].textContent}…`);
});
els.pick_start.addEventListener('click', () => setPickMode('start'));
els.pick_end.addEventListener('click', () => setPickMode('end'));
els.plan_route.addEventListener('click', planRoute);
els.swap_route.addEventListener('click', swapRoute);
els.clear_route.addEventListener('click', clearAll);
els.load_sample.addEventListener('click', loadSample);
els.route_file.addEventListener('change', importRouteFile);
els.send_route.addEventListener('click', publishRoute);
els.toggle_navigation.addEventListener('click', toggleNavigation);
els.check_bridge.addEventListener('click', checkBridgeStatus);
els.nav_progress.addEventListener('input', () => {
  state.progress = Number(els.nav_progress.value);
  updateProgressUi();
});
els.nav_progress.addEventListener('change', publishRoute);

checkBridgeStatus();

function restoreOverlays() {
  if (!map.isStyleLoaded()) return;
  ensureRouteLayer('planned', state.planned, '#22d3ee', '#005eff');
  ensureRouteLayer('imported', state.imported, '#ffb340', '#5c3510', true);
  setStatus(state.start && state.end ? '起终点已就绪，可以开始规划' : '请选择起点和终点');
}

function ensureRouteLayer(id, data, color, casing, dashed = false) {
  if (!map.getSource(id)) {
    map.addSource(id, { type: 'geojson', data });
  } else {
    map.getSource(id).setData(data);
  }

  if (!map.getLayer(`${id}-casing`)) {
    map.addLayer({
      id: `${id}-casing`,
      type: 'line',
      source: id,
      layout: { 'line-cap': 'round', 'line-join': 'round' },
      paint: { 'line-color': casing, 'line-width': dashed ? 8 : 11, 'line-opacity': 0.78 }
    });
  }
  if (!map.getLayer(`${id}-line`)) {
    map.addLayer({
      id: `${id}-line`,
      type: 'line',
      source: id,
      layout: { 'line-cap': 'round', 'line-join': 'round' },
      paint: {
        'line-color': color,
        'line-width': dashed ? 4 : 6,
        'line-opacity': 0.96,
        ...(dashed ? { 'line-dasharray': [2, 1.4] } : {})
      }
    });
  }
}

function updateSource(id, data) {
  if (map.getSource(id)) map.getSource(id).setData(data);
}

function setPickMode(mode) {
  state.pickMode = mode;
  els.pick_start.classList.toggle('active', mode === 'start');
  els.pick_end.classList.toggle('active', mode === 'end');
  map.getCanvas().style.cursor = mode ? 'crosshair' : '';
  els.map_tip.textContent = mode === 'start'
    ? '点击地图设置起点'
    : mode === 'end' ? '点击地图设置终点' : '先选择“起点”或“终点”，再点击地图';
}

function setPoint(kind, coordinates) {
  state[kind] = coordinates;
  const isStart = kind === 'start';
  const markerKey = isStart ? 'startMarker' : 'endMarker';
  const label = isStart ? 'A' : 'B';
  if (state[markerKey]) state[markerKey].remove();

  const element = document.createElement('div');
  element.className = `route-marker ${isStart ? 'start' : 'end'}`;
  element.innerHTML = `<span>${label}</span>`;
  state[markerKey] = new maplibregl.Marker({ element, draggable: true, anchor: 'bottom' })
    .setLngLat(coordinates)
    .addTo(map);
  state[markerKey].on('dragend', () => {
    const lngLat = state[markerKey].getLngLat();
    state[kind] = [lngLat.lng, lngLat.lat];
    updatePointLabels();
  });
  updatePointLabels();
}

function updatePointLabels() {
  els.start_label.textContent = state.start ? formatCoordinate(state.start) : '点击后在地图选择';
  els.end_label.textContent = state.end ? formatCoordinate(state.end) : '点击后在地图选择';
}

function formatCoordinate([lng, lat]) {
  return `${lng.toFixed(5)}, ${lat.toFixed(5)}`;
}

async function planRoute() {
  if (!state.start || !state.end) {
    setStatus('请先选择起点和终点', 'error');
    return;
  }

  const coordinates = `${state.start.join(',')};${state.end.join(',')}`;
  const url = `${OSRM_ROOT}/route/v1/driving/${coordinates}?overview=full&geometries=geojson&steps=true&alternatives=false`;
  setStatus('正在请求 OSRM 规划路线…');
  els.plan_route.disabled = true;

  try {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const result = await response.json();
    if (result.code !== 'Ok' || !result.routes?.length) throw new Error(result.message || '未找到可行路线');

    const route = result.routes[0];
    state.planned = {
      type: 'FeatureCollection',
      features: [{ type: 'Feature', properties: {}, geometry: route.geometry }]
    };
    updateSource('planned', state.planned);
    fitData(state.planned, 90);
    renderSummary(route);
    prepareLvglRoute(route);
    setStatus('路线规划完成', 'success');
    await publishRoute();
  } catch (error) {
    setStatus(`规划失败：${error.message}。可检查网络或改用本地 OSRM。`, 'error');
  } finally {
    els.plan_route.disabled = false;
  }
}

function renderSummary(route) {
  els.route_summary.hidden = false;
  els.distance.textContent = route.distance >= 1000
    ? `${(route.distance / 1000).toFixed(1)} km`
    : `${Math.round(route.distance)} m`;
  els.duration.textContent = route.duration >= 3600
    ? `${Math.floor(route.duration / 3600)} h ${Math.round((route.duration % 3600) / 60)} min`
    : `${Math.max(1, Math.round(route.duration / 60))} min`;

  const steps = route.legs.flatMap((leg) => leg.steps || []);
  els.steps.innerHTML = steps.map((step) => {
    const road = escapeHtml(step.name || '未命名道路');
    const instruction = maneuverText(step.maneuver);
    return `<li>${instruction}，进入 ${road}<span>${formatDistance(step.distance)}</span></li>`;
  }).join('');
}

function maneuverText(maneuver = {}) {
  const typeMap = {
    depart: '从起点出发', arrive: '到达目的地', turn: '转弯', continue: '继续行驶',
    merge: '汇入道路', fork: '在岔路口', roundabout: '进入环岛', 'new name': '沿道路继续'
  };
  const modifierMap = {
    left: '向左', right: '向右', straight: '直行', 'slight left': '稍向左',
    'slight right': '稍向右', 'sharp left': '向左急转', 'sharp right': '向右急转', uturn: '掉头'
  };
  const type = typeMap[maneuver.type] || '继续行驶';
  const modifier = modifierMap[maneuver.modifier] || '';
  return `${modifier}${type}`;
}

function formatDistance(meters) {
  return meters >= 1000 ? `${(meters / 1000).toFixed(1)} km` : `${Math.round(meters)} m`;
}

function swapRoute() {
  if (!state.start || !state.end) return;
  const start = [...state.start];
  setPoint('start', state.end);
  setPoint('end', start);
  setStatus('已交换起点和终点');
}

function clearAll() {
  stopNavigation();
  state.start = null;
  state.end = null;
  state.planned = EMPTY_COLLECTION;
  state.imported = EMPTY_COLLECTION;
  state.startMarker?.remove();
  state.endMarker?.remove();
  state.startMarker = null;
  state.endMarker = null;
  state.lastRoute = null;
  state.lvglPoints = [];
  state.progress = 0;
  updateSource('planned', state.planned);
  updateSource('imported', state.imported);
  updatePointLabels();
  els.route_summary.hidden = true;
  els.route_file.value = '';
  els.send_route.disabled = true;
  els.nav_progress.disabled = true;
  els.toggle_navigation.disabled = true;
  updateProgressUi();
  setPickMode(null);
  setStatus('已清除路线');
}

function loadSample() {
  setPoint('start', [104.0527, 30.5747]);
  setPoint('end', [104.0796, 30.5668]);
  state.imported = structuredClone(sampleRoute);
  updateSource('imported', state.imported);
  fitData(state.imported, 90);
  setStatus('已加载成都示例轨迹；点击“开始规划”可比较规划路线', 'success');
}

async function importRouteFile(event) {
  const file = event.target.files?.[0];
  if (!file) return;
  try {
    const text = await file.text();
    const data = file.name.toLowerCase().endsWith('.gpx') ? parseGpx(text) : normalizeGeoJson(JSON.parse(text));
    if (!data.features.length) throw new Error('文件中没有可显示的线状路线');
    state.imported = data;
    updateSource('imported', data);
    fitData(data, 70);
    setStatus(`已加载 ${file.name}`, 'success');
  } catch (error) {
    setStatus(`导入失败：${error.message}`, 'error');
  }
}

function normalizeGeoJson(data) {
  const features = data.type === 'FeatureCollection'
    ? data.features
    : data.type === 'Feature' ? [data] : [{ type: 'Feature', properties: {}, geometry: data }];
  return {
    type: 'FeatureCollection',
    features: features.filter((feature) => ['LineString', 'MultiLineString'].includes(feature.geometry?.type))
  };
}

function parseGpx(text) {
  const xml = new DOMParser().parseFromString(text, 'application/xml');
  if (xml.querySelector('parsererror')) throw new Error('GPX XML 格式无效');
  const groups = [...xml.querySelectorAll('trkseg, rte')];
  const features = groups.map((group, index) => {
    const points = [...group.querySelectorAll('trkpt, rtept')]
      .map((point) => [Number(point.getAttribute('lon')), Number(point.getAttribute('lat'))])
      .filter(([lng, lat]) => Number.isFinite(lng) && Number.isFinite(lat));
    return {
      type: 'Feature',
      properties: { name: `GPX 路段 ${index + 1}` },
      geometry: { type: 'LineString', coordinates: points }
    };
  }).filter((feature) => feature.geometry.coordinates.length >= 2);
  return { type: 'FeatureCollection', features };
}

function fitData(data, padding = 60) {
  const coordinates = [];
  for (const feature of data.features || []) collectCoordinates(feature.geometry?.coordinates, coordinates);
  if (!coordinates.length) return;
  const bounds = coordinates.reduce(
    (box, coordinate) => box.extend(coordinate),
    new maplibregl.LngLatBounds(coordinates[0], coordinates[0])
  );
  map.fitBounds(bounds, { padding, duration: 900, maxZoom: 16 });
}

function collectCoordinates(value, output) {
  if (!Array.isArray(value)) return;
  if (value.length >= 2 && typeof value[0] === 'number' && typeof value[1] === 'number') {
    output.push(value);
    return;
  }
  value.forEach((entry) => collectCoordinates(entry, output));
}

function setStatus(message, tone = '') {
  els.status.textContent = message;
  els.status.className = `status ${tone}`.trim();
}

function prepareLvglRoute(route) {
  state.lastRoute = route;
  state.lvglPoints = routeToLvglPoints(route.geometry.coordinates);
  state.routeRevision = Date.now() >>> 0;
  state.progress = 0;
  els.send_route.disabled = false;
  els.nav_progress.disabled = false;
  els.toggle_navigation.disabled = false;
  updateProgressUi();
  setPhoneStatus(`已转换为 ${state.lvglPoints.length} 个 LVGL 路线点`, 'success');
}

async function publishRoute() {
  if (!state.lastRoute || state.lvglPoints.length < 2 || state.syncBusy) return;
  state.syncBusy = true;
  els.send_route.disabled = true;
  const firstStep = state.lastRoute.legs?.[0]?.steps?.find((step) => step.maneuver?.type !== 'depart')
    || state.lastRoute.legs?.[0]?.steps?.[0]
    || {};
  const payload = {
    route_revision: state.routeRevision,
    sequence: (++state.sequence) >>> 0,
    distance_m: Math.round(state.lastRoute.distance || 0),
    duration_s: Math.round(state.lastRoute.duration || 0),
    progress_per_mille: state.progress,
    points: state.lvglPoints,
    maneuver: {
      type: firstStep.maneuver?.type || '',
      road: firstStep.name || '',
      distance_m: Math.round(firstStep.distance || 0)
    }
  };

  try {
    const response = await fetch('/api/route', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    const result = await response.json();
    if (!response.ok || !result.ok) throw new Error(result.error || `HTTP ${response.status}`);
    setBridgeOnline(true);
    setPhoneStatus(`已同步：${result.point_count} 点，进度 ${Math.round(state.progress / 10)}%`, 'success');
  } catch (error) {
    setBridgeOnline(false);
    setPhoneStatus(`同步失败：${error.message}`, 'error');
  } finally {
    state.syncBusy = false;
    els.send_route.disabled = !state.lastRoute;
  }
}

async function checkBridgeStatus() {
  try {
    const response = await fetch('/api/status', { cache: 'no-store' });
    const result = await response.json();
    if (!response.ok || !result.ok) throw new Error(`HTTP ${response.status}`);
    setBridgeOnline(true);
    setPhoneStatus(result.route
      ? `桥接服务在线，最近序列 ${result.route.sequence}`
      : '桥接服务在线，等待第一条路线', 'success');
  } catch (error) {
    setBridgeOnline(false);
    setPhoneStatus(`桥接服务不可用：${error.message}`, 'error');
  }
}

function toggleNavigation() {
  if (state.navigationTimer) {
    stopNavigation();
    return;
  }
  if (!state.lastRoute) return;
  els.toggle_navigation.textContent = '暂停模拟导航';
  state.navigationTimer = window.setInterval(async () => {
    state.progress = Math.min(1000, state.progress + 8);
    updateProgressUi();
    await publishRoute();
    if (state.progress >= 1000) stopNavigation();
  }, 500);
}

function stopNavigation() {
  if (state.navigationTimer) window.clearInterval(state.navigationTimer);
  state.navigationTimer = null;
  els.toggle_navigation.textContent = '开始模拟导航';
}

function updateProgressUi() {
  els.nav_progress.value = String(state.progress);
  els.progress_label.textContent = `${Math.round(state.progress / 10)}%`;
}

function setBridgeOnline(online) {
  els.bridge_indicator.textContent = online ? '已连接' : '未连接';
  els.bridge_indicator.className = `bridge-indicator ${online ? 'online' : 'offline'}`;
}

function setPhoneStatus(message, tone = '') {
  els.phone_status.textContent = message;
  els.phone_status.className = `status ${tone}`.trim();
}

function routeToLvglPoints(coordinates) {
  const width = 456;
  const height = 248;
  const padding = 12;
  const projected = coordinates.map(([lng, lat]) => projectMercator(lng, lat));
  const xs = projected.map(([x]) => x);
  const ys = projected.map(([, y]) => y);
  const minX = Math.min(...xs);
  const maxX = Math.max(...xs);
  const minY = Math.min(...ys);
  const maxY = Math.max(...ys);
  const spanX = Math.max(maxX - minX, Number.EPSILON);
  const spanY = Math.max(maxY - minY, Number.EPSILON);
  const scale = Math.min((width - padding * 2) / spanX, (height - padding * 2) / spanY);
  const contentWidth = spanX * scale;
  const contentHeight = spanY * scale;
  const offsetX = (width - contentWidth) / 2;
  const offsetY = (height - contentHeight) / 2;
  const pixels = projected.map(([x, y]) => [
    clamp(Math.round(offsetX + (x - minX) * scale), 0, width - 1),
    clamp(Math.round(offsetY + (y - minY) * scale), 0, height - 1)
  ]);
  return simplifyToLimit(removeAdjacentDuplicates(pixels), 64);
}

function projectMercator(lng, lat) {
  const limitedLat = clamp(lat, -85.05112878, 85.05112878);
  const sin = Math.sin(limitedLat * Math.PI / 180);
  return [
    (lng + 180) / 360,
    0.5 - Math.log((1 + sin) / (1 - sin)) / (4 * Math.PI)
  ];
}

function simplifyToLimit(points, limit) {
  if (points.length <= limit) return points;
  let tolerance = 0.75;
  let result = points;
  while (result.length > limit && tolerance <= 64) {
    result = douglasPeucker(points, tolerance);
    tolerance *= 1.45;
  }
  if (result.length <= limit) return result;
  const sampled = [];
  for (let i = 0; i < limit; i += 1) {
    sampled.push(points[Math.round(i * (points.length - 1) / (limit - 1))]);
  }
  return removeAdjacentDuplicates(sampled);
}

function douglasPeucker(points, tolerance) {
  if (points.length <= 2) return points;
  let maxDistance = 0;
  let splitIndex = 0;
  const first = points[0];
  const last = points[points.length - 1];
  for (let i = 1; i < points.length - 1; i += 1) {
    const distance = segmentDistance(points[i], first, last);
    if (distance > maxDistance) {
      maxDistance = distance;
      splitIndex = i;
    }
  }
  if (maxDistance <= tolerance) return [first, last];
  return [
    ...douglasPeucker(points.slice(0, splitIndex + 1), tolerance).slice(0, -1),
    ...douglasPeucker(points.slice(splitIndex), tolerance)
  ];
}

function segmentDistance(point, start, end) {
  const dx = end[0] - start[0];
  const dy = end[1] - start[1];
  if (dx === 0 && dy === 0) return Math.hypot(point[0] - start[0], point[1] - start[1]);
  const t = clamp(((point[0] - start[0]) * dx + (point[1] - start[1]) * dy) / (dx * dx + dy * dy), 0, 1);
  return Math.hypot(point[0] - (start[0] + t * dx), point[1] - (start[1] + t * dy));
}

function removeAdjacentDuplicates(points) {
  return points.filter((point, index) => index === 0
    || point[0] !== points[index - 1][0]
    || point[1] !== points[index - 1][1]);
}

function clamp(value, minimum, maximum) {
  return Math.min(maximum, Math.max(minimum, value));
}

function escapeHtml(value) {
  return String(value).replace(/[&<>'"]/g, (char) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;'
  })[char]);
}
