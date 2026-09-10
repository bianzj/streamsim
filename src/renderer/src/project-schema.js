export const PROJECT_SCHEMA_VERSION = 6

export const PROJECT_MODES = ['eVoxelEB', 'eFacetEB', 'eFacetRT', 'eVoxelRT', 'eRaytracing']
export const DEFAULT_SCENE_MODEL = 'models/default_building.obj'
export const DEFAULT_SENSOR_BANDS = [475, 560, 668, 717, 842, 10500]

const number = (value, fallback) => Number.isFinite(Number(value)) ? Number(value) : fallback
export const MAX_SENSOR_BANDS = 800

const cleanNumber = (value) => Number(Number(value).toFixed(6))
const normalizedAzimuth = (value) => ((Number(value) % 360) + 360) % 360

export function sensorCruisePositions(sensor = {}, scene = {}) {
  const width = Math.max(0.000001, number(scene.x, 60))
  const depth = Math.max(0.000001, number(scene.y, 60))
  const startX = ((number(sensor.positionX, width / 2) % width) + width) % width
  const startY = ((number(sensor.positionY, depth / 2) % depth) + depth) % depth
  const height = Math.max(0.01, number(sensor.height, 3000))
  const headingDegrees = normalizedAzimuth(number(sensor.cruiseHeading, 90))
  const step = Math.max(0.01, number(sensor.cruiseStep, 25))
  const count = Math.max(1, Math.min(10000, Math.round(number(sensor.cruiseCount, 20))))
  const route = ['line', 'rectangle', 'random'].includes(sensor.cruiseRoute) ? sensor.cruiseRoute : 'line'
  const wrap = (value, extent) => ((value % extent) + extent) % extent
  const point = (x, y, heading, z = height) => ({ x: cleanNumber(wrap(x, width)), y: cleanNumber(wrap(y, depth)), z: cleanNumber(Math.max(0.01, z)), heading: cleanNumber(normalizedAzimuth(heading)) })

  if (route === 'rectangle') {
    const rectangleWidth = Math.max(step, Math.min(width, number(sensor.cruiseRectangleWidth, Math.min(width * 0.25, 500))))
    const rectangleHeight = Math.max(step, Math.min(depth, number(sensor.cruiseRectangleHeight, Math.min(depth * 0.25, 500))))
    const perimeter = 2 * (rectangleWidth + rectangleHeight)
    return Array.from({ length: count }, (_, index) => {
      const distance = index * step % perimeter
      if (distance < rectangleWidth) return point(startX - rectangleWidth / 2 + distance, startY - rectangleHeight / 2, 0)
      if (distance < rectangleWidth + rectangleHeight) return point(startX + rectangleWidth / 2, startY - rectangleHeight / 2 + distance - rectangleWidth, 90)
      if (distance < rectangleWidth * 2 + rectangleHeight) return point(startX + rectangleWidth / 2 - (distance - rectangleWidth - rectangleHeight), startY + rectangleHeight / 2, 180)
      return point(startX - rectangleWidth / 2, startY + rectangleHeight / 2 - (distance - rectangleWidth * 2 - rectangleHeight), 270)
    })
  }

  if (route === 'random') {
    let seed = Math.max(1, Math.round(number(sensor.cruiseRandomSeed, 1))) >>> 0
    const random = () => {
      seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0
      return seed / 4294967296
    }
    const result = []
    let x = startX, y = startY, currentHeading = headingDegrees
    for (let index = 0; index < count; index += 1) {
      if (index < count - 1) currentHeading = random() * 360
      result.push(point(x, y, currentHeading))
      const heading = currentHeading * Math.PI / 180
      x = wrap(x + step * Math.cos(heading), width)
      y = wrap(y + step * Math.sin(heading), depth)
    }
    return result
  }

  const targetX = number(sensor.cruiseTargetX, startX + Math.cos(headingDegrees * Math.PI / 180) * step * Math.max(1, count - 1))
  const targetY = number(sensor.cruiseTargetY, startY + Math.sin(headingDegrees * Math.PI / 180) * step * Math.max(1, count - 1))
  const targetZ = Math.max(0.01, number(sensor.cruiseTargetZ, height))
  let deltaX = targetX - startX, deltaY = targetY - startY, deltaZ = targetZ - height
  let length = Math.hypot(deltaX, deltaY, deltaZ)
  if (length < 0.000001) {
    const heading = headingDegrees * Math.PI / 180
    deltaX = Math.cos(heading) * step
    deltaY = Math.sin(heading) * step
    deltaZ = 0
    length = step
  }
  const horizontalLength = Math.hypot(deltaX, deltaY)
  const forwardHeading = horizontalLength < 0.000001 ? headingDegrees : normalizedAzimuth(Math.atan2(deltaY, deltaX) * 180 / Math.PI)
  const period = 2 * length
  return Array.from({ length: count }, (_, index) => {
    const phase = index * step % period
    const forward = phase < length
    const distance = forward ? phase : period - phase
    return point(startX + deltaX / length * distance, startY + deltaY / length * distance, forward ? forwardHeading : forwardHeading + 180, height + deltaZ / length * distance)
  })
}

const isMobileAgentType = (type) => type === 'Human' || type === 'Vehicle' || type === 'Ship'
const withoutSmokeLabel = (item = {}) => ({ ...item, label: String(item.label || '').replaceAll('火焰/烟羽', '火焰') })
const WATER_PRESET_ALIASES = {
  water_radiation: 'water_surface',
  water_temperature_model: 'water_temperature',
  water_model: 'water_set'
}

const canonicalPresetName = (name) => WATER_PRESET_ALIASES[name] || name

function canonicalPresetItems(items = []) {
  const merged = new Map()
  for (const item of items) {
    const name = canonicalPresetName(item?.name)
    if (!name) continue
    const priority = item.name === name ? 1 : 0
    const previous = merged.get(name)
    if (!previous || priority >= previous.priority) merged.set(name, { priority, item: { ...item, name } })
  }
  return [...merged.values()].map(({ item }) => item)
}

function normalizeMaterialItem(item = {}) {
  const name = canonicalPresetName(item.name)
  const wood = item.energyModel === 'wood' || name === 'tree_branch_wood' || name === 'tree_trunk_wood'
  if (!wood) return { ...item, name }
  const fallback = createMaterialPresets().materials.find((entry) => entry.name === name)
  const hasSolidParameters = Number.isFinite(Number(item.params?.cs ?? item.params?.specificHeat))
  return {
    ...item,
    name,
    type: 'Vegetation',
    energyModel: 'wood',
    params: hasSolidParameters ? { ...(fallback?.params || {}), ...(item.params || {}) } : { ...(fallback?.params || {}) }
  }
}

function normalizeMovement(item = {}) {
  const vehicle = item.type === 'Vehicle' || item.type === 'Ship'
  const defaults = {
    speed: vehicle ? 8.3 : 1.3,
    speedVariation: vehicle ? 1 : 0.2,
    range: vehicle ? 50 : 20,
    turnInterval: vehicle ? 8 : 5
  }
  const movement = item.movement || {}
  return {
    enabled: Boolean(movement.enabled),
    mode: 'random',
    speed: Math.max(0, number(movement.speed, defaults.speed)),
    speedVariation: Math.max(0, number(movement.speedVariation, defaults.speedVariation)),
    range: Math.max(0.1, number(movement.range, defaults.range)),
    turnInterval: Math.max(0.1, number(movement.turnInterval, defaults.turnInterval)),
    seed: Math.round(number(movement.seed, 1))
  }
}

export function sensorBandValues(sensor = {}) {
  const customValues = String(sensor.bands || '').split(/[\s,;]+/).map(Number)
    .filter((value) => Number.isFinite(value) && value > 0)
  const customCount = new Set(customValues.map((value) => cleanNumber(value).toFixed(6))).size
  const values = []
  if (sensor.continuousBands) {
    const start = number(sensor.bandStart, 400)
    const end = number(sensor.bandEnd, 2500)
    const step = number(sensor.bandStep, 10)
    if (start > 0 && end >= start && step > 0) {
      const count = Math.floor((end - start) / step + 1e-9) + 1
      const generatedCapacity = Math.max(0, MAX_SENSOR_BANDS - customCount)
      const generatedCount = Math.min(count, generatedCapacity)
      for (let index = 0; index < generatedCount; index += 1) values.push(cleanNumber(start + index * step))
      if (count <= generatedCapacity && values.length < generatedCapacity && Math.abs(start + (count - 1) * step - end) > 1e-6) values.push(cleanNumber(end))
    }
  }
  values.push(...customValues)
  const unique = []
  const keys = new Set()
  for (const value of values) {
    const cleaned = cleanNumber(value)
    const key = cleaned.toFixed(6)
    if (!keys.has(key)) { keys.add(key); unique.push(cleaned) }
  }
  // HiStream uses band 0 as the representative 3D/process result.  Keep the
  // merged list in physical wavelength order so an appended TIR band cannot
  // unexpectedly become the first band of a continuous VNIR/SWIR run.
  return unique.sort((a, b) => a - b).slice(0, MAX_SENSOR_BANDS)
}

