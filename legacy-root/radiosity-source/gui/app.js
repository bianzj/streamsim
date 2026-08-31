import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];
const canvas = $('#viewport');
const SOIL_GRID_RESOLUTION = 48;
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, preserveDrawingBuffer: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;

const world = new THREE.Scene();
world.background = new THREE.Color(0x07100c);
world.fog = new THREE.FogExp2(0x07100c, 0.025);
const camera = new THREE.PerspectiveCamera(38, 1, 0.05, 200);
camera.position.set(9.5, 6.8, 10.5);
const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.target.set(0, 2.8, 0);
controls.minDistance = 2;
controls.maxDistance = 40;

world.add(new THREE.HemisphereLight(0xdcefe4, 0x17251a, 2.0));
const sunlight = new THREE.DirectionalLight(0xffdfaa, 2.8);
sunlight.position.set(-6, 10, 6);
world.add(sunlight);
const grid = new THREE.GridHelper(18, 18, 0x294838, 0x16271d);
grid.position.y = -0.012;
world.add(grid);

const state = {
  workspace: 'radiosity',
  backend: 'gpu',
  display: 'scene',
  mesh: null,
  facetCount: 0,
  leafFacetCount: 0,
  treeCount: 1,
  soilFacetCount: 0,
  baseColors: [],
  baseMaterial: null,
  resultMaterial: null,
  result: null,
  running: false,
  sensorCamera: null,
  sensorHelper: null,
  sceneCenter: new THREE.Vector3(0, 2.8, 0),
  sceneRadius: 4.2,
  selectedFacet: null,
};

function showToast(message) {
  const toast = $('#toast');
  toast.textContent = message;
  toast.classList.add('show');
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.classList.remove('show'), 5500);
}

function log(message) {
  const line = document.createElement('div');
  line.innerHTML = `<time>${new Date().toLocaleTimeString('zh-CN', { hour12: false })}</time><span></span>`;
  line.querySelector('span').textContent = message;
  $('#log-list').append(line);
  $('#log-list').scrollTop = $('#log-list').scrollHeight;
}

function setProgress(value, stage, mode = 'running') {
  const progress = Math.max(0, Math.min(100, Number(value) || 0));
  $('#top-progress-bar').style.width = `${progress}%`;
  $('#top-progress-value').textContent = `${progress}%`;
  $('#top-stage').textContent = stage;
  $('#run-percent').textContent = `${progress}%`;
  $('#run-stage').textContent = stage;
  $('#run-dot').className = `status-dot ${mode}`;
  $('#run-state').textContent = mode === 'running' ? '正在运行' : mode === 'done' ? '运行完成' : mode === 'error' ? '运行失败' : '等待运行';
}

