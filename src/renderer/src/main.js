import { webApi } from './web-api.js'
import { loadXmlScene } from './scene-loader.js'
import { createDefaultProject, createMaterialPresets } from './project-schema.js'
import * as THREE from 'three'
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js'
import './styles.css'

const $ = (selector) => document.querySelector(selector)
const $$ = (selector) => [...document.querySelectorAll(selector)]
const api = webApi

const state = {
  mode: 'eFacetRT',
  executable: '',
  executableExists: true,
  radiosityExecutable: '',
  radiosityExecutableExists: false,
  inputPath: '',
  outputDir: '',
  xmlText: '',
  xmlDirty: false,
  project: null,
  projectPath: '',
  running: false,
  startedAt: 0,
  logCount: 0,
  panel: 'scene',
  config: null,
  platform: 'win32',
  importedObject: null,
  pendingObject: null,
  editingObjectIndex: -1,
  selectedObjectIndex: -1,
  editingAttributeIndex: -1,
  presetPhysicalType: 'Vegetation',
  resultFiles: [],
  selectedResult: null,
  resultRaster: null
}

function mergeNamedPresets(presets, values) {
  const merged = new Map(presets.map((item) => [item.name, item]))
  for (const item of values || []) {
    const preset = merged.get(item.name) || {}
    merged.set(item.name, { ...preset, ...item, params: { ...(preset.params || {}), ...(item.params || {}) } })
  }
  return [...merged.values()]
}

function ensureMaterialPresets(config) {
  const presets = createMaterialPresets()
  config.spectra = mergeNamedPresets(presets.spectra, config.spectra)
  config.canopies = mergeNamedPresets(presets.canopies, config.canopies)
  config.thermals = mergeNamedPresets(presets.thermals, config.thermals)
  config.materials = mergeNamedPresets(presets.materials, config.materials)
  return config
}

const defaultConfig = () => {
  const config = createDefaultProject().configuration
  config.meteo.path = 'HiStream 内置气象数据'
  return config
}

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>'"]/g, (char) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;' })[char])
}

function xmlValue(doc, selector, fallback = '') {
  return doc.querySelector(selector)?.textContent?.trim() || fallback
}

function parsePair(value, fallback) {
  const parts = String(value).split(',').map(Number)
  return parts.length >= 2 && parts.every(Number.isFinite) ? parts : fallback
}

function parseMaterialNode(node) {
  const type = node.tagName === 'leafBio' ? 'Vegetation' : node.tagName === 'waterSet' ? 'Water' : 'Soil'
  let params
  if (type === 'Vegetation') {
    params = {
      Vcmax: Number(xmlValue(node, 'Vcmax', 80)), m: Number(xmlValue(node, 'm', 9)),
      BallBerry: Number(xmlValue(node, 'BallBerry', 0.01)), Type: Number(xmlValue(node, 'Type', 3)),
      kV: Number(xmlValue(node, 'kV', 0.6396)), Rdparam: Number(xmlValue(node, 'Rdparam', 0.015)),
      Tparam: xmlValue(node, 'Tparam', '0.2,0.3,288,313,328'), Tyear: Number(xmlValue(node, 'Tyear', 25)),
      beta: Number(xmlValue(node, 'beta', 0.507)), kNPQs: Number(xmlValue(node, 'kNPQs', 0)),
      qLs: Number(xmlValue(node, 'qLs', 1)), stressfactor: Number(xmlValue(node, 'stressfactor', 1)), Tcor: Number(xmlValue(node, 'Tcor', 0))
    }
  } else if (type === 'Water') {
    params = {
      rss: Number(xmlValue(node, 'rss', 0)), heatCapacity: Number(xmlValue(node, 'heatCapacity', 4186000)),
      mixingDepth: Number(xmlValue(node, 'mixingDepth', 0.5)), evaporationCoefficient: Number(xmlValue(node, 'evaporationCoefficient', 1))
    }
  } else {
    params = {
      method: Number(xmlValue(node, 'method', 1)), rss: Number(xmlValue(node, 'rss', 2000)),
      cs: Number(xmlValue(node, 'cs', 1180)), rhos: Number(xmlValue(node, 'rhos', 1800)),
      lambdas: Number(xmlValue(node, 'lambdas', 1.55)), Tsoil: Number(xmlValue(node, 'Tsoil', 25)),
      SMC: Number(xmlValue(node, 'SMC', 25)), Satwater: Number(xmlValue(node, 'Satwater', 0.45))
    }
  }
  return { name: node.getAttribute('name') || 'material', type, params }
}

function parseXml(content) {
  const doc = new DOMParser().parseFromString(content, 'application/xml')
  if (doc.querySelector('parsererror')) throw new Error('XML 格式错误，请检查标签是否闭合')
  const base = defaultConfig()
  const lightPair = parsePair(xmlValue(doc, 'Geometry Light light lightAngle lightAngle', '30,135'), [30, 135])
  const viewPair = parsePair(xmlValue(doc, 'Geometry Sensor sensor viewAngle viewAngle[type="custom"] viewAngles', '0,0'), [0, 0])
  const objectNodes = [...doc.querySelectorAll('Scene Object object')]
  const names = objectNodes.map((node) => node.getAttribute('objName') || node.getAttribute('name') || `对象 ${node.getAttribute('id') || ''}`)
  const items = objectNodes.map((node) => {
    const type = xmlValue(node, 'types', 'Other')
    const meshNames = xmlValue(node, 'meshNames', node.getAttribute('objName') || 'object').split(',')
    const spectralNames = xmlValue(node, 'spectralNames', 'soil').split(',')
    const thermalNames = xmlValue(node, 'thermalNames', 'soil_temperature').split(',')
    const defaultMaterial = type === 'Vegetation' ? 'leaf_c3' : type === 'Water' ? 'water_set' : 'soilset'
    return {
      name: node.getAttribute('objName') || node.getAttribute('name') || 'object', type,
      shape: xmlValue(node, 'shapeTypes', ''), dimensions: xmlValue(node, 'shapes', '1,1,1').split(',').map(Number),
      fileName: xmlValue(node, 'fileName', xmlValue(node, 'objectfile', '')), positionFile: xmlValue(node, 'objectPosition', ''),
      materialName: xmlValue(node, 'bioNames', xmlValue(node, 'propNames', defaultMaterial)),
      canopyName: xmlValue(node, 'canopyNames', 'canopy_default'),
      meshes: meshNames.map((name, index) => ({ name, spectralName: spectralNames[index] || spectralNames[0] || 'soil', thermalName: thermalNames[index] || thermalNames[0] || 'soil_temperature' }))
    }
  })
  const spectra = [...doc.querySelectorAll('Attribute Spectral spectral')].map((node) => ({
    name: node.getAttribute('name') || 'spectral', model: node.getAttribute('type') || 'custom',
    reflectance: xmlValue(node, 'reflectance', '0.20'), transmittance: xmlValue(node, 'transmittance', '0.0'),
    refTir: Number(xmlValue(node, 'ref_TIR', 0.05)), tauTir: Number(xmlValue(node, 'tau_TIR', 0)), fileName: xmlValue(node, 'spectral_file', ''),
    params: { Cab: Number(xmlValue(node, 'Cab', 40)), Cw: Number(xmlValue(node, 'Cw', 0.01)), Cdm: Number(xmlValue(node, 'Cdm', 0.01)), Cs: Number(xmlValue(node, 'Cs', 0)), N: Number(xmlValue(node, 'N', 1.5)), SMC: Number(xmlValue(node, 'SMC', 25)), BSMBrightness: Number(xmlValue(node, 'BSMBrightness', 0.5)), BSMlat: Number(xmlValue(node, 'BSMlat', 0)), BSMlon: Number(xmlValue(node, 'BSMlon', 0)) }
  }))
  const thermals = [...doc.querySelectorAll('Attribute Thermal thermal')].map((node) => ({ name: node.getAttribute('name') || 'temperature', sunlitTemperature: Number(xmlValue(node, 'sunlitTemperature', 305)), shadedTemperature: Number(xmlValue(node, 'shadedTemperature', 295)) }))
  const materials = [...doc.querySelectorAll('Attribute Biochemistry leafBio, Attribute Biochemistry soilSet, Attribute Biochemistry waterSet')]
    .filter((node) => node.getAttribute('name') !== 'unused_leaf')
    .map(parseMaterialNode)
  const canopies = [...doc.querySelectorAll('Attribute Canopy canopy')].map((node) => ({
    name: node.getAttribute('name') || 'canopy_default',
    lai: Number(xmlValue(node, 'lai', 3)), density: Number(xmlValue(node, 'density', 1)),
    hc: Number(xmlValue(node, 'hc', 2)), G: Number(xmlValue(node, 'G', 0.5)),
    LIDFa: Number(xmlValue(node, 'LIDFa', -0.35)), LIDFb: Number(xmlValue(node, 'LIDFb', -0.15)),
    hspot: Number(xmlValue(node, 'hspot', 0.2)), leafwidth: Number(xmlValue(node, 'leafwidth', 0.1))
  }))
  return {
    outDir: xmlValue(doc, 'Control outDir', ''),
    scene: {
      x: Number(xmlValue(doc, 'Scene sceneSizeX', base.scene.x)),
      y: Number(xmlValue(doc, 'Scene sceneSizeY', base.scene.y)),
      height: Number(xmlValue(doc, 'Scene Height', base.scene.height)),
      voxel: Number(xmlValue(doc, 'Scene voxelSize', base.scene.voxel)),
      terrain: xmlValue(doc, 'Control isDEM', '0') === '1'
    },
    light: {
      zenith: lightPair[0], azimuth: lightPair[1],
      direct: Number(xmlValue(doc, 'Geometry Light light direct', base.light.direct)),
      diffuse: Number(xmlValue(doc, 'Geometry Light light diffuse', base.light.diffuse)),
      skyTemperature: Number(xmlValue(doc, 'Geometry Light light skyTemperature', base.light.skyTemperature))
    },
    sensor: {
      x: Number(xmlValue(doc, 'Geometry Sensor sensor pixelResolutionX', base.sensor.x)),
      y: Number(xmlValue(doc, 'Geometry Sensor sensor pixelResolutionY', base.sensor.y)),
      bands: xmlValue(doc, 'Geometry Sensor sensor controlBand', base.sensor.bands),
      vza: viewPair[0], vaa: viewPair[1],
      image: xmlValue(doc, 'Control isImage', '1') === '1',
      temperature: xmlValue(doc, 'Control isTemperature', '1') === '1',
      albedo: xmlValue(doc, 'Control isAlbedo', '0') === '1'
    },
    control: {
      depth: Number(xmlValue(doc, 'Control rayTracingDepth', base.control.depth)),
      gpu: Number(xmlValue(doc, 'Control GPU', base.control.gpu))
    },
    objects: { count: objectNodes.length, names: names.length ? names : ['均质地表'], items },
    spectra: spectra.length ? spectra : base.spectra,
    canopies: canopies.length ? canopies : base.canopies,
    thermals: thermals.length ? thermals : base.thermals,
    materials: materials.length ? materials : base.materials,
    meteo: {
      path: xmlValue(doc, 'Meteorology filePath', '未配置'),
      start: Number(xmlValue(doc, 'Meteorology startTimeNode', 0)),
      end: Number(xmlValue(doc, 'Meteorology endTimeNode', 0))
    }
  }
}

function updateXml(selector, value) {
  if (!state.xmlText) return
  const parsed = new DOMParser().parseFromString(state.xmlText, 'application/xml')
  const node = parsed.querySelector(selector)
  if (!node) return
  node.textContent = String(value)
  state.xmlText = new XMLSerializer().serializeToString(parsed)
  state.xmlDirty = true
  $('#xmlEditor').value = state.xmlText
  $('#unsavedMark').hidden = false
  $('#xmlStatus').textContent = '有未保存的修改'
}

function updatePair(selector, index, value) {
  if (!state.xmlText) return
  const parsed = new DOMParser().parseFromString(state.xmlText, 'application/xml')
  const node = parsed.querySelector(selector)
  if (!node) return
  const pair = parsePair(node.textContent, [0, 0])
  pair[index] = Number(value)
  updateXml(selector, `${pair[0]},${pair[1]}`)
}

function fileName(path) {
  if (!path) return '未打开工程'
  return path.replaceAll('\\', '/').split('/').pop()
}

function toast(title, message, kind = '') {
  const item = document.createElement('div')
  item.className = `toast ${kind}`
  item.innerHTML = `<strong>${escapeHtml(title)}</strong><span>${escapeHtml(message)}</span>`
  $('#toastStack').append(item)
  setTimeout(() => item.remove(), 4200)
}

function addLog(text, kind = '') {
  const lines = String(text).replace(/\r/g, '').split('\n').filter(Boolean)
  for (const line of lines) {
    const row = document.createElement('div')
    row.className = `log-line ${kind}`
    row.innerHTML = `<time>${new Date().toLocaleTimeString('zh-CN', { hour12: false })}</time><span></span>`
    row.querySelector('span').textContent = line
    $('#consoleOutput').append(row)
    state.logCount += 1
  }
  $('#logCount').textContent = state.logCount
  $('#consoleOutput').scrollTop = $('#consoleOutput').scrollHeight
}

function setEngineState(label, detail, css = 'ready') {
  $('#engineState').textContent = label
  $('#enginePath').textContent = detail
  $('#engineDot').className = css
}

function setRunning(running) {
  state.running = running
  $('#runBtn').disabled = running
  $('#stopBtn').disabled = !running
  $$('#modeGrid button').forEach((button) => { button.disabled = running })
  if (running) setEngineState('模拟运行中', `PID ${state.pid || '...'}`, 'running')
  else {
    const facet = state.mode === 'eFacetRT' || state.mode === 'eFacetEB'
    const exists = state.executableExists
    const path = state.executable
    setEngineState(exists ? (facet ? 'HiStream Facet 待命' : 'HiStream 待命') : '未找到引擎', path, exists ? 'ready' : '')
  }
}

// Three.js 场景
const viewport = $('#viewport')
const scene = new THREE.Scene()
scene.background = new THREE.Color(0x091310)
scene.fog = new THREE.FogExp2(0x091310, 0.012)
const camera = new THREE.PerspectiveCamera(42, 1, 0.1, 1200)
camera.position.set(25, 22, 28)
const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' })
renderer.setPixelRatio(Math.min(devicePixelRatio, 2))
renderer.shadowMap.enabled = true
renderer.shadowMap.type = THREE.PCFSoftShadowMap
renderer.outputColorSpace = THREE.SRGBColorSpace
renderer.toneMapping = THREE.ACESFilmicToneMapping
renderer.toneMappingExposure = 1.05
viewport.append(renderer.domElement)
const controls = new OrbitControls(camera, renderer.domElement)
controls.enableDamping = true
controls.dampingFactor = 0.075
controls.maxPolarAngle = Math.PI * 0.48
controls.minDistance = 4
controls.maxDistance = 320
scene.add(new THREE.HemisphereLight(0xb7ddd0, 0x172118, 1.65))
const sun = new THREE.DirectionalLight(0xfff0c6, 3.2)
sun.castShadow = true
sun.shadow.mapSize.set(2048, 2048)
sun.shadow.camera.left = sun.shadow.camera.bottom = -70
sun.shadow.camera.right = sun.shadow.camera.top = 70
sun.shadow.bias = -0.0005
scene.add(sun)
const world = new THREE.Group()
scene.add(world)
let grid = null
let voxelPreview = null
let currentStyle = 'solid'
let resultViewer = null

function seeded(index) {
  const value = Math.sin(index * 91.17 + 13.41) * 43758.5453
  return value - Math.floor(value)
}

function disposeObject(object) {
  object.traverse((child) => {
    child.geometry?.dispose?.()
    if (Array.isArray(child.material)) child.material.forEach((item) => item.dispose())
    else child.material?.dispose?.()
  })
}

function clearWorld() {
  while (world.children.length) {
    const child = world.children.pop()
    disposeObject(child)
  }
  grid = null
  voxelPreview = null
}

function material(color, roughness = .82) {
  const result = new THREE.MeshStandardMaterial({ color, roughness, metalness: .02 })
  result.userData.baseColor = result.color.getHex()
  return result
}