export function sensorViewAngles(sensor = {}, sunAzimuth = 0) {
  const angles = []
  const keys = new Set()
  const add = (vza, vaa) => {
    const zenith = Math.max(0, Math.min(89.999, cleanNumber(vza)))
    // Nadir has no directional azimuth, but retaining the requested plane
    // azimuth gives the camera a continuous roll at VZA=0 instead of rotating
    // the sampling grid abruptly relative to the neighbouring ±5° views.
    const azimuth = cleanNumber(normalizedAzimuth(vaa))
    // All nadir azimuths describe the same observation; keep the first
    // (principal-plane) one so the angle chart has exactly one VZA=0 point.
    const key = zenith < 1e-6 ? 'nadir' : `${zenith.toFixed(4)},${azimuth.toFixed(4)}`
    if (!keys.has(key)) { keys.add(key); angles.push([zenith, azimuth]) }
  }
  if (!sensor.principalPlane && !sensor.hemisphere) {
    add(number(sensor.vza, 0), number(sensor.vaa, 0))
  }
  if (sensor.principalPlane) {
    for (const offset of [0, 90]) {
      for (let signedZenith = -75; signedZenith <= 75; signedZenith += 5) {
        add(Math.abs(signedZenith), Number(sunAzimuth) + offset + (signedZenith < 0 ? 180 : 0))
      }
    }
  }
  if (sensor.hemisphere) {
    const goldenAngle = Math.PI * (3 - Math.sqrt(5))
    const minimumCosZenith = Math.cos(75 * Math.PI / 180)
    for (let index = 0; index < 128; index += 1) {
      // TiRT-EB-style view cap: equal-area directions span VZA 0..75 degrees,
      // rather than extending the observation set to the 90-degree horizon.
      const cosZenith = 1 - (1 - minimumCosZenith) * index / 127
      add(Math.acos(cosZenith) * 180 / Math.PI, index * goldenAngle * 180 / Math.PI)
    }
  }
  return angles
}

// Retain bound or edited legacy spectra so existing simulations do not change.
export function simplifySolarSpectra(spectra, configuration = {}) {
  const references = new Set([configuration.scene?.background?.spectralName])
  for (const item of configuration.objects?.items || []) {
    references.add(item.spectralName)
    for (const mesh of item.meshes || []) references.add(mesh.spectralName)
  }
  const retired = {
    pv_monocrystalline: ['单晶硅太阳能板光谱', 'pv_monocrystalline_silicon.txt', 0.06, 0.10],
    pv_polycrystalline: ['多晶硅太阳能板光谱', 'pv_polycrystalline_silicon.txt', 0.09, 0.11],
    pv_bifacial: ['双面光伏组件光谱', 'pv_bifacial_glass.txt', 0.07, 0.09]
  }
  return spectra.filter((item) => {
    const old = retired[item.name]
    if (!old || references.has(item.name)) return true
    const unchanged = item.label === old[0] && item.model === 'file'
      && item.fileName === `assets/spectral-library/${old[1]}`
      && Number(item.reflectance) === old[2] && Number(item.transmittance) === 0
      && Number(item.refTir) === old[3] && Number(item.tauTir) === 0
      && !Object.keys(item.params || {}).length
      && !item.physicalTexture?.enabled && !item.physicalTexture?.fileName
      && item.previewTexture?.enabled === true && item.previewTexture?.preset === 'solar-panel'
      && Number(item.previewTexture?.repeatSize) === 0.18
    return !unchanged
  }).map((item) => item.name === 'pv_opaque' && ['光伏板不透明光谱', '光伏板通用不透明光谱'].includes(item.label)
    ? { ...item, label: '太阳能板光谱' } : item)
}