function parseObj(text) {
  const vertices = [];
  const faces = [];
  let treeCount = 1;
  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line) continue;
    if (line.startsWith('# facetlab-tree-count ')) {
      treeCount = Math.max(1, Number(line.split(/\s+/).at(-1)) || 1);
      continue;
    }
    if (line.startsWith('#')) continue;
    const parts = line.split(/\s+/);
    if (parts[0] === 'v') {
      vertices.push(parts.slice(1, 4).map(Number));
    } else if (parts[0] === 'f') {
      const indices = parts.slice(1).map((token) => {
        const raw = Number(token.split('/')[0]);
        return raw > 0 ? raw - 1 : vertices.length + raw;
      });
      for (let index = 1; index + 1 < indices.length; ++index) {
        faces.push([indices[0], indices[index], indices[index + 1]]);
      }
    }
  }
  if (!vertices.length || !faces.length) throw new Error('OBJ 场景为空');
  const minimum = [Infinity, Infinity, Infinity];
  const maximum = [-Infinity, -Infinity, -Infinity];
  for (const vertex of vertices) {
    for (let axis = 0; axis < 3; ++axis) {
      minimum[axis] = Math.min(minimum[axis], vertex[axis]);
      maximum[axis] = Math.max(maximum[axis], vertex[axis]);
    }
  }
  for (const vertex of vertices) vertex[1] -= minimum[1];

  const positions = [];
  for (const face of faces) {
    for (const index of face) positions.push(...vertices[index]);
  }
  const margin = 0.5 * Math.max(maximum[0] - minimum[0], maximum[2] - minimum[2]);
  const x0 = minimum[0] - margin;
  const x1 = maximum[0] + margin;
  const z0 = minimum[2] - margin;
  const z1 = maximum[2] + margin;
  for (let row = 0; row < SOIL_GRID_RESOLUTION; ++row) {
    const za = z0 + (z1 - z0) * row / SOIL_GRID_RESOLUTION;
    const zb = z0 + (z1 - z0) * (row + 1) / SOIL_GRID_RESOLUTION;
    for (let column = 0; column < SOIL_GRID_RESOLUTION; ++column) {
      const xa = x0 + (x1 - x0) * column / SOIL_GRID_RESOLUTION;
      const xb = x0 + (x1 - x0) * (column + 1) / SOIL_GRID_RESOLUTION;
      positions.push(
        xa, -0.01, za, xb, -0.01, zb, xb, -0.01, za,
        xa, -0.01, za, xa, -0.01, zb, xb, -0.01, zb,
      );
    }
  }
  return {
    positions: new Float32Array(positions),
    leafFacetCount: faces.length,
    soilFacetCount: 2 * SOIL_GRID_RESOLUTION * SOIL_GRID_RESOLUTION,
    treeCount,
    bounds: {
      minimum: [x0, -0.01, z0],
      maximum: [x1, maximum[1] - minimum[1], z1],
    },
  };
}

async function loadScene() {
  const response = await fetch('/api/scene');
  if (!response.ok) throw new Error(await response.text());
  const parsed = parseObj(await response.text());
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(parsed.positions, 3));
  geometry.computeVertexNormals();
  geometry.setAttribute('color', new THREE.BufferAttribute(
    new Float32Array(parsed.positions.length), 3
  ));
  state.baseMaterial = new THREE.MeshStandardMaterial({
    vertexColors: true,
    side: THREE.DoubleSide,
    roughness: 0.86,
    metalness: 0,
  });
  state.resultMaterial = new THREE.MeshBasicMaterial({
    vertexColors: true,
    side: THREE.DoubleSide,
    toneMapped: false,
  });
  state.mesh = new THREE.Mesh(geometry, state.baseMaterial);
  state.facetCount = parsed.positions.length / 9;
  state.leafFacetCount = parsed.leafFacetCount;
  state.soilFacetCount = parsed.soilFacetCount;
  state.treeCount = parsed.treeCount;
  world.add(state.mesh);

  const soilGeometry = new THREE.BufferGeometry();
  const soilOffset = state.leafFacetCount * 9;
  soilGeometry.setAttribute(
    'position',
    new THREE.BufferAttribute(parsed.positions.slice(soilOffset), 3),
  );
  const soilGrid = new THREE.LineSegments(
    new THREE.EdgesGeometry(soilGeometry, 1),
    new THREE.LineBasicMaterial({ color: 0x263a2d, transparent: true, opacity: 0.32 }),
  );
  soilGrid.position.y = 0.006;
  world.add(soilGrid);
  for (let facet = 0; facet < state.facetCount; ++facet) {
    state.baseColors.push(new THREE.Color(
      facet < state.leafFacetCount ? 0x3d985d : 0x77593c
    ));
  }
  applyDisplay();
  const minimum = new THREE.Vector3(...parsed.bounds.minimum);
  const maximum = new THREE.Vector3(...parsed.bounds.maximum);
  state.sceneCenter.set(
    0.5 * (minimum.x + maximum.x),
    minimum.y + 0.38 * (maximum.y - minimum.y),
    0.5 * (minimum.z + maximum.z),
  );
  state.sceneRadius = 0.5 * maximum.clone().sub(minimum).length();
  // 大场景需要更低的指数雾密度，否则相机后退后远处林冠会被雾完全吞没。
  world.fog.density = Math.min(0.025, 0.25 / Math.max(state.sceneRadius, 1));
  controls.maxDistance = Math.max(40, state.sceneRadius * 4);
  controls.target.copy(state.sceneCenter);
  camera.position.copy(state.sceneCenter).addScaledVector(
    new THREE.Vector3(1, 0.72, 1.12).normalize(),
    Math.max(12, state.sceneRadius * 2.15),
  );
  camera.far = Math.max(200, state.sceneRadius * 8);
  camera.updateProjectionMatrix();
  controls.update();
  $('#leaf-count').textContent = state.leafFacetCount.toLocaleString();
  $('#soil-count').textContent = state.soilFacetCount.toLocaleString();
  $('#scene-summary').textContent =
    `${state.treeCount} 棵树 · ${state.leafFacetCount.toLocaleString()} 个叶片面元 · ` +
    `${state.soilFacetCount.toLocaleString()} 个土壤面元`;
  $('#triangle-stat').textContent = `${state.facetCount.toLocaleString()} TRI`;
  log(`场景加载完成：${state.facetCount} 个面元。`);
}

