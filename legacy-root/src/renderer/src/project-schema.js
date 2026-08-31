export const PROJECT_SCHEMA_VERSION = 1

export const PROJECT_MODES = ['eVoxelEB', 'eFacetEB', 'eFacetRT', 'eVoxelRT', 'eRaytracing']

const number = (value, fallback) => Number.isFinite(Number(value)) ? Number(value) : fallback

export function createMaterialPresets() {
  return {
    spectra: [
      { name: 'soil', label: '典型土壤', model: 'BSM', reflectance: '0.20', transmittance: '0.0', refTir: 0.05, tauTir: 0, params: { SMC: 25, BSMBrightness: 0.5, BSMlat: 0, BSMlon: 0 } },
      { name: 'green_leaf', label: '健康绿叶', model: 'Prospect', reflectance: '0.08,0.45', transmittance: '0.04,0.35', refTir: 0.03, tauTir: 0, params: { Cab: 40, Cw: 0.01, Cdm: 0.01, Cs: 0, N: 1.5 } },
      { name: 'dry_leaf', label: '干燥叶片', model: 'Prospect', reflectance: '0.16,0.38', transmittance: '0.08,0.25', refTir: 0.05, tauTir: 0, params: { Cab: 15, Cw: 0.003, Cdm: 0.02, Cs: 0, N: 1.8 } },
      { name: 'concrete', label: '混凝土', model: 'custom', reflectance: '0.35', transmittance: '0.0', refTir: 0.08, tauTir: 0, params: {} },
      { name: 'water_surface', label: '水面', model: 'custom', reflectance: '0.04', transmittance: '0.0', refTir: 0.02, tauTir: 0, params: {} }
    ],
    canopies: [
      { name: 'canopy_default', label: '默认植被冠层', lai: 3, density: 1, hc: 2, G: 0.5, LIDFa: -0.35, LIDFb: -0.15, hspot: 0.2, leafwidth: 0.1 }
    ],
    thermals: [
      { name: 'soil_temperature', label: '土壤温度', sunlitTemperature: 305, shadedTemperature: 295 },
      { name: 'vegetation_temperature', label: '植被温度', sunlitTemperature: 300, shadedTemperature: 296 },
      { name: 'building_temperature', label: '建筑表面温度', sunlitTemperature: 310, shadedTemperature: 300 },
      { name: 'water_temperature', label: '水体温度', sunlitTemperature: 295, shadedTemperature: 295 }
    ],
    materials: [
      {
        name: 'leaf_c3', label: 'C3 植被', type: 'Vegetation',
        params: { Vcmax: 80, m: 9, BallBerry: 0.01, Type: 3, kV: 0.6396, Rdparam: 0.015, Tparam: '0.2,0.3,288,313,328', Tyear: 25, beta: 0.507, kNPQs: 0, qLs: 1, stressfactor: 1, Tcor: 0 }
      },
      {
        name: 'leaf_c4', label: 'C4 植被', type: 'Vegetation',
        params: { Vcmax: 60, m: 4, BallBerry: 0.01, Type: 4, kV: 0.7, Rdparam: 0.012, Tparam: '0.2,0.3,288,313,328', Tyear: 25, beta: 0.507, kNPQs: 0, qLs: 1, stressfactor: 1, Tcor: 0 }
      },
      { name: 'soilset', label: '典型壤土', type: 'Soil', params: { method: 1, rss: 2000, cs: 1180, rhos: 1800, lambdas: 1.55, Tsoil: 25, SMC: 25, Satwater: 0.45 } },
      { name: 'soil_dry', label: '干燥土壤', type: 'Soil', params: { method: 1, rss: 4000, cs: 1000, rhos: 1600, lambdas: 0.8, Tsoil: 30, SMC: 10, Satwater: 0.38 } },
      { name: 'soil_wet', label: '湿润土壤', type: 'Soil', params: { method: 1, rss: 800, cs: 1400, rhos: 1900, lambdas: 2.0, Tsoil: 22, SMC: 35, Satwater: 0.50 } },
      { name: 'water_set', label: '静水表面', type: 'Water', params: { rss: 0, heatCapacity: 4186000, mixingDepth: 0.5, evaporationCoefficient: 1 } }
    ]
  }
}