export function createMaterialPresets() {
  return {
    spectra: [
      { name: 'pv_opaque', label: '太阳能板光谱', model: 'custom', reflectance: '0.08', transmittance: '0', refTir: 0.1, tauTir: 0, previewTexture: { enabled: true, preset: 'solar-panel', repeatSize: 0.18 } },
      { name: 'soil', label: '典型土壤', model: 'BSM', reflectance: '0.20', transmittance: '0.0', refTir: 0.05, tauTir: 0, params: { SMC: 25, BSMBrightness: 0.5, BSMlat: 25, BSMlon: 45 } },
      { name: 'green_leaf', label: '健康绿叶', model: 'Prospect', reflectance: '0.08', transmittance: '0.04', refTir: 0.03, tauTir: 0, params: { Cab: 40, Cw: 0.01, Cdm: 0.01, Cs: 0, N: 1.5 } },
      { name: 'dry_leaf', label: '干燥叶片', model: 'Prospect', reflectance: '0.16', transmittance: '0.08', refTir: 0.05, tauTir: 0, params: { Cab: 15, Cw: 0.003, Cdm: 0.02, Cs: 0, N: 1.8 } },
      { name: 'maize_leaf', label: '玉米健康叶片', model: 'Prospect', reflectance: '0.07', transmittance: '0.04', refTir: 0.025, tauTir: 0, params: { Cab: 48, Cw: 0.015, Cdm: 0.009, Cs: 0, N: 1.45 } },
      { name: 'wheat_leaf', label: '小麦健康叶片', model: 'Prospect', reflectance: '0.08', transmittance: '0.045', refTir: 0.03, tauTir: 0, params: { Cab: 42, Cw: 0.012, Cdm: 0.01, Cs: 0, N: 1.5 } },
      { name: 'rice_leaf', label: '水稻健康叶片', model: 'Prospect', reflectance: '0.075', transmittance: '0.045', refTir: 0.025, tauTir: 0, params: { Cab: 45, Cw: 0.014, Cdm: 0.009, Cs: 0, N: 1.5 } },
      { name: 'senescent_crop_leaf', label: '衰老作物叶片', model: 'Prospect', reflectance: '0.18', transmittance: '0.09', refTir: 0.045, tauTir: 0, params: { Cab: 8, Cw: 0.004, Cdm: 0.02, Cs: 0.2, N: 1.8 } },
      { name: 'broadleaf_tree_leaf', label: '阔叶树叶片', model: 'Prospect', reflectance: '0.07', transmittance: '0.04', refTir: 0.025, tauTir: 0, params: { Cab: 52, Cw: 0.016, Cdm: 0.012, Cs: 0, N: 1.55 } },
      { name: 'conifer_needle', label: '针叶树针叶', model: 'Prospect', reflectance: '0.09', transmittance: '0.025', refTir: 0.035, tauTir: 0, params: { Cab: 38, Cw: 0.011, Cdm: 0.016, Cs: 0, N: 1.75 } },
      { name: 'tree_wood', label: '树干与树枝木质部', model: 'custom', reflectance: '0.18', transmittance: '0.0', refTir: 0.06, tauTir: 0, params: {} },
      { name: 'soil_dry_spectral', label: '干燥壤土光谱', model: 'BSM', reflectance: '0.28', transmittance: '0.0', refTir: 0.07, tauTir: 0, params: { SMC: 8, BSMBrightness: 0.68, BSMlat: 25, BSMlon: 45 } },
      { name: 'soil_wet_spectral', label: '湿润壤土光谱', model: 'BSM', reflectance: '0.12', transmittance: '0.0', refTir: 0.035, tauTir: 0, params: { SMC: 38, BSMBrightness: 0.35, BSMlat: 25, BSMlon: 45 } },
      { name: 'concrete', label: '混凝土', model: 'custom', reflectance: '0.35', transmittance: '0.0', refTir: 0.08, tauTir: 0, previewTexture: { enabled: true, preset: 'concrete', repeatSize: 2 }, physicalTexture: { enabled: false, fileName: 'assets/texture-library/concrete_variance.jpg', strength: 0.2, repeatSize: 2 }, params: {} },
      { name: 'asphalt', label: '沥青路面', model: 'custom', reflectance: '0.12', transmittance: '0.0', refTir: 0.05, tauTir: 0, previewTexture: { enabled: true, preset: 'gravel', repeatSize: 3 }, physicalTexture: { enabled: false, fileName: 'assets/texture-library/asphalt_variance.jpg', strength: 0.2, repeatSize: 3 }, params: {} },
      { name: 'red_brick', label: '红砖墙面', model: 'custom', reflectance: '0.28', transmittance: '0.0', refTir: 0.07, tauTir: 0, previewTexture: { enabled: true, preset: 'red-brick', repeatSize: 1.2 }, physicalTexture: { enabled: false, fileName: 'assets/texture-library/brick_variance.jpg', strength: 0.2, repeatSize: 2 }, params: {} },
      { name: 'human_surface', label: '人员典型光谱', model: 'custom', reflectance: '0.30', transmittance: '0.0', refTir: 0.02, tauTir: 0, params: {} },
      { name: 'vehicle_surface', label: '载具典型光谱', model: 'custom', reflectance: '0.20', transmittance: '0.0', refTir: 0.08, tauTir: 0, previewTexture: { enabled: true, preset: 'metal', repeatSize: 1.5 }, physicalTexture: { enabled: false, fileName: 'assets/texture-library/metal_variance.jpg', strength: 0.15, repeatSize: 1.5 }, params: {} },
      { name: 'ship_surface', label: '船舶典型光谱', model: 'custom', reflectance: '0.22', transmittance: '0.0', refTir: 0.06, tauTir: 0, params: {} },
      { name: 'water_surface', label: '水体光谱', model: 'custom', reflectance: '0.04', transmittance: '0.0', refTir: 0.02, tauTir: 0, params: {} }
      ,{ name: 'fire_medium', label: '火焰介质', model: 'custom', reflectance: '0.0', transmittance: '0.0', refTir: 0, tauTir: 0, params: {} }
      ,{ name: 'fog_medium', label: '雾介质', model: 'custom', reflectance: '0.0', transmittance: '0.0', refTir: 0, tauTir: 0, params: {} }
    ],
    canopies: [
      { name: 'canopy_default', label: '混沌介质（多孔透射）', structureType: 'canopy', lai: 3, density: 1, hc: 2, G: 0.5, LIDFa: -0.35, LIDFb: -0.15, hspot: 0.2, leafwidth: 0.1 },
      { name: 'tree_leaf_canopy', label: '树叶（多孔透射）', structureType: 'canopy', lai: 3, density: 1, hc: 2, G: 0.5, LIDFa: -0.35, LIDFb: -0.15, hspot: 0.2, leafwidth: 0.08 },
      { name: 'tree_branch_rigid', label: '树枝（刚性木质）', structureType: 'rigid', lai: 0, density: 1, hc: 2, G: 0.5, LIDFa: 0, LIDFb: 0, hspot: 0, leafwidth: 0 },
      { name: 'tree_trunk_rigid', label: '树干（刚性木质）', structureType: 'rigid', lai: 0, density: 1, hc: 2, G: 0.5, LIDFa: 0, LIDFb: 0, hspot: 0, leafwidth: 0 },
      { name: 'rigid_body', label: '刚体（仅轮廓不透光）', structureType: 'rigid', lai: 0, density: 0, hc: 2, G: 0.5, LIDFa: -0.35, LIDFb: -0.15, hspot: 0, leafwidth: 0.1 },
      { name: 'fire_medium', label: '火焰（参与介质）', structureType: 'fire', lai: 0, density: 0, hc: 3, G: 0, LIDFa: 0, LIDFb: 0, hspot: 0, leafwidth: 0, extinction: 1.2, scatteringAlbedo: 0.72, asymmetry: 0.55, emissionScale: 1, fixedTemperature: 1100 },
      { name: 'fog_medium', label: '雾（参与介质）', structureType: 'fog', lai: 0, density: 0, hc: 10, G: 0, LIDFa: 0, LIDFb: 0, hspot: 0, leafwidth: 0, extinction: 0.08, scatteringAlbedo: 0.95, asymmetry: 0.85, emissionScale: 1, fixedTemperature: 288 }
    ],
    thermals: [
      { name: 'soil_temperature', label: '土壤温度', sunlitTemperature: 305, shadedTemperature: 295 },
      { name: 'vegetation_temperature', label: '植被温度', sunlitTemperature: 300, shadedTemperature: 296 },
      { name: 'tree_wood_temperature', label: '树干与树枝温度', sunlitTemperature: 303, shadedTemperature: 297 },
      { name: 'building_temperature', label: '建筑表面温度', sunlitTemperature: 310, shadedTemperature: 300 },
      { name: 'human_temperature', label: '人员表面温度', sunlitTemperature: 310, shadedTemperature: 307 },
      { name: 'vehicle_temperature', label: '载具表面温度', sunlitTemperature: 315, shadedTemperature: 305 },
      { name: 'ship_temperature', label: '船舶表面温度', sunlitTemperature: 313, shadedTemperature: 303 },
      { name: 'pv_temperature', label: '太阳能板初始温度', sunlitTemperature: 318, shadedTemperature: 303 },
      { name: 'water_temperature', label: '水体初始温度', sunlitTemperature: 295, shadedTemperature: 295 }
      ,{ name: 'fire_temperature', label: '火焰固定温度', sunlitTemperature: 1100, shadedTemperature: 1100 }
      ,{ name: 'fog_temperature', label: '雾介质温度', sunlitTemperature: 288, shadedTemperature: 288 }
    ],
    materials: [
      // Representative dry concrete surface; users can edit these thermal properties.
      { name: 'building_surface', label: '建筑表面', type: 'Building', params: { method: 2, rss: 1000000000, cs: 880, rhos: 2300, lambdas: 1.4, Tsoil: 25, SMC: 0, Satwater: 0 } },
      { name: 'pv_panel', label: '通用太阳能板（FacetEB）', type: 'Building', energyModel: 'photovoltaic', params: { eta25: 0.21, gamma: -0.0035, bifaciality: 0, heatCapacityPerArea: 12000, convectiveScale: 1 } },
      { name: 'pv_panel_mono', label: '单晶硅太阳能板（FacetEB）', type: 'Building', energyModel: 'photovoltaic', params: { eta25: 0.22, gamma: -0.0034, bifaciality: 0, heatCapacityPerArea: 12000, convectiveScale: 1 } },
      { name: 'pv_panel_poly', label: '多晶硅太阳能板（FacetEB）', type: 'Building', energyModel: 'photovoltaic', params: { eta25: 0.19, gamma: -0.0038, bifaciality: 0, heatCapacityPerArea: 12500, convectiveScale: 1 } },
      { name: 'pv_panel_bifacial', label: '双面太阳能板（FacetEB）', type: 'Building', energyModel: 'photovoltaic', params: { eta25: 0.215, gamma: -0.0034, bifaciality: 0.75, heatCapacityPerArea: 12500, convectiveScale: 1.1 } },
      {
        name: 'leaf_c3', label: 'C3 植被', type: 'Vegetation',
        params: { Vcmax: 80, m: 9, BallBerry: 0.01, Type: 3, kV: 0.6396, Rdparam: 0.015, Tparam: '0.2,0.3,288,313,328', Tyear: 25, beta: 0.507, kNPQs: 0, qLs: 1, stressfactor: 1, Tcor: 0 }
      },
      {
        name: 'leaf_c4', label: 'C4 植被', type: 'Vegetation',
        params: { Vcmax: 60, m: 4, BallBerry: 0.01, Type: 4, kV: 0.7, Rdparam: 0.012, Tparam: '0.2,0.3,288,313,328', Tyear: 25, beta: 0.507, kNPQs: 0, qLs: 1, stressfactor: 1, Tcor: 0 }
      },
      {
        name: 'tree_branch_wood', label: '树枝木质部', type: 'Vegetation', energyModel: 'wood',
        params: { method: 1, rss: 1000000000, cs: 1700, rhos: 550, lambdas: 0.14, Tsoil: 25, SMC: 0, Satwater: 0, convectiveScale: 1.5 }
      },
      {
        name: 'tree_trunk_wood', label: '树干木质部', type: 'Vegetation', energyModel: 'wood',
        params: { method: 1, rss: 1000000000, cs: 1700, rhos: 650, lambdas: 0.16, Tsoil: 25, SMC: 0, Satwater: 0, convectiveScale: 1.2 }
      },
      { name: 'soilset', label: '典型壤土', type: 'Soil', params: { method: 2, rss: 2000, cs: 1180, rhos: 1800, lambdas: 1.55, Tsoil: 25, SMC: 25, Satwater: 0.45 } },
      { name: 'soil_dry', label: '干燥土壤', type: 'Soil', params: { method: 2, rss: 4000, cs: 1000, rhos: 1600, lambdas: 0.8, Tsoil: 30, SMC: 10, Satwater: 0.38 } },
      { name: 'soil_wet', label: '湿润土壤', type: 'Soil', params: { method: 2, rss: 800, cs: 1400, rhos: 1900, lambdas: 2.0, Tsoil: 22, SMC: 35, Satwater: 0.50 } },
      { name: 'ship_material', label: '船舶表面物化', type: 'Ship', params: { method: 1, rss: 4000, cs: 900, rhos: 2700, lambdas: 10, Tsoil: 30, SMC: 0, Satwater: 0 } },
      { name: 'water_set', label: '水体物化参数', type: 'Water', params: { rss: 0, heatCapacity: 4186000, mixingDepth: 0.5, evaporationCoefficient: 1, brdfModel: 1, refractiveIndex: 1.333, slopeVariance: 0, diffuseFraction: 0.02 } }
    ]
  }
}