function turbo(value) {
  const x = THREE.MathUtils.clamp(value, 0, 1);
  return new THREE.Color(
    THREE.MathUtils.clamp(1.5 - Math.abs(4 * x - 3), 0, 1),
    THREE.MathUtils.clamp(1.5 - Math.abs(4 * x - 2), 0, 1),
    THREE.MathUtils.clamp(1.5 - Math.abs(4 * x - 1), 0, 1),
  );
}

function displayValues(mode) {
  const result = state.result;
  if (!result || mode === 'scene') return null;
  const source = mode === 'sunlit'
    ? result.sunlit
    : mode === 'lightEnhancement'
      ? result.lightEnhancement
      : result.radiosity;
  if (!source) return null;
  const values = new Float32Array(state.facetCount);
  const selectedSide = mode === 'radiosityFront'
    ? 0
    : mode === 'radiosityBack'
      ? 1
      : null;
  for (let facet = 0; facet < state.facetCount; ++facet) {
    const front = source[facet * 2] || 0;
    const back = source[facet * 2 + 1] || 0;
    values[facet] = selectedSide === null
      ? Math.max(front, back)
      : selectedSide === 0 ? front : back;
  }
  return values;
}

function colorFacets(values, base = false) {
  if (!state.mesh) return { minimum: 0, maximum: 1 };
  const scaleValues = values
    ? values.subarray(0, Math.min(state.leafFacetCount, values.length))
    : null;
  const minimum = scaleValues?.length ? Math.min(...scaleValues) : 0;
  const maximum = scaleValues?.length ? Math.max(...scaleValues) : 1;
  const attribute = state.mesh.geometry.getAttribute('color');
  for (let facet = 0; facet < state.facetCount; ++facet) {
    const normalized = values
      ? (values[facet] - minimum) / Math.max(maximum - minimum, 1e-9)
      : 0;
    const color = values ? turbo(normalized) : state.baseColors[facet];
    for (let corner = 0; corner < 3; ++corner) {
      attribute.setXYZ(facet * 3 + corner, color.r, color.g, color.b);
    }
  }
  attribute.needsUpdate = true;
  state.mesh.material = values ? state.resultMaterial : state.baseMaterial;
  return { minimum, maximum };
}

function applyDisplay() {
  const values = displayValues(state.display);
  const range = colorFacets(values);
  const legend = $('#legend');
  legend.hidden = !values;
  if (!values) return;
  const labels = {
    sunlit: ['叶片双面最大受光率', ''],
    lightEnhancement: ['叶片光照增强', ' W/m²'],
    radiosity: ['叶片双面最大辐射度', ' W/m²'],
    radiosityFront: ['叶片正面辐射度', ' W/m²'],
    radiosityBack: ['叶片背面辐射度', ' W/m²'],
  };
  const [label, unit] = labels[state.display];
  $('#legend-label').textContent = label;
  $('#legend-min').textContent = range.minimum.toFixed(1);
  $('#legend-max').textContent = `${range.maximum.toFixed(1)}${unit}`;
}