export function createDefaultProject(options = {}) {
  const mode = PROJECT_MODES.includes(options.mode) ? options.mode : 'eFacetRT'
  const now = new Date().toISOString()
  const presets = createMaterialPresets()
  return {
    schemaVersion: PROJECT_SCHEMA_VERSION,
    name: String(options.name || '未命名工程'),
    mode,
    createdAt: options.createdAt || now,
    updatedAt: now,
    inputFile: 'Input.xml',
    folders: {
      models: 'models',
      positions: 'positions',
      spectral: 'spectral',
      meteorology: 'meteorology',
      output: 'output'
    },
    configuration: {
      outDir: 'output',
      scene: {
        x: number(options.sceneX, 60),
        y: number(options.sceneY, 45),
        height: number(options.sceneHeight, 15),
        voxel: number(options.voxelSize, 1),
        terrain: false
      },
      light: { zenith: 30, azimuth: 135, direct: 0.8, diffuse: 0.2, skyTemperature: 250 },
      sensor: { x: 16, y: 16, bands: '10500', vza: 0, vaa: 0, image: true, temperature: true, albedo: false },
      control: { depth: 4, gpu: 0 },
      objects: { count: 0, names: ['均质地表'], items: [] },
      spectra: presets.spectra,
      canopies: presets.canopies,
      thermals: presets.thermals,
      materials: presets.materials,
      meteo: { path: 'defined/meteo.txt', start: 24, end: 25 }
    }
  }
}

export function normalizeProject(value = {}) {
  const defaults = createDefaultProject(value)
  const configuration = value.configuration || {}
  return {
    ...defaults,
    ...value,
    schemaVersion: PROJECT_SCHEMA_VERSION,
    mode: PROJECT_MODES.includes(value.mode) ? value.mode : defaults.mode,
    folders: { ...defaults.folders, ...(value.folders || {}) },
    configuration: {
      ...defaults.configuration,
      ...configuration,
      scene: { ...defaults.configuration.scene, ...(configuration.scene || {}) },
      light: { ...defaults.configuration.light, ...(configuration.light || {}) },
      sensor: { ...defaults.configuration.sensor, ...(configuration.sensor || {}) },
      control: { ...defaults.configuration.control, ...(configuration.control || {}) },
      objects: { ...defaults.configuration.objects, ...(configuration.objects || {}) },
      spectra: Array.isArray(configuration.spectra) && configuration.spectra.length ? configuration.spectra : defaults.configuration.spectra,
      canopies: Array.isArray(configuration.canopies) && configuration.canopies.length ? configuration.canopies : defaults.configuration.canopies,
      thermals: Array.isArray(configuration.thermals) && configuration.thermals.length ? configuration.thermals : defaults.configuration.thermals,
      materials: Array.isArray(configuration.materials) && configuration.materials.length ? configuration.materials : defaults.configuration.materials,
      meteo: { ...defaults.configuration.meteo, ...(configuration.meteo || {}) }
    }
  }
}

const xmlEscape = (value) => String(value ?? '')
  .replaceAll('&', '&amp;')
  .replaceAll('<', '&lt;')
  .replaceAll('>', '&gt;')
  .replaceAll('"', '&quot;')
  .replaceAll("'", '&apos;')

function objectXml(item, index) {
  const name = xmlEscape(item.name || `object_${index + 1}`)
  const dimensions = Array.isArray(item.dimensions) && item.dimensions.length >= 3 ? item.dimensions.join(',') : '1,1,1'
  const meshes = Array.isArray(item.meshes) && item.meshes.length ? item.meshes : []
  const meshNames = meshes.length ? meshes.map((mesh) => mesh.name).join(',') : (item.meshNames || name)
  const spectralNames = meshes.length ? meshes.map((mesh) => mesh.spectralName).join(',') : (item.spectralName || 'soil')
  const thermalNames = meshes.length ? meshes.map((mesh) => mesh.thermalName).join(',') : (item.thermalName || 'soil_temperature')
  const defaultMaterial = item.type === 'Vegetation' ? 'leaf_c3' : item.type === 'Water' ? 'water_set' : 'soilset'
  const materialName = item.materialName || item.bioName || defaultMaterial
  const canopyName = item.canopyName || 'canopy_default'
  const fileName = item.fileName ? `\n        <fileName>${xmlEscape(item.fileName)}</fileName>\n        <objectfile>${xmlEscape(item.fileName)}</objectfile>` : ''
  const position = item.positionFile ? `\n        <objectPosition>${xmlEscape(item.positionFile)}</objectPosition>\n        <isdisfromFile>1</isdisfromFile>` : '\n        <isdisfromFile>0</isdisfromFile>'
  return `      <object objName="${name}">
        <types>${xmlEscape(item.type || 'Vegetation')}</types>
        <meshNames>${xmlEscape(meshNames)}</meshNames>
        <spectralNames>${xmlEscape(spectralNames)}</spectralNames>
        <thermalNames>${xmlEscape(thermalNames)}</thermalNames>
        <canopyNames>${xmlEscape(canopyName)}</canopyNames>
        <bioNames>${xmlEscape(materialName)}</bioNames>
        <propNames>${xmlEscape(materialName)}</propNames>
        <shapeTypes>${xmlEscape(item.shape || 'cube')}</shapeTypes>
        <shapes>${xmlEscape(dimensions)}</shapes>
        <isLarge>0</isLarge>${fileName}${position}
      </object>`
}