export function createDefaultProject(options = {}) {
  const mode = PROJECT_MODES.includes(options.mode) ? options.mode : 'eVoxelEB'
  const now = new Date().toISOString()
  const presets = createMaterialPresets()
  return {
    schemaVersion: PROJECT_SCHEMA_VERSION,
    format: 'streamsim-project',
    name: String(options.name || '未命名工程'),
    mode,
    createdAt: options.createdAt || now,
    updatedAt: now,
    folders: {
      models: 'models',
      positions: 'positions',
      spectral: 'spectral',
      meteorology: 'meteorology',
      terrain: 'terrain',
      output: 'output'
    },
    configuration: {
      outDir: 'output',
      scene: {
        x: number(options.sceneX, 60),
        y: number(options.sceneY, 60),
        height: number(options.sceneHeight, 15),
        offsetX: number(options.sceneOffsetX, 0),
        offsetY: number(options.sceneOffsetY, 0),
        offsetZ: number(options.sceneOffsetZ, 0),
        voxel: number(options.voxelSize, 1),
        voxelFillThreshold: number(options.voxelFillThreshold, 0.05),
        terrain: false,
        demFile: '',
        demInfo: null,
        background: {
          spectralName: 'soil', thermalName: 'soil_temperature',
          materialType: 'Soil', materialName: 'soilset',
          heterogeneityEnabled: false, angularEffectStrength: 0.5
        }
      },
      light: { zenith: 30, azimuth: 135, direct: 0.8, diffuse: 0.2, skyTemperature: 250 },
      sensor: {
        x: 512, y: 512, bands: DEFAULT_SENSOR_BANDS.join(' '), projection: 'parallel',
        vza: 0, vaa: 0,
        continuousBands: false, bandStart: 400, bandEnd: 2500, bandStep: 10,
        principalPlane: false, hemisphere: false,
        positionX: number(options.sceneX, 60) / 2,
        positionY: number(options.sceneY, 60) / 2,
        height: 3000,
        fov: 60,
        cruiseEnabled: false,
        cruiseHeading: 90,
        cruiseRoute: 'line',
        cruiseTargetX: number(options.sceneX, 60) * 0.75,
        cruiseTargetY: number(options.sceneY, 60) / 2,
        cruiseTargetZ: 3000,
        cruiseRectangleWidth: Math.min(number(options.sceneX, 60) * 0.25, 500),
        cruiseRectangleHeight: Math.min(number(options.sceneY, 60) * 0.25, 500),
        cruiseRandomSeed: 1,
        cruiseStep: 25,
        cruiseCount: 20,
        image: true, process: false, radiationProcess: false, energyProcess: false, temperature: true, albedo: false
      },
      control: {
        depth: 4, samples: 32, gpu: 0,
        heterogeneousVoxel: false,
        periodicTraversalCount: 0,
        skyboxEnabled: false,
        radiationSolver: 'traditional',
        spectralAccelerationWidth: 100,
        couplingIterations: 20, temperatureTolerance: 0.05, temperatureRelaxation: 0.5,
        soilTemperatureMethod: 2,
        vegetationTemperatureMethod: 0
      },
      fluid: {
        enabled: false,
        voxelSize: Math.max(0.1, number(options.voxelSize, 1)),
        timeStep: 5,
        pressureIterations: 20,
        windDirection: 0,
        buoyancy: 0.033,
        dragCoefficient: 0.3,
        thermalCoupling: 1,
        diffusivity: 0.1,
        smokeEmission: 0.02,
        maxVelocity: 30,
        outputWind: false,
        outputAirTemperature: false,
        outputHeight: 2
      },
      objects: { count: 0, names: [], items: [] },
      spectra: presets.spectra,
      canopies: presets.canopies,
      thermals: presets.thermals,
      materials: presets.materials,
      meteo: {
        path: 'defined/meteo.txt', start: 1, end: 48,
        z: 15, Tsold: 300, SatWater: 0.45, dTime: 1800,
        latitude: 40, longitude: 116
      },
      atmosphere: {
        enabled: false,
        model: 'midlatitude-summer',
        waterVapor: 2,
        aerosol: 'rural',
        visibility: 23,
        lutFile: 'assets/atmosphere/simple_modtran_lut.csv'
      }
    }
  }
}