function setWorkspace(workspace) {
  state.workspace = workspace;
  $$('.workflow').forEach((button) =>
    button.classList.toggle('active', button.dataset.workspace === workspace));
  $$('[data-inspector]').forEach((panel) => {
    panel.hidden = panel.dataset.inspector !== workspace;
  });
  $('#pipeline-image').hidden = workspace !== 'image';
  $('#pipeline-radiosity').hidden = workspace !== 'radiosity';
  $('#image-metrics').hidden = workspace !== 'image';
  $('#radiosity-metrics').hidden = workspace !== 'radiosity';
  $('#inspector-title').textContent = workspace === 'image'
    ? '图像模拟'
    : '辐射度求解';
  $('#viewport-eyebrow').textContent = workspace === 'image'
    ? 'IMAGE SIMULATION'
    : 'FACET RADIOSITY';
  $('#viewport-heading').textContent = workspace === 'image'
    ? '传感器场景预览'
    : '逐面元辐射度三维结果';
  $('#run-current').textContent = workspace === 'image'
    ? '运行图像模拟'
    : '运行辐射度求解';
  $$('[data-radiosity-only]').forEach((button) => {
    button.hidden = workspace !== 'radiosity';
  });
  if (workspace === 'image') {
    state.display = 'scene';
    $('#image-result').hidden = true;
    $('#facet-readout').hidden = true;
  } else {
    state.display = state.result ? 'lightEnhancement' : 'scene';
    $('#image-result').hidden = true;
    $('#facet-readout').hidden = !state.result || state.selectedFacet === null;
  }
  $$('#display-modes button').forEach((button) =>
    button.classList.toggle('active', button.dataset.mode === state.display));
  applyDisplay();
}

function updateSensorCamera() {
  const zenith = Number($('#view-zenith').value) * Math.PI / 180;
  const azimuth = Number($('#view-azimuth').value) * Math.PI / 180;
  const fov = Number($('#field-of-view').value);
  const radius = Math.max(
    13,
    state.sceneRadius / Math.max(Math.sin(fov * Math.PI / 360), 0.2) * 1.06,
  );
  const target = state.sceneCenter;
  const position = new THREE.Vector3(
    radius * Math.sin(zenith) * Math.sin(azimuth),
    radius * Math.cos(zenith),
    radius * Math.sin(zenith) * Math.cos(azimuth),
  ).add(target);
  const projection = $('#image-projection').value;
  if (state.sensorHelper) world.remove(state.sensorHelper);
  if (projection === 'orthographic') {
    const extent = state.sceneRadius * 1.04;
    state.sensorCamera = new THREE.OrthographicCamera(
      -extent, extent, extent, -extent, 0.1, radius * 4
    );
  } else {
    state.sensorCamera = new THREE.PerspectiveCamera(fov, 1, 0.1, radius * 4);
  }
  state.sensorCamera.position.copy(position);
  state.sensorCamera.lookAt(target);
  state.sensorCamera.updateProjectionMatrix();
  state.sensorHelper = new THREE.CameraHelper(state.sensorCamera);
  state.sensorHelper.material.transparent = true;
  state.sensorHelper.material.opacity = 0.32;
  world.add(state.sensorHelper);
}

function imageBandColors(band) {
  if (band === 'rgb') return null;
  const values = new Float32Array(state.facetCount);
  if (band === 'red' || band === 'nir') {
    for (let facet = 0; facet < state.facetCount; ++facet) {
      const leaf = facet < state.leafFacetCount;
      values[facet] = band === 'red' ? (leaf ? 0.08 : 0.20) : (leaf ? 0.45 : 0.20);
    }
    return { values, grayscale: true };
  }
  const temperatures = state.result?.energy?.temperatureC;
  for (let facet = 0; facet < state.facetCount; ++facet) {
    values[facet] = temperatures?.[facet] ??
      (facet < state.leafFacetCount ? 27 : 32);
  }
  return { values, grayscale: false };
}