function generateWorld(config = defaultConfig()) {
  clearWorld()
  const sx = Math.max(4, Math.min(config.scene.x || 60, 180))
  const sz = Math.max(4, Math.min(config.scene.y || 45, 180))
  const displayScale = Math.min(1, 45 / Math.max(sx, sz))
  const width = sx * displayScale
  const depth = sz * displayScale
  world.userData.extent = Math.max(width, depth)
  if (!state.inputPath) {
    updateStats()
    return
  }
  const groundGeo = new THREE.PlaneGeometry(width, depth, 36, 36)
  const positions = groundGeo.attributes.position
  for (let i = 0; i < positions.count; i++) {
    const x = positions.getX(i), y = positions.getY(i)
    const edge = Math.min(1, Math.max(0, (1 - Math.abs(x) / (width / 2)) * (1 - Math.abs(y) / (depth / 2)) * 5))
    const height = config.scene.terrain ? (Math.sin(x * .35) * .16 + Math.cos(y * .27) * .13 + seeded(i) * .06) * edge : 0
    positions.setZ(i, height)
  }
  groundGeo.computeVertexNormals()
  const ground = new THREE.Mesh(groundGeo, material(0x34493b, .96))
  ground.rotation.x = -Math.PI / 2
  ground.receiveShadow = true
  ground.userData.kind = 'ground'
  world.add(ground)
  grid = new THREE.GridHelper(Math.max(width, depth), Math.max(8, Math.round(Math.max(width, depth) / 1.5)), 0x5c8876, 0x274438)
  grid.position.y = .012
  grid.material.opacity = .23
  grid.material.transparent = true
  world.add(grid)

  applyViewStyle(currentStyle)
  updateStats()
}

function updateSun(config) {
  const zenith = THREE.MathUtils.degToRad(config.light.zenith), azimuth = THREE.MathUtils.degToRad(config.light.azimuth), radius = 45
  sun.position.set(Math.sin(zenith) * Math.sin(azimuth) * radius, Math.cos(zenith) * radius, Math.sin(zenith) * Math.cos(azimuth) * radius)
  sun.intensity = 1.2 + config.light.direct * 2.5
}

function applyViewStyle(style) {
  currentStyle = style
  world.traverse((child) => {
    if (!child.isMesh || child === voxelPreview) return
    const materials = Array.isArray(child.material) ? child.material : [child.material]
    for (const item of materials) {
      if (!item?.color) continue
      item.wireframe = style === 'wireframe'
      if (style === 'thermal') {
        const colors = { vegetation: 0xe7b84a, building: 0xd3544c, ground: 0x4c72bd, sensor: 0xe8ebe9 }
        item.color.setHex(colors[child.userData.kind] || 0x61b68e)
      } else if (item.userData.baseColor != null) item.color.setHex(item.userData.baseColor)
    }
  })
}

function fitCamera() {
  const extent = world.userData.extent || 20
  camera.position.set(extent * .68, extent * .56, extent * .74)
  controls.target.set(0, 1.3, 0)
  controls.update()
}

function updateStats() {
  let triangles = 0
  world.traverse((object) => {
    if (!object.isMesh || !object.geometry) return
    const count = object.geometry.index ? object.geometry.index.count / 3 : object.geometry.attributes.position.count / 3
    triangles += count * (object.isInstancedMesh ? object.count : 1)
  })
  $('#triangles').textContent = `${Math.round(triangles / 100) / 10}K TRI`
}

function resize() {
  renderer.setSize(viewport.clientWidth, viewport.clientHeight, false)
  camera.aspect = viewport.clientWidth / Math.max(1, viewport.clientHeight)
  camera.updateProjectionMatrix()
}
new ResizeObserver(resize).observe(viewport)
let frameCount = 0, lastFps = performance.now()
function animate(now) {
  requestAnimationFrame(animate)
  controls.update(); renderer.render(scene, camera); renderResultViewer(); frameCount++
  if (now - lastFps > 750) {
    $('#fps').textContent = `${Math.round(frameCount * 1000 / (now - lastFps))} FPS`
    frameCount = 0; lastFps = now
  }
}
requestAnimationFrame(animate)

// 属性面板
const panelTitles = { scene: '场景', light: '太阳与天空', sensor: '传感器', objects: '场景对象', materials: '材质属性', meteorology: '气象驱动' }
function inputUnit(id, value, unit, attrs = '') { return `<div class="input-with-unit"><input class="input" id="${id}" value="${escapeHtml(value)}" ${attrs}><span>${unit}</span></div>` }
function field(label, content) { return `<div class="field"><label>${label}</label>${content}</div>` }
function switchRow(label, id, on) { return `<div class="switch-row"><span>${label}</span><button class="switch ${on ? 'on' : ''}" id="${id}" role="switch" aria-checked="${on}"></button></div>` }
function builtinNotice() { return state.inputPath ? '' : '<div class="property-group"><div class="notice">尚未打开工程。请点击“打开工程”，选择已有工程的 project.json 或 Input.xml。</div></div>' }

function libraryCard(title, subtitle, values) {
  const detail = values.map(([key, value]) => `${key}: ${value}`).join('；')
  const search = `${title} ${subtitle} ${detail}`.toLowerCase()
  return `<div class="library-card" data-library-search="${escapeHtml(search)}" title="${escapeHtml(`${title} · ${subtitle}\n${detail}`)}"><strong>${escapeHtml(title)}</strong><small>${escapeHtml(subtitle)}</small><div class="library-values">${values.map(([key, value]) => `<span><i>${escapeHtml(key)}</i>${escapeHtml(value)}</span>`).join('')}</div></div>`
}

function physicalValues(item) {
  const p = item.params || {}
  if (item.type === 'Vegetation') return [['Vcmax', p.Vcmax], ['BallBerry', p.BallBerry], ['kV', p.kV], ['Rd', p.Rdparam], ['Tyear', p.Tyear], ['胁迫', p.stressfactor]]
  if (item.type === 'Water') return [['rss', p.rss], ['热容', p.heatCapacity], ['混合深度', p.mixingDepth], ['蒸发系数', p.evaporationCoefficient]]
  return [['rss', p.rss], ['热容', p.cs], ['密度', p.rhos], ['导热率', p.lambdas], ['SMC', p.SMC], ['饱和量', p.Satwater]]
}

function libraryHeading(title, count, kind) {
  return `<div class="library-heading"><h3>${escapeHtml(title)} · ${count}</h3><div class="library-actions"><input class="input library-filter" data-library-filter="${kind}" placeholder="筛选 ${escapeHtml(title)}"><button class="button ghost library-add" type="button" data-preset-kind="${kind}"><svg><use href="#i-plus"/></svg>新增</button></div></div>`
}

function materialLibraryHtml(c) {
  const spectra = c.spectra.map((item) => libraryCard(item.label || item.name, `${item.name} · ${item.model}`, [['反射率', item.reflectance], ['透射率', item.transmittance], ['TIR反射', item.refTir]])).join('')
  const thermals = c.thermals.map((item) => libraryCard(item.label || item.name, item.name, [['阳面', `${item.sunlitTemperature} K`], ['阴面', `${item.shadedTemperature} K`]])).join('')
  const types = { Vegetation: '植被生理生化', Soil: '土壤表面物化', Water: '水体物性' }
  const materials = c.materials.map((item) => libraryCard(item.label || item.name, `${item.name} · ${types[item.type] || item.type}`, physicalValues(item))).join('')
  return `<div class="property-group">${libraryHeading('光谱特征', c.spectra.length, 'spectrum')}<div class="library-list" data-library-list="spectrum">${spectra}</div></div><div class="property-group">${libraryHeading('温度特征', c.thermals.length, 'thermal')}<div class="library-list" data-library-list="thermal">${thermals}</div></div><div class="property-group">${libraryHeading('物化属性', c.materials.length, 'physical')}<div class="library-list" data-library-list="physical">${materials}</div></div><div class="property-group"><div class="notice">这里是当前工程的属性预设库，用来给各场景 OBJ 提供光谱、温度和物化属性选项；在此新增预设不会自动创建场景对象。</div></div>`
}

function renderInspector() {
  const c = state.config || defaultConfig()
  $('#inspectorTitle').textContent = panelTitles[state.panel]
  const voxelSize = Math.max(.1, Number(c.scene.voxel) || 1)
  const voxelShape = [c.scene.x, c.scene.y, c.scene.height].map((size) => Math.ceil(Number(size) / voxelSize))
  const voxelCount = voxelShape.reduce((total, size) => total * size, 1)
  const voxelMode = state.mode === 'eVoxelRT' || state.mode === 'eVoxelEB'
  const voxelTarget = state.mode === 'eFacetEB' ? 'eVoxelEB' : 'eVoxelRT'
  const voxelTargetLabel = voxelTarget === 'eVoxelEB' ? '体元能量平衡' : '体元辐射传输'
  const voxelChannel = `<div class="property-group"><h3>面元 → 体元通道</h3><div class="metric-grid"><div class="metric"><strong>${voxelShape.join(' × ')}</strong><small>体元网格</small></div><div class="metric"><strong>${voxelCount.toLocaleString('zh-CN')}</strong><small>包围盒体元数</small></div></div><div class="notice">保留 OBJ、材质和位置；当前按对象类型、包围盒尺寸与分布位置建立稀疏体元。</div><button class="button ghost" id="facetToVoxelAction" style="width:100%;justify-content:center;margin-top:9px" ${voxelMode ? 'disabled' : ''}>${voxelMode ? '体元通道已启用' : `转换到${voxelTargetLabel}`}</button></div>`
  let html = ''
  if (state.panel === 'scene') {
    const projectBlock = `<div class="property-group"><h3>当前工程</h3><div class="path-input"><span title="${escapeHtml(state.inputPath)}">${escapeHtml(state.inputPath || '未打开 Input.xml')}</span></div><button class="button ghost" id="openProjectAction" style="width:100%;justify-content:center;margin-top:9px">打开其他工程</button></div>`
    html = `${projectBlock}<div class="property-group"><h3>空间范围</h3>${field('X 尺寸', inputUnit('sceneX', c.scene.x, 'm', 'type="number" min="1"'))}${field('Y 尺寸', inputUnit('sceneY', c.scene.y, 'm', 'type="number" min="1"'))}${field('最大高度', inputUnit('sceneHeight', c.scene.height, 'm', 'type="number" min="0.1"'))}${field('体元大小', inputUnit('voxelSize', c.scene.voxel, 'm', 'type="number" min="0.1" step="0.1"'))}${switchRow('启用 DEM 地形', 'terrainSwitch', c.scene.terrain)}</div>${voxelChannel}
      <div class="property-group"><h3>计算控制</h3>${field('追踪深度', `<input class="input" id="rayDepth" type="number" min="1" max="64" value="${c.control.depth}">`)}${field('GPU 序号', `<input class="input" id="gpuIndex" type="number" min="0" value="${c.control.gpu}">`)}</div>${builtinNotice()}`
  } else if (state.panel === 'light') {
    html = `<div class="property-group"><h3>太阳位置</h3>${field('太阳天顶角', inputUnit('sunZenith', c.light.zenith, '°', 'type="number" min="0" max="90"'))}${field('太阳方位角', inputUnit('sunAzimuth', c.light.azimuth, '°', 'type="number" min="0" max="360"'))}</div>
      <div class="property-group"><h3>辐照条件</h3>${field('直射比例', `<div class="range-wrap"><input class="range" id="directLight" type="range" min="0" max="1" step="0.01" value="${c.light.direct}"><span class="range-value" id="directValue">${c.light.direct.toFixed(2)}</span></div>`)}${field('漫射比例', `<div class="range-wrap"><input class="range" id="diffuseLight" type="range" min="0" max="1" step="0.01" value="${c.light.diffuse}"><span class="range-value" id="diffuseValue">${c.light.diffuse.toFixed(2)}</span></div>`)}${field('天空温度', inputUnit('skyTemperature', c.light.skyTemperature, 'K', 'type="number"'))}</div>${builtinNotice()}`
  } else if (state.panel === 'sensor') {
    html = `<div class="property-group"><h3>成像参数</h3>${field('图像分辨率', `<div class="input-row"><input class="input" id="sensorX" type="number" value="${c.sensor.x}"><input class="input" id="sensorY" type="number" value="${c.sensor.y}"></div>`)}${field('波段 [nm]', `<input class="input" id="sensorBands" value="${escapeHtml(c.sensor.bands)}">`)}${field('观测天顶角', inputUnit('viewZenith', c.sensor.vza, '°', 'type="number"'))}${field('观测方位角', inputUnit('viewAzimuth', c.sensor.vaa, '°', 'type="number"'))}</div>
      <div class="property-group"><h3>输出产品</h3>${switchRow('保存影像', 'imageSwitch', c.sensor.image)}${switchRow('亮温产品', 'temperatureSwitch', c.sensor.temperature)}${switchRow('反照率产品', 'albedoSwitch', c.sensor.albedo)}</div>${builtinNotice()}`
  } else if (state.panel === 'objects') {
    const objectCards = (c.objects.items || []).map((item, index) => {
      const meshBindings = (item.meshes || []).map((mesh) => `${mesh.name}: ${mesh.spectralName} / ${mesh.thermalName}`).join('；') || '无 Mesh 映射'
      return `<div class="object-card ${state.selectedObjectIndex === index ? 'active' : ''}" data-index="${index}" role="button" tabindex="0">
        <div class="object-card-head"><strong>${escapeHtml(item.name)}</strong><div class="object-card-actions"><button class="button ghost object-material" type="button">属性</button><button class="button ghost object-distribution" type="button">分布</button><button class="button ghost danger object-delete" type="button">删除</button></div></div>
        <small title="${escapeHtml(meshBindings)}">属性 · ${escapeHtml(item.materialName || '未定义物化属性')} · ${escapeHtml(item.canopyName || '未定义冠层')} · ${escapeHtml(meshBindings)}</small>
        <small>分布 · ${escapeHtml(distributionSummary(item))}</small>
        <small title="${escapeHtml(item.fileName)}">OBJ · ${escapeHtml(item.fileName || '无 OBJ 文件')}</small>
      </div>`
    }).join('')
    html = `<div class="property-group"><h3>场景统计</h3><div class="metric-grid"><div class="metric"><strong>${c.objects.count || '—'}</strong><small>计算对象</small></div><div class="metric"><strong>${c.spectra.length}</strong><small>光谱定义</small></div></div></div>
      <div class="property-group"><h3>OBJ 属性与分布</h3>${objectCards || '<div class="notice">尚未导入 OBJ。</div>'}<button class="button ghost" id="objectImportAction" style="width:100%;justify-content:center;margin-top:8px"><svg><use href="#i-import"/></svg>新增 OBJ 原型</button></div>
      <div class="property-group"><div class="notice">每个 OBJ 同时保存一套计算属性绑定和一套独立实例分布；点击“属性”或“分布”分别查看。</div></div>`
  } else if (state.panel === 'materials') {
    html = materialLibraryHtml(c)
  } else {
    html = `<div class="property-group"><h3>时间范围</h3>${field('开始节点', `<input class="input" id="meteoStart" type="number" value="${c.meteo.start}">`)}${field('结束节点', `<input class="input" id="meteoEnd" type="number" value="${c.meteo.end}">`)}</div>
      <div class="property-group"><h3>驱动文件</h3><div class="path-input"><span title="${escapeHtml(c.meteo.path)}">${escapeHtml(c.meteo.path)}</span></div></div>${builtinNotice()}`
  }
  $('#inspectorContent').innerHTML = html
  bindInspector()
}

function bindValue(id, target, selector, converter = Number) {
  const input = $(`#${id}`)
  input?.addEventListener('change', () => {
    target(converter(input.value)); updateXml(selector, input.value); refreshFromState()
  })
}

function bindSwitch(id, getter, setter, selector) {
  const button = $(`#${id}`)
  button?.addEventListener('click', () => {
    const next = !getter(); setter(next); button.classList.toggle('on', next); button.setAttribute('aria-checked', String(next)); updateXml(selector, next ? 1 : 0); refreshFromState(false)
  })
}