export function normalizeProject(value = {}) {
  const defaults = createDefaultProject(value)
  const { inputFile: _legacyInputFile, ...source } = value
  const configuration = value.configuration || {}
  const sourceSensor = configuration.sensor || {}
  const normalizedSceneX = Math.max(0.1, number(configuration.scene?.x, defaults.configuration.scene.x))
  const normalizedSceneY = Math.max(0.1, number(configuration.scene?.y, defaults.configuration.scene.y))
  const normalizedSensorX = number(sourceSensor.positionX, normalizedSceneX / 2)
  const normalizedSensorY = number(sourceSensor.positionY, normalizedSceneY / 2)
  const normalizedCruiseHeading = normalizedAzimuth(number(sourceSensor.cruiseHeading, 90))
  const normalizedCruiseStep = Math.max(0.01, number(sourceSensor.cruiseStep, 25))
  const normalizedCruiseCount = Math.max(1, Math.min(10000, Math.round(number(sourceSensor.cruiseCount, 20))))
  const fallbackCruiseSpan = normalizedCruiseStep * Math.max(1, normalizedCruiseCount - 1)
  const sourceLight = configuration.light || {}
  const directFraction = Math.max(0, Math.min(1, number(sourceLight.direct, defaults.configuration.light.direct)))
  const sourceMeteo = configuration.meteo || {}
  const meteoStart = Math.max(0, Math.round(number(sourceMeteo.start, defaults.configuration.meteo.start)))
  const meteoEnd = Math.max(meteoStart + 1, Math.round(number(sourceMeteo.end, defaults.configuration.meteo.end)))
  const energyMode = value.mode === 'eFacetEB' || value.mode === 'eVoxelEB'
  const legacyProcess = Boolean(sourceSensor.process)
  const sourceSpectra = simplifySolarSpectra(canonicalPresetItems(Array.isArray(configuration.spectra) && configuration.spectra.length
    ? configuration.spectra : defaults.configuration.spectra
  ), configuration)
  const spectra = sourceSpectra.map((item) => {
    const normalizedItem = withoutSmokeLabel(item)
    const builtInPreviewTexture = {
      concrete: { enabled: true, preset: 'concrete', repeatSize: 2 },
      asphalt: { enabled: true, preset: 'gravel', repeatSize: 3 },
      red_brick: { enabled: true, preset: 'red-brick', repeatSize: 1.2 },
      vehicle_surface: { enabled: true, preset: 'metal', repeatSize: 1.5 }
    }[String(item.name || '')]
    const sourcePreviewTexture = item.previewTexture || builtInPreviewTexture || {}
    const previewTexture = {
      enabled: Boolean(sourcePreviewTexture.enabled),
      preset: String(sourcePreviewTexture.preset || 'concrete'),
      repeatSize: Math.max(0.05, number(sourcePreviewTexture.repeatSize, 2))
    }
    const sourceTexture = item.physicalTexture || {}
    const physicalTexture = {
      enabled: Boolean(sourceTexture.enabled),
      fileName: String(sourceTexture.fileName || ''),
      strength: Math.max(0, Math.min(1, number(sourceTexture.strength, 0.2))),
      repeatSize: Math.max(0.01, number(sourceTexture.repeatSize, 2))
    }
    if (item.model !== 'BSM') return { ...normalizedItem, previewTexture, physicalTexture }
    const params = item.params || {}
    let latitude = number(params.BSMlat, 25)
    let longitude = number(params.BSMlon, 45)
    // 0/0 collapses BSM onto the very dark third basis spectrum and is not a useful soil default.
    if (Math.abs(latitude) < 1e-9 && Math.abs(longitude) < 1e-9) {
      latitude = 25
      longitude = 45
    }
    return {
      ...normalizedItem,
      previewTexture,
      physicalTexture,
      params: {
        ...params,
        SMC: number(params.SMC, 25),
        BSMBrightness: number(params.BSMBrightness, 0.5),
        BSMlat: latitude,
        BSMlon: longitude
      }
    }
  })
  const configuredFluidOutputHeight = number(
    configuration.fluid?.outputHeight,
    defaults.configuration.fluid.outputHeight
  )
  const sourceObjects = configuration.objects || {}
  const objectItems = (Array.isArray(sourceObjects.items) ? sourceObjects.items : defaults.configuration.objects.items)
    .map((item) => {
      const normalized = {
        ...item,
        materialName: canonicalPresetName(item.materialName),
        spectralName: canonicalPresetName(item.spectralName),
        thermalName: canonicalPresetName(item.thermalName),
        meshes: Array.isArray(item.meshes) ? item.meshes.map((mesh) => {
          const materialName = canonicalPresetName(mesh.materialName || item.materialName)
          const material = (configuration.materials || []).find((entry) => canonicalPresetName(entry.name) === materialName)
          const wood = mesh.energyModel === 'wood' || material?.energyModel === 'wood'
            || materialName === 'tree_branch_wood' || materialName === 'tree_trunk_wood'
          return {
            ...mesh,
            spectralName: canonicalPresetName(mesh.spectralName),
            thermalName: canonicalPresetName(mesh.thermalName),
            materialName,
            canopyName: mesh.canopyName || item.canopyName,
            ...(wood ? { energyModel: 'wood' } : {})
          }
        }) : item.meshes
      }
      if (item.distribution && typeof item.distribution === 'object' && !Array.isArray(item.distribution)) {
        const densityUnit = item.distribution.densityUnit
        const legacyDensity = !['hectare', 'squareKilometer'].includes(densityUnit)
        const distributionArea = Math.abs(number(item.distribution.maxX, 0) - number(item.distribution.minX, 0))
          * Math.abs(number(item.distribution.maxY, 0) - number(item.distribution.minY, 0))
        const migratedDensity = item.distribution.basis === 'count' && distributionArea > 0
          ? number(item.distribution.count, 1) / distributionArea * 10000
          : number(item.distribution.density, 0.02) * 10000
        const perHectareDensity = densityUnit === 'squareKilometer'
          ? number(item.distribution.density, 20000) / 100
          : number(item.distribution.density, 200)
        normalized.distribution = {
          ...item.distribution,
          density: legacyDensity
            ? Math.max(0.01, Number(migratedDensity.toFixed(2)))
            : Math.max(0.01, Number(perHectareDensity.toFixed(2))),
          densityUnit: 'hectare'
        }
      }
      if (item.type === 'Fire' || item.type === 'Fog') {
        const fog = item.type === 'Fog' || item.medium?.kind === 'fog'
        const standardAxes = item.medium?.coordinateOrder === 'XYZ'
        const dimensions = Array.isArray(item.dimensions) ? [...item.dimensions] : [1, 1, 1]
        const parameters = { ...(item.generation?.parameters || {}) }
        if (!standardAxes) {
          ;[dimensions[1], dimensions[2]] = [dimensions[2], dimensions[1]]
          ;[parameters.sizeY, parameters.sizeZ] = [parameters.sizeZ, parameters.sizeY]
          ;[parameters.positionY, parameters.positionZ] = [parameters.positionZ, parameters.positionY]
        }
        normalized.dimensions = dimensions
        normalized.medium = { ...(item.medium || {}), kind: fog ? 'fog' : 'fire', radiationOnly: true, coordinateOrder: 'XYZ' }
        normalized.generation = { ...(item.generation || {}), type: fog ? '雾' : '火焰', parameters }
      }
      if (isMobileAgentType(item.type)) normalized.movement = normalizeMovement(item)
      else delete normalized.movement
      return normalized
    })
  return {
    ...defaults,
    ...source,
    format: 'streamsim-project',
    schemaVersion: PROJECT_SCHEMA_VERSION,
    mode: PROJECT_MODES.includes(value.mode) ? value.mode : defaults.mode,
    folders: { ...defaults.folders, ...(value.folders || {}) },
    configuration: {
      ...defaults.configuration,
      ...configuration,
      scene: {
        ...defaults.configuration.scene,
        ...(configuration.scene || {}),
        offsetX: number(configuration.scene?.offsetX, defaults.configuration.scene.offsetX),
        offsetY: number(configuration.scene?.offsetY, defaults.configuration.scene.offsetY),
        offsetZ: number(configuration.scene?.offsetZ, defaults.configuration.scene.offsetZ),
        background: {
          ...defaults.configuration.scene.background,
          ...(configuration.scene?.background || {}),
          spectralName: canonicalPresetName(configuration.scene?.background?.spectralName || defaults.configuration.scene.background.spectralName),
          thermalName: canonicalPresetName(configuration.scene?.background?.thermalName || defaults.configuration.scene.background.thermalName),
          materialName: canonicalPresetName(configuration.scene?.background?.materialName || defaults.configuration.scene.background.materialName),
          heterogeneityEnabled: Boolean(configuration.scene?.background?.heterogeneityEnabled),
          angularEffectStrength: Math.max(0, Math.min(1, number(
            configuration.scene?.background?.angularEffectStrength,
            defaults.configuration.scene.background.angularEffectStrength
          )))
        }
      },
      light: { ...defaults.configuration.light, ...sourceLight, direct: directFraction, diffuse: 1 - directFraction },
      sensor: {
        ...defaults.configuration.sensor,
        ...sourceSensor,
        continuousBands: Boolean(sourceSensor.continuousBands),
        bandStart: number(sourceSensor.bandStart, 400),
        bandEnd: number(sourceSensor.bandEnd, 2500),
        bandStep: Math.max(0.000001, number(sourceSensor.bandStep, 10)),
        principalPlane: Boolean(sourceSensor.principalPlane),
        hemisphere: Boolean(sourceSensor.hemisphere),
        fov: Math.max(0.1, Math.min(120, number(sourceSensor.fov, 60))),
        cruiseEnabled: Boolean(sourceSensor.cruiseEnabled),
        cruiseHeading: normalizedCruiseHeading,
        cruiseRoute: ['line', 'rectangle', 'random'].includes(sourceSensor.cruiseRoute) ? sourceSensor.cruiseRoute : 'line',
        cruiseTargetX: number(sourceSensor.cruiseTargetX, normalizedSensorX + Math.cos(normalizedCruiseHeading * Math.PI / 180) * fallbackCruiseSpan),
        cruiseTargetY: number(sourceSensor.cruiseTargetY, normalizedSensorY + Math.sin(normalizedCruiseHeading * Math.PI / 180) * fallbackCruiseSpan),
        cruiseTargetZ: Math.max(0.01, number(sourceSensor.cruiseTargetZ, Math.max(0.01, number(sourceSensor.height, 3000)))),
        cruiseRectangleWidth: Math.max(normalizedCruiseStep, number(sourceSensor.cruiseRectangleWidth, Math.min(normalizedSceneX * 0.25, 500))),
        cruiseRectangleHeight: Math.max(normalizedCruiseStep, number(sourceSensor.cruiseRectangleHeight, Math.min(normalizedSceneY * 0.25, 500))),
        cruiseRandomSeed: Math.max(1, Math.round(number(sourceSensor.cruiseRandomSeed, 1))),
        cruiseStep: normalizedCruiseStep,
        cruiseCount: normalizedCruiseCount,
        radiationProcess: sourceSensor.radiationProcess ?? (!energyMode && legacyProcess),
        energyProcess: sourceSensor.energyProcess ?? (energyMode && legacyProcess)
      },
      control: {
        ...defaults.configuration.control,
        ...(configuration.control || {}),
        heterogeneousVoxel: Boolean(configuration.control?.heterogeneousVoxel),
        // Legacy projects used a separate switch plus periodicNeighborCount.
        // New projects use one 0..20 value, where zero means disabled.
        periodicBoundary: undefined,
        periodicNeighborCount: undefined,
        periodicTraversalCount: Math.max(0, Math.min(20, Math.round(number(
          configuration.control?.periodicTraversalCount
            ?? (configuration.control?.periodicBoundary
              ? configuration.control?.periodicNeighborCount : 0),
          0
        )))),
        skyboxEnabled: Boolean(configuration.control?.skyboxEnabled),
        radiationSolver: configuration.control?.radiationSolver === 'accelerated'
          ? 'accelerated' : 'traditional',
        spectralAccelerationWidth: Math.max(1, Math.min(1000, number(
          configuration.control?.spectralAccelerationWidth, 100
        ))),
        soilTemperatureMethod: Math.max(0, Math.min(2, Math.round(number(
          configuration.control?.soilTemperatureMethod,
          configuration.materials?.find((item) => item.type === 'Soil')?.params?.method ?? 2
        )))),
        vegetationTemperatureMethod: Math.max(0, Math.min(1, Math.round(number(
          configuration.control?.vegetationTemperatureMethod, 0
        ))))
      },
      fluid: {
        ...defaults.configuration.fluid,
        ...(configuration.fluid || {}),
        enabled: Boolean(configuration.fluid?.enabled),
        voxelSize: Math.max(0.1, number(
          configuration.fluid?.voxelSize,
          number(configuration.scene?.voxel, defaults.configuration.scene.voxel)
        )),
        timeStep: Math.max(0.1, number(configuration.fluid?.timeStep, defaults.configuration.fluid.timeStep)),
        pressureIterations: Math.max(2, Math.min(100, Math.round(number(configuration.fluid?.pressureIterations, defaults.configuration.fluid.pressureIterations)))),
        windDirection: ((number(configuration.fluid?.windDirection, defaults.configuration.fluid.windDirection) % 360) + 360) % 360,
        buoyancy: Math.max(0, number(configuration.fluid?.buoyancy, defaults.configuration.fluid.buoyancy)),
        dragCoefficient: Math.max(0, number(configuration.fluid?.dragCoefficient, defaults.configuration.fluid.dragCoefficient)),
        thermalCoupling: Math.max(0, number(configuration.fluid?.thermalCoupling, defaults.configuration.fluid.thermalCoupling)),
        diffusivity: Math.max(0, number(configuration.fluid?.diffusivity, defaults.configuration.fluid.diffusivity)),
        smokeEmission: Math.max(0, number(configuration.fluid?.smokeEmission, defaults.configuration.fluid.smokeEmission)),
        maxVelocity: Math.max(0.1, number(configuration.fluid?.maxVelocity, defaults.configuration.fluid.maxVelocity)),
        outputWind: Boolean(configuration.fluid?.outputWind),
        outputAirTemperature: Boolean(configuration.fluid?.outputAirTemperature),
        // Earlier builds clamped this value to 0 for flat scenes.
        outputHeight: configuredFluidOutputHeight > 0
          ? configuredFluidOutputHeight
          : defaults.configuration.fluid.outputHeight
      },
      atmosphere: {
        ...defaults.configuration.atmosphere,
        ...(configuration.atmosphere || {}),
        enabled: Boolean(configuration.atmosphere?.enabled),
        model: ['tropical', 'midlatitude-summer', 'midlatitude-winter'].includes(configuration.atmosphere?.model)
          ? configuration.atmosphere.model : defaults.configuration.atmosphere.model,
        waterVapor: Math.max(0.5, Math.min(5, number(
          configuration.atmosphere?.waterVapor,
          defaults.configuration.atmosphere.waterVapor
        ))),
        aerosol: ['rural', 'urban'].includes(configuration.atmosphere?.aerosol)
          ? configuration.atmosphere.aerosol : defaults.configuration.atmosphere.aerosol,
        visibility: Math.max(10, Math.min(50, number(
          configuration.atmosphere?.visibility,
          defaults.configuration.atmosphere.visibility
        )))
      },
      objects: {
        ...defaults.configuration.objects, ...sourceObjects, items: objectItems,
        count: objectItems.length,
        names: objectItems.map((item) => item.name)
      },
      spectra,
      canopies: (Array.isArray(configuration.canopies) && configuration.canopies.length ? configuration.canopies : defaults.configuration.canopies)
        .map((item) => {
          const rawType = String(item.structureType ?? 'canopy').toLowerCase()
          const structureType = rawType === '1' || rawType === 'rigid' ? 'rigid'
            : rawType === '2' || rawType === 'fire' ? 'fire'
              : rawType === '3' || rawType === 'fog' ? 'fog' : 'canopy'
          return {
            ...withoutSmokeLabel(item), structureType,
            extinction: Math.max(0, number(item.extinction, structureType === 'fire' ? 1.2 : structureType === 'fog' ? 0.08 : 0)),
            scatteringAlbedo: Math.max(0, Math.min(1, number(item.scatteringAlbedo, structureType === 'fog' ? 0.95 : structureType === 'fire' ? 0.72 : 0))),
            asymmetry: Math.max(-0.99, Math.min(0.99, number(item.asymmetry, structureType === 'fog' ? 0.85 : structureType === 'fire' ? 0.55 : 0))),
            emissionScale: Math.max(0, number(item.emissionScale, structureType === 'fire' || structureType === 'fog' ? 1 : 0)),
            fixedTemperature: Math.max(0, number(item.fixedTemperature, structureType === 'fire' ? 1100 : structureType === 'fog' ? 288 : 0))
          }
        }),
      thermals: canonicalPresetItems(Array.isArray(configuration.thermals) && configuration.thermals.length ? configuration.thermals : defaults.configuration.thermals)
        .map(withoutSmokeLabel),
      materials: canonicalPresetItems(Array.isArray(configuration.materials) && configuration.materials.length ? configuration.materials : defaults.configuration.materials).map(normalizeMaterialItem),
      meteo: {
        ...defaults.configuration.meteo,
        ...sourceMeteo,
        start: meteoStart,
        end: meteoEnd,
        z: Math.max(0, number(sourceMeteo.z, defaults.configuration.meteo.z)),
        Tsold: Math.max(150, Math.min(400, number(sourceMeteo.Tsold, defaults.configuration.meteo.Tsold))),
        SatWater: Math.max(0, Math.min(1, number(sourceMeteo.SatWater, defaults.configuration.meteo.SatWater))),
        dTime: Math.max(1, number(sourceMeteo.dTime, defaults.configuration.meteo.dTime)),
        latitude: Math.max(-90, Math.min(90, number(sourceMeteo.latitude, defaults.configuration.meteo.latitude))),
        longitude: Math.max(-180, Math.min(180, number(sourceMeteo.longitude, defaults.configuration.meteo.longitude)))
      }
    }
  }
}