function setBandDisplay(band) {
  const data = imageBandColors(band);
  if (!data) {
    colorFacets(null, true);
    return;
  }
  if (!data.grayscale) {
    colorFacets(data.values);
    return;
  }
  const attribute = state.mesh.geometry.getAttribute('color');
  for (let facet = 0; facet < state.facetCount; ++facet) {
    const value = THREE.MathUtils.clamp(data.values[facet] * 1.8, 0, 1);
    const color = new THREE.Color(value, value, value);
    for (let corner = 0; corner < 3; ++corner) {
      attribute.setXYZ(facet * 3 + corner, color.r, color.g, color.b);
    }
  }
  attribute.needsUpdate = true;
}

async function runImageSimulation() {
  if (state.running || !state.mesh) return;
  state.running = true;
  $('#run-image').disabled = true;
  $('#run-current').disabled = true;
  const size = Number($('#image-resolution-input').value);
  const band = $('#image-band-input').value;
  const bandNames = { rgb: '可见光 RGB', red: '红光 660 nm', nir: '近红外 850 nm', thermal: '热红外 10,500 nm' };
  try {
    setProgress(8, '设置传感器与观测几何');
    log(`图像模拟开始：${size} × ${size}，${bandNames[band]}。`);
    updateSensorCamera();
    setBandDisplay(band);
    await new Promise((resolve) => {
      let finished = false;
      const finish = () => {
        if (finished) return;
        finished = true;
        resolve();
      };
      requestAnimationFrame(finish);
      setTimeout(finish, 100);
    });
    setProgress(45, 'Three.js 离屏场景光栅');
    const start = performance.now();
    const target = new THREE.WebGLRenderTarget(size, size, {
      minFilter: THREE.LinearFilter,
      magFilter: THREE.LinearFilter,
      format: THREE.RGBAFormat,
      type: THREE.UnsignedByteType,
    });
    renderer.setRenderTarget(target);
    renderer.render(world, state.sensorCamera);
    const pixels = new Uint8Array(size * size * 4);
    renderer.readRenderTargetPixels(target, 0, 0, size, size, pixels);
    renderer.setRenderTarget(null);
    target.dispose();

    setProgress(80, '生成传感器影像产品');
    const output = $('#sensor-output');
    output.width = size;
    output.height = size;
    const context = output.getContext('2d');
    const image = context.createImageData(size, size);
    for (let y = 0; y < size; ++y) {
      const sourceRow = (size - 1 - y) * size * 4;
      const targetRow = y * size * 4;
      image.data.set(pixels.subarray(sourceRow, sourceRow + size * 4), targetRow);
    }
    context.putImageData(image, 0, 0);
    const elapsed = performance.now() - start;
    $('#image-title').textContent = bandNames[band];
    $('#image-resolution').textContent = `${size} × ${size}`;
    $('#image-band').textContent = bandNames[band];
    $('#image-time').textContent = `${elapsed.toFixed(1)} ms`;
    $('#metric-image-size').textContent = `${size} × ${size}`;
    $('#metric-image-band').textContent = bandNames[band];
    $('#metric-image-time').textContent = `${elapsed.toFixed(1)} ms`;
    $('#metric-view-angle').textContent = `${$('#view-zenith').value}° / ${$('#view-azimuth').value}°`;
    $('#image-result').hidden = false;
    setProgress(100, '传感器影像已生成', 'done');
    log(`图像模拟完成：${elapsed.toFixed(1)} ms。`);
  } catch (error) {
    setProgress(0, error.message, 'error');
    showToast(error.message);
  } finally {
    applyDisplay();
    state.running = false;
    $('#run-image').disabled = false;
    $('#run-current').disabled = false;
  }
}