function spectrumXml(item, index) {
  const model = ['custom', 'Prospect', 'BSM'].includes(item.model) ? item.model : 'custom'
  const common = `<reflectance>${xmlEscape(item.reflectance || '0.20')}</reflectance><transmittance>${xmlEscape(item.transmittance || '0.0')}</transmittance>\n        <tau_TIR>${number(item.tauTir, 0)}</tau_TIR><ref_TIR>${number(item.refTir, 0.05)}</ref_TIR>`
  const params = item.params || {}
  if (model === 'Prospect') return `<spectral id="${index + 1}" name="${xmlEscape(item.name)}" type="Prospect">\n        ${common}\n        <Cab>${number(params.Cab, 40)}</Cab><Cw>${number(params.Cw, 0.01)}</Cw><Cdm>${number(params.Cdm, 0.01)}</Cdm><Cs>${number(params.Cs, 0)}</Cs><N>${number(params.N, 1.5)}</N>\n      </spectral>`
  if (model === 'BSM') return `<spectral id="${index + 1}" name="${xmlEscape(item.name)}" type="BSM">\n        ${common}\n        <SMC>${number(params.SMC, 25)}</SMC><BSMBrightness>${number(params.BSMBrightness, 0.5)}</BSMBrightness><BSMlat>${number(params.BSMlat, 0)}</BSMlat><BSMlon>${number(params.BSMlon, 0)}</BSMlon>\n      </spectral>`
  return `<spectral id="${index + 1}" name="${xmlEscape(item.name)}" type="custom">\n        ${common}<spectral_file>${xmlEscape(item.fileName || '')}</spectral_file>\n      </spectral>`
}

function canopyXml(item) {
  return `<canopy name="${xmlEscape(item.name)}"><lai>${number(item.lai, 3)}</lai><density>${number(item.density, 1)}</density><hc>${number(item.hc, 2)}</hc><G>${number(item.G, 0.5)}</G><LIDFa>${number(item.LIDFa, -0.35)}</LIDFa><LIDFb>${number(item.LIDFb, -0.15)}</LIDFb><hspot>${number(item.hspot, 0.2)}</hspot><leafwidth>${number(item.leafwidth, 0.1)}</leafwidth></canopy>`
}

function thermalXml(item, index) {
  return `<thermal id="${index + 1}" name="${xmlEscape(item.name)}"><sunlitTemperature>${number(item.sunlitTemperature, 305)}</sunlitTemperature><shadedTemperature>${number(item.shadedTemperature, 295)}</shadedTemperature></thermal>`
}