export function parseProjectJson(content) {
  let value
  try {
    value = JSON.parse(String(content || ''))
  } catch (error) {
    throw new Error(`project.json 格式错误：${error.message}`)
  }
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('project.json 根节点必须是对象')
  if (value.format && value.format !== 'streamsim-project') throw new Error(`不支持的工程格式：${value.format}`)
  return normalizeProject(value)
}

export function stringifyProject(value) {
  return `${JSON.stringify(normalizeProject(value), null, 2)}\n`
}

export function validateProject(value) {
  const errors = []
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return { valid: false, errors: ['project.json 根节点必须是对象'], project: normalizeProject() }
  }
  if (value.format && value.format !== 'streamsim-project') errors.push('format 必须是 streamsim-project')
  const sourceVersion = Number(value.schemaVersion ?? 1)
  if (!Number.isInteger(sourceVersion) || sourceVersion < 1 || sourceVersion > PROJECT_SCHEMA_VERSION) {
    errors.push(`schemaVersion 仅支持 1-${PROJECT_SCHEMA_VERSION}`)
  }
  if (!PROJECT_MODES.includes(value.mode)) errors.push('运行模式无效')
  if (!value.configuration || typeof value.configuration !== 'object' || Array.isArray(value.configuration)) {
    errors.push('configuration 必须是对象')
  }
  const project = normalizeProject(value)
  const c = project.configuration
  if (!(c.scene.x > 0) || !(c.scene.y > 0) || !(c.scene.height >= 0)) errors.push('场景尺寸无效')
  if (![c.scene.offsetX, c.scene.offsetY, c.scene.offsetZ].every(Number.isFinite)) errors.push('场景 XYZ 偏移必须为有效数值')
  if (!(c.scene.voxel > 0)) errors.push('体素尺寸必须大于 0')
  if (!(Number(c.sensor.x) > 0) || !(Number(c.sensor.y) > 0)) errors.push('传感器分辨率无效')
  if (!sensorBandValues(c.sensor).length) errors.push('至少需要一个有效波段')
  if (!sensorViewAngles(c.sensor, c.light.azimuth).length) errors.push('至少需要一个有效观测角')
  if (!Array.isArray(c.objects.items)) errors.push('objects.items 必须是数组')
  const pv = c.materials.filter(m => m.energyModel === 'photovoltaic')
  for (const m of pv) {
    const p = m.params || {}
    if (![p.eta25,p.gamma,p.bifaciality,p.heatCapacityPerArea,p.convectiveScale].every(Number.isFinite) || !(p.eta25>=0 && p.eta25<=0.5 && p.gamma>=-0.02 && p.gamma<=0 && p.bifaciality>=0 && p.bifaciality<=1 && p.heatCapacityPerArea>=0 && p.convectiveScale>0)) errors.push('光伏参数无效：'+m.name)
  }
  const names = new Set(pv.map(m=>m.name))
  if (names.has(c.scene.background.materialName)) errors.push('光伏板请作为场景对象添加，背景地面不支持光伏材质')
  const used = c.objects.items.some(o => (o.meshes?.length ? o.meshes : [o]).some(m=>names.has(m.materialName || o.materialName)))
  if (used && project.mode === 'eVoxelEB') errors.push('光伏热电耦合请选择 eFacetEB 模式')
  return { valid: errors.length === 0, errors, project }
}

const xmlEscape = (value) => String(value ?? '')
  .replaceAll('&', '&amp;')
  .replaceAll('<', '&lt;')
  .replaceAll('>', '&gt;')
  .replaceAll('"', '&quot;')
  .replaceAll("'", '&apos;')

function objectXml(item, index, resolvePath = (value) => value) {
  const name = xmlEscape(item.name || `object_${index + 1}`)
  const dimensions = Array.isArray(item.dimensions) && item.dimensions.length >= 3 ? item.dimensions.join(',') : '1,1,1'
  const meshes = Array.isArray(item.meshes) && item.meshes.length ? item.meshes : []
  const meshNames = meshes.length ? meshes.map((mesh) => mesh.name).join(',') : (item.meshNames || name)
  const spectralNames = meshes.length ? meshes.map((mesh) => mesh.spectralName).join(',') : (item.spectralName || 'soil')
  const thermalNames = meshes.length ? meshes.map((mesh) => mesh.thermalName).join(',') : (item.thermalName || 'soil_temperature')
  const mediumObject = item.type === 'Fire' || item.type === 'Fog' || Boolean(item.medium)
  const engineType = mediumObject ? 'Vegetation' : isMobileAgentType(item.type) ? 'Building' : (item.type || 'Vegetation')
  const defaultMaterial = engineType === 'Vegetation' ? 'leaf_c3' : engineType === 'Water' ? 'water_set' : 'soilset'
  const materialName = item.materialName || item.bioName || defaultMaterial
  const canopyName = item.canopyName || (item.type === 'Fire' ? 'fire_medium' : item.type === 'Fog' ? 'fog_medium' : engineType === 'Vegetation' ? 'canopy_default' : 'rigid_body')
  const materialNames = meshes.length ? meshes.map((mesh) => mesh.materialName || materialName).join(',') : materialName
  const canopyNames = meshes.length ? meshes.map((mesh) => mesh.canopyName || canopyName).join(',') : canopyName
  const resolvedFileName = item.fileName ? resolvePath(item.fileName) : ''
  const resolvedPosition = item.positionFile ? resolvePath(item.positionFile) : ''
  // Participating media use HiStream's filled primitive voxelization.  The OBJ
  // remains in project.json only as a Three.js preview mesh.
  const fileName = resolvedFileName && !mediumObject ? `\n        <fileName>${xmlEscape(resolvedFileName)}</fileName>\n        <objectfile>${xmlEscape(resolvedFileName)}</objectfile>` : ''
  const mediumKind = mediumObject ? `\n        <mediumKind>${item.type === 'Fog' || item.medium?.kind === 'fog' ? 'fog' : 'fire'}</mediumKind>` : ''
  const position = resolvedPosition ? `\n        <objectPosition>${xmlEscape(resolvedPosition)}</objectPosition>\n        <isdisfromFile>1</isdisfromFile>` : '\n        <isdisfromFile>0</isdisfromFile>'
  return `      <object objName="${name}">
        <types>${xmlEscape(engineType)}</types>
        <meshNames>${xmlEscape(meshNames)}</meshNames>
        <spectralNames>${xmlEscape(spectralNames)}</spectralNames>
        <thermalNames>${xmlEscape(thermalNames)}</thermalNames>
        <canopyNames>${xmlEscape(canopyNames)}</canopyNames>
        <bioNames>${xmlEscape(materialNames)}</bioNames>
        <propNames>${xmlEscape(materialNames)}</propNames>
        <shapeTypes>${xmlEscape(item.shape || 'cube')}</shapeTypes>
        <shapes>${xmlEscape(dimensions)}</shapes>
        <isLarge>0</isLarge>${mediumKind}${fileName}${position}
      </object>`
}