function bindInspector() {
  const c = state.config
  if (!c) return
  bindValue('sceneX', (v) => c.scene.x = v, 'Scene sceneSizeX'); bindValue('sceneY', (v) => c.scene.y = v, 'Scene sceneSizeY')
  bindValue('sceneHeight', (v) => c.scene.height = v, 'Scene Height'); bindValue('voxelSize', (v) => c.scene.voxel = v, 'Scene voxelSize')
  bindValue('rayDepth', (v) => c.control.depth = v, 'Control rayTracingDepth'); bindValue('gpuIndex', (v) => c.control.gpu = v, 'Control GPU')
  bindSwitch('terrainSwitch', () => c.scene.terrain, (v) => c.scene.terrain = v, 'Control isDEM')
  const bindAngle = (id, key, index) => $(`#${id}`)?.addEventListener('change', (event) => { c.light[key] = Number(event.target.value); updatePair('Geometry Light light lightAngle lightAngle', index, event.target.value); refreshFromState() })
  bindAngle('sunZenith', 'zenith', 0); bindAngle('sunAzimuth', 'azimuth', 1)
  const bindRange = (id, key, valueId, selector) => {
    const input = $(`#${id}`)
    input?.addEventListener('input', () => { c.light[key] = Number(input.value); $(`#${valueId}`).textContent = Number(input.value).toFixed(2); updateSun(c) })
    input?.addEventListener('change', () => { updateXml(selector, input.value); refreshFromState(false) })
  }
  bindRange('directLight', 'direct', 'directValue', 'Geometry Light light direct'); bindRange('diffuseLight', 'diffuse', 'diffuseValue', 'Geometry Light light diffuse')
  bindValue('skyTemperature', (v) => c.light.skyTemperature = v, 'Geometry Light light skyTemperature')
  bindValue('sensorX', (v) => c.sensor.x = v, 'Geometry Sensor sensor pixelResolutionX'); bindValue('sensorY', (v) => c.sensor.y = v, 'Geometry Sensor sensor pixelResolutionY')
  bindValue('sensorBands', (v) => c.sensor.bands = v, 'Geometry Sensor sensor controlBand', String)
  const bindView = (id, key, index) => $(`#${id}`)?.addEventListener('change', (event) => { c.sensor[key] = Number(event.target.value); updatePair('Geometry Sensor sensor viewAngle viewAngle[type="custom"] viewAngles', index, event.target.value); refreshFromState(false) })
  bindView('viewZenith', 'vza', 0); bindView('viewAzimuth', 'vaa', 1)
  bindSwitch('imageSwitch', () => c.sensor.image, (v) => c.sensor.image = v, 'Control isImage'); bindSwitch('temperatureSwitch', () => c.sensor.temperature, (v) => c.sensor.temperature = v, 'Control isTemperature'); bindSwitch('albedoSwitch', () => c.sensor.albedo, (v) => c.sensor.albedo = v, 'Control isAlbedo')
  bindValue('meteoStart', (v) => c.meteo.start = v, 'Meteorology startTimeNode'); bindValue('meteoEnd', (v) => c.meteo.end = v, 'Meteorology endTimeNode')
  $('#objectImportAction')?.addEventListener('click', importObj)
  $$('.object-card[data-index]').forEach((card) => {
    const showAttributes = () => showObjectAttributeDialog(Number(card.dataset.index))
    card.addEventListener('click', (event) => { if (!event.target.closest('button')) showAttributes() })
    card.addEventListener('keydown', (event) => { if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); showAttributes() } })
  })
  $$('.object-material').forEach((button) => button.addEventListener('click', (event) => { event.stopPropagation(); showObjectAttributeDialog(Number(button.closest('.object-card').dataset.index)) }))
  $$('.object-distribution').forEach((button) => button.addEventListener('click', (event) => { event.stopPropagation(); showObjectDialog(Number(button.closest('.object-card').dataset.index)) }))
  $$('.object-delete').forEach((button) => button.addEventListener('click', (event) => { event.stopPropagation(); deleteSceneObject(Number(button.closest('.object-card').dataset.index)) }))
  $('#openProjectAction')?.addEventListener('click', chooseXml)
  $$('.library-add').forEach((button) => button.addEventListener('click', () => showPresetDialog(button.dataset.presetKind)))
  $$('.library-filter').forEach((input) => input.addEventListener('input', () => {
    const keyword = input.value.trim().toLowerCase()
    $$(`[data-library-list="${input.dataset.libraryFilter}"] .library-card`).forEach((card) => card.classList.toggle('filtered-out', !card.dataset.librarySearch.includes(keyword)))
  }))
  $('#facetToVoxelAction:not([disabled])')?.addEventListener('click', () => selectMode(state.mode === 'eFacetEB' ? 'eVoxelEB' : 'eVoxelRT', true))
}

function refreshFromState(rebuild = true) {
  const c = state.config
  $('#sceneSizeBadge').textContent = `${c.scene.x} × ${c.scene.y} m`; $('#lightBadge').textContent = `${c.light.zenith}° / ${c.light.azimuth}°`; $('#sensorBadge').textContent = `${c.sensor.x} × ${c.sensor.y}`
  $('#objectBadge').textContent = c.objects.count || '0'; $('#materialBadge').textContent = `${c.spectra.length + c.thermals.length + c.materials.length}`; $('#meteoBadge').textContent = state.inputPath ? `${c.meteo.start}–${c.meteo.end}` : '未加载'; updateSun(c)
  if (rebuild) { generateWorld(c); fitCamera() }
}

async function loadActualScene(config) {
  await loadXmlScene({ api, world, config, log: addLog })
  updateStats(); applyViewStyle(currentStyle); fitCamera()
}

function loadXml(result) {
  try {
    const config = ensureMaterialPresets(parseXml(result.content))
    const project = result.project || null
    const storedItems = project?.configuration?.objects?.items || []
    config.objects.items = config.objects.items.map((item, index) => {
      const stored = storedItems.find((entry) => (entry.fileName && entry.fileName === item.fileName) || entry.name === item.name) || storedItems[index]
      return stored ? { ...stored, ...item, distribution: stored.distribution, instanceCount: stored.instanceCount } : item
    })
    if (project) project.configuration = config
    Object.assign(state, { inputPath: result.path, xmlText: result.content, xmlDirty: false, project, projectPath: result.projectPath || '', config, outputDir: config.outDir || result.path.replace(/[\\/][^\\/]+$/, '') , selectedObjectIndex: -1, editingObjectIndex: -1, editingAttributeIndex: -1 })
    if (project?.mode) selectMode(project.mode)
    $('#xmlEditor').value = result.content; $('#xmlEditor').hidden = false; $('#emptyEditor').hidden = true; $('#applyXmlBtn').disabled = false; $('#xmlStatus').textContent = result.path
    $('#projectName').textContent = project?.name || fileName(result.path); $('#unsavedMark').hidden = true
    refreshFromState(); loadActualScene(config); renderInspector(); addLog(`已加载配置：${result.path}`, 'success'); toast('配置已加载', fileName(result.path))
  } catch (error) { toast('无法读取配置', error.message, 'error'); addLog(error.message, 'error') }
}

async function chooseXml() { const result = await api.chooseXml(); if (result) loadXml(result) }
async function saveXml(fromProject = false) {
  fromProject = fromProject === true
  if (!state.xmlText) { await chooseXml(); return false }
  try {
    const config = fromProject ? state.config : parseXml(state.xmlText)
    state.config = config
    if (state.project) {
      state.project.configuration = config
      state.project.mode = state.mode
    }
    const result = await api.saveXml({ path: state.inputPath, content: state.xmlText, project: state.project })
    if (!result?.path) return false
    state.inputPath = result.path
    state.project = result.project || state.project
    state.projectPath = result.projectPath || state.projectPath
    state.xmlText = result.content || state.xmlText
    state.xmlDirty = false
    $('#xmlEditor').value = state.xmlText
    $('#unsavedMark').hidden = true; $('#xmlStatus').textContent = result.path; $('#projectName').textContent = state.project?.name || fileName(result.path)
    addLog(`已保存配置：${result.path}`, 'success'); toast('保存成功', fileName(result.path)); return true
  } catch (error) { toast('保存失败', error.message, 'error'); return false }
}

function ensureProjectState() {
  if (state.project) return state.project
  const parts = state.inputPath.replaceAll('\\', '/').split('/')
  state.project = createDefaultProject({ name: parts.at(-2) || '导入工程', mode: state.mode })
  state.project.configuration = state.config
  return state.project
}

function uniqueName(value, fallback) {
  const cleaned = String(value || '').replace(/\.[^.]+$/, '').replace(/[^\p{L}\p{N}_-]+/gu, '_').replace(/^_+|_+$/g, '')
  return cleaned || fallback
}

function sanitizeObjContent(content) {
  const sanitized = String(content)
    .split(/\r?\n/)
    .filter((line) => !/^\s*(mtllib|usemtl)\b/i.test(line))
    .join('\n')
    .trimEnd()
  return sanitized ? sanitized + '\n' : ''
}

function finiteNumber(value, fallback) {
  const number = Number(value)
  return Number.isFinite(number) ? number : fallback
}

const MIN_DISTRIBUTION_DENSITY = .01
const DISTRIBUTION_DENSITY_DECIMALS = 2

function normalizeDistributionDensity(value, fallback = .02) {
  const number = Number(value)
  const safe = Number.isFinite(number) && number > 0 ? number : fallback
  return Math.max(MIN_DISTRIBUTION_DENSITY, Number(safe.toFixed(DISTRIBUTION_DENSITY_DECIMALS)))
}

function distributionDensityInputValue(value) {
  return normalizeDistributionDensity(value).toFixed(DISTRIBUTION_DENSITY_DECIMALS)
}

function distributionArea(distribution) {
  const width = Math.max(0, Math.abs(distribution.maxX - distribution.minX))
  const height = Math.max(0, Math.abs(distribution.maxY - distribution.minY))
  return Math.max(.000001, width * height)
}

function populationFromValues(countValue, densityValue, area, basis) {
  let count = Math.max(1, Math.min(10000, Math.round(finiteNumber(countValue, 1))))
  let density = normalizeDistributionDensity(densityValue)
  if (basis === 'density') {
    count = Math.max(1, Math.min(10000, Math.round(area * density)))
  } else {
    density = normalizeDistributionDensity(count / area)
  }
  return { count, density }
}

function defaultDistribution(item = {}) {
  const sceneX = Math.max(1, finiteNumber(state.config?.scene?.x, 60))
  const sceneY = Math.max(1, finiteNumber(state.config?.scene?.y, 45))
  const vegetation = item.type === 'Vegetation' || !item.type
  const density = .02
  const count = Math.max(1, Math.min(10000, Math.round(sceneX * sceneY * density)))
  return {
    mode: 'random', basis: 'count', count, density,
    rows: 4, columns: 6,
    minX: 0, maxX: sceneX,
    minY: 0, maxY: sceneY,
    seed: 1, scaleMin: vegetation ? .85 : 1, scaleMax: vegetation ? 1.15 : 1,
    z: 0, points: ''
  }
}

function distributionFromForm(form, options = {}) {
  const values = Object.fromEntries(new FormData(form).entries())
  const defaults = defaultDistribution({ type: values.objectType })
  const densityInput = String(values.distributionDensity ?? '').trim()
  const densityValue = Number(densityInput)
  if (options.requireValidDensity && (!densityInput || !Number.isFinite(densityValue) || densityValue <= 0 || normalizeDistributionDensity(densityValue) < MIN_DISTRIBUTION_DENSITY)) {
    throw new Error('分布密度必须填写大于 0 的有效数值，且保留两位小数')
  }
  const minX = finiteNumber(values.distributionMinX, defaults.minX)
  const maxX = finiteNumber(values.distributionMaxX, defaults.maxX)
  const minY = finiteNumber(values.distributionMinY, defaults.minY)
  const maxY = finiteNumber(values.distributionMaxY, defaults.maxY)
  const basis = values.distributionBasis === 'density' ? 'density' : 'count'
  const population = populationFromValues(
    values.distributionCount,
    values.distributionDensity,
    distributionArea({ minX, maxX, minY, maxY }),
    basis
  )
  return {
    mode: ['random', 'grid', 'single', 'manual'].includes(values.distributionMode) ? values.distributionMode : defaults.mode,
    basis,
    count: population.count,
    density: population.density,
    rows: Math.max(1, Math.round(finiteNumber(values.distributionRows, defaults.rows))),
    columns: Math.max(1, Math.round(finiteNumber(values.distributionColumns, defaults.columns))),
    minX, maxX, minY, maxY,
    seed: Math.round(finiteNumber(values.distributionSeed, defaults.seed)),
    scaleMin: Math.max(.01, finiteNumber(values.distributionScaleMin, defaults.scaleMin)),
    scaleMax: Math.max(.01, finiteNumber(values.distributionScaleMax, defaults.scaleMax)),
    z: finiteNumber(values.distributionZ, defaults.z), points: String(values.distributionPoints || '').trim()
  }
}

function setDistributionForm(form, value) {
  const distribution = { ...defaultDistribution(), ...(value || {}) }
  const fields = {
    distributionMode: distribution.mode, distributionBasis: distribution.basis,
    distributionCount: distribution.count, distributionDensity: distributionDensityInputValue(distribution.density),
    distributionRows: distribution.rows, distributionColumns: distribution.columns,
    distributionMinX: distribution.minX, distributionMaxX: distribution.maxX,
    distributionMinY: distribution.minY, distributionMaxY: distribution.maxY,
    distributionSeed: distribution.seed, distributionScaleMin: distribution.scaleMin,
    distributionScaleMax: distribution.scaleMax, distributionZ: distribution.z,
    distributionPoints: distribution.points || ''
  }
  for (const [name, fieldValue] of Object.entries(fields)) if (form.elements[name]) form.elements[name].value = fieldValue
}

function seededRandom(seed) {
  let value = seed >>> 0
  return () => {
    value += 0x6D2B79F5
    let result = value
    result = Math.imul(result ^ result >>> 15, result | 1)
    result ^= result + Math.imul(result ^ result >>> 7, result | 61)
    return ((result ^ result >>> 14) >>> 0) / 4294967296
  }
}

function distributionTargetCount(distribution) {
  const width = Math.max(0, Math.abs(distribution.maxX - distribution.minX))
  const height = Math.max(0, Math.abs(distribution.maxY - distribution.minY))
  const requested = distribution.basis === 'density' ? Math.round(width * height * distribution.density) : Math.round(distribution.count)
  return Math.max(1, Math.min(10000, requested || 1))
}

function generateDistribution(distribution) {
  const d = { ...defaultDistribution(), ...distribution }
  const minX = Math.min(d.minX, d.maxX), maxX = Math.max(d.minX, d.maxX)
  const minY = Math.min(d.minY, d.maxY), maxY = Math.max(d.minY, d.maxY)
  const scaleMin = Math.min(d.scaleMin, d.scaleMax), scaleMax = Math.max(d.scaleMin, d.scaleMax)
  if (d.mode === 'manual') {
    const points = String(d.points || '').split(/\r?\n/).map((line) => line.trim()).filter(Boolean).map((line, index) => {
      const values = line.split(/[\s,]+/).map(Number)
      if (values.length < 2 || !values.every(Number.isFinite)) throw new Error(`自定义坐标第 ${index + 1} 行无效`)
      return { x: values[0], y: values[1], z: values[2] ?? d.z, scale: values[3] ?? 1, rotation: values[4] ?? 0 }
    })
    if (!points.length) throw new Error('自定义坐标不能为空')
    if (points.length > 10000) throw new Error('单个 OBJ 最多支持 10000 个实例')
    if (points.some((item) => item.scale <= 0)) throw new Error('实例缩放必须大于 0')
    return points
  }
  if (d.mode === 'single') return [{ x: (minX + maxX) / 2, y: (minY + maxY) / 2, z: d.z, scale: scaleMin, rotation: 0 }]
  const targetCount = distributionTargetCount(d)
  if (d.mode === 'grid') {
    const width = Math.max(.001, maxX - minX), height = Math.max(.001, maxY - minY)
    const columns = Math.max(1, Math.ceil(Math.sqrt(targetCount * width / height)))
    const rows = Math.max(1, Math.ceil(targetCount / columns))
    d.rows = rows; d.columns = columns
    return Array.from({ length: targetCount }, (_, index) => {
      const row = Math.floor(index / columns), column = index % columns
      return {
        x: columns === 1 ? (minX + maxX) / 2 : minX + (maxX - minX) * column / (columns - 1),
        y: rows === 1 ? (minY + maxY) / 2 : minY + (maxY - minY) * row / (rows - 1),
        z: d.z, scale: scaleMin, rotation: 0
      }
    })
  }
  const random = seededRandom(d.seed)
  return Array.from({ length: targetCount }, () => ({
    x: minX + random() * (maxX - minX), y: minY + random() * (maxY - minY), z: d.z,
    scale: scaleMin + random() * (scaleMax - scaleMin), rotation: random() * 360
  }))
}