function leafDoubleSidedStatistics(source) {
  const values = new Float32Array(state.leafFacetCount);
  let sum = 0;
  let minimum = Infinity;
  let maximum = -Infinity;
  for (let facet = 0; facet < state.leafFacetCount; ++facet) {
    const value = Math.max(
      source[facet * 2] || 0,
      source[facet * 2 + 1] || 0,
    );
    values[facet] = value;
    sum += value;
    minimum = Math.min(minimum, value);
    maximum = Math.max(maximum, value);
  }
  return {
    mean: sum / Math.max(values.length, 1),
    minimum: Number.isFinite(minimum) ? minimum : 0,
    maximum: Number.isFinite(maximum) ? maximum : 0,
  };
}

function updateFacetReadout(facet) {
  if (!state.result || facet < 0 || facet >= state.facetCount) return;
  state.selectedFacet = facet;
  const front = state.result.radiosity[facet * 2] || 0;
  const back = state.result.radiosity[facet * 2 + 1] || 0;
  const enhancement = Math.max(
    state.result.lightEnhancement[facet * 2] || 0,
    state.result.lightEnhancement[facet * 2 + 1] || 0,
  );
  const isLeaf = facet < state.leafFacetCount;
  const facetsPerTree = Math.floor(state.leafFacetCount / Math.max(state.treeCount, 1));
  const owner = isLeaf
    ? `第 ${Math.floor(facet / facetsPerTree) + 1} 棵树 · 局部面元 ${facet % facetsPerTree}`
    : `土壤面元 ${facet - state.leafFacetCount}`;
  $('#facet-id').textContent = `#${facet}`;
  $('#facet-owner').textContent = owner;
  $('#facet-front').textContent = `${front.toFixed(3)} W/m²`;
  $('#facet-back').textContent = `${back.toFixed(3)} W/m²`;
  $('#facet-maximum').textContent = `${Math.max(front, back).toFixed(3)} W/m²`;
  $('#facet-enhancement').textContent = `${enhancement.toFixed(3)} W/m²`;
  $('#facet-readout').hidden = state.workspace !== 'radiosity';
}

const raycaster = new THREE.Raycaster();
const pointer = new THREE.Vector2();
canvas.addEventListener('click', (event) => {
  if (state.workspace !== 'radiosity' || !state.result || !state.mesh) return;
  const bounds = canvas.getBoundingClientRect();
  pointer.x = ((event.clientX - bounds.left) / bounds.width) * 2 - 1;
  pointer.y = -((event.clientY - bounds.top) / bounds.height) * 2 + 1;
  raycaster.setFromCamera(pointer, camera);
  const hit = raycaster.intersectObject(state.mesh, false)[0];
  if (hit?.faceIndex !== undefined) updateFacetReadout(hit.faceIndex);
});