function spectrumXml(item, index, resolvePath = (value) => value) {
  const model = ['custom', 'Prospect', 'BSM', 'file'].includes(item.model) ? item.model : 'custom'
  const texture = item.physicalTexture || {}
  const textureFile = texture.fileName ? resolvePath(texture.fileName) : ''
  const common = `<reflectance>${xmlEscape(item.reflectance || '0.20')}</reflectance><transmittance>${xmlEscape(item.transmittance || '0.0')}</transmittance>\n        <tau_TIR>${number(item.tauTir, 0)}</tau_TIR><ref_TIR>${number(item.refTir, 0.05)}</ref_TIR>\n        <physicalTextureEnabled>${texture.enabled ? 1 : 0}</physicalTextureEnabled><physicalTextureFile>${xmlEscape(textureFile)}</physicalTextureFile><physicalTextureStrength>${Math.max(0, Math.min(1, number(texture.strength, 0.2)))}</physicalTextureStrength><physicalTextureRepeatSize>${Math.max(0.01, number(texture.repeatSize, 2))}</physicalTextureRepeatSize>`
  const params = item.params || {}
  if (model === 'Prospect') return `<spectral id="${index + 1}" name="${xmlEscape(item.name)}" type="Prospect">\n        ${common}\n        <Cab>${number(params.Cab, 40)}</Cab><Cw>${number(params.Cw, 0.01)}</Cw><Cdm>${number(params.Cdm, 0.01)}</Cdm><Cs>${number(params.Cs, 0)}</Cs><N>${number(params.N, 1.5)}</N>\n      </spectral>`
  if (model === 'BSM') return `<spectral id="${index + 1}" name="${xmlEscape(item.name)}" type="BSM">\n        ${common}\n        <SMC>${number(params.SMC, 25)}</SMC><BSMBrightness>${number(params.BSMBrightness, 0.5)}</BSMBrightness><BSMlat>${number(params.BSMlat, 25)}</BSMlat><BSMlon>${number(params.BSMlon, 45)}</BSMlon>\n      </spectral>`
  if (model === 'file') return `<spectral id="${index + 1}" name="${xmlEscape(item.name)}" type="file">\n        ${common}<spectral_file>${xmlEscape(resolvePath(item.fileName || ''))}</spectral_file>\n      </spectral>`
  return `<spectral id="${index + 1}" name="${xmlEscape(item.name)}" type="custom">\n        ${common}<spectral_file>${xmlEscape(resolvePath(item.fileName || ''))}</spectral_file>\n      </spectral>`
}

function canopyXml(item) {
  const structureType = ['canopy', 'rigid', 'fire', 'fog'].includes(item.structureType) ? item.structureType : 'canopy'
  return `<canopy name="${xmlEscape(item.name)}"><structureType>${structureType}</structureType><lai>${number(item.lai, 3)}</lai><density>${number(item.density, 1)}</density><hc>${number(item.hc, 2)}</hc><G>${number(item.G, 0.5)}</G><LIDFa>${number(item.LIDFa, -0.35)}</LIDFa><LIDFb>${number(item.LIDFb, -0.15)}</LIDFb><hspot>${number(item.hspot, 0.2)}</hspot><leafwidth>${number(item.leafwidth, 0.1)}</leafwidth><extinction>${Math.max(0, number(item.extinction, 0))}</extinction><scatteringAlbedo>${Math.max(0, Math.min(1, number(item.scatteringAlbedo, 0)))}</scatteringAlbedo><asymmetry>${Math.max(-0.99, Math.min(0.99, number(item.asymmetry, 0)))}</asymmetry><emissionScale>${Math.max(0, number(item.emissionScale, 0))}</emissionScale><fixedTemperature>${Math.max(0, number(item.fixedTemperature, 0))}</fixedTemperature></canopy>`
}

function thermalXml(item, index) {
  return `<thermal id="${index + 1}" name="${xmlEscape(item.name)}"><sunlitTemperature>${number(item.sunlitTemperature, 305)}</sunlitTemperature><shadedTemperature>${number(item.shadedTemperature, 295)}</shadedTemperature></thermal>`
}

function materialXml(item) {
  const params = item.params || {}
  const name = xmlEscape(item.name)
  if (item.energyModel === 'photovoltaic') return '<property><surfaceEnergy name="'+name+'"><photovoltaic>1</photovoltaic><vegetation>0</vegetation>'+Object.entries(params).map(([k,v])=>'<'+k+'>'+xmlEscape(v)+'</'+k+'>').join('')+'</surfaceEnergy></property>'
  if (item.type === 'Vegetation') return `<property><leafBio name="${name}"><Vcmax>${number(params.Vcmax, 80)}</Vcmax><m>${number(params.m, 9)}</m><BallBerry>${number(params.BallBerry, 0.01)}</BallBerry><Type>${number(params.Type, 3)}</Type><kV>${number(params.kV, 0.6396)}</kV><Rdparam>${number(params.Rdparam, 0.015)}</Rdparam><Tparam>${xmlEscape(params.Tparam || '0.2,0.3,288,313,328')}</Tparam><Tyear>${number(params.Tyear, 25)}</Tyear><beta>${number(params.beta, 0.507)}</beta><kNPQs>${number(params.kNPQs, 0)}</kNPQs><qLs>${number(params.qLs, 1)}</qLs><stressfactor>${number(params.stressfactor, 1)}</stressfactor><Tcor>${number(params.Tcor, 0)}</Tcor></leafBio></property>`
  if (item.type === 'Water') return `<property><waterSet name="${name}"><rss>${number(params.rss, 0)}</rss><heatCapacity>${number(params.heatCapacity, 4186000)}</heatCapacity><mixingDepth>${number(params.mixingDepth, 0.5)}</mixingDepth><evaporationCoefficient>${number(params.evaporationCoefficient, 1)}</evaporationCoefficient><brdfModel>${number(params.brdfModel, 1) ? 'CoxMunk' : 'Lambert'}</brdfModel><refractiveIndex>${number(params.refractiveIndex, 1.333)}</refractiveIndex><slopeVariance>${Math.max(0, number(params.slopeVariance, 0))}</slopeVariance><diffuseFraction>${Math.max(0, Math.min(1, number(params.diffuseFraction, 0.02)))}</diffuseFraction></waterSet></property>`
  return `<property><soilSet name="${name}"><method>${number(params.method, 1)}</method><rss>${number(params.rss, 2000)}</rss><cs>${number(params.cs, 1180)}</cs><rhos>${number(params.rhos, 1800)}</rhos><lambdas>${number(params.lambdas, 1.55)}</lambdas><Tsoil>${number(params.Tsoil, 0.25)}</Tsoil><SMC>${number(params.SMC, 25)}</SMC><Satwater>${number(params.Satwater, 0.45)}</Satwater></soilSet></property>`
}