function distributionSummary(item) {
  const labels = { random: '随机', grid: '网格', single: '单实例', manual: '自定义' }
  const count = item.instanceCount || (item.distribution ? distributionTargetCount({ ...defaultDistribution(item), ...item.distribution }) : 1)
  const control = item.distribution?.basis === 'density' ? `${item.distribution.density} 个/m²` : `${count} 个实例`
  return `${labels[item.distribution?.mode] || '已有位置'} · ${control}`
}

function optionList(items, newLabel, selected = '__new__') {
  const create = `<option value="__new__" ${selected === '__new__' ? 'selected' : ''}>新建：${escapeHtml(newLabel)}</option>`
  return create + items.map((item) => `<option value="${escapeHtml(item.name)}" ${item.name === selected ? 'selected' : ''}>${escapeHtml(item.label || item.name)} · ${escapeHtml(item.name)}</option>`).join('')
}

function formField(name) { return document.querySelector("#materialForm [name=\"" + name + "\"]") }

function setFormField(name, value) {
  const input = formField(name)
  if (input && value !== undefined && value !== null) input.value = value
}

function physicalTypeForObject(type) {
  if (type === 'Vegetation' || type === 'Water') return type
  return 'Soil'
}

function physicalInput(label, name, value, step = 'any') {
  return `<label class="dialog-field"><span>${escapeHtml(label)}</span><input class="input" name="${name}" type="number" step="${step}" value="${escapeHtml(value)}"></label>`
}

function renderModelParameters() {
  const model = $('#spectralModel').value
  if (model === 'Prospect') {
    $('#modelParameterFields').innerHTML = `${field('Cab', '<input class="input" name="paramCab" type="number" step="0.1" value="40">')}${field('Cw', '<input class="input" name="paramCw" type="number" step="0.001" value="0.01">')}${field('Cdm', '<input class="input" name="paramCdm" type="number" step="0.001" value="0.01">')}${field('Cs', '<input class="input" name="paramCs" type="number" step="0.001" value="0">')}${field('N', '<input class="input" name="paramN" type="number" step="0.1" value="1.5">')}`
  } else if (model === 'BSM') {
    $('#modelParameterFields').innerHTML = `${field('SMC', '<input class="input" name="paramSMC" type="number" step="0.1" value="25">')}${field('亮度', '<input class="input" name="paramBSMBrightness" type="number" step="0.1" value="0.5">')}${field('纬度参数', '<input class="input" name="paramBSMlat" type="number" step="0.1" value="0">')}${field('经度参数', '<input class="input" name="paramBSMlon" type="number" step="0.1" value="0">')}`
  } else $('#modelParameterFields').innerHTML = '<div class="notice">自定义模型直接使用各波段反射率和透射率，填写单个光学反射率和透射率值。</div>'
}

function applySpectrumPreset() {
  const selected = state.config.spectra.find((item) => item.name === document.querySelector("#spectralPreset").value)
  const nameInput = document.querySelector("#spectralMaterialName")
  if (selected) {
    nameInput.value = selected.name
    nameInput.readOnly = true
    document.querySelector("#spectralModel").value = selected.model || "custom"
    setFormField("reflectance", selected.reflectance || "0.20")
    setFormField("transmittance", selected.transmittance || "0.0")
    setFormField("refTir", selected.refTir ?? 0.05)
    setFormField("tauTir", selected.tauTir ?? 0)
    renderModelParameters()
    const params = selected.params || {}
    for (const [key, value] of Object.entries({ Cab: params.Cab, Cw: params.Cw, Cdm: params.Cdm, Cs: params.Cs, N: params.N, SMC: params.SMC, BSMBrightness: params.BSMBrightness, BSMlat: params.BSMlat, BSMlon: params.BSMlon })) setFormField("param" + key, value)
  } else {
    nameInput.readOnly = false
    renderModelParameters()
  }
}

function renderSpectrumPresetOptions() {
  const type = document.querySelector("#materialObjectType").value
  const preferred = { Vegetation: "green_leaf", Soil: "soil", Building: "concrete", Other: "concrete", Water: "water_surface" }[type]
  const current = document.querySelector("#spectralPreset").value
  const selected = state.config.spectra.some((item) => item.name === current) ? current : preferred
  document.querySelector("#spectralPreset").innerHTML = optionList(state.config.spectra, document.querySelector("#spectralMaterialName").value, selected)
  applySpectrumPreset()
}

function applyThermalPreset() {
  const selected = state.config.thermals.find((item) => item.name === document.querySelector("#thermalPreset").value)
  const nameInput = document.querySelector("#thermalMaterialName")
  if (selected) {
    nameInput.value = selected.name
    nameInput.readOnly = true
    setFormField("sunlitTemperature", selected.sunlitTemperature)
    setFormField("shadedTemperature", selected.shadedTemperature)
  } else nameInput.readOnly = false
}

function renderThermalPresetOptions() {
  const type = document.querySelector("#materialObjectType").value
  const preferred = { Vegetation: "vegetation_temperature", Soil: "soil_temperature", Building: "building_temperature", Other: "building_temperature", Water: "water_temperature" }[type]
  const current = document.querySelector("#thermalPreset").value
  const selected = state.config.thermals.some((item) => item.name === current) ? current : preferred
  document.querySelector("#thermalPreset").innerHTML = optionList(state.config.thermals, document.querySelector("#thermalMaterialName").value, selected)
  applyThermalPreset()
}

function renderPhysicalParameters() {
  const type = physicalTypeForObject($('#materialObjectType').value)
  const selected = state.config.materials.find((item) => item.name === $('#physicalMaterialPreset').value)
  const nameInput = $('#physicalMaterialName')
  if (selected) {
    nameInput.value = selected.name
    nameInput.readOnly = true
    $('#physicalParameterFields').innerHTML = `<div class="notice">使用“${escapeHtml(selected.label || selected.name)}”预设：${physicalValues(selected).map(([key, value]) => `${escapeHtml(key)}=${escapeHtml(value)}`).join('，')}</div>`
    return
  }
  nameInput.readOnly = false
  if (nameInput.dataset.newName) nameInput.value = nameInput.dataset.newName
  const defaults = createMaterialPresets().materials.find((item) => item.type === type)?.params || {}
  if (type === 'Vegetation') {
    $('#physicalParameterFields').innerHTML = [
      physicalInput('Vcmax', 'bioVcmax', defaults.Vcmax, 0.1), physicalInput('气孔斜率 m', 'bioM', defaults.m, 0.1),
      physicalInput('Ball-Berry 截距', 'bioBallBerry', defaults.BallBerry, 0.001), physicalInput('光合类型', 'bioType', defaults.Type, 1),
      physicalInput('kV', 'bioKV', defaults.kV, 0.0001), physicalInput('暗呼吸系数', 'bioRdparam', defaults.Rdparam, 0.001),
      `<label class="dialog-field"><span>温度响应参数</span><input class="input" name="bioTparam" value="${escapeHtml(defaults.Tparam)}"></label>`,
      physicalInput('年均温', 'bioTyear', defaults.Tyear, 0.1), physicalInput('beta', 'bioBeta', defaults.beta, 0.001),
      physicalInput('NPQ 系数', 'bioKNPQs', defaults.kNPQs, 0.01), physicalInput('光限制', 'bioQLs', defaults.qLs, 0.01),
      physicalInput('胁迫因子', 'bioStressfactor', defaults.stressfactor, 0.01), physicalInput('温度校正', 'bioTcor', defaults.Tcor, 1)
    ].join('')
  } else if (type === 'Water') {
    $('#physicalParameterFields').innerHTML = [
      physicalInput('表面阻力 rss', 'waterRss', defaults.rss, 1), physicalInput('体积热容', 'waterHeatCapacity', defaults.heatCapacity, 1),
      physicalInput('混合深度', 'waterMixingDepth', defaults.mixingDepth, 0.1), physicalInput('蒸发系数', 'waterEvaporationCoefficient', defaults.evaporationCoefficient, 0.1)
    ].join('')
  } else {
    $('#physicalParameterFields').innerHTML = [
      physicalInput('方法', 'soilMethod', defaults.method, 1), physicalInput('表面阻力 rss', 'soilRss', defaults.rss, 1),
      physicalInput('比热容 cs', 'soilCs', defaults.cs, 1), physicalInput('密度 rhos', 'soilRhos', defaults.rhos, 1),
      physicalInput('导热率 λ', 'soilLambdas', defaults.lambdas, 0.01), physicalInput('初始温度', 'soilTsoil', defaults.Tsoil, 0.1),
      physicalInput('土壤含水量 SMC', 'soilSMC', defaults.SMC, 0.1), physicalInput('饱和含水量', 'soilSatwater', defaults.Satwater, 0.01)
    ].join('')
  }
}

function renderPhysicalPresetOptions() {
  const objectType = $('#materialObjectType').value
  const type = physicalTypeForObject(objectType)
  const materials = state.config.materials.filter((item) => item.type === type)
  const preferred = { Vegetation: 'leaf_c3', Soil: 'soilset', Building: 'soil_dry', Other: 'soilset', Water: 'water_set' }[objectType]
  $('#physicalMaterialPreset').innerHTML = optionList(materials, $('#physicalMaterialName').value, materials.some((item) => item.name === preferred) ? preferred : materials[0]?.name || '__new__')
  renderPhysicalParameters()
}

function renderMeshMaterialRows() {
  const pending = state.pendingObject
  if (!pending) return
  const type = $('#materialObjectType').value
  const preferredSpectrum = { Vegetation: 'green_leaf', Soil: 'soil', Building: 'concrete', Other: 'concrete', Water: 'water_surface' }[type]
  const preferredThermal = { Vegetation: 'vegetation_temperature', Soil: 'soil_temperature', Building: 'building_temperature', Other: 'building_temperature', Water: 'water_temperature' }[type]
  const spectrumOptions = optionList(state.config.spectra || [], $('#spectralMaterialName').value, document.querySelector("#spectralPreset").value || preferredSpectrum)
  const thermalOptions = optionList(state.config.thermals || [], $('#thermalMaterialName').value, document.querySelector("#thermalPreset").value || preferredThermal)
  $('#meshMaterialRows').innerHTML = pending.meshNames.map((name, index) => `<tr><td title="${escapeHtml(name)}">${escapeHtml(name)}</td><td><select class="select mesh-spectrum" data-index="${index}">${spectrumOptions}</select></td><td><select class="select mesh-thermal" data-index="${index}">${thermalOptions}</select></td></tr>`).join('')
}

function showMaterialDialog() {
  const pending = state.pendingObject
  if (!pending) return
  $('#materialForm').reset()
  const base = uniqueName(pending.source.path, 'object')
  $('#materialObjectName').value = base
  $('#physicalMaterialName').value = `${base}_material`
  $('#spectralMaterialName').value = `${base}_spectral`
  $('#thermalMaterialName').value = `${base}_temperature`
  $('#physicalMaterialName').dataset.newName = `${base}_material`
  $('#spectralPreset').value = '__new__'
  $('#thermalPreset').value = '__new__'
  $('#physicalMaterialPreset').value = '__new__'
  $('#spectralModel').value = 'Prospect'
  renderModelParameters(); renderSpectrumPresetOptions(); renderThermalPresetOptions(); renderPhysicalPresetOptions(); renderMeshMaterialRows()
  $('#materialDialog').hidden = false
}

function hideMaterialDialog(discard = true) {
  $('#materialDialog').hidden = true
  if (discard && state.pendingObject) disposeObject(state.pendingObject.object)
  if (discard) state.pendingObject = null
}

async function importObj() {
  if (!state.inputPath) { toast('无法导入 OBJ', '请先新建或打开工程', 'error'); return }
  const result = await api.chooseObj()
  if (!result) return
  try {
    const content = sanitizeObjContent(result.content)
    const object = new OBJLoader().parse(content)
    const meshNames = []
    object.traverse((child) => { if (child.isMesh) { const name = child.name || child.material?.name || `mesh_${meshNames.length + 1}`; if (!meshNames.includes(name)) meshNames.push(name) } })
    if (!meshNames.length) throw new Error('OBJ 中没有可用 mesh')
    const box = new THREE.Box3().setFromObject(object), size = box.getSize(new THREE.Vector3()), center = box.getCenter(new THREE.Vector3()), scale = 8 / Math.max(size.x, size.y, size.z, .001)
    object.position.sub(center).multiplyScalar(scale); object.scale.setScalar(scale); object.position.y += size.y * scale / 2
    state.pendingObject = { source: { ...result, content }, object, meshNames, dimensions: [size.x, size.y, size.z] }
    showMaterialDialog()
  } catch (error) { toast('OBJ 解析失败', error.message, 'error') }
}

function upsertByName(list, value) {
  const index = list.findIndex((item) => item.name === value.name)
  if (index >= 0) list[index] = value
  else list.push(value)
}

function physicalFromForm(values, type) {
  let params
  if (type === 'Vegetation') {
    params = {
      Vcmax: Number(values.bioVcmax), m: Number(values.bioM), BallBerry: Number(values.bioBallBerry), Type: Number(values.bioType),
      kV: Number(values.bioKV), Rdparam: Number(values.bioRdparam), Tparam: values.bioTparam, Tyear: Number(values.bioTyear),
      beta: Number(values.bioBeta), kNPQs: Number(values.bioKNPQs), qLs: Number(values.bioQLs),
      stressfactor: Number(values.bioStressfactor), Tcor: Number(values.bioTcor)
    }
  } else if (type === 'Water') {
    params = { rss: Number(values.waterRss), heatCapacity: Number(values.waterHeatCapacity), mixingDepth: Number(values.waterMixingDepth), evaporationCoefficient: Number(values.waterEvaporationCoefficient) }
  } else {
    params = {
      method: Number(values.soilMethod), rss: Number(values.soilRss), cs: Number(values.soilCs), rhos: Number(values.soilRhos),
      lambdas: Number(values.soilLambdas), Tsoil: Number(values.soilTsoil), SMC: Number(values.soilSMC), Satwater: Number(values.soilSatwater)
    }
  }
  return { name: values.materialName, type, params }
}

function presetSelect(label, id, name, options) {
  return `<label class="dialog-field"><span>${escapeHtml(label)}</span><select class="select" id="${id}" name="${name}">${options}</select></label>`
}

function presetText(label, name, value, attrs = '') {
  return `<label class="dialog-field"><span>${escapeHtml(label)}</span><input class="input" name="${name}" value="${escapeHtml(value)}" ${attrs}></label>`
}