async function runRadiositySimulation() {
  if (state.running) return;
  state.running = true;
  $('#run-radiosity').disabled = true;
  $('#run-current').disabled = true;
  setProgress(1, state.backend === 'gpu' ? '启动 Vulkan GPU 后端' : '启动 CPU 光栅后端');
  log(`逐面元辐射度求解开始：${state.backend.toUpperCase()}。`);
  try {
    const response = await fetch('/api/run', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ backend: state.backend }),
    });
    if (!response.ok || !response.body) throw new Error(await response.text());
    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let pending = '';
    while (true) {
      const { value, done } = await reader.read();
      pending += decoder.decode(value || new Uint8Array(), { stream: !done });
      const lines = pending.split('\n');
      pending = lines.pop() || '';
      for (const line of lines) {
        if (!line.trim()) continue;
        const event = JSON.parse(line);
        if (event.type === 'progress') {
          setProgress(event.value, event.stage);
        } else if (event.type === 'result') {
          state.result = event.result;
          state.display = 'lightEnhancement';
          state.selectedFacet = null;
          $('#facet-readout').hidden = true;
          $$('#display-modes button').forEach((button) =>
            button.classList.toggle('active', button.dataset.mode === state.display));
          applyDisplay();
          const stats = leafDoubleSidedStatistics(event.result.radiosity);
          const enhancementStats =
            leafDoubleSidedStatistics(event.result.lightEnhancement);
          $('#metric-backend').textContent =
            `${event.result.backend} / ${event.result.timing.totalMs.toFixed(1)} ms`;
          $('#metric-radiosity-mean').textContent = `${stats.mean.toFixed(2)} W/m²`;
          $('#metric-radiosity-range').textContent =
            `${stats.minimum.toFixed(2)} – ${stats.maximum.toFixed(2)} W/m²`;
          $('#metric-enhancement').textContent =
            `${enhancementStats.mean.toFixed(2)} / ${enhancementStats.maximum.toFixed(2)} W/m²`;
          $('#metric-graph').textContent =
            `${event.result.graph.directedEdges.toLocaleString()} / ${event.result.graph.closureError.toExponential(1)}`;
          $('#metric-facets').textContent =
            `${event.result.facetCount.toLocaleString()} / ${(event.result.facetCount * 2).toLocaleString()}`;
          $('#metric-convergence').textContent =
            `${event.result.iterations} / ${event.result.maxDelta.toExponential(2)}`;
          setProgress(100, '逐面元辐射度求解完成', 'done');
          log(
            `求解完成：${event.result.facetCount.toLocaleString()} 个面元，` +
            `叶片平均辐射度 ${stats.mean.toFixed(2)} W/m²。`
          );
        } else if (event.type === 'error') {
          throw new Error(event.message);
        }
      }
      if (done) break;
    }
  } catch (error) {
    setProgress(0, error.message, 'error');
    showToast(error.message);
    log(`运行失败：${error.message}`);
  } finally {
    state.running = false;
    $('#run-radiosity').disabled = false;
    $('#run-current').disabled = false;
  }
}

$$('.workflow').forEach((button) =>
  button.addEventListener('click', () => setWorkspace(button.dataset.workspace)));
$$('.backend-switch button').forEach((button) => button.addEventListener('click', () => {
  state.backend = button.dataset.backend;
  $$('.backend-switch button').forEach((item) => item.classList.toggle('active', item === button));
}));
$$('#display-modes button').forEach((button) => button.addEventListener('click', () => {
  if (button.dataset.mode !== 'scene' && !state.result) {
    showToast('请先运行辐射度求解。');
    return;
  }
  state.display = button.dataset.mode;
  $$('#display-modes button').forEach((item) => item.classList.toggle('active', item === button));
  applyDisplay();
}));
$('#run-image').addEventListener('click', runImageSimulation);
$('#run-radiosity').addEventListener('click', runRadiositySimulation);
$('#run-current').addEventListener('click', () =>
  state.workspace === 'image' ? runImageSimulation() : runRadiositySimulation());
$('#close-image').addEventListener('click', () => { $('#image-result').hidden = true; });
$('#save-image').addEventListener('click', () => {
  const link = document.createElement('a');
  link.download = `facetlab-${$('#image-band-input').value}.png`;
  link.href = $('#sensor-output').toDataURL('image/png');
  link.click();
});
['view-zenith', 'view-azimuth', 'field-of-view', 'image-projection'].forEach((id) =>
  $('#' + id).addEventListener('change', updateSensorCamera));
$('#image-resolution-input').addEventListener('change', (event) => {
  $('#sensor-badge').textContent = `${event.target.value}²`;
});

function resize() {
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  if (canvas.width !== Math.floor(width * renderer.getPixelRatio()) ||
      canvas.height !== Math.floor(height * renderer.getPixelRatio())) {
    renderer.setSize(width, height, false);
    camera.aspect = width / Math.max(height, 1);
    camera.updateProjectionMatrix();
  }
}
function animate() {
  requestAnimationFrame(animate);
  resize();
  controls.update();
  renderer.render(world, camera);
}

loadScene().then(updateSensorCamera).catch((error) => {
  showToast(error.message);
  $('#service-dot').className = 'status-dot error';
});
setWorkspace('radiosity');
animate();