export function projectToXml(value, paths = {}) {
  const project = normalizeProject(value)
  const c = project.configuration
  const energyMode = project.mode === 'eFacetEB' || project.mode === 'eVoxelEB'
  const radiationProcess = Boolean(c.sensor.radiationProcess)
  const energyProcess = Boolean(c.sensor.energyProcess)
  const activeProcess = energyMode ? (radiationProcess || energyProcess) : radiationProcess
  const outputDir = paths.outputDir || c.outDir || 'output'
  const definedDir = paths.definedDir || 'defined'
  const resolveProjectPath = paths.resolveProjectPath || ((value) => value)
  const slash = String(definedDir).includes('\\') ? '\\' : '/'
  const definedFile = (name) => `${String(definedDir).replace(/[\\/]$/, '')}${slash}${name}`
  const objects = (c.objects.items || [])
    .filter((item) => project.mode === 'eVoxelRT' || project.mode === 'eVoxelEB' || !(item.type === 'Fire' || item.type === 'Fog' || item.medium))
    .map((item, index) => objectXml(item, index, resolveProjectPath)).join('\n')
  const spectra = c.spectra.map((item, index) => spectrumXml(item, index, resolveProjectPath)).join('\n      ')
  const canopies = c.canopies.map(canopyXml).join('\n      ')
  const thermals = c.thermals.map(thermalXml).join('\n      ')
  const materials = c.materials.map((item) => materialXml(item.type === 'Soil' ? {
    ...item,
    params: { ...(item.params || {}), method: c.control.soilTemperatureMethod }
  } : item)).join('\n      ')
  const background = c.scene.background || {}
  const backgroundMaterial = c.materials.find((item) => item.name === background.materialName)
  const backgroundType = backgroundMaterial?.type || background.materialType || 'Soil'
  // Preserve the selected DEM path while disabled. The isDEM switch controls
  // whether terrain and object elevation offsets are applied.
  const demFile = c.scene.demFile
    ? `\n    <DEM>${xmlEscape(resolveProjectPath(c.scene.demFile))}</DEM>` : ''
  const bands = sensorBandValues(c.sensor)
  const perspective = c.sensor.projection === 'perspective'
  const cruiseEnabled = perspective && Boolean(c.sensor.cruiseEnabled)
  const cruisePositions = cruiseEnabled ? sensorCruisePositions(c.sensor, c.scene) : []
  const effectivePerspectiveAzimuth = cruiseEnabled
    ? normalizedAzimuth(number(cruisePositions[0]?.heading, c.sensor.cruiseHeading) + number(c.sensor.vaa, 0) + 180)
    : number(c.sensor.vaa, 0)
  const viewAngles = perspective
    ? [[number(c.sensor.vza, 0), effectivePerspectiveAzimuth]]
    : sensorViewAngles(c.sensor, c.light.azimuth)
  const viewAngleXml = viewAngles.map((angle) => `<viewAngles>${angle[0]},${angle[1]}</viewAngles>`).join('')
  const cruiseXml = cruisePositions.length
    ? `\n        <uavPositions enabled="1" route="${xmlEscape(c.sensor.cruiseRoute || 'line')}" step="${number(c.sensor.cruiseStep, 25)}" count="${cruisePositions.length}" targetX="${number(c.sensor.cruiseTargetX, c.sensor.positionX)}" targetY="${number(c.sensor.cruiseTargetY, c.sensor.positionY)}" targetZ="${number(c.sensor.cruiseTargetZ, c.sensor.height)}">${cruisePositions.map((position) => `<position heading="${number(position.heading, 0)}">${position.x},${position.y},${position.z}</position>`).join('')}</uavPositions>`
    : ''
  const atmosphere = c.atmosphere || {}
  const atmosphereLut = paths.atmosphereLutPath || atmosphere.lutFile || 'assets/atmosphere/simple_modtran_lut.csv'
  return `<?xml version="1.0" encoding="UTF-8"?>
<HiStreamProject schemaVersion="${PROJECT_SCHEMA_VERSION}" mode="${xmlEscape(project.mode)}">
  <Control>
    <outDir>${xmlEscape(outputDir)}</outDir>
    <rayTracingDepth>${number(c.control.depth, 4)}</rayTracingDepth>
    <sampleCount>${number(c.control.samples, 32)}</sampleCount>
    <periodicNeighborCount>${Math.max(0, Math.min(20, Math.round(number(c.control.periodicTraversalCount, 0))))}</periodicNeighborCount>
    <skyboxEnabled>${c.control.skyboxEnabled ? 1 : 0}</skyboxEnabled>
    <radiationSolver>${c.control.radiationSolver === 'accelerated' ? 'accelerated' : 'traditional'}</radiationSolver>
    <spectralAccelerationWidth>${Math.max(1, Math.min(1000, number(c.control.spectralAccelerationWidth, 100)))}</spectralAccelerationWidth>
    <GPU>${number(c.control.gpu, 0)}</GPU>
    <couplingIterations>${number(c.control.couplingIterations, 20)}</couplingIterations>
    <temperatureTolerance>${number(c.control.temperatureTolerance, 0.05)}</temperatureTolerance>
    <temperatureRelaxation>${number(c.control.temperatureRelaxation, 0.5)}</temperatureRelaxation>
    <soilTemperatureMethod>${Math.max(0, Math.min(2, Math.round(number(c.control.soilTemperatureMethod, 2))))}</soilTemperatureMethod>
    <vegetationTemperatureMethod>${Math.max(0, Math.min(1, Math.round(number(c.control.vegetationTemperatureMethod, 0))))}</vegetationTemperatureMethod>
    <isDEM>${c.scene.terrain ? 1 : 0}</isDEM>
    <isImage>${c.sensor.image ? 1 : 0}</isImage>
    <isProcess>${activeProcess ? 1 : 0}</isProcess>
    <isRadiationProcess>${radiationProcess ? 1 : 0}</isRadiationProcess>
    <isEnergyProcess>${energyProcess ? 1 : 0}</isEnergyProcess>
    <isTemperature>${c.sensor.temperature ? 1 : 0}</isTemperature>
    <isAlbedo>${c.sensor.albedo ? 1 : 0}</isAlbedo>
    <isDisplay>0</isDisplay>
    <isOrth>0</isOrth>
    <isUAVtrave>${cruiseEnabled ? 1 : 0}</isUAVtrave>
  </Control>
  <Geometry>
    <Light>
      <light name="Solar">
        <lightAngle><lightAngle>${number(c.light.zenith, 30)},${number(c.light.azimuth, 135)}</lightAngle></lightAngle>
        <skyTemperature>${number(c.light.skyTemperature, 250)}</skyTemperature>
        <directScatteringRatio>${number(c.light.direct, 0.8)}</directScatteringRatio>
        <esunFileName>${xmlEscape(definedFile('Esun_.dat'))}</esunFileName>
        <eskyFileName>${xmlEscape(definedFile('Esky_.dat'))}</eskyFileName>
      </light>
    </Light>
    <Sensor>
      <sensor name="MainSensor" type="Multispectral sensor">
        <pixelResolutionX>${number(c.sensor.x, 512)}</pixelResolutionX>
        <pixelResolutionY>${number(c.sensor.y, 512)}</pixelResolutionY>
        <controlBand>${xmlEscape((bands.length ? bands : DEFAULT_SENSOR_BANDS).join(','))}</controlBand>
        <continuousBand enabled="${c.sensor.continuousBands ? 1 : 0}"><customBands>${xmlEscape(c.sensor.bands || '')}</customBands><start>${number(c.sensor.bandStart, 400)}</start><end>${number(c.sensor.bandEnd, 2500)}</end><step>${Math.max(0.000001, number(c.sensor.bandStep, 10))}</step></continuousBand>
        <projection>${c.sensor.projection === 'perspective' ? 'perspective' : 'parallel'}</projection>
        <sensorPosition>${number(c.sensor.positionX, number(c.scene.x, 60) / 2)},${number(c.sensor.positionY, number(c.scene.y, 60) / 2)},${number(c.sensor.height, 3000)}</sensorPosition>
        <FOV>${Math.max(0.1, Math.min(120, number(c.sensor.fov, 60)))}</FOV>${cruiseXml}
        <observationSampling principalPlane="${c.sensor.principalPlane ? 1 : 0}" hemisphere="${c.sensor.hemisphere ? 1 : 0}" hemisphereCount="128" customVza="${number(c.sensor.vza, 0)}" customVaa="${effectivePerspectiveAzimuth}" />
        <viewAngle><viewAngle type="custom">${viewAngleXml}</viewAngle></viewAngle>
      </sensor>
    </Sensor>
  </Geometry>
  <Atmosphere>
    <enabled>${atmosphere.enabled ? 1 : 0}</enabled>
    <model>${xmlEscape(atmosphere.model || 'midlatitude-summer')}</model>
    <waterVapor>${Math.max(0.5, Math.min(5, number(atmosphere.waterVapor, 2)))}</waterVapor>
    <aerosol>${xmlEscape(atmosphere.aerosol || 'rural')}</aerosol>
    <visibility>${Math.max(10, Math.min(50, number(atmosphere.visibility, 23)))}</visibility>
    <lutFile>${xmlEscape(atmosphereLut)}</lutFile>
  </Atmosphere>
  <Scene>
    <sceneSizeX>${number(c.scene.x, 8)}</sceneSizeX>
    <sceneSizeY>${number(c.scene.y, 8)}</sceneSizeY>
    <Height>${number(c.scene.height, 4)}</Height>
    <offsetX>${number(c.scene.offsetX, 0)}</offsetX>
    <offsetY>${number(c.scene.offsetY, 0)}</offsetY>
    <offsetZ>${number(c.scene.offsetZ, 0)}</offsetZ>
    <voxelSize>${number(c.scene.voxel, 1)}</voxelSize>
    <voxelFillThreshold>${number(c.scene.voxelFillThreshold, 0.05)}</voxelFillThreshold>
    <bgSpectral>${xmlEscape(background.spectralName || 'soil')}</bgSpectral>
    <bgThermal>${xmlEscape(background.thermalName || 'soil_temperature')}</bgThermal>
    <bgBioType>${xmlEscape(backgroundType)}</bgBioType>
    <bgBioName>${xmlEscape(background.materialName || 'soilset')}</bgBioName>
    <backgroundHeterogeneity>${background.heterogeneityEnabled ? 'Hapke' : 'None'}</backgroundHeterogeneity>
    <backgroundAngularStrength>${background.heterogeneityEnabled ? Math.max(0, Math.min(1, number(background.angularEffectStrength, 0.5))) : 0}</backgroundAngularStrength>${demFile}
    <Object>
${objects}
    </Object>
  </Scene>
  <Attribute>
    <Spectral>
      ${spectra}
    </Spectral>
    <Thermal>${thermals}</Thermal>
    <Canopy>${canopies}</Canopy>
    <Biochemistry>${materials}</Biochemistry>
    <Aerodynamics><aerodynamic name="default"><type>0</type><L>0</L><ustar>0.3</ustar><hc_veg>1</hc_veg><hc_build>1</hc_build><lai>0</lai><leaf_width>0.1</leaf_width><stepsizeatmos>1000</stepsizeatmos></aerodynamic></Aerodynamics>
  </Attribute>
  <Meteorology>
    <startTimeNode>${number(c.meteo.start, 1)}</startTimeNode><endTimeNode>${number(c.meteo.end, 48)}</endTimeNode>
    <z>${number(c.meteo.z, 15)}</z><Tsold>${number(c.meteo.Tsold, 300)}</Tsold><SatWater>${number(c.meteo.SatWater, 0.45)}</SatWater><dTime>${number(c.meteo.dTime, 1800)}</dTime>
    <filePath>${xmlEscape(paths.meteoPath || definedFile('meteo.txt'))}</filePath>
    <Latitude>${number(c.meteo.latitude, 40)}</Latitude><Longitude>${number(c.meteo.longitude, 116)}</Longitude>
  </Meteorology>
</HiStreamProject>
`
}