function renderPresetParameterFields(spectralModel = 'custom') {
  const kind = $('#presetKind').value
  const target = $('#presetParameterFields')
  if (kind === 'spectrum') {
    const models = [['custom', '自定义'], ['Prospect', 'PROSPECT'], ['BSM', 'BSM']]
    const modelOptions = models.map(([value, label]) => `<option value="${value}" ${value === spectralModel ? 'selected' : ''}>${label}</option>`).join('')
    let parameters = ''
    if (spectralModel === 'Prospect') {
      parameters = `${physicalInput('Cab', 'paramCab', 40, 0.1)}${physicalInput('Cw', 'paramCw', 0.01, 0.001)}${physicalInput('Cdm', 'paramCdm', 0.01, 0.001)}${physicalInput('Cs', 'paramCs', 0, 0.001)}${physicalInput('N', 'paramN', 1.5, 0.1)}`
    } else if (spectralModel === 'BSM') {
      parameters = `${physicalInput('土壤含水量 SMC', 'paramSMC', 25, 0.1)}${physicalInput('亮度', 'paramBSMBrightness', 0.5, 0.1)}${physicalInput('纬度参数', 'paramBSMlat', 0, 0.1)}${physicalInput('经度参数', 'paramBSMlon', 0, 0.1)}`
    }
    target.innerHTML = `${presetSelect('光谱模型', 'presetSpectralModel', 'spectralModel', modelOptions)}${presetText('反射率', 'reflectance', spectralModel === 'Prospect' ? '0.08' : '0.20', 'required')}${presetText('透射率', 'transmittance', spectralModel === 'Prospect' ? '0.04' : '0.0', 'required')}${physicalInput('TIR 反射率', 'refTir', 0.05, 0.01)}${physicalInput('TIR 透射率', 'tauTir', 0, 0.01)}${parameters}`
    $('#presetSpectralModel').addEventListener('change', (event) => renderPresetParameterFields(event.target.value))
    return
  }
  if (kind === 'thermal') {
    target.innerHTML = `${physicalInput('阳面温度（K）', 'sunlitTemperature', 305, 0.1)}${physicalInput('阴面温度（K）', 'shadedTemperature', 295, 0.1)}`
    return
  }
  const typeOptions = [['Vegetation', '植被生理生化'], ['Soil', '土壤表面物化'], ['Water', '水体物性']].map(([value, label]) => `<option value="${value}" ${value === state.presetPhysicalType ? 'selected' : ''}>${label}</option>`).join('')
  const defaults = createMaterialPresets().materials.find((item) => item.type === state.presetPhysicalType)?.params || {}
  let parameters
  if (state.presetPhysicalType === 'Vegetation') {
    parameters = [
      physicalInput('Vcmax', 'bioVcmax', defaults.Vcmax, 0.1), physicalInput('气孔斜率 m', 'bioM', defaults.m, 0.1),
      physicalInput('Ball-Berry 截距', 'bioBallBerry', defaults.BallBerry, 0.001), physicalInput('光合类型', 'bioType', defaults.Type, 1),
      physicalInput('kV', 'bioKV', defaults.kV, 0.0001), physicalInput('暗呼吸系数', 'bioRdparam', defaults.Rdparam, 0.001),
      presetText('温度响应参数', 'bioTparam', defaults.Tparam), physicalInput('年均温', 'bioTyear', defaults.Tyear, 0.1),
      physicalInput('beta', 'bioBeta', defaults.beta, 0.001), physicalInput('NPQ 系数', 'bioKNPQs', defaults.kNPQs, 0.01),
      physicalInput('光限制', 'bioQLs', defaults.qLs, 0.01), physicalInput('胁迫因子', 'bioStressfactor', defaults.stressfactor, 0.01),
      physicalInput('温度校正', 'bioTcor', defaults.Tcor, 1)
    ].join('')
  } else if (state.presetPhysicalType === 'Water') {
    parameters = `${physicalInput('表面阻力 rss', 'waterRss', defaults.rss, 1)}${physicalInput('体积热容', 'waterHeatCapacity', defaults.heatCapacity, 1)}${physicalInput('混合深度', 'waterMixingDepth', defaults.mixingDepth, 0.1)}${physicalInput('蒸发系数', 'waterEvaporationCoefficient', defaults.evaporationCoefficient, 0.1)}`
  } else {
    parameters = `${physicalInput('方法', 'soilMethod', defaults.method, 1)}${physicalInput('表面阻力 rss', 'soilRss', defaults.rss, 1)}${physicalInput('比热容 cs', 'soilCs', defaults.cs, 1)}${physicalInput('密度 rhos', 'soilRhos', defaults.rhos, 1)}${physicalInput('导热率 λ', 'soilLambdas', defaults.lambdas, 0.01)}${physicalInput('初始温度', 'soilTsoil', defaults.Tsoil, 0.1)}${physicalInput('土壤含水量 SMC', 'soilSMC', defaults.SMC, 0.1)}${physicalInput('饱和含水量', 'soilSatwater', defaults.Satwater, 0.01)}`
  }
  target.innerHTML = `${presetSelect('物化类型', 'presetPhysicalType', 'physicalType', typeOptions)}${parameters}`
  $('#presetPhysicalType').addEventListener('change', (event) => { state.presetPhysicalType = event.target.value; renderPresetParameterFields() })
}

function showPresetDialog(kind = 'spectrum') {
  if (!state.inputPath) { toast('无法新增材质属性', '请先新建或打开工程', 'error'); return }
  $('#presetForm').reset()
  $('#presetKind').value = kind
  $('#presetName').value = ''
  $('#presetLabel').value = ''
  state.presetPhysicalType = 'Vegetation'
  renderPresetParameterFields()
  $('#presetDialog').hidden = false
  setTimeout(() => $('#presetName').focus(), 0)
}

function hidePresetDialog() { $('#presetDialog').hidden = true }

function presetFromForm(values) {
  const name = uniqueName(values.presetName, 'material')
  const label = String(values.presetLabel || '').trim() || name
  if (values.presetKind === 'spectrum') {
    return {
      list: state.config.spectra,
      item: { name, label, model: values.spectralModel, reflectance: values.reflectance, transmittance: values.transmittance, refTir: Number(values.refTir), tauTir: Number(values.tauTir), params: { Cab: Number(values.paramCab), Cw: Number(values.paramCw), Cdm: Number(values.paramCdm), Cs: Number(values.paramCs), N: Number(values.paramN), SMC: Number(values.paramSMC), BSMBrightness: Number(values.paramBSMBrightness), BSMlat: Number(values.paramBSMlat), BSMlon: Number(values.paramBSMlon) } }
    }
  }
  if (values.presetKind === 'thermal') {
    return { list: state.config.thermals, item: { name, label, sunlitTemperature: Number(values.sunlitTemperature), shadedTemperature: Number(values.shadedTemperature) } }
  }
  return { list: state.config.materials, item: { ...physicalFromForm({ ...values, materialName: name }, values.physicalType), label } }
}

async function savePreset(event) {
  event.preventDefault()
  const submit = $('#presetForm button[type="submit"]')
  const values = Object.fromEntries(new FormData($('#presetForm')).entries())
  const { list, item } = presetFromForm(values)
  if (list.some((entry) => entry.name === item.name)) { toast('名称已存在', `请更换内部名称：${item.name}`, 'error'); return }
  try {
    submit.disabled = true
    ensureProjectState()
    list.push(item)
    state.project.configuration = state.config
    state.xmlDirty = true
    $('#unsavedMark').hidden = false
    if (!(await saveXml(true))) { list.pop(); throw new Error('预设保存失败') }
    hidePresetDialog(); refreshFromState(false); renderInspector()
    addLog(`材质预设已加入工程：${item.name}`, 'success')
  } catch (error) { toast('新增材质属性失败', error.message, 'error') }
  finally { submit.disabled = false }
}

async function saveImportedObject(event) {
  event.preventDefault()
  const pending = state.pendingObject
  if (!pending) return
  const submit = $('#materialForm button[type="submit"]')
  try {
    submit.disabled = true
    ensureProjectState()
    const values = Object.fromEntries(new FormData($('#materialForm')).entries())
    const imported = await api.importObj({ name: `${uniqueName(values.objectName, 'object')}.obj`, content: pending.source.content })
    const distribution = defaultDistribution({ type: values.objectType })
    const instances = generateDistribution(distribution)
    const savedDistribution = await api.saveDistribution({ path: imported.positionPath, name: values.objectName, instances })
    const spectrum = { name: values.spectralName, model: values.spectralModel, reflectance: values.reflectance, transmittance: values.transmittance, refTir: Number(values.refTir), tauTir: Number(values.tauTir), params: { Cab: values.paramCab, Cw: values.paramCw, Cdm: values.paramCdm, Cs: values.paramCs, N: values.paramN, SMC: values.paramSMC, BSMBrightness: values.paramBSMBrightness, BSMlat: values.paramBSMlat, BSMlon: values.paramBSMlon } }
    const thermal = { name: values.thermalName, sunlitTemperature: Number(values.sunlitTemperature), shadedTemperature: Number(values.shadedTemperature) }
    const spectralSelections = $$('.mesh-spectrum'), thermalSelections = $$('.mesh-thermal')
    const physicalType = physicalTypeForObject(values.objectType)
    const physical = state.config.materials.find((item) => item.name === values.materialPreset) || physicalFromForm(values, physicalType)
    if (spectralSelections.some((select) => select.value === '__new__')) upsertByName(state.config.spectra, spectrum)
    if (thermalSelections.some((select) => select.value === '__new__')) upsertByName(state.config.thermals, thermal)
    if (values.materialPreset === '__new__') upsertByName(state.config.materials, physical)
    const meshes = pending.meshNames.map((name, index) => ({ name, spectralName: spectralSelections[index].value === '__new__' ? spectrum.name : spectralSelections[index].value, thermalName: thermalSelections[index].value === '__new__' ? thermal.name : thermalSelections[index].value }))
    if (!physical?.name || !state.config.materials.some((entry) => entry.name === physical.name)) throw new Error('物化属性未定义')
    for (const mesh of meshes) {
      if (!state.config.spectra.some((entry) => entry.name === mesh.spectralName)) throw new Error('光谱属性不存在：' + mesh.spectralName)
      if (!state.config.thermals.some((entry) => entry.name === mesh.thermalName)) throw new Error('温度属性不存在：' + mesh.thermalName)
    }
    const item = { name: values.objectName, type: values.objectType, materialName: physical.name, canopyName: 'canopy_default', fileName: imported.path, positionFile: savedDistribution.path, shape: 'cube', dimensions: pending.dimensions, meshes, distribution, instanceCount: instances.length }
    const existing = state.config.objects.items.findIndex((entry) => entry.name === item.name)
    if (existing >= 0) state.config.objects.items[existing] = item
    else state.config.objects.items.push(item)
    const itemIndex = existing >= 0 ? existing : state.config.objects.items.length - 1
    state.config.objects.count = state.config.objects.items.length
    state.config.objects.names = state.config.objects.items.map((entry) => entry.name)
    state.project.configuration = state.config
    state.xmlDirty = true
    if (!(await saveXml(true))) throw new Error('工程配置保存失败')

    disposeObject(pending.object); state.pendingObject = null
    hideMaterialDialog(false); refreshFromState(false); await loadActualScene(state.config)
    state.panel = 'objects'; state.selectedObjectIndex = itemIndex
    $$('.tree-item').forEach((button) => button.classList.toggle('active', button.dataset.panel === 'objects'))
    renderInspector(); await showObjectDialog(itemIndex)
    addLog(`OBJ 属性已加入工程：${imported.path}`, 'success'); toast('OBJ 属性配置完成', `${item.name} · 请继续设置实例分布`)
  } catch (error) { toast('OBJ 材质保存失败', error.message, 'error') }
  finally { submit.disabled = false }
}


function bindingOptionList(items, selected, fallback) {
  if (!items.length) return '<option value="">无可用属性</option>'
  const selectedName = items.some((item) => item.name === selected) ? selected : (items.some((item) => item.name === fallback) ? fallback : items[0].name)
  return items.map((item) => `<option value="${escapeHtml(item.name)}" ${item.name === selectedName ? 'selected' : ''}>${escapeHtml(item.label || item.name)} · ${escapeHtml(item.name)}</option>`).join('')
}

function renderObjectAttributeSummary() {
  const form = $('#objectAttributeForm')
  const material = state.config?.materials?.find((item) => item.name === form.elements.materialName.value)
  const summary = material ? physicalValues(material).map(([key, value]) => `${key}=${value}`).join('，') : '未找到物化属性定义'
  $('#objectAttributeSummary').textContent = material ? `${material.label || material.name} · ${summary}` : summary
}

function renderObjectAttributeOptions(useCurrent = false) {
  const item = state.config?.objects?.items?.[state.editingAttributeIndex]
  if (!item) return
  const form = $('#objectAttributeForm')
  const type = form.elements.objectType.value
  const physicalType = physicalTypeForObject(type)
  const materials = state.config.materials.filter((entry) => entry.type === physicalType)
  const preferredMaterial = { Vegetation: 'leaf_c3', Soil: 'soilset', Building: 'soil_dry', Other: 'soilset', Water: 'water_set' }[type]
  const currentMaterial = useCurrent ? form.elements.materialName.value : item.materialName
  form.elements.materialName.innerHTML = bindingOptionList(materials, currentMaterial, preferredMaterial)
  form.elements.canopyName.innerHTML = bindingOptionList(state.config.canopies || [], item.canopyName, 'canopy_default')

  const currentMeshes = useCurrent
    ? $$('#objectAttributeMeshRows tr').map((row) => ({ name: row.dataset.meshName, spectralName: row.querySelector('.attribute-spectrum').value, thermalName: row.querySelector('.attribute-thermal').value }))
    : (item.meshes?.length ? item.meshes : [{ name: item.name, spectralName: item.spectralName, thermalName: item.thermalName }])
  const preferredSpectrum = { Vegetation: 'green_leaf', Soil: 'soil', Building: 'concrete', Other: 'concrete', Water: 'water_surface' }[type]
  const preferredThermal = { Vegetation: 'vegetation_temperature', Soil: 'soil_temperature', Building: 'building_temperature', Other: 'building_temperature', Water: 'water_temperature' }[type]
  $('#objectAttributeMeshRows').innerHTML = currentMeshes.map((mesh) => `<tr data-mesh-name="${escapeHtml(mesh.name)}"><td title="${escapeHtml(mesh.name)}">${escapeHtml(mesh.name)}</td><td><select class="select attribute-spectrum">${bindingOptionList(state.config.spectra, mesh.spectralName, document.querySelector("#spectralPreset").value || preferredSpectrum)}</select></td><td><select class="select attribute-thermal">${bindingOptionList(state.config.thermals, mesh.thermalName, document.querySelector("#thermalPreset").value || preferredThermal)}</select></td></tr>`).join('')
  renderObjectAttributeSummary()
}

function showObjectAttributeDialog(index) {
  const item = state.config?.objects?.items?.[index]
  if (!item) return
  state.editingAttributeIndex = index
  state.selectedObjectIndex = index
  const form = $('#objectAttributeForm')
  form.reset()
  form.elements.objectName.value = item.name
  form.elements.objectType.value = item.type || 'Other'
  form.elements.objectPath.value = item.fileName || ''
  renderObjectAttributeOptions(false)
  $$('.object-card').forEach((card) => card.classList.toggle('active', Number(card.dataset.index) === index))
  $('#objectAttributeDialog').hidden = false
}

function hideObjectAttributeDialog() {
  $('#objectAttributeDialog').hidden = true
  state.editingAttributeIndex = -1
}

async function saveObjectAttributes(event) {
  event.preventDefault()
  const index = state.editingAttributeIndex
  const item = state.config?.objects?.items?.[index]
  if (!item) return
  const form = $('#objectAttributeForm')
  const submit = form.querySelector('button[type="submit"]')
  const type = form.elements.objectType.value
  const materialName = form.elements.materialName.value
  const canopyName = form.elements.canopyName.value
  const material = state.config.materials.find((entry) => entry.name === materialName)
  const canopy = state.config.canopies.find((entry) => entry.name === canopyName)
  const meshes = $$('#objectAttributeMeshRows tr').map((row) => ({
    name: row.dataset.meshName,
    spectralName: row.querySelector('.attribute-spectrum').value,
    thermalName: row.querySelector('.attribute-thermal').value
  }))
  try {
    submit.disabled = true
    if (!material || material.type !== physicalTypeForObject(type)) throw new Error('所选物化属性与对象类型不匹配')
    if (!canopy) throw new Error('所选冠层属性不存在')
    if (!meshes.length) throw new Error('OBJ 没有可配置的 Mesh')
    for (const mesh of meshes) {
      if (!state.config.spectra.some((entry) => entry.name === mesh.spectralName)) throw new Error('光谱属性不存在：' + mesh.spectralName)
      if (!state.config.thermals.some((entry) => entry.name === mesh.thermalName)) throw new Error('温度属性不存在：' + mesh.thermalName)
    }
    const previous = { type: item.type, materialName: item.materialName, canopyName: item.canopyName, meshes: item.meshes }
    Object.assign(item, { type, materialName, canopyName, meshes })
    ensureProjectState().configuration = state.config
    state.xmlDirty = true
    if (!(await saveXml(true))) { Object.assign(item, previous); throw new Error('工程配置保存失败') }
    hideObjectAttributeDialog()
    await loadActualScene(state.config)
    refreshFromState(false); renderInspector()
    addLog(`OBJ 属性已更新：${item.name}`, 'success')
    toast('OBJ 属性已更新', item.name)
  } catch (error) { toast('OBJ 属性保存失败', error.message, 'error') }
  finally { submit.disabled = false }
}