function materialXml(item) {
  const params = item.params || {}
  const name = xmlEscape(item.name)
  if (item.type === 'Vegetation') return `<property><leafBio name="${name}"><Vcmax>${number(params.Vcmax, 80)}</Vcmax><m>${number(params.m, 9)}</m><BallBerry>${number(params.BallBerry, 0.01)}</BallBerry><Type>${number(params.Type, 3)}</Type><kV>${number(params.kV, 0.6396)}</kV><Rdparam>${number(params.Rdparam, 0.015)}</Rdparam><Tparam>${xmlEscape(params.Tparam || '0.2,0.3,288,313,328')}</Tparam><Tyear>${number(params.Tyear, 25)}</Tyear><beta>${number(params.beta, 0.507)}</beta><kNPQs>${number(params.kNPQs, 0)}</kNPQs><qLs>${number(params.qLs, 1)}</qLs><stressfactor>${number(params.stressfactor, 1)}</stressfactor><Tcor>${number(params.Tcor, 0)}</Tcor></leafBio></property>`
  if (item.type === 'Water') return `<property><waterSet name="${name}"><rss>${number(params.rss, 0)}</rss><heatCapacity>${number(params.heatCapacity, 4186000)}</heatCapacity><mixingDepth>${number(params.mixingDepth, 0.5)}</mixingDepth><evaporationCoefficient>${number(params.evaporationCoefficient, 1)}</evaporationCoefficient></waterSet></property>`
  return `<property><soilSet name="${name}"><method>${number(params.method, 1)}</method><rss>${number(params.rss, 2000)}</rss><cs>${number(params.cs, 1180)}</cs><rhos>${number(params.rhos, 1800)}</rhos><lambdas>${number(params.lambdas, 1.55)}</lambdas><Tsoil>${number(params.Tsoil, 0.25)}</Tsoil><SMC>${number(params.SMC, 25)}</SMC><Satwater>${number(params.Satwater, 0.45)}</Satwater></soilSet></property>`
}

export function projectToXml(value, paths = {}) {
  const project = normalizeProject(value)
  const c = project.configuration
  const outputDir = paths.outputDir || c.outDir || 'output'
  const definedDir = paths.definedDir || 'defined'
  const slash = String(definedDir).includes('\\') ? '\\' : '/'
  const definedFile = (name) => `${String(definedDir).replace(/[\\/]$/, '')}${slash}${name}`
  const objects = (c.objects.items || []).map(objectXml).join('\n')
  const spectra = c.spectra.map(spectrumXml).join('\n      ')
  const canopies = c.canopies.map(canopyXml).join('\n      ')
  const thermals = c.thermals.map(thermalXml).join('\n      ')
  const materials = c.materials.map(materialXml).join('\n      ')
  return `<?xml version="1.0" encoding="UTF-8"?>
<HiStreamProject schemaVersion="${PROJECT_SCHEMA_VERSION}" mode="${xmlEscape(project.mode)}">
  <Control>
    <outDir>${xmlEscape(outputDir)}</outDir>
    <rayTracingDepth>${number(c.control.depth, 4)}</rayTracingDepth>
    <GPU>${number(c.control.gpu, 0)}</GPU>
    <isDEM>${c.scene.terrain ? 1 : 0}</isDEM>
    <isImage>${c.sensor.image ? 1 : 0}</isImage>
    <isTemperature>${c.sensor.temperature ? 1 : 0}</isTemperature>
    <isAlbedo>${c.sensor.albedo ? 1 : 0}</isAlbedo>
    <isDisplay>0</isDisplay>
    <isOrth>0</isOrth>
    <isUAVtrave>0</isUAVtrave>
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
        <pixelResolutionX>${number(c.sensor.x, 16)}</pixelResolutionX>
        <pixelResolutionY>${number(c.sensor.y, 16)}</pixelResolutionY>
        <controlBand>${xmlEscape(c.sensor.bands || '10500')}</controlBand>
        <viewAngle><viewAngle type="custom"><viewAngles>${number(c.sensor.vza, 0)},${number(c.sensor.vaa, 0)}</viewAngles></viewAngle></viewAngle>
      </sensor>
    </Sensor>
  </Geometry>
  <Scene>
    <sceneSizeX>${number(c.scene.x, 8)}</sceneSizeX>
    <sceneSizeY>${number(c.scene.y, 8)}</sceneSizeY>
    <Height>${number(c.scene.height, 4)}</Height>
    <voxelSize>${number(c.scene.voxel, 1)}</voxelSize>
    <bgSpectral>soil</bgSpectral>
    <bgThermal>soil_temperature</bgThermal>
    <bgBioType>Soil</bgBioType>
    <bgBioName>soilset</bgBioName>
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
    <startTimeNode>${number(c.meteo.start, 24)}</startTimeNode><endTimeNode>${number(c.meteo.end, 25)}</endTimeNode>
    <z>1</z><Tsold>300</Tsold><SatWater>0.45</SatWater><dTime>1800</dTime>
    <filePath>${xmlEscape(paths.meteoPath || definedFile('meteo.txt'))}</filePath>
    <Latitude>40</Latitude><Longitude>116</Longitude>
  </Meteorology>
</HiStreamProject>
`
}