function syncDistributionPopulation(form, source = 'count') {
  const mode = form.elements.distributionMode.value
  if (mode !== 'random' && mode !== 'grid') return
  const minX = finiteNumber(form.elements.distributionMinX.value, 0)
  const maxX = finiteNumber(form.elements.distributionMaxX.value, 1)
  const minY = finiteNumber(form.elements.distributionMinY.value, 0)
  const maxY = finiteNumber(form.elements.distributionMaxY.value, 1)
  const area = distributionArea({ minX, maxX, minY, maxY })
  const population = source === 'density'
    ? populationFromValues(form.elements.distributionCount.value, form.elements.distributionDensity.value, area, 'density')
    : populationFromValues(form.elements.distributionCount.value, form.elements.distributionDensity.value, area, 'count')
  form.elements.distributionBasis.value = source === 'density' ? 'density' : 'count'
  form.elements.distributionCount.value = population.count
  form.elements.distributionDensity.value = distributionDensityInputValue(population.density)
}

function updateDistributionFormState(form) {
  const mode = form.elements.distributionMode.value
  const basis = form.elements.distributionBasis.value
  const usesPopulation = mode === 'random' || mode === 'grid'
  form.elements.distributionBasis.disabled = !usesPopulation
  form.elements.distributionCount.disabled = !usesPopulation
  form.elements.distributionDensity.disabled = !usesPopulation
  form.elements.distributionPoints.closest('.distribution-points').hidden = mode !== 'manual'

  let detail
  if (mode === 'manual') {
    const count = String(form.elements.distributionPoints.value || '').split(/\r?\n/).map((line) => line.trim()).filter(Boolean).length
    detail = `自定义坐标 · ${count} 个实例`
  } else if (mode === 'single') detail = '单实例 · 位于当前分布范围中心'
  else {
    const distribution = distributionFromForm(form)
    distribution.basis = basis
    distribution.count = finiteNumber(form.elements.distributionCount.value, distribution.count)
    distribution.density = finiteNumber(form.elements.distributionDensity.value, distribution.density)
    const count = distributionTargetCount(distribution)
    detail = `${mode === 'grid' ? '规则网格' : '纯随机'} · 将生成 ${count} 个实例`
  }
  $('#objectDistributionStatus').textContent = `${detail}；保存后覆盖该 OBJ 自己的位置文件。`
}

async function showObjectDialog(index) {
  const item = state.config?.objects?.items?.[index]
  if (!item) return
  state.editingObjectIndex = index
  const form = $('#objectForm')
  form.reset()
  form.elements.objectName.value = item.name
  form.elements.objectType.value = item.type
  form.elements.objectPath.value = item.fileName || ''
  let distribution = item.distribution
  if (!distribution && item.positionFile) {
    try {
      const result = await api.readText(item.positionFile)
      distribution = { ...defaultDistribution(item), mode: 'manual', points: result.content.trim() }
    } catch (error) { toast('位置文件读取失败', error.message, 'error') }
  }
  state.selectedObjectIndex = index
  setDistributionForm(form, distribution || defaultDistribution(item))
  updateDistributionFormState(form)
  renderInspector()
  $('#objectDialog').hidden = false
}

function hideObjectDialog() {
  $('#objectDialog').hidden = true
  state.editingObjectIndex = -1
}

async function saveSceneObject(event) {
  event.preventDefault()
  const index = state.editingObjectIndex
  const item = state.config?.objects?.items?.[index]
  if (!item) return
  const submit = $('#objectForm button[type="submit"]')
  try {
    submit.disabled = true
    const form = $('#objectForm')
    const nextName = String(form.elements.objectName.value).trim()
    if (!nextName) throw new Error('对象名称不能为空')
    if (state.config.objects.items.some((entry, itemIndex) => itemIndex !== index && entry.name === nextName)) throw new Error(`对象名称已存在：${nextName}`)
    const distribution = distributionFromForm(form, { requireValidDensity: true })
    const instances = generateDistribution(distribution)
    const saved = await api.saveDistribution({ path: item.positionFile, name: form.elements.objectName.value, instances })
    item.name = nextName
    item.positionFile = saved.path
    item.distribution = distribution
    item.instanceCount = instances.length
    state.config.objects.count = state.config.objects.items.length
    state.config.objects.names = state.config.objects.items.map((entry) => entry.name)
    ensureProjectState().configuration = state.config
    state.xmlDirty = true
    if (!(await saveXml(true))) throw new Error('工程配置保存失败')
    hideObjectDialog()
    await loadActualScene(state.config)
    refreshFromState(false); renderInspector()
    addLog(`对象分布已更新：${item.name} · ${instances.length} 个实例`, 'success')
    toast('对象已更新', `${item.name} · ${instances.length} 个实例`)
  } catch (error) { toast('对象修改失败', error.message, 'error') }
  finally { submit.disabled = false }
}

async function deleteSceneObject(index) {
  const item = state.config?.objects?.items?.[index]
  if (!item) return
  const previousSelectedIndex = state.selectedObjectIndex
  state.config.objects.items.splice(index, 1)
  if (state.selectedObjectIndex === index) state.selectedObjectIndex = -1
  else if (state.selectedObjectIndex > index) state.selectedObjectIndex -= 1
  state.config.objects.count = state.config.objects.items.length
  state.config.objects.names = state.config.objects.items.map((entry) => entry.name)
  try {
    ensureProjectState().configuration = state.config
    state.xmlDirty = true
    if (!(await saveXml(true))) throw new Error('工程配置保存失败')
    await loadActualScene(state.config)
    refreshFromState(false); renderInspector()
    addLog(`已从场景删除对象：${item.name}`, 'success')
    toast('对象已删除', item.name)
  } catch (error) {
    state.config.objects.items.splice(index, 0, item)
    state.selectedObjectIndex = previousSelectedIndex
    state.config.objects.count = state.config.objects.items.length
    state.config.objects.names = state.config.objects.items.map((entry) => entry.name)
    toast('删除对象失败', error.message, 'error')
  }
}

function builtInOutputPath() {
  const name = { eVoxelEB: 'minimal_voxeleb', eFacetEB: 'minimal_voxeleb', eFacetRT: 'radiosity_gpu', eVoxelRT: 'minimal_voxelrt', eRaytracing: 'minimal_raytracing' }[state.mode]
  return `streamsim/models/histream/examples/${name}`
}

async function runSimulation() {
  if (!state.inputPath) { toast("无法运行", "请先打开任意场景的 Input.xml", "error"); return }
  if (state.running || (state.xmlDirty && !(await saveXml()))) return
  try {
    state.startedAt = Date.now(); state.outputDir = state.config?.outDir || state.outputDir
    const response = await api.run({ mode: state.mode, executable: state.executable, inputPath: state.inputPath })
    state.pid = response.pid; setRunning(true)
  } catch (error) { addLog(error.message, 'error'); toast('无法启动模拟', error.message, 'error'); setRunning(false) }
}

function handleSimulationEvent(event) {
  if (event.type === 'started') { state.pid = event.pid; setRunning(true); addLog(`启动进程 PID ${event.pid}`, 'success'); addLog(event.command, 'command') }
  else if (event.type === 'stdout') addLog(event.text)
  else if (event.type === 'stderr' || event.type === 'error') addLog(event.text, 'error')
  else if (event.type === 'closed') {
    const success = event.code === 0
    const engineName = state.mode === 'eFacetRT' || state.mode === 'eFacetEB' ? 'HiStream Facet' : 'HiStream'
    addLog(success ? `模拟完成，耗时 ${(event.elapsed / 1000).toFixed(2)} 秒` : `模拟已结束，退出码 ${event.code}${event.signal ? `，信号 ${event.signal}` : ''}`, success ? 'success' : 'error')
    toast(success ? '模拟完成' : '模拟结束', success ? `${engineName} 已正常完成计算` : `退出码：${event.code}`, success ? '' : 'error'); setRunning(false)
    if (success) {
      state.outputDir = state.config?.outDir || state.outputDir
      showResults().catch((error) => {
        $('#resultStatus').textContent = error.message
        toast('结果读取失败', error.message, 'error')
      })
    }
  }
}

function formatBytes(value) {
  if (value < 1024) return `${value} B`
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`
  return `${(value / 1024 / 1024).toFixed(1)} MB`
}


function emptyResultPreview(title, message) {
  $('#resultPreview').innerHTML = `<div class="empty-editor"><svg><use href="#i-layers"/></svg><h3>${escapeHtml(title)}</h3><p>${escapeHtml(message)}</p></div>`
}

function decodeResultPixels(result) {
  const source = atob(result.pixels || '')
  return Uint8Array.from(source, (character) => character.charCodeAt(0))
}

function grayscaleResultPixels(bytes, width, height) {
  const canvas = document.createElement('canvas')
  canvas.width = width; canvas.height = height
  const context = canvas.getContext('2d')
  const image = context.createImageData(width, height)
  for (let index = 0; index < bytes.length; index += 1) {
    const value = bytes[index]
    const offset = index * 4
    image.data[offset] = value; image.data[offset + 1] = value; image.data[offset + 2] = value; image.data[offset + 3] = 255
  }
  context.putImageData(image, 0, 0)
  return canvas
}

function resultRasterFromEnvi(result) {
  const width = result.previewWidth, height = result.previewHeight
  const bytes = decodeResultPixels(result)
  return { width, height, bytes, textureCanvas: grayscaleResultPixels(bytes, width, height) }
}

function resultViewActions() {
  return '<div class="result-view-actions"><button class="button ghost result-2d-btn" id="result2dBtn" type="button">二维图像</button><button class="button ghost result-3d-btn" id="result3dBtn" type="button">三维显示</button></div>'
}

function resultLayerList(result) {
  if (!result || result.bands <= 1) return ''
  const names = result.bandNames || []
  return `<div class="result-layer-list" aria-label="波段图层">${Array.from({ length: result.bands }, (_, index) => `<button class="result-layer ${index === result.band ? 'active' : ''}" type="button" data-band="${index}"><span>图层 ${index + 1}</span><small>${escapeHtml(names[index] || `波段 ${index + 1}`)}</small></button>`).join('')}</div>`
}

function bindResultLayers(file) {
  $$('.result-layer').forEach((button) => button.addEventListener('click', () => openResultFile(file, Number(button.dataset.band))))
}

function bindResultViewActions(result, file, raster, image = false) {
  $('#result3dBtn')?.addEventListener('click', () => showResult3D(raster, file, image ? result : null))
  $('#result2dBtn')?.addEventListener('click', () => image ? drawImagePreview(result, file) : result.kind === 'tiff' ? drawTiffPreview(result, file) : drawEnviPreview(result, file))
}

function drawEnviPreview(result, file, format = 'ENVI') {
  disposeResultViewer()
  const raster = resultRasterFromEnvi(result)
  state.resultRaster = raster
  $('#resultPreview').innerHTML = `<div class="result-preview-card"><div class="result-image-stage"><canvas class="result-canvas" id="resultCanvas" width="${result.previewWidth}" height="${result.previewHeight}"></canvas></div>${resultLayerList(result)}${resultViewActions()}<div class="result-meta"><span>${result.width} × ${result.height}</span><span>${format} · ${result.bands} 个独立波段图层</span><span>当前图层：${escapeHtml(result.bandNames?.[result.band] || `波段 ${result.band + 1}`)}</span><span>MIN ${Number(result.minimum).toPrecision(6)}</span><span>MAX ${Number(result.maximum).toPrecision(6)}</span></div></div>`
  const canvas = $('#resultCanvas')
  canvas.getContext('2d').drawImage(raster.textureCanvas, 0, 0)
  bindResultLayers(file)
  bindResultViewActions(result, file, raster)
}

function drawTiffPreview(result, file) {
  drawEnviPreview(result, file, 'TIFF')
}

function rasterFromImage(image) {
  const scale = Math.min(1, 768 / Math.max(image.naturalWidth, image.naturalHeight, 1))
  const width = Math.max(1, Math.round(image.naturalWidth * scale))
  const height = Math.max(1, Math.round(image.naturalHeight * scale))
  const canvas = document.createElement('canvas')
  canvas.width = width; canvas.height = height
  const context = canvas.getContext('2d', { willReadFrequently: true })
  context.drawImage(image, 0, 0, width, height)
  const source = context.getImageData(0, 0, width, height).data
  const bytes = new Uint8Array(width * height)
  for (let index = 0; index < bytes.length; index += 1) {
    const offset = index * 4
    bytes[index] = Math.round(source[offset] * .2126 + source[offset + 1] * .7152 + source[offset + 2] * .0722)
  }
  return { width, height, bytes, textureCanvas: canvas }
}

async function drawImagePreview(result, file) {
  disposeResultViewer()
  state.resultRaster = null
  const imageUrl = `data:${result.mimeType};base64,${result.data}`
  $('#resultPreview').innerHTML = `<div class="result-preview-card"><div class="result-image-stage"><img class="result-image" id="resultImage" alt="${escapeHtml(file.name)}"></div>${resultViewActions()}<div class="result-meta"><span>${escapeHtml(file.name)}</span><span>${escapeHtml(result.mimeType)}</span></div></div>`
  const image = $('#resultImage')
  await new Promise((resolve, reject) => {
    image.onload = () => resolve()
    image.onerror = () => reject(new Error('图像结果无法解码'))
    image.src = imageUrl
  })
  const raster = rasterFromImage(image)
  state.resultRaster = raster
  bindResultViewActions(result, file, raster, true)
}

function disposeResultViewer() {
  if (!resultViewer) return
  resultViewer.resizeObserver?.disconnect()
  resultViewer.controls.dispose()
  resultViewer.renderer.dispose()
  resultViewer.scene.traverse((object) => {
    object.geometry?.dispose?.()
    if (Array.isArray(object.material)) object.material.forEach((material) => material.dispose?.())
    else object.material?.dispose?.()
  })
  resultViewer.texture?.dispose?.()
  resultViewer = null
}

function resizeResultViewer() {
  if (!resultViewer) return
  const host = resultViewer.host
  const width = Math.max(1, host.clientWidth), height = Math.max(1, host.clientHeight)
  resultViewer.renderer.setSize(width, height, false)
  resultViewer.camera.aspect = width / height
  resultViewer.camera.updateProjectionMatrix()
}

function renderResultViewer() {
  if (!resultViewer || $('#resultDialog').hidden) return
  resultViewer.controls.update()
  resultViewer.renderer.render(resultViewer.scene, resultViewer.camera)
}

function showResult3D(raster, file, sourceResult = null) {
  if (!raster) return
  disposeResultViewer()
  const rasterResult = ['envi', 'tiff'].includes(sourceResult?.kind) ? sourceResult : null
  const bandLayers = rasterResult ? resultLayerList(rasterResult) : ''
  $('#resultPreview').innerHTML = `<div class="result-preview-card result-3d-card"><div class="result-3d-viewport" id="result3dViewport"></div>${resultViewActions()}${bandLayers}<div class="result-meta"><span>${raster.width} × ${raster.height}</span><span>整幅图像三维显示</span><span>高度按像素亮度映射</span></div></div>`
  const host = $('#result3dViewport')
  const resultScene = new THREE.Scene()
  resultScene.background = new THREE.Color(0x07100e)
  resultScene.add(new THREE.HemisphereLight(0xc8e8dc, 0x14211b, 1.8))
  const resultSun = new THREE.DirectionalLight(0xfff0d0, 3)
  resultSun.position.set(8, 14, 10)
  resultScene.add(resultSun)
  const camera3d = new THREE.PerspectiveCamera(42, 1, .1, 120)
  camera3d.position.set(15, 13, 16)
  const controls3d = new OrbitControls(camera3d, host)
  controls3d.enableDamping = true
  controls3d.dampingFactor = .08
  controls3d.maxPolarAngle = Math.PI * .48
  controls3d.minDistance = 5
  controls3d.maxDistance = 55
  controls3d.target.set(0, 1.2, 0)

  const columns = Math.max(2, Math.min(180, raster.width))
  const rows = Math.max(2, Math.min(180, raster.height))
  const width = 16
  const depth = width * raster.height / Math.max(1, raster.width)
  const positions = [], uvs = [], indices = []
  for (let row = 0; row < rows; row += 1) {
    const sy = Math.round(row * (raster.height - 1) / Math.max(1, rows - 1))
    for (let column = 0; column < columns; column += 1) {
      const sx = Math.round(column * (raster.width - 1) / Math.max(1, columns - 1))
      const brightness = raster.bytes[sy * raster.width + sx] / 255
      positions.push((column / (columns - 1) - .5) * width, .08 + brightness * 4.12, (.5 - row / (rows - 1)) * depth)
      uvs.push(column / (columns - 1), 1 - row / (rows - 1))
    }
  }
  for (let row = 0; row < rows - 1; row += 1) for (let column = 0; column < columns - 1; column += 1) {
    const a = row * columns + column, b = a + 1, c = a + columns, d = c + 1
    indices.push(a, c, b, b, c, d)
  }
  const geometry = new THREE.BufferGeometry()
  geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3))
  geometry.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2))
  geometry.setIndex(indices)
  geometry.computeVertexNormals()
  const texture = new THREE.CanvasTexture(raster.textureCanvas)
  texture.colorSpace = THREE.SRGBColorSpace
  texture.minFilter = THREE.NearestFilter
  texture.magFilter = THREE.NearestFilter
  texture.generateMipmaps = false
  const surface = new THREE.Mesh(geometry, new THREE.MeshStandardMaterial({ map: texture, roughness: .88, metalness: .02, side: THREE.DoubleSide }))
  surface.castShadow = surface.receiveShadow = true
  resultScene.add(surface)
  const base = new THREE.Mesh(new THREE.PlaneGeometry(width, depth), new THREE.MeshBasicMaterial({ color: 0x13251f, transparent: true, opacity: .8, side: THREE.DoubleSide }))
  base.rotation.x = -Math.PI / 2; base.position.y = -.06
  resultScene.add(base)
  const resultGrid = new THREE.GridHelper(Math.max(width, depth), 16, 0x5f9b83, 0x274638)
  resultGrid.position.y = -.04; resultGrid.material.opacity = .25; resultGrid.material.transparent = true
  resultScene.add(resultGrid)
  resultViewer = { host, scene: resultScene, camera: camera3d, controls: controls3d, renderer: new THREE.WebGLRenderer({ antialias: true, alpha: true }), texture }
  resultViewer.renderer.setPixelRatio(Math.min(devicePixelRatio, 2))
  resultViewer.renderer.outputColorSpace = THREE.SRGBColorSpace
  resultViewer.renderer.toneMapping = THREE.ACESFilmicToneMapping
  resultViewer.renderer.toneMappingExposure = 1.05
  host.append(resultViewer.renderer.domElement)
  resultViewer.resizeObserver = new ResizeObserver(resizeResultViewer)
  resultViewer.resizeObserver.observe(host)
  resizeResultViewer()
  controls3d.update()
  $('#result3dBtn').classList.add('active')
  if (rasterResult) bindResultLayers(file)
  $('#result2dBtn').addEventListener('click', () => rasterResult ? (rasterResult.kind === 'tiff' ? drawTiffPreview(rasterResult, file) : drawEnviPreview(rasterResult, file)) : drawImagePreview(sourceResult, file))

}

function facetControls(result, metric, side) {
  const metrics = result.metrics || []
  const metricButtons = metrics.map((item) => `<button class="button ghost facet-metric ${item.id === metric ? 'active' : ''}" type="button" data-metric="${escapeHtml(item.id)}">${escapeHtml(item.label)}</button>`).join('')
  return `<div class="facet-controls"><div><span>显示指标</span>${metricButtons}</div><div><span>面方向</span><button class="button ghost facet-side ${side === 'both' ? 'active' : ''}" type="button" data-side="both">正反两面</button><button class="button ghost facet-side ${side === 'front' ? 'active' : ''}" type="button" data-side="front">正面</button><button class="button ghost facet-side ${side === 'back' ? 'active' : ''}" type="button" data-side="back">背面</button></div></div>`
}

function facetIndices(result, side) {
  const total = result.facetCount * 2
  if (side === 'front') return Array.from({ length: result.facetCount }, (_, index) => index * 2)
  if (side === 'back') return Array.from({ length: result.facetCount }, (_, index) => index * 2 + 1)
  return Array.from({ length: total }, (_, index) => index)
}

function facetStats(values, indices) {
  let minimum = Infinity, maximum = -Infinity
  for (const index of indices) {
    const value = Number(values[index])
    if (Number.isFinite(value)) { minimum = Math.min(minimum, value); maximum = Math.max(maximum, value) }
  }
  if (!Number.isFinite(minimum)) return { minimum: 0, maximum: 0 }
  return { minimum, maximum }
}

function facetMetric(result, metric) {
  return result.metrics.find((item) => item.id === metric) || result.metrics[0]
}

function bindFacetControls(result, file, metric, side) {
  $$('.facet-metric').forEach((button) => button.addEventListener('click', () => drawFacetPreview(result, file, button.dataset.metric, side)))
  $$('.facet-side').forEach((button) => button.addEventListener('click', () => drawFacetPreview(result, file, metric, button.dataset.side)))
  $('#facet3dBtn')?.addEventListener('click', () => showFacet3D(result, file, metric, side))
  $('#facet2dBtn')?.addEventListener('click', () => drawFacetPreview(result, file, metric, side))
}

function facetViewActions() {
  return '<div class="result-view-actions"><button class="button ghost facet-2d-btn" id="facet2dBtn" type="button">面元热图</button><button class="button ghost facet-3d-btn" id="facet3dBtn" type="button">面元三维</button></div>'
}

function drawFacetPreview(result, file, metric = 'radiosity', side = 'both') {
  disposeResultViewer()
  const selected = facetMetric(result, metric)
  const indices = facetIndices(result, side)
  const stats = facetStats(selected.values, indices)
  const columns = Math.max(1, Math.ceil(Math.sqrt(indices.length)))
  const rows = Math.max(1, Math.ceil(indices.length / columns))
  const width = Math.min(768, columns), height = Math.max(1, Math.ceil(rows * width / columns))
  $('#resultPreview').innerHTML = `<div class="result-preview-card"><div class="facet-image-stage"><canvas class="result-canvas" id="facetCanvas" width="${width}" height="${height}"></canvas></div>${facetControls(result, selected.id, side)}${facetViewActions()}<div class="result-meta"><span>${result.facetCount.toLocaleString('zh-CN')} 个面 · ${indices.length.toLocaleString('zh-CN')} 个面值</span><span>${escapeHtml(selected.label)} · ${side === 'front' ? '正面' : side === 'back' ? '背面' : '正反两面'}</span><span>MIN ${Number(stats.minimum).toPrecision(6)}</span><span>MAX ${Number(stats.maximum).toPrecision(6)}</span><span>后端 ${escapeHtml(result.backend || 'HiStream')}</span></div></div>`
  const canvas = $('#facetCanvas')
  const context = canvas.getContext('2d')
  const range = stats.maximum - stats.minimum
  context.fillStyle = '#08120f'; context.fillRect(0, 0, width, height)
  indices.forEach((index, position) => {
    const value = Number(selected.values[index])
    const normalized = Number.isFinite(value) ? (range ? (value - stats.minimum) / range : .5) : 0
    context.fillStyle = Number.isFinite(value) ? `hsl(${(1 - normalized) * 225}, 82%, 52%)` : '#111f1a'
    const x = Math.floor((position % columns) * width / columns)
    const y = Math.floor(Math.floor(position / columns) * height / rows)
    const nextX = Math.ceil((position % columns + 1) * width / columns)
    const nextY = Math.ceil((Math.floor(position / columns) + 1) * height / rows)
    context.fillRect(x, y, Math.max(1, nextX - x), Math.max(1, nextY - y))
  })
  bindFacetControls(result, file, selected.id, side)
}

function showFacet3D(result, file, metric = 'radiosity', side = 'both') {
  disposeResultViewer()
  const selected = facetMetric(result, metric)
  const indices = facetIndices(result, side)
  const stats = facetStats(selected.values, indices)
  const rawPositions = Array.isArray(result.vertexPositions) ? result.vertexPositions : []
  const hasGeometry = rawPositions.length >= result.facetCount * 9
  $('#resultPreview').innerHTML = `<div class="result-preview-card result-3d-card"><div class="result-3d-viewport" id="result3dViewport"></div>${facetControls(result, selected.id, side)}${facetViewActions()}<div class="result-meta"><span>${result.facetCount.toLocaleString('zh-CN')} 个面 · ${indices.length.toLocaleString('zh-CN')} 个面值</span><span>颜色：${escapeHtml(selected.label)}</span><span>正反两面数据独立显示</span><span>MIN ${Number(stats.minimum).toPrecision(6)} · MAX ${Number(stats.maximum).toPrecision(6)}</span>${hasGeometry ? '<span>真实 OBJ 面元空间位置</span>' : '<span>旧结果无几何坐标，使用面元序号排列</span>'}</div></div>`
  const host = $('#result3dViewport')
  const resultScene = new THREE.Scene()
  resultScene.background = new THREE.Color(0x07100e)
  resultScene.add(new THREE.HemisphereLight(0xc8e8dc, 0x14211b, 1.8))
  const resultSun = new THREE.DirectionalLight(0xfff0d0, 3)
  resultSun.position.set(8, 14, 10); resultScene.add(resultSun)
  const camera3d = new THREE.PerspectiveCamera(42, 1, .01, 300)
  const controls3d = new OrbitControls(camera3d, host)
  controls3d.enableDamping = true; controls3d.dampingFactor = .08; controls3d.maxPolarAngle = Math.PI * .49
  controls3d.minDistance = .5; controls3d.maxDistance = 300

  const range = stats.maximum - stats.minimum
  const valueColor = (value) => {
    const normalized = Number.isFinite(value) ? (range ? (value - stats.minimum) / range : .5) : 0
    return { normalized, color: new THREE.Color().setHSL((1 - normalized) * .67, Number.isFinite(value) ? .82 : 0, Number.isFinite(value) ? .5 : .16) }
  }

  if (hasGeometry) {
    let minimum = [Infinity, Infinity, Infinity], maximum = [-Infinity, -Infinity, -Infinity]
    for (let index = 0; index < result.facetCount * 9; index += 3) {
      for (let axis = 0; axis < 3; axis += 1) {
        const value = Number(rawPositions[index + axis])
        if (!Number.isFinite(value)) continue
        minimum[axis] = Math.min(minimum[axis], value); maximum[axis] = Math.max(maximum[axis], value)
      }
    }
    const spanX = Math.max(.001, maximum[0] - minimum[0]), spanY = Math.max(.001, maximum[1] - minimum[1]), spanZ = Math.max(.001, maximum[2] - minimum[2])
    const modelScale = 20 / Math.max(spanX, spanY, spanZ)
    const centerX = (minimum[0] + maximum[0]) / 2, centerZ = (minimum[2] + maximum[2]) / 2
    const toWorld = (index) => new THREE.Vector3((Number(rawPositions[index]) - centerX) * modelScale, (Number(rawPositions[index + 1]) - minimum[1]) * modelScale, (Number(rawPositions[index + 2]) - centerZ) * modelScale)
    const positions = [], colors = []
    const addFace = (facet, localSide) => {
      const valueIndex = facet * 2 + localSide
      const value = Number(selected.values[valueIndex])
      const { color } = valueColor(value)
      const offset = localSide === 0 ? .002 : -.002
      const a = toWorld(facet * 9), b = toWorld(facet * 9 + 3), c = toWorld(facet * 9 + 6)
      const normal = new THREE.Triangle(a, b, c).getNormal(new THREE.Vector3()).multiplyScalar(offset)
      const triangle = localSide === 0 ? [a, b, c] : [a, c, b]
      for (const point of triangle) {
        const shifted = point.clone().add(normal)
        positions.push(shifted.x, shifted.y, shifted.z)
        colors.push(color.r, color.g, color.b)
      }
    }
    indices.forEach((index) => addFace(Math.floor(index / 2), index % 2))
    const geometry = new THREE.BufferGeometry()
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3))
    geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3))
    geometry.computeVertexNormals()
    const surface = new THREE.Mesh(geometry, new THREE.MeshStandardMaterial({ vertexColors: true, roughness: .86, metalness: .02, side: THREE.DoubleSide }))
    surface.castShadow = surface.receiveShadow = true
    resultScene.add(surface)
    const groundWidth = Math.max(.5, spanX * modelScale), groundDepth = Math.max(.5, spanZ * modelScale)
    const base = new THREE.Mesh(new THREE.PlaneGeometry(groundWidth * 1.08, groundDepth * 1.08), new THREE.MeshBasicMaterial({ color: 0x13251f, transparent: true, opacity: .8, side: THREE.DoubleSide }))
    base.rotation.x = -Math.PI / 2; base.position.y = -.04; resultScene.add(base)
    const resultGrid = new THREE.GridHelper(Math.max(groundWidth, groundDepth) * 1.08, 22, 0x5f9b83, 0x274638)
    resultGrid.position.y = -.02; resultGrid.scale.z = groundDepth / Math.max(groundWidth, groundDepth); resultGrid.scale.x = groundWidth / Math.max(groundWidth, groundDepth); resultGrid.material.opacity = .25; resultGrid.material.transparent = true; resultScene.add(resultGrid)
    camera3d.position.set(Math.max(16, groundWidth * 1.25), Math.max(12, spanY * modelScale * .85 + 8), Math.max(16, groundDepth * 1.25))
    controls3d.target.set(0, Math.max(.5, spanY * modelScale * .35), 0)
  } else {
    const columns = Math.max(1, Math.ceil(Math.sqrt(indices.length)))
    const rows = Math.max(1, Math.ceil(indices.length / columns))
    const sideLength = 22, cell = sideLength / Math.max(columns, rows)
    const geometry = new THREE.BoxGeometry(Math.max(.04, cell * .72), 1, Math.max(.04, cell * .72))
    const bars = new THREE.InstancedMesh(geometry, new THREE.MeshStandardMaterial({ vertexColors: true, roughness: .82, metalness: .02 }), indices.length)
    const matrix = new THREE.Matrix4(), color = new THREE.Color()
    indices.forEach((index, position) => {
      const value = Number(selected.values[index]), { normalized, color: nextColor } = valueColor(value)
      const height = Number.isFinite(value) ? .18 + normalized * 6 : .04
      const column = position % columns, row = Math.floor(position / columns)
      matrix.compose(new THREE.Vector3((column - (columns - 1) / 2) * cell, height / 2, ((rows - 1) / 2 - row) * cell), new THREE.Quaternion(), new THREE.Vector3(1, height, 1))
      bars.setMatrixAt(position, matrix); color.copy(nextColor); bars.setColorAt(position, color)
    })
    bars.instanceMatrix.needsUpdate; if (bars.instanceColor) bars.instanceColor.needsUpdate = true
    resultScene.add(bars)
    const base = new THREE.Mesh(new THREE.PlaneGeometry(sideLength, sideLength), new THREE.MeshBasicMaterial({ color: 0x13251f, transparent: true, opacity: .8, side: THREE.DoubleSide }))
    base.rotation.x = -Math.PI / 2; base.position.y = -.04; resultScene.add(base)
    const resultGrid = new THREE.GridHelper(sideLength, 22, 0x5f9b83, 0x274638)
    resultGrid.position.y = -.02; resultGrid.material.opacity = .25; resultGrid.material.transparent = true; resultScene.add(resultGrid)
    camera3d.position.set(sideLength * .8, sideLength * .75, sideLength * .9); controls3d.target.set(0, 1.8, 0)
  }

  resultViewer = { host, scene: resultScene, camera: camera3d, controls: controls3d, renderer: new THREE.WebGLRenderer({ antialias: true, alpha: true }) }
  resultViewer.renderer.setPixelRatio(Math.min(devicePixelRatio, 2)); resultViewer.renderer.outputColorSpace = THREE.SRGBColorSpace
  resultViewer.renderer.toneMapping = THREE.ACESFilmicToneMapping; resultViewer.renderer.toneMappingExposure = 1.05
  host.append(resultViewer.renderer.domElement)
  resultViewer.resizeObserver = new ResizeObserver(resizeResultViewer); resultViewer.resizeObserver.observe(host)
  resizeResultViewer(); controls3d.update(); $('#facet3dBtn').classList.add('active'); bindFacetControls(result, file, selected.id, side)
}


async function openResultFile(file, band = 0) {
  if (!file) return
  try {
    state.selectedResult = file
    $$('.result-file').forEach((button) => button.classList.toggle('active', Number(button.dataset.index) === state.resultFiles.indexOf(file)))
    $('#resultStatus').textContent = `正在读取 ${file.name}...`
    const result = await api.readResult(file.path, band)
    if (result.kind === 'envi') drawEnviPreview(result, file)
    else if (result.kind === 'tiff') drawTiffPreview(result, file)
    else if (result.kind === 'facet') showFacet3D(result, file)
    else if (result.kind === 'image') await drawImagePreview(result, file)
    else if (result.kind === 'text') { disposeResultViewer(); state.resultRaster = null; $('#resultPreview').innerHTML = `<div class="result-preview-card"><pre class="result-text">${escapeHtml(result.content)}</pre><button class="button ghost result-open-external" type="button">用系统程序打开</button></div>` }
    else { disposeResultViewer(); state.resultRaster = null; $('#resultPreview').innerHTML = `<div class="empty-editor"><svg><use href="#i-layers"/></svg><h3>暂不支持内置预览</h3><p>可在资源管理器中定位并使用外部程序打开该文件。</p><button class="button ghost result-open-external" type="button">用系统程序打开</button></div>` }
    $('#resultPreview').querySelector('.result-open-external')?.addEventListener('click', () => api.openPath(file.path))
    $('#resultStatus').textContent = result.kind === 'envi' ? `ENVI 浮点影像 · ${file.name}` : result.kind === 'tiff' ? `TIFF 栅格影像 · ${file.name}` : result.kind === 'facet' ? `面元结果 · ${file.name}` : result.kind === 'image' ? `图像结果 · ${file.name}` : file.name
  } catch (error) {
    emptyResultPreview('结果读取失败', error.message)
    $('#resultStatus').textContent = error.message
  }
}

function renderResultFiles() {
  if (!state.resultFiles.length) {
    $('#resultFiles').innerHTML = '<div class="result-empty-list">输出目录中尚无结果文件。<br>请先运行模拟，然后点击“刷新”。</div>'
    emptyResultPreview('尚无模拟结果', 'HiStream 完成计算后，结果会显示在这里。')
    return
  }
  $('#resultFiles').innerHTML = state.resultFiles.map((file, index) => {
    const extension = file.kind === 'envi' ? 'ENVI' : file.kind === 'tiff' ? 'TIFF' : file.kind === 'facet' ? 'FACET' : file.name.split('.').pop()
    const time = new Date(file.modifiedAt).toLocaleString('zh-CN', { hour12: false })
    return `<button class="result-file" data-index="${index}"><span class="result-file-icon">${escapeHtml(extension)}</span><span><strong title="${escapeHtml(file.name)}">${escapeHtml(file.name)}</strong><small>${formatBytes(file.size)} · ${time}</small></span></button>`
  }).join('')
  $$('.result-file').forEach((button) => button.addEventListener('click', () => openResultFile(state.resultFiles[Number(button.dataset.index)])))
}

async function refreshResults(openFirst = false) {
  if (!state.outputDir) throw new Error('当前工程未配置输出目录')
  const result = await api.listResults(state.outputDir)
  state.outputDir = result.directory
  state.resultFiles = result.files
  $('#resultDirectory').textContent = result.directory
  renderResultFiles()
  const selected = state.selectedResult && state.resultFiles.find((file) => file.path === state.selectedResult.path)
  if (selected) await openResultFile(selected)
  else if (openFirst && state.resultFiles.length) await openResultFile(state.resultFiles[0])
}

async function showResults() {
  if (!state.inputPath) { toast('无法查看结果', '请先新建或打开工程', 'error'); return }
  $('#resultDialog').hidden = false
  try {
    await refreshResults(true)
  } catch (error) {
    state.resultFiles = []
    $('#resultFiles').innerHTML = '<div class="result-empty-list">无法读取输出目录。</div>'
    emptyResultPreview('无法打开模拟结果', error.message)
    $('#resultStatus').textContent = error.message
  }
}

function hideResults() {
  disposeResultViewer()
  state.resultRaster = null
  $('#resultDialog').hidden = true
}

function selectMode(mode, announce = false) {
  const button = $(`.mode-card[data-mode="${mode}"]`)
  if (!button) return
  const previousMode = state.mode
  const facetMode = previousMode === 'eFacetRT' || previousMode === 'eFacetEB' || previousMode === 'eRaytracing'
  const voxelMode = mode === 'eVoxelRT' || mode === 'eVoxelEB'
  const converted = facetMode && voxelMode
  state.mode = mode
  $$('.mode-card').forEach((item) => item.classList.toggle('active', item === button))
  if (state.project) {
    state.project.mode = mode
    if (converted) state.project.voxelization = { sourceMode: previousMode, targetMode: mode, voxelSize: Number(state.config?.scene?.voxel) || 1, updatedAt: new Date().toISOString() }
  }
  if (converted) {
    $('#voxelBtn').classList.add('active')
    if (voxelPreview) voxelPreview.visible = true
  }
  if (announce) {
    if (!state.running) setRunning(false)
    if (state.project) { state.xmlDirty = true; $('#unsavedMark').hidden = false }
    addLog(converted ? `面元场景已接入${button.querySelector('span').textContent}，体元大小 ${Number(state.config?.scene?.voxel) || 1} m` : `计算模式切换为 ${button.querySelector('span').textContent}`)
    if (converted) toast('面元 → 体元通道已启用', 'OBJ、材质和位置保持不变，运行时生成稀疏体元')
    renderInspector()
  }
}

function showProjectDialog() {
  $('#newProjectMode').value = state.mode
  $('#projectDirectory').value = localStorage.getItem('histreamProjectDirectory') || ''
  $('#projectDialog').hidden = false
  setTimeout(() => ($('#projectDirectory').value ? $('#newProjectName') : $('#projectDirectory')).focus(), 0)
}

function hideProjectDialog() { $('#projectDialog').hidden = true }

async function createProject(event) {
  event.preventDefault()
  const submit = $('#projectForm button[type="submit"]')
  const values = Object.fromEntries(new FormData($('#projectForm')).entries())
  try {
    submit.disabled = true
    const result = await api.createProject(values)
    localStorage.setItem('histreamProjectDirectory', values.directory)
    loadXml(result)
    hideProjectDialog()
    addLog(`已创建工程：${result.projectDir}`, 'success')
    toast('工程创建成功', result.project.name)
  } catch (error) { toast('创建工程失败', error.message, 'error') }
  finally { submit.disabled = false }
}

$$('.mode-card').forEach((button) => button.addEventListener('click', () => selectMode(button.dataset.mode, true)))
$$('.tree-item').forEach((button) => button.addEventListener('click', () => { state.panel = button.dataset.panel; $$('.tree-item').forEach((item) => item.classList.toggle('active', item === button)); renderInspector() }))
$$('#viewStyle button').forEach((button) => button.addEventListener('click', () => { $$('#viewStyle button').forEach((item) => item.classList.toggle('active', item === button)); applyViewStyle(button.dataset.style) }))
$('#gridBtn').addEventListener('click', () => { if (!grid) return; grid.visible = !grid.visible; $('#gridBtn').classList.toggle('active', grid.visible) })
$('#voxelBtn').addEventListener('click', () => { if (!voxelPreview) return; voxelPreview.visible = !voxelPreview.visible; $('#voxelBtn').classList.toggle('active', voxelPreview.visible) })
$('#fitBtn').addEventListener('click', fitCamera); $('#openXmlBtn').addEventListener('click', chooseXml); $('#drawerOpenBtn').addEventListener('click', chooseXml); $('#saveXmlBtn').addEventListener('click', saveXml); $('#importObjBtn').addEventListener('click', importObj); $('#runBtn').addEventListener('click', runSimulation); $('#stopBtn').addEventListener('click', () => api.stop())
$('#newProjectBtn').addEventListener('click', showProjectDialog)
$('#closeProjectBtn').addEventListener('click', hideProjectDialog)
$('#projectDialog').addEventListener('click', (event) => { if (event.target === $('#projectDialog')) hideProjectDialog() })
$('#projectForm').addEventListener('submit', createProject)
$('#closeMaterialBtn').addEventListener('click', () => hideMaterialDialog(true))
$('#materialDialog').addEventListener('click', (event) => { if (event.target === $('#materialDialog')) hideMaterialDialog(true) })
$('#materialForm').addEventListener('submit', saveImportedObject)
$('#closeObjectAttributeBtn').addEventListener('click', hideObjectAttributeDialog)
$('#objectAttributeDialog').addEventListener('click', (event) => { if (event.target === $('#objectAttributeDialog')) hideObjectAttributeDialog() })
$('#objectAttributeForm').addEventListener('submit', saveObjectAttributes)
$('#objectAttributeType').addEventListener('change', () => renderObjectAttributeOptions(true))
$('#objectAttributeMaterial').addEventListener('change', renderObjectAttributeSummary)
$('#closeObjectBtn').addEventListener('click', hideObjectDialog)
$('#objectDialog').addEventListener('click', (event) => { if (event.target === $('#objectDialog')) hideObjectDialog() })
$('#objectForm').addEventListener('submit', saveSceneObject)
$('#objectForm').addEventListener('input', (event) => {
  const form = $('#objectForm')
  if (event.target.name === 'distributionCount') syncDistributionPopulation(form, 'count')
  else if (event.target.name === 'distributionDensity') syncDistributionPopulation(form, 'density')
  else if (['distributionMinX', 'distributionMaxX', 'distributionMinY', 'distributionMaxY'].includes(event.target.name)) syncDistributionPopulation(form, form.elements.distributionBasis.value)
  updateDistributionFormState(form)
})
$('#objectForm').addEventListener('change', (event) => {
  const form = $('#objectForm')
  if (event.target.name === 'distributionBasis') syncDistributionPopulation(form, event.target.value)
  updateDistributionFormState(form)
})
$('#spectralModel').addEventListener('change', renderModelParameters)
document.querySelector("#spectralPreset").addEventListener("change", () => { applySpectrumPreset(); renderMeshMaterialRows() })
document.querySelector("#thermalPreset").addEventListener("change", () => { applyThermalPreset(); renderMeshMaterialRows() })
$('#materialObjectType').addEventListener('change', () => {
  const type = $('#materialObjectType').value
  $('#spectralModel').value = type === 'Vegetation' ? 'Prospect' : type === 'Soil' ? 'BSM' : 'custom'
  renderModelParameters(); renderSpectrumPresetOptions(); renderThermalPresetOptions(); renderPhysicalPresetOptions(); renderMeshMaterialRows()
})
$('#physicalMaterialPreset').addEventListener('change', renderPhysicalParameters)
$('#closePresetBtn').addEventListener('click', hidePresetDialog)
$('#presetDialog').addEventListener('click', (event) => { if (event.target === $('#presetDialog')) hidePresetDialog() })
$('#presetKind').addEventListener('change', () => { state.presetPhysicalType = 'Vegetation'; renderPresetParameterFields() })
$('#presetForm').addEventListener('submit', savePreset)


$('#engineBtn').addEventListener('click', async () => { if (state.mode === 'eFacetRT' || state.mode === 'eFacetEB') { toast('HiStream Facet 引擎', state.executable); return } const path = await api.chooseExecutable(); if (path) { state.executable = path; state.executableExists = true; setRunning(false); addLog(`HiStream 引擎：${path}`, 'success') } })
$('#openOutputBtn').addEventListener('click', showResults)
$('#openOutputTopBtn').addEventListener('click', showResults)
$('#closeResultBtn').addEventListener('click', hideResults)
$('#resultDialog').addEventListener('click', (event) => { if (event.target === $('#resultDialog')) hideResults() })
$('#resultRefreshBtn').addEventListener('click', async () => { try { await refreshResults(true) } catch (error) { toast('刷新结果失败', error.message, 'error') } })
$('#resultFolderBtn').addEventListener('click', async () => { try { await api.openPath(state.outputDir) } catch (error) { toast('无法打开输出目录', error.message, 'error') } })
$('#clearLogBtn').addEventListener('click', () => { $('#consoleOutput').innerHTML = ''; state.logCount = 0; $('#logCount').textContent = '0' })
$('#xmlBtn').addEventListener('click', () => { $('#xmlDrawer').hidden = false }); $('#closeXmlBtn').addEventListener('click', () => { $('#xmlDrawer').hidden = true })
$('#xmlDrawer').addEventListener('click', (event) => { if (event.target === $('#xmlDrawer')) $('#xmlDrawer').hidden = true })
$('#xmlEditor').addEventListener('input', () => { state.xmlText = $('#xmlEditor').value; state.xmlDirty = true; $('#unsavedMark').hidden = false; $('#xmlStatus').textContent = '有未保存的修改' })
$('#applyXmlBtn').addEventListener('click', async () => { try { state.config = ensureMaterialPresets(parseXml(state.xmlText)); if (await saveXml()) { refreshFromState(); loadActualScene(state.config); renderInspector(); $('#xmlDrawer').hidden = true } } catch (error) { toast('XML 格式错误', error.message, 'error') } })
window.addEventListener('keydown', (event) => { if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') { event.preventDefault(); saveXml() } if (event.key === 'F5') { event.preventDefault(); runSimulation() } if (event.key === 'Escape') { if (!$('#resultDialog').hidden) hideResults(); else if (!$('#objectAttributeDialog').hidden) hideObjectAttributeDialog(); else if (!$('#objectDialog').hidden) hideObjectDialog(); else if (!$('#materialDialog').hidden) hideMaterialDialog(true); else if (!$('#presetDialog').hidden) hidePresetDialog(); else if (!$('#projectDialog').hidden) hideProjectDialog(); else if (!$('#xmlDrawer').hidden) $('#xmlDrawer').hidden = true } })
setInterval(() => { if (state.running) { const seconds = (Date.now() - state.startedAt) / 1000; $('#elapsed').textContent = `${String(Math.floor(seconds / 60)).padStart(2, '0')}:${String(Math.floor(seconds % 60)).padStart(2, '0')}.${Math.floor((seconds % 1) * 10)}` } }, 100)

async function init() {
  state.config = defaultConfig(); generateWorld(state.config); updateSun(state.config); fitCamera(); renderInspector(); resize()
  try {
    const defaults = await api.defaults()
    state.executable = defaults.executable; state.executableExists = defaults.executableExists; state.radiosityExecutable = defaults.radiosityExecutable; state.radiosityExecutableExists = defaults.radiosityExecutableExists; state.platform = defaults.platform; setRunning(false)
    addLog(`GPU Radiosity：${defaults.radiosityExecutable}`, defaults.radiosityExecutableExists ? 'success' : 'error')
    addLog(`默认 HiStream：${defaults.executable}`, 'success'); addLog(`Three.js ${defaults.versions.three} · Node ${defaults.versions.node}`)
  } catch (error) { state.executableExists = false; setEngineState('桌面桥接不可用', error.message, ''); addLog(error.message, 'error') }
  api.onSimulationEvent(handleSimulationEvent)
}

init()
