import { createServer } from 'node:http'
import { execFile, spawn } from 'node:child_process'
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads'
import { createInterface } from 'node:readline'
import { appendFileSync, closeSync, copyFileSync, cpSync, createReadStream, existsSync, mkdirSync, openSync, readFileSync, readSync, readdirSync, renameSync, statSync, unlinkSync, writeFileSync, writeSync } from 'node:fs'
import { inflateSync } from 'node:zlib'
import { basename, dirname, extname, isAbsolute, join, relative, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createDefaultProject, DEFAULT_SCENE_MODEL, normalizeProject, PROJECT_MODES, sensorBandValues, sensorViewAngles, stringifyProject, validateProject } from './src/renderer/src/project-schema.js'
import { prepareRuntimeSceneProject } from './tools/runtime-scene.mjs'

const PROJECT_ROOT = dirname(fileURLToPath(import.meta.url))
const RESOURCE_ROOT = resolve(process.env.STREAMSIM_RESOURCE_ROOT || PROJECT_ROOT)
const GUI_OUTPUT_DIR = join(RESOURCE_ROOT, 'gui')
const apiOnly = process.argv.includes('--api-only')
const modes = new Set(PROJECT_MODES)
const MODEL_ROOT = join(PROJECT_ROOT, 'models')
const MAX_FACET_TRIANGLES = 1_000_000
const defaultSceneAsset = join(RESOURCE_ROOT, 'assets', 'obj-library', 'building', 'house_a.obj')
const histreamCandidates = [
  join(RESOURCE_ROOT, 'engine', 'histream.exe'),
  join(MODEL_ROOT, 'bin_x64', 'Release', 'histream.exe'),
  join(MODEL_ROOT, 'bin_x64', 'Debug', 'histream.exe'),
  join(MODEL_ROOT, 'histream', 'bin', 'Debug', 'histream.exe'),
  join(MODEL_ROOT, 'histream', 'bin', 'Release', 'histream.exe')
]
const mime = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.svg': 'image/svg+xml', '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.gif': 'image/gif', '.bmp': 'image/bmp' }
const resultImageMime = { '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.gif': 'image/gif', '.bmp': 'image/bmp' }
let child = null
let childExecutable = ''
let resettingProcesses = false
let projectFile = ''
let projectDir = ''
const clients = new Set()

function executable() {
  return histreamCandidates.find(existsSync) || histreamCandidates[0]
}

function processExists(pid) {
  if (!pid) return false
  try { process.kill(pid, 0); return true }
  catch { return false }
}

function taskkill(args, label) {
  return new Promise((resolveTaskkill) => {
    execFile('taskkill.exe', args, { windowsHide: true, encoding: 'utf8', maxBuffer: 1024 * 1024 }, (error, stdout, stderr) => {
      resolveTaskkill({ label, killed: !error, detail: String(stdout || stderr || '').trim() })
    })
  })
}

async function resetSimulationProcesses() {
  if (resettingProcesses) throw new Error('模拟环境正在重置')
  resettingProcesses = true
  const running = child
  const runningPid = running?.pid || 0
  if (running) running.streamsimReset = true
  if (running?.facetWorker) await running.facetWorker.terminate()
  const results = []
  try {
    if (process.platform === 'win32') {
      if (runningPid) results.push(await taskkill(['/PID', String(runningPid), '/T', '/F'], `PID ${runningPid}`))
      const imageNames = new Set(['histream.exe', 'radiosity_web_runner.exe'])
      if (childExecutable) imageNames.add(basename(childExecutable))
      for (const name of imageNames) results.push(await taskkill(['/IM', name, '/T', '/F'], name))
    } else if (running) {
      running.kill('SIGKILL')
      results.push({ label: `PID ${runningPid}`, killed: true, detail: '' })
    }
    if (runningPid && processExists(runningPid)) {
      if (running) running.streamsimReset = false
      child = running
      throw new Error(`无法终止模拟进程 PID ${runningPid}`)
    }
    child = null
    childExecutable = ''
    return { activePid: runningPid || null, terminated: results.filter((item) => item.killed).map((item) => item.label) }
  } finally {
    resettingProcesses = false
  }
}


function json(response, status, value) {
  response.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8' })
  response.end(JSON.stringify(value))
}

function emit(payload) {
  const line = `data: ${JSON.stringify(payload)}\n\n`
  for (const response of clients) response.write(line)
}

async function body(request) {
  const chunks = []
  for await (const chunk of request) chunks.push(chunk)
  return JSON.parse(Buffer.concat(chunks).toString('utf8') || '{}')
}

function sanitizeObjContent(value) {
  const content = String(value || '')
    .split(/\r?\n/)
    .filter((line) => !/^\s*(mtllib|usemtl)\b/i.test(line))
    .join('\n')
    .trimEnd()
  return content ? content + '\n' : ''
}

function normalizeSpectrumContent(value) {
  const rows = String(value || '').replace(/^\uFEFF/, '').split(/\r?\n/)
    .map((line) => line.replace(/#.*$/, '').trim()).filter(Boolean)
  if (rows.length < 2 || rows.length > 3) throw new Error('波谱文件必须为 2 行或 3 行：波长、反射率、可选透射率')
  const parseRow = (row, label) => {
    const values = row.split(/[\s,;]+/).filter(Boolean).map(Number)
    if (values.length < 2 || !values.every(Number.isFinite)) throw new Error(`${label}必须包含至少 2 个有效数值`)
    return values
  }
  const wavelengths = parseRow(rows[0], '第一行波长')
  const reflectance = parseRow(rows[1], '第二行反射率')
  const transmittance = rows[2] ? parseRow(rows[2], '第三行透射率') : wavelengths.map(() => 0)
  if (reflectance.length !== wavelengths.length || transmittance.length !== wavelengths.length) throw new Error('波长、反射率和透射率的数值数量必须相同')
  if (wavelengths.some((value) => value <= 0)) throw new Error('波长必须大于 0')
  const points = wavelengths.map((wavelength, index) => ({ wavelength, reflectance: reflectance[index], transmittance: transmittance[index] }))
    .sort((left, right) => left.wavelength - right.wavelength)
  if (points.some((point, index) => index > 0 && point.wavelength === points[index - 1].wavelength)) throw new Error('波长不能重复')
  if (points.some((point) => point.reflectance < 0 || point.transmittance < 0 || point.reflectance > 1 || point.transmittance > 1 || point.reflectance + point.transmittance > 1)) throw new Error('反射率、透射率必须在 0–1 内且二者之和不能超过 1')
  return [
    points.map((point) => point.wavelength).join(' '),
    points.map((point) => point.reflectance).join(' '),
    points.map((point) => point.transmittance).join(' ')
  ].join('\n') + '\n'
}

function normalizeHostPath(value) {
  const source = String(value || "").trim()
  const windowsPath = source.match(/^([A-Za-z]):(.*)$/)
  if (process.platform === "linux" && windowsPath) return "/mnt/" + windowsPath[1].toLowerCase() + "/" + windowsPath[2].replaceAll("\\", "/").replace(/^\/+/, '')
  const wslPath = source.match(/^\/mnt\/([A-Za-z])\/(.*)$/)
  if (process.platform === "win32" && wslPath) return wslPath[1].toUpperCase() + ":" + String.fromCharCode(92) + wslPath[2].replaceAll("/", String.fromCharCode(92))
  return source
}

function histreamRuntimePath(value, enginePath = executable()) {
  const source = String(value || '').trim()
  if (process.platform !== 'linux' || extname(enginePath).toLowerCase() !== '.exe') return source
  const wslPath = source.match(/^\/mnt\/([A-Za-z])\/(.*)$/)
  return wslPath
    ? wslPath[1].toUpperCase() + ':\\' + wslPath[2].replaceAll('/', '\\')
    : source
}

function windowsPath(value) {
  const source = String(value || '').trim()
  const wslPath = source.match(/^\/mnt\/([A-Za-z])\/(.*)$/)
  return wslPath
    ? wslPath[1].toUpperCase() + ':\\' + wslPath[2].replace(/^\/+/, '').replaceAll('/', '\\')
    : source
}

function projectStoredPath(value, baseDir) {
  const source = String(value || '').trim()
  if (!source) return ''
  const hostPath = normalizeHostPath(source)
  if (!isAbsolute(hostPath)) return hostPath.replaceAll('\\', '/')
  const absolutePath = resolve(hostPath)
  const local = relative(resolve(baseDir), absolutePath)
  if (local && local !== '..' && !local.startsWith('../') && !isAbsolute(local))
    return local.replaceAll('\\', '/')
  return windowsPath(absolutePath)
}

function normalizeStoredProjectPaths(project, baseDir) {
  const configuration = project?.configuration
  if (!configuration) return project
  if (configuration.outDir) configuration.outDir = projectStoredPath(configuration.outDir, baseDir)
  if (configuration.meteo?.path) configuration.meteo.path = projectStoredPath(configuration.meteo.path, baseDir)
  if (configuration.scene?.demFile) configuration.scene.demFile = projectStoredPath(configuration.scene.demFile, baseDir)
  for (const item of configuration.objects?.items || []) {
    if (item.fileName) item.fileName = projectStoredPath(item.fileName, baseDir)
    if (item.positionFile) item.positionFile = projectStoredPath(item.positionFile, baseDir)
  }
  for (const item of configuration.spectra || []) {
    if (item.fileName) item.fileName = projectStoredPath(item.fileName, baseDir)
    if (item.physicalTexture?.fileName)
      item.physicalTexture.fileName = projectStoredPath(item.physicalTexture.fileName, baseDir)
  }
  return project
}

function resolveProjectFile(value) {
  let path = normalizeHostPath(value).replace(/^"|"$/g, '')
  if (!path) throw new Error('请输入已有工程目录或 project.json 完整路径')
  path = resolve(path)
  if (!existsSync(path)) throw new Error(`找不到已有工程：${path}`)

  if (statSync(path).isDirectory()) {
    path = join(path, 'project.json')
    if (!existsSync(path)) throw new Error(`工程目录中找不到 project.json：${dirname(path)}`)
  }

  if (!statSync(path).isFile()) throw new Error(`工程入口不是文件：${path}`)
  if (basename(path).toLowerCase() !== 'project.json') throw new Error('工程入口必须是 project.json')
  let source
  try { source = JSON.parse(readFileSync(path, 'utf8')) }
  catch (error) { throw new Error(`project.json 格式错误：${error.message}`) }
  const validation = validateProject(source)
  if (!validation.valid) throw new Error(validation.errors.join('；'))
  return path
}

function projectFromSource(source) {
  if (source && typeof source === 'object') return normalizeProject(source)
  let parsed
  try { parsed = JSON.parse(String(source || '')) }
  catch (error) { throw new Error(`project.json 格式错误：${error.message}`) }
  return normalizeProject(parsed)
}

const objTriangleCountCache = new Map()

function projectAssetPath(value, baseDir) {
  const hostPath = normalizeHostPath(value)
  if (!hostPath) return ''
  if (isAbsolute(hostPath)) return resolve(hostPath)
  const localPath = resolve(baseDir, hostPath)
  const sharedPath = /^assets[\\/]/i.test(hostPath) ? resolve(RESOURCE_ROOT, hostPath) : ''
  return sharedPath && !existsSync(localPath) && existsSync(sharedPath) ? sharedPath : localPath
}

function objTriangleCount(path) {
  if (!path || !existsSync(path) || !statSync(path).isFile()) return 0
  const info = statSync(path)
  const cached = objTriangleCountCache.get(path)
  if (cached?.size === info.size && cached?.mtimeMs === info.mtimeMs) return cached.triangles
  const source = readFileSync(path, 'utf8')
  let triangles = 0
  for (const match of source.matchAll(/^[ \t]*f[ \t]+([^\r\n#]+)/gm)) {
    const vertices = match[1].trim().split(/[ \t]+/).filter(Boolean).length
    if (vertices >= 3) triangles += vertices - 2
  }
  objTriangleCountCache.set(path, { size: info.size, mtimeMs: info.mtimeMs, triangles })
  return triangles
}

function positionInstanceCount(path) {
  if (!path || !existsSync(path) || !statSync(path).isFile()) return 1
  let count = 0
  for (const line of readFileSync(path, 'utf8').split(/\r?\n/)) {
    const value = line.replace(/#.*/, '').trim()
    if (value && !value.startsWith('//')) count += 1
  }
  return count
}

function estimateFacetTriangles(project, baseDir) {
  const details = []
  let total = 2
  for (const item of project.configuration?.objects?.items || []) {
    if (item.type === 'Fire' || item.type === 'Fog' || item.medium) continue
    const objectPath = projectAssetPath(item.fileName, baseDir)
    const triangles = item.fileName ? objTriangleCount(objectPath) : 12
    if (!triangles) continue
    const positionPath = projectAssetPath(item.positionFile, baseDir)
    const instances = item.positionFile ? positionInstanceCount(positionPath) : 1
    const expandedTriangles = triangles * instances
    total += expandedTriangles
    details.push({
      name: String(item.name || basename(objectPath, extname(objectPath)) || '未命名对象'),
      triangles,
      instances,
      expandedTriangles
    })
  }
  details.sort((left, right) => right.expandedTriangles - left.expandedTriangles)
  return { total, details }
}

function assertFacetScale(project, baseDir, mode) {
  const estimate = estimateFacetTriangles(project, baseDir)
  if (estimate.total <= MAX_FACET_TRIANGLES) return estimate
  const largest = estimate.details[0]
  const source = largest
    ? `最大来源：${largest.name}（${largest.triangles.toLocaleString('zh-CN')} 面 × ${largest.instances.toLocaleString('zh-CN')} 实例 = ${largest.expandedTriangles.toLocaleString('zh-CN')} 面）。`
    : ''
  const recommendation = mode === 'eFacetEB'
    ? '请切换到“体元辐射传输与能量平衡（Voxel RT–EB）”'
    : '请切换到“体元辐射传输（VoxelRT）”'
  throw new Error(
    `面元规模预估为 ${estimate.total.toLocaleString('zh-CN')} 个三角面，超过安全上限 ${MAX_FACET_TRIANGLES.toLocaleString('zh-CN')}。` +
    `${source}${recommendation}，或减少实例数量、简化 OBJ 后再运行。`)
}

const atmosphereLutCache = new Map()

function loadAtmosphereLut(path) {
  const normalized = resolve(normalizeHostPath(path))
  if (atmosphereLutCache.has(normalized)) return atmosphereLutCache.get(normalized)
  if (!existsSync(normalized)) throw new Error(`找不到大气查找表：${normalized}`)
  const spectra = new Map(), waterVapor = new Set(), visibility = new Set(), altitude = new Set(), zenith = new Set()
  let version2 = false
  for (const line of readFileSync(normalized, 'utf8').split(/\r?\n/)) {
    if (!line || line.startsWith('#')) continue
    if (line.startsWith('atmosphere_model')) { version2 = true; continue }
    if (line.startsWith('visibility_km')) continue
    const cells = line.split(',')
    const model = version2 ? cells[0] : 'midlatitude-summer'
    const aerosol = version2 ? cells[1] : 'rural'
    const values = (version2 ? cells.slice(2) : cells).map(Number)
    if (values.length < (version2 ? 7 : 6) || !values.every(Number.isFinite)) continue
    const [water, v, a, z, wavelength, transmittance, pathRadiance] = version2
      ? values : [2, ...values]
    waterVapor.add(water); visibility.add(v); altitude.add(a); zenith.add(z)
    const key = `${model}|${aerosol}|${water}|${v}|${a}|${z}`
    const spectrum = spectra.get(key) || { wavelength: [], transmittance: [], pathRadiance: [] }
    spectrum.wavelength.push(wavelength)
    spectrum.transmittance.push(Math.max(0, Math.min(1, transmittance)))
    spectrum.pathRadiance.push(Math.max(0, pathRadiance))
    spectra.set(key, spectrum)
  }
  const table = {
    spectra,
    waterVapor: [...waterVapor].sort((a, b) => a - b),
    visibility: [...visibility].sort((a, b) => a - b),
    altitude: [...altitude].sort((a, b) => a - b),
    zenith: [...zenith].sort((a, b) => a - b)
  }
  if (!spectra.size || !table.visibility.length) throw new Error(`大气查找表为空：${normalized}`)
  atmosphereLutCache.set(normalized, table)
  return table
}

function atmosphereAxisBracket(axis, value) {
  if (value <= axis[0]) return [axis[0], axis[0]]
  if (value >= axis.at(-1)) return [axis.at(-1), axis.at(-1)]
  const right = axis.findIndex((item) => item >= value)
  return [axis[right - 1], axis[right]]
}

function atmosphereAxisWeight(value, low, high, upper) {
  if (high <= low) return upper ? 0 : 1
  const fraction = Math.max(0, Math.min(1, (value - low) / (high - low)))
  return upper ? fraction : 1 - fraction
}

function atmosphereSpectralSample(spectrum, wavelength) {
  const wavelengths = spectrum.wavelength
  if (wavelength <= wavelengths[0]) return [spectrum.transmittance[0], spectrum.pathRadiance[0]]
  if (wavelength >= wavelengths.at(-1)) return [spectrum.transmittance.at(-1), spectrum.pathRadiance.at(-1)]
  let low = 0, high = wavelengths.length - 1
  while (high - low > 1) {
    const middle = (low + high) >> 1
    if (wavelengths[middle] < wavelength) low = middle
    else high = middle
  }
  const weight = (wavelength - wavelengths[low]) / (wavelengths[high] - wavelengths[low])
  return [
    spectrum.transmittance[low] + (spectrum.transmittance[high] - spectrum.transmittance[low]) * weight,
    spectrum.pathRadiance[low] + (spectrum.pathRadiance[high] - spectrum.pathRadiance[low]) * weight
  ]
}

function sampleAtmosphereLut(table, model, aerosol, waterVapor, visibility, altitude, zenith, wavelength) {
  if (wavelength < 350 || wavelength > 14000) return { transmittance: 1, pathRadiance: 0 }
  const wb = atmosphereAxisBracket(table.waterVapor, waterVapor)
  const vb = atmosphereAxisBracket(table.visibility, visibility)
  const ab = atmosphereAxisBracket(table.altitude, altitude)
  const zb = atmosphereAxisBracket(table.zenith, Math.abs(zenith))
  let transmittance = 0, pathRadiance = 0, accumulatedWeight = 0
  for (let iw = 0; iw < 2; iw += 1) for (let iv = 0; iv < 2; iv += 1) for (let ia = 0; ia < 2; ia += 1) for (let iz = 0; iz < 2; iz += 1) {
    const w = wb[iw]
    const v = vb[iv], a = ab[ia], z = zb[iz]
    const weight = atmosphereAxisWeight(waterVapor, wb[0], wb[1], Boolean(iw))
      * atmosphereAxisWeight(visibility, vb[0], vb[1], Boolean(iv))
      * atmosphereAxisWeight(altitude, ab[0], ab[1], Boolean(ia))
      * atmosphereAxisWeight(Math.abs(zenith), zb[0], zb[1], Boolean(iz))
    if (!weight) continue
    const spectrum = table.spectra.get(`${model}|${aerosol}|${w}|${v}|${a}|${z}`)
    if (!spectrum) continue
    const sample = atmosphereSpectralSample(spectrum, wavelength)
    transmittance += weight * sample[0]
    pathRadiance += weight * sample[1]
    accumulatedWeight += weight
  }
  if (!accumulatedWeight) return { transmittance: 1, pathRadiance: 0 }
  return {
    transmittance: Math.max(0, Math.min(1, transmittance / accumulatedWeight)),
    pathRadiance: Math.max(0, pathRadiance / accumulatedWeight)
  }
}

function planckRadiance(wavelengthNm, temperatureK) {
  const wavelengthUm = wavelengthNm > 50 ? wavelengthNm / 1000 : wavelengthNm
  return 1.19104e8 / (Math.pow(wavelengthUm, 5) * Math.expm1(14387.7 / (temperatureK * wavelengthUm)))
}

function inversePlanckTemperature(wavelengthNm, radiance) {
  const wavelengthUm = wavelengthNm > 50 ? wavelengthNm / 1000 : wavelengthNm
  return 14387.7 / (wavelengthUm * Math.log(1.19104e8 / (radiance * Math.pow(wavelengthUm, 5)) + 1))
}


function outputGeometrySuffix(mode) {
  if (mode === 'eFacetRT' || mode === 'eFacetEB') return '_f'
  if (mode === 'eVoxelRT' || mode === 'eVoxelEB') return '_v'
  return ''
}

function isEnergyBalanceMode(mode) {
  return mode === "eFacetEB" || mode === "eVoxelEB"
}

function processOutputSelection(projectSource, mode) {
  const sensor = projectFromSource(projectSource).configuration.sensor
  const legacy = Boolean(sensor.process)
  const radiation = sensor.radiationProcess == null
    ? (!isEnergyBalanceMode(mode) && legacy) : Boolean(sensor.radiationProcess)
  const energy = sensor.energyProcess == null
    ? (isEnergyBalanceMode(mode) && legacy) : Boolean(sensor.energyProcess)
  return { radiation, energy }
}

function processOutputEnabled(projectSource, mode) {
  const selected = processOutputSelection(projectSource, mode)
  return isEnergyBalanceMode(mode) ? selected.radiation || selected.energy : selected.radiation
}

function outputAngleToken(value) {
  const number = Number(value)
  return Number.isFinite(number) ? number.toFixed(2) : '0.00'
}

function outputTimeToken(julianTime) {
  const value = Number(julianTime)
  if (!Number.isFinite(value)) return ''
  let day = Math.floor(value)
  let minutes = Math.round((value - day) * 1440)
  if (minutes >= 1440) { day += Math.floor(minutes / 1440); minutes %= 1440 }
  if (minutes < 0) minutes = 0
  const hour = String(Math.floor(minutes / 60)).padStart(2, '0')
  const minute = String(minutes % 60).padStart(2, '0')
  return 'DOY' + day + '_' + hour + '-' + minute
}

function outputTiffName(mode, values = {}) {
  const suffix = outputGeometrySuffix(mode) + (values.atmosphere ? '_a' : '')
  const vza = outputAngleToken(values.vza)
  const vaa = outputAngleToken(values.vaa)
  if (isEnergyBalanceMode(mode)) {
    const time = String(values.timeToken || outputTimeToken(values.julianTime) || 'UNKNOWN')
    return 'T=' + time + '_VZA=' + vza + '_VAA=' + vaa + suffix + '.tif'
  }
  return 'SZA=' + outputAngleToken(values.sza) + '_SAA=' + outputAngleToken(values.saa) +
    '_VZA=' + vza + '_VAA=' + vaa + suffix + '.tif'
}

function writeRadiosityEnvi(jsonPath) {
  if (!existsSync(jsonPath)) throw new Error(`找不到 GPU Radiosity 结果：${jsonPath}`)
  let result
  try {
    result = JSON.parse(readFileSync(jsonPath, 'utf8'))
  } catch (error) {
    throw new Error(`GPU Radiosity 结果格式错误：${error.message}`)
  }

  const facetCount = Number(result.facetCount)
  if (!Number.isInteger(facetCount) || facetCount < 1) throw new Error('GPU Radiosity 结果缺少有效的 facetCount')
  const sources = [
    ['Radiosity', result.radiosity],
    ['Light enhancement', result.lightEnhancement],
    ['Sunlit fraction', result.sunlit]
  ]
  for (const [name, values] of sources) {
    if (!Array.isArray(values) || values.length < facetCount * 2) throw new Error(`${name} 数据不完整`)
  }

  const faceValueCount = facetCount * 2
  const width = Math.ceil(Math.sqrt(faceValueCount))
  const height = Math.ceil(faceValueCount / width)
  const pixelCount = width * height
  const image = Buffer.alloc(pixelCount * sources.length * 4)
  for (let index = 0; index < pixelCount * sources.length; index += 1) image.writeFloatLE(Number.NaN, index * 4)
  for (let band = 0; band < sources.length; band += 1) {
    const values = sources[band][1]
    for (let face = 0; face < faceValueCount; face += 1) image.writeFloatLE(Number(values[face]), (band * pixelCount + face) * 4)
  }

  const imagePath = jsonPath.replace(/\.json$/i, '.img')
  const headerPath = jsonPath.replace(/\.json$/i, '.hdr')
  writeFileSync(imagePath, image)
  writeFileSync(headerPath, [
    'ENVI',
    `samples = ${width}`,
    `lines = ${height}`,
    `bands = ${sources.length}`,
    'header offset = 0',
    'file type = ENVI Standard',
    'data type = 4',
    'interleave = bsq',
    'byte order = 0',
    `band names = {${sources.map(([name]) => name).join(', ')}}`,
    ''
  ].join('\n'), 'utf8')
  return { headerPath, imagePath, width, height, bands: sources.length, faceValueCount }
}

function readRadiosityResult(jsonPath, geometryOnly = false) {
  if (!existsSync(jsonPath)) throw new Error(`找不到面元结果：${jsonPath}`)
  // 大场景/多波段面元结果 JSON 会超过 512 MB。若用 readFileSync(..., 'utf8') + JSON.parse，
  // 会触发 V8 单字符串长度上限（0x1fffffe8 字符）：
  //   Cannot create a string longer than 0x1fffffe8 characters
  // 这里改为直接基于 Buffer 逐数字解析（与 sampleRadiosityResult 同一思路），
  // 不把整个文件转换成 JS 字符串，因此不再受该上限限制。
  const source = readFileSync(jsonPath)
  const header = source.subarray(0, Math.min(source.length, 1 << 16)).toString('utf8')
  const facetCount = Number(header.match(/"facetCount"\s*:\s*(\d+)/)?.[1])
  if (!Number.isInteger(facetCount) || facetCount < 1) throw new Error('面元结果缺少有效的 facetCount')
  const quantity = String(header.match(/"quantity"\s*:\s*"([^"]*)"/)?.[1] || '')
  const units = String(header.match(/"units"\s*:\s*"([^"]*)"/)?.[1] || '')
  const backend = String(header.match(/"backend"\s*:\s*"([^"]*)"/)?.[1] || '')
  const surfaceCount = facetCount * 2

  const readScalar = (key) => {
    const keyOffset = source.indexOf('"' + key + '"')
    if (keyOffset < 0) return undefined
    const colon = source.indexOf(58, keyOffset)
    if (colon < 0) return undefined
    let end = colon + 1
    while (end < source.length && ![44, 10, 13, 125].includes(source[end])) end += 1
    const value = Number(source.toString('ascii', colon + 1, end).trim())
    return Number.isFinite(value) ? value : undefined
  }
  const iterations = readScalar('iterations')
  const maxDelta = readScalar('maxDelta')

  // 顶层数值数组按固定顺序写入；用递增的 searchFrom 避免对超大文件反复全量扫描。
  let searchFrom = 0
  const findArray = (key) => {
    const keyOffset = source.indexOf('"' + key + '"', searchFrom)
    if (keyOffset < 0) return -1
    const arrayOffset = source.indexOf(91, keyOffset)
    if (arrayOffset < 0) throw new Error('面元结果缺少 ' + key + ' 数组')
    searchFrom = keyOffset + key.length + 2
    return arrayOffset
  }
  const readFixed = (key, count) => {
    const arrayOffset = findArray(key)
    if (arrayOffset < 0) return null
    const values = new Float32Array(count)
    let position = arrayOffset + 1
    for (let index = 0; index < count; index += 1) {
      while (position < source.length && (source[position] === 32 || source[position] === 9 || source[position] === 10 || source[position] === 13 || source[position] === 44)) position += 1
      if (position >= source.length) throw new Error(key + ' 数据不完整')
      const numberStart = position
      while (position < source.length && source[position] !== 44 && source[position] !== 93) position += 1
      if (position === numberStart) throw new Error(key + ' 数据不完整')
      const value = Number(source.toString('ascii', numberStart, position))
      if (!Number.isFinite(value)) throw new Error(key + ' 包含无效数值')
      values[index] = value
    }
    while (position < source.length && (source[position] === 32 || source[position] === 9 || source[position] === 10 || source[position] === 13)) position += 1
    if (position >= source.length || source[position] !== 93) throw new Error(key + ' 数据不完整')
    return values
  }
  const readDynamic = (key) => {
    const arrayOffset = findArray(key)
    if (arrayOffset < 0) return null
    const values = []
    let position = arrayOffset + 1
    while (true) {
      while (position < source.length && (source[position] === 32 || source[position] === 9 || source[position] === 10 || source[position] === 13 || source[position] === 44)) position += 1
      if (position >= source.length || source[position] === 93) break
      const numberStart = position
      while (position < source.length && source[position] !== 44 && source[position] !== 93) position += 1
      const value = Number(source.toString('ascii', numberStart, position))
      if (!Number.isFinite(value)) throw new Error(key + ' 包含无效数值')
      values.push(value)
    }
    return values
  }

  const arrays = {}
  arrays.vertexPositions = readFixed('vertexPositions', facetCount * 9)
  if (!arrays.vertexPositions) throw new Error('面元结果缺少完整的三角形坐标')
  if (geometryOnly) return { facetCount, vertexPositions: arrays.vertexPositions, backend, metrics: [], bandRadiosity: new Float32Array(0) }
  arrays.wavelengths = readDynamic('wavelengths')
  // 文件内的写入顺序：sunlit, radiosity, lightEnhancement, temperature, ...
  const metricKeys = ['sunlit', 'radiosity', 'lightEnhancement', 'temperature', 'netRadiation', 'sensibleHeat', 'latentHeat', 'storageHeat', 'photovoltaicPower']
  for (const key of metricKeys) arrays[key] = readFixed(key, surfaceCount)
  arrays.bandRadiosity = readFixed('bandRadiosity', surfaceCount * Math.max(1, (arrays.wavelengths || []).length))

  const wavelengths = (arrays.wavelengths || []).filter((value) => Number.isFinite(value) && value > 0)
  const bandRadiosity = arrays.bandRadiosity || new Float32Array(0)
  if (bandRadiosity.length && bandRadiosity.length < surfaceCount * Math.max(1, wavelengths.length)) {
    throw new Error('面元多波段辐亮度数据不完整')
  }

  const radianceResult = quantity.toLowerCase() === 'spectral radiance'
  const metricDefinitions = [
    ['radiosity', radianceResult ? '光谱辐亮度 [W m⁻² sr⁻¹ μm⁻¹]' : '辐射度', true],
    ['lightEnhancement', '光照增强', true],
    ['sunlit', '光照比例', true],
    ['temperature', '温度 [K]', false],
    ['netRadiation', '净辐射 [W m⁻²]', false],
    ['sensibleHeat', '显热 [W m⁻²]', false],
    ['latentHeat', '潜热 [W m⁻²]', false],
    ['storageHeat', '储热 [W m⁻²]', false],
    ['photovoltaicPower', '光伏功率 [W m⁻²]', false]
  ]
  const metrics = []
  for (const [id, name, required] of metricDefinitions) {
    const values = arrays[id]
    if (required && !values) throw new Error('面元结果缺少 ' + id + ' 数组')
    if (values) metrics.push([id, name, values])
  }
  return {
    facetCount,
    metrics,
    vertexPositions: arrays.vertexPositions,
    backend,
    iterations,
    maxDelta,
    wavelengths,
    bandRadiosity,
    quantity,
    units
  }
}

const THREE_DIMENSIONAL_FACET_SAMPLE_LIMIT = 1000000

function sampleRadiosityResult(jsonPath, maximumFacets = THREE_DIMENSIONAL_FACET_SAMPLE_LIMIT, geometryOnly = false) {
  if (!existsSync(jsonPath)) throw new Error('找不到面元结果：' + jsonPath)
  const source = readFileSync(jsonPath)
  const header = source.subarray(0, Math.min(source.length, 65536)).toString('utf8')
  const sourceFacetCount = Number(header.match(/"facetCount"\s*:\s*(\d+)/)?.[1])
  if (!Number.isInteger(sourceFacetCount) || sourceFacetCount < 1) throw new Error('面元结果缺少有效的 facetCount')
  const facetCount = Math.min(sourceFacetCount, maximumFacets)
  const sourceFacets = new Uint32Array(facetCount)
  for (let sampledFacet = 0; sampledFacet < facetCount; sampledFacet += 1) {
    sourceFacets[sampledFacet] = facetCount === sourceFacetCount
      ? sampledFacet
      : Math.min(sourceFacetCount - 1, Math.floor(sampledFacet * sourceFacetCount / facetCount))
  }

  const sampleArray = (key, valuesPerFacet) => {
    const keyOffset = source.indexOf('"' + key + '"')
    const arrayOffset = keyOffset < 0 ? -1 : source.indexOf(91, keyOffset)
    if (arrayOffset < 0) throw new Error('面元结果缺少 ' + key + ' 数组')
    const values = new Array(facetCount * valuesPerFacet)
    let position = arrayOffset + 1
    let sourceValue = 0
    let sampledFacet = 0
    let targetStart = sourceFacets[0] * valuesPerFacet
    let targetEnd = targetStart + valuesPerFacet
    while (position < source.length && sampledFacet < facetCount) {
      while (position < source.length && (source[position] === 32 || source[position] === 9 || source[position] === 10 || source[position] === 13 || source[position] === 44)) position += 1
      if (position >= source.length || source[position] === 93) break
      const numberStart = position
      while (position < source.length && source[position] !== 44 && source[position] !== 93) position += 1
      if (sourceValue >= targetStart && sourceValue < targetEnd) {
        const value = Number(source.toString('ascii', numberStart, position).trim())
        if (!Number.isFinite(value)) throw new Error(key + ' 包含无效数值')
        values[sampledFacet * valuesPerFacet + sourceValue - targetStart] = value
      }
      sourceValue += 1
      if (sourceValue === targetEnd) {
        sampledFacet += 1
        if (sampledFacet < facetCount) {
          targetStart = sourceFacets[sampledFacet] * valuesPerFacet
          targetEnd = targetStart + valuesPerFacet
        }
      }
    }
    if (sampledFacet !== facetCount) throw new Error(key + ' 数据不完整')
    return values
  }
  const scalar = (key) => {
    const keyOffset = source.indexOf('"' + key + '"')
    if (keyOffset < 0) return undefined
    const colon = source.indexOf(58, keyOffset)
    if (colon < 0) return undefined
    let end = colon + 1
    while (end < source.length && ![44, 10, 13, 125].includes(source[end])) end += 1
    const value = Number(source.toString('ascii', colon + 1, end).trim())
    return Number.isFinite(value) ? value : undefined
  }
  const backend = header.match(/"backend"\s*:\s*"([^"]*)"/)?.[1]
  const quantity = header.match(/"quantity"\s*:\s*"([^"]*)"/)?.[1] || ''
  const units = header.match(/"units"\s*:\s*"([^"]*)"/)?.[1] || ''
  const radiosityLabel = quantity.toLowerCase() === 'spectral radiance'
    ? '光谱辐亮度 [W m⁻² sr⁻¹ μm⁻¹]'
    : `辐射度${units ? ` [${units.replaceAll('-2', '⁻²')}]` : ' [W m⁻²]'}`
  const vertexPositions = sampleArray('vertexPositions', 9)
  const metricDefinitions = [
    ['radiosity', radiosityLabel, true],
    ['lightEnhancement', '光照增强 [-]', true],
    ['sunlit', '光照比例 [-]', true],
    ['temperature', '温度 [K]'],
    ['netRadiation', '净辐射 [W m⁻²]'],
    ['sensibleHeat', '显热 [W m⁻²]'],
    ['latentHeat', '潜热 [W m⁻²]'],
    ['storageHeat', '储热 [W m⁻²]']
  ]
  const metrics = metricDefinitions.filter(([key, , required]) => {
    const exists = source.indexOf('"' + key + '"') >= 0
    if (required && !exists && !geometryOnly) throw new Error('面元结果缺少 ' + key + ' 数组')
    return exists && !geometryOnly
  }).map(([id, label]) => ({ id, label, values: sampleArray(id, 2) }))

  return {
    kind: 'facet',
    path: jsonPath,
    facetCount,
    sourceFacetCount,
    sampled: facetCount < sourceFacetCount,
    metrics,
    vertexPositions,
    backend,
    iterations: scalar('iterations'),
    maxDelta: scalar('maxDelta')
  }
}

function isRadiosityResultFile(path) {
  let descriptor
  try {
    descriptor = openSync(path, 'r')
    const sample = Buffer.alloc(Math.min(65536, statSync(path).size))
    readSync(descriptor, sample, 0, sample.length, 0)
    const header = sample.toString('utf8')
    return /"backend"\s*:/.test(header) &&
      /"facetCount"\s*:\s*\d+/.test(header) &&
      /"vertexPositions"\s*:\s*\[/.test(header)
  } catch {
    return false
  } finally {
    if (descriptor != null) closeSync(descriptor)
  }
}

function processResultMetadata(path) {
  if (extname(path).toLowerCase() !== '.json' || statSync(path).size > 1024 * 1024) return null
  try {
    const metadata = JSON.parse(readFileSync(path, 'utf8'))
    return String(metadata.kind || '').endsWith('-process') ? metadata : null
  } catch {
    return null
  }
}

// FacetRT and FacetEB use the same display-only sampling limit. Files on disk
// retain every facet and all process values.
function readProcessResult(metadataPath, maximumElements = THREE_DIMENSIONAL_FACET_SAMPLE_LIMIT) {
  const metadata = processResultMetadata(metadataPath)
  if (!metadata) throw new Error('无效的三维过程结果')
  const dataPath = resolve(dirname(metadataPath), String(metadata.dataFile || ''))
  if (!existsSync(dataPath)) throw new Error('找不到过程二进制数据：' + dataPath)
  const data = readFileSync(dataPath)
  const recordFloats = Number(metadata.recordFloats)
  const fields = Array.isArray(metadata.fields)
    ? metadata.fields.filter((field) => field && typeof field === 'object' && Number.isInteger(Number(field.offset)))
    : []
  if (!Number.isInteger(recordFloats) || recordFloats < 1 || !fields.length) throw new Error('过程结果字段定义无效')
  const recordBytes = recordFloats * 4
  const sourceCount = Number(metadata.geometry === 'facet' ? metadata.facetCount : metadata.voxelCount)
  if (!Number.isInteger(sourceCount) || sourceCount < 1) throw new Error('过程结果缺少有效元素数量')

  if (metadata.geometry === 'facet') {
    const geometryPath = resolve(dirname(metadataPath), String(metadata.geometryFile || '../faceteb.json'))
    const geometry = sampleRadiosityResult(geometryPath, maximumElements, true)
    if (data.length < sourceCount * 2 * recordBytes) throw new Error('面元过程二进制数据不完整')
    const sampledMetrics = fields.map((field) => ({
      id: String(field.id || 'field_' + field.offset),
      label: String(field.label || field.id || '字段 ' + field.offset),
      values: new Array(geometry.facetCount * 2)
    }))
    for (let facet = 0; facet < geometry.facetCount; facet += 1) {
      const sourceFacet = geometry.facetCount === sourceCount
        ? facet
        : Math.min(sourceCount - 1, Math.floor(facet * sourceCount / geometry.facetCount))
      for (let side = 0; side < 2; side += 1) {
        const recordOffset = (sourceFacet * 2 + side) * recordBytes
        for (let metric = 0; metric < sampledMetrics.length; metric += 1) {
          sampledMetrics[metric].values[facet * 2 + side] =
            data.readFloatLE(recordOffset + Number(fields[metric].offset) * 4)
        }
      }
    }
    return {
      ...geometry,
      path: metadataPath,
      metrics: sampledMetrics,
      processType: metadata.processType,
      time: metadata.time,
      node: metadata.node,
      backend: 'HiStream Facet ' + String(metadata.processType || 'process')
    }
  }

  if (metadata.geometry !== 'voxel') throw new Error('不支持的过程几何类型：' + metadata.geometry)
  if (data.length < sourceCount * recordBytes) throw new Error('体元过程二进制数据不完整')
  const voxelCount = Math.min(sourceCount, maximumElements)
  const sourceVoxels = new Uint32Array(voxelCount)
  for (let voxel = 0; voxel < voxelCount; voxel += 1) {
    sourceVoxels[voxel] = voxelCount === sourceCount
      ? voxel
      : Math.min(sourceCount - 1, Math.floor(voxel * sourceCount / voxelCount))
  }
  const positionOffsets = Array.isArray(metadata.positionOffsets) ? metadata.positionOffsets.map(Number) : [0, 1, 2]
  const voxelPositions = new Array(voxelCount * 3)
  const metrics = fields.map((field) => ({
    id: String(field.id || 'field_' + field.offset),
    label: String(field.label || field.id || '字段 ' + field.offset),
    values: new Array(voxelCount)
  }))
  const profileMetadata = metadata.soilProfile && typeof metadata.soilProfile === 'object'
    ? metadata.soilProfile : null
  const profileLayerCount = Number(profileMetadata?.layerCount)
  const profileOffsets = Array.isArray(profileMetadata?.offsets)
    ? profileMetadata.offsets.map(Number) : []
  const hasSoilProfile = Number.isInteger(profileLayerCount) && profileLayerCount > 0 &&
    profileOffsets.length === profileLayerCount &&
    profileOffsets.every((offset) => Number.isInteger(offset) && offset >= 0 && offset < recordFloats)
  const profileDepths = hasSoilProfile && Array.isArray(profileMetadata.depths) &&
    profileMetadata.depths.length === profileLayerCount
    ? profileMetadata.depths.map(Number) : Array.from({ length: profileLayerCount }, (_, layer) => layer)
  const soilProfile = hasSoilProfile ? {
    layerCount: profileLayerCount,
    temperatureUnit: String(profileMetadata.temperatureUnit || 'degC'),
    depthUnit: String(profileMetadata.depthUnit || 'm'),
    depths: profileDepths,
    values: Array.from({ length: profileLayerCount }, () => new Array(voxelCount))
  } : null
  for (let voxel = 0; voxel < voxelCount; voxel += 1) {
    const recordOffset = sourceVoxels[voxel] * recordBytes
    for (let axis = 0; axis < 3; axis += 1) {
      voxelPositions[voxel * 3 + axis] = data.readFloatLE(recordOffset + positionOffsets[axis] * 4)
    }
    for (let metric = 0; metric < metrics.length; metric += 1) {
      metrics[metric].values[voxel] = data.readFloatLE(recordOffset + Number(fields[metric].offset) * 4)
    }
    if (soilProfile) {
      for (let layer = 0; layer < soilProfile.layerCount; layer += 1) {
        soilProfile.values[layer][voxel] = data.readFloatLE(recordOffset + profileOffsets[layer] * 4)
      }
    }
  }
  return {
    kind: 'voxel',
    path: metadataPath,
    voxelCount,
    sourceVoxelCount: sourceCount,
    sampled: voxelCount < sourceCount,
    voxelSize: Number(metadata.voxelSize) || 1,
    voxelPositions,
    metrics,
    soilProfile,
    processType: metadata.processType,
    time: metadata.time,
    node: metadata.node,
    backend: 'HiStream Voxel ' + String(metadata.processType || 'process')
  }
}

function writeRadiosityTiffForAngle(jsonPath, inputPath, mode, viewAngleOverride = null, includeProcess = true, removeStepFiles = false, step = null, sharedResult = null) {
  const { facetCount, metrics, vertexPositions, backend, bandRadiosity } = sharedResult || readRadiosityResult(jsonPath, Boolean(step))
  if (!vertexPositions || vertexPositions.length < facetCount * 9) throw new Error('面元结果缺少完整的三角形坐标')
  if (!inputPath || !existsSync(inputPath)) throw new Error('找不到生成影像所需的 project.json')
  const project = projectFromSource(readFileSync(inputPath, 'utf8'))
  const { scene, sensor, light, atmosphere = {}, meteo } = project.configuration
  const positiveInteger = (value, fallback) => {
    value = Math.round(Number(value))
    return Number.isInteger(value) && value > 0 ? value : fallback
  }
  const width = positiveInteger(sensor.x, 512)
  const height = positiveInteger(sensor.y, 512)
  const wavelengths = sensorBandValues(sensor)
  if (!wavelengths.length) wavelengths.push(1)
  const temperatureOutput = isEnergyBalanceMode(mode) && Boolean(sensor.temperature)
  const imageBandCount = temperatureOutput ? 1 : wavelengths.length
  const isOpticalWavelength = (wavelength) => {
    const value = Number(wavelength)
    const nanometers = value <= 2.5 ? value * 1000 : value
    return nanometers <= 2500
  }
  const bandNames = temperatureOutput
    ? ['温度 [K]']
    : wavelengths.map((wavelength, index) => {
      return isOpticalWavelength(wavelength)
        ? 'Reflectance [-] @ ' + wavelength + ' nm'
        : 'Spectral radiance [W m-2 sr-1 um-1] @ ' + wavelength + ' nm'
    })
  const viewAngles = viewAngleOverride || [Number(sensor.vza) || 0, Number(sensor.vaa) || 0]
  const sunAngles = [Number(light.zenith) || 0, Number(light.azimuth) || 0]
  const dot3 = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
  const subtract3 = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
  const cross3 = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
  const length3 = (value) => Math.hypot(value[0], value[1], value[2])
  const normalize3 = (value, fallback) => {
    const length = length3(value)
    return length > 1e-9 ? value.map((component) => component / length) : fallback
  }
  const cameraBasis = (forward, perspectiveView = false) => {
    const north = [1, 0, 0]
    const worldUp = [0, 1, 0]
    const preferredUp = perspectiveView ? worldUp : north
    const fallbackUp = perspectiveView ? north : worldUp
    const preferredProjection = dot3(preferredUp, forward)
    let up = preferredUp.map((component, axis) => component - preferredProjection * forward[axis])
    if (length3(up) <= 1e-9) {
      const fallbackProjection = dot3(fallbackUp, forward)
      up = fallbackUp.map((component, axis) => component - fallbackProjection * forward[axis])
    }
    up = normalize3(up, [1, 0, 0])
    const right = normalize3(cross3(forward, up), [0, 0, 1])
    return { right, up: normalize3(cross3(right, forward), up) }
  }
  const projection = String(sensor.projection || 'parallel').trim().toLowerCase()
  const perspective = ['perspective', 'central', 'center'].includes(projection)
  const sceneWidth = Math.max(1e-6, Number(scene.x) || 1)
  const sceneDepth = Math.max(1e-6, Number(scene.y) || 1)
  const sensorPosition = [Number(sensor.positionX), Number(sensor.positionY), Number(sensor.height)]
  const sensorWorld = sensorPosition.every(Number.isFinite)
    ? sensorPosition
    : [sceneWidth / 2, sceneDepth / 2, 3000]
  const cameraPosition = [sensorWorld[0] - sceneWidth / 2, Math.max(0.01, sensorWorld[2]), sensorWorld[1] - sceneDepth / 2]
  const directionFromAngles = (zenithDegrees, azimuthDegrees) => {
    const zenith = (Number.isFinite(zenithDegrees) ? zenithDegrees : 0) * Math.PI / 180
    const azimuth = (Number.isFinite(azimuthDegrees) ? azimuthDegrees : 0) * Math.PI / 180
    return [Math.sin(zenith) * Math.cos(azimuth), Math.cos(zenith), Math.sin(zenith) * Math.sin(azimuth)]
  }
  let cameraOut, cameraForward, cameraRight, cameraUp, perspectiveTanHalfFov = 1
  if (perspective) {
    cameraOut = directionFromAngles(viewAngles[0], viewAngles[1])
    cameraForward = cameraOut.map((component) => -component)
    ;({ right: cameraRight, up: cameraUp } = cameraBasis(cameraForward, true))
    const verticalFov = Math.max(0.1, Math.min(120, Number(sensor.fov) || 60))
    perspectiveTanHalfFov = Math.tan(verticalFov * 0.5 * Math.PI / 180)
  } else {
    // Azimuth is clockwise from +X (north) toward +Z (east).
    cameraOut = directionFromAngles(viewAngles[0], viewAngles[1])
    cameraForward = cameraOut.map((component) => -component)
    ;({ right: cameraRight, up: cameraUp } = cameraBasis(cameraForward))
  }
  const horizontalViewLength = Math.hypot(cameraOut[0], cameraOut[2])
  const namingViewAngles = perspective
    ? [Math.acos(Math.max(-1, Math.min(1, cameraOut[1]))) * 180 / Math.PI,
      horizontalViewLength < 1e-9 ? 0 : (Math.atan2(cameraOut[2], cameraOut[0]) * 180 / Math.PI + 360) % 360]
    : viewAngles
  const atmosphereEnabled = Boolean(atmosphere.enabled)
  const rawAtmosphereModel = String(atmosphere.model || '')
  const atmosphereModel = ['tropical', 'midlatitude-summer', 'midlatitude-winter'].includes(rawAtmosphereModel)
    ? rawAtmosphereModel : 'midlatitude-summer'
  const atmosphereWaterVapor = Math.max(0.5, Math.min(5, Number(atmosphere.waterVapor) || 2))
  const rawAtmosphereAerosol = String(atmosphere.aerosol || '')
  const atmosphereAerosol = ['rural', 'urban'].includes(rawAtmosphereAerosol) ? rawAtmosphereAerosol : 'rural'
  const atmosphereVisibility = Math.max(10, Math.min(50, Number(atmosphere.visibility) || 23))
  const configuredAtmospherePath = String(atmosphere.lutFile || '')
  const atmospherePath = configuredAtmospherePath && configuredAtmospherePath !== 'assets/atmosphere/simple_modtran_lut.csv'
    ? (isAbsolute(configuredAtmospherePath) ? configuredAtmospherePath : resolve(dirname(inputPath), configuredAtmospherePath))
    : join(RESOURCE_ROOT, 'assets', 'atmosphere', 'simple_modtran_lut.csv')
  const atmosphereTable = atmosphereEnabled ? loadAtmosphereLut(atmospherePath) : null
  const atmosphereAltitude = Math.max(0, Number(sensorWorld[2]) || 0) / 1000
  const thermalOutputWavelength = wavelengths.find((wavelength) => !isOpticalWavelength(wavelength)) || 10500
  const skyTemperature = Math.max(120, Number(light.skyTemperature) || 250)
  const skyOutputValue = (wavelengthNm, skyZenithDegrees, asTemperature) => {
    const wavelength = wavelengthNm > 0 && wavelengthNm <= 50 ? wavelengthNm * 1000 : wavelengthNm
    const zenithRadians = Math.max(0, Math.min(89.5, skyZenithDegrees)) * Math.PI / 180
    const correction = atmosphereTable
      ? sampleAtmosphereLut(atmosphereTable, atmosphereModel, atmosphereAerosol,
        atmosphereWaterVapor, atmosphereVisibility, atmosphereAltitude,
        skyZenithDegrees, wavelength)
      : { transmittance: 1, pathRadiance: 0 }
    if (wavelength <= 2500) {
      const spectralWeight = Math.max(0.06, Math.min(2, Math.pow(550 / Math.max(350, Math.min(2500, wavelength)), 1.8)))
      const aerosol = Math.max(0.4, Math.min(2.3, 23 / atmosphereVisibility))
      const cosineZenith = Math.max(0.12, Math.cos(zenithRadians))
      const airMass = 1 / cosineZenith
      const empiricalTransmittance = Math.exp(-(0.10 * spectralWeight + 0.025 * aerosol) * airMass)
      const transmittance = atmosphereTable ? correction.transmittance : empiricalTransmittance
      const horizonBoost = 1 + 0.55 * (1 - cosineZenith)
      return Math.max(0.002, Math.min(0.85,
        (1 - transmittance) * 0.55 * spectralWeight * horizonBoost + 0.008 * aerosol * horizonBoost))
    }
    let radiance = atmosphereTable ? correction.pathRadiance : 0
    if (!(radiance > 0)) {
      const cosineZenith = Math.max(0.05, Math.cos(zenithRadians))
      const zenithEmissivity = atmosphereTable && correction.transmittance < 0.999
        ? Math.max(0.08, Math.min(0.995, 1 - correction.transmittance)) : 0.72
      // Keep the fallback sky visibly directional: zenith radiance uses the
      // configured emissivity and increases linearly with 1-cos(theta)
      // toward the horizon radiance.
      const atmosphericFraction = Math.max(zenithEmissivity, Math.min(1,
        1 - (1 - zenithEmissivity) * cosineZenith))
      radiance = planckRadiance(wavelength, skyTemperature) * atmosphericFraction
    }
    return asTemperature ? inversePlanckTemperature(wavelength, radiance) : radiance
  }
  const point = (offset) => [Number(vertexPositions[offset]), Number(vertexPositions[offset + 1]), Number(vertexPositions[offset + 2])]
  for (let vertex = 0; vertex < facetCount * 3; vertex += 1) {
    const position = point(vertex * 3)
    if (!position.every(Number.isFinite)) throw new Error('第 ' + (Math.floor(vertex / 3) + 1) + ' 个面元包含无效坐标')
  }
  const imageAspect = width / height
  let viewMinimumX = perspective ? -1 : Infinity
  let viewMaximumX = perspective ? 1 : -Infinity
  let viewMinimumY = perspective ? -1 : Infinity
  let viewMaximumY = perspective ? 1 : -Infinity
  if (!perspective) {
    // Match Geometry::createSensor(): the angled source image is fitted to
    // the scene ground extent, not to the height-dependent mesh bounds.
    const groundCorners = [
      [-sceneWidth / 2, 0, -sceneDepth / 2],
      [-sceneWidth / 2, 0, sceneDepth / 2],
      [sceneWidth / 2, 0, -sceneDepth / 2],
      [sceneWidth / 2, 0, sceneDepth / 2]
    ]
    for (const position of groundCorners) {
      const projectedX = dot3(position, cameraRight)
      const projectedY = dot3(position, cameraUp)
      viewMinimumX = Math.min(viewMinimumX, projectedX)
      viewMaximumX = Math.max(viewMaximumX, projectedX)
      viewMinimumY = Math.min(viewMinimumY, projectedY)
      viewMaximumY = Math.max(viewMaximumY, projectedY)
    }
  }
  let viewRangeX = Math.max(1e-6, viewMaximumX - viewMinimumX)
  let viewRangeY = Math.max(1e-6, viewMaximumY - viewMinimumY)
  const viewCenterX = (viewMinimumX + viewMaximumX) / 2
  const viewCenterY = (viewMinimumY + viewMaximumY) / 2
  if (viewRangeX / viewRangeY > imageAspect) viewRangeY = viewRangeX / imageAspect
  else viewRangeX = viewRangeY * imageAspect
  viewRangeX *= 1 + 0.5 / width
  viewRangeY *= 1 + 0.5 / height
  viewMinimumX = viewCenterX - viewRangeX / 2
  viewMaximumY = viewCenterY + viewRangeY / 2

  const orthographicScale = 1.0
  let orthRangeEast = sceneDepth
  let orthRangeNorth = sceneWidth
  if (orthRangeEast / orthRangeNorth > imageAspect) orthRangeNorth = orthRangeEast / imageAspect
  else orthRangeEast = orthRangeNorth * imageAspect
  orthRangeEast /= orthographicScale
  orthRangeNorth /= orthographicScale
  const orthMinimumZ = -orthRangeEast / 2
  const orthMaximumX = orthRangeNorth / 2

  const pixelCount = width * height
  const viewVertex = (position) => {
    if (!perspective) return {
      x: (dot3(position, cameraRight) - viewMinimumX) / viewRangeX * width,
      y: (viewMaximumY - dot3(position, cameraUp)) / viewRangeY * height,
      depth: dot3(position, cameraOut), valid: true
    }
    const relative = subtract3(position, cameraPosition)
    const cameraDepth = dot3(relative, cameraForward)
    const halfWidth = perspectiveTanHalfFov * width / height
    return {
      x: (0.5 + dot3(relative, cameraRight) / (2 * cameraDepth * halfWidth)) * width,
      y: (0.5 - dot3(relative, cameraUp) / (2 * cameraDepth * perspectiveTanHalfFov)) * height,
      depth: -cameraDepth, valid: cameraDepth > 1e-6
    }
  }
  const edge = (a, b, x, y) => (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x)
  const observedSideIndices = new Int32Array(facetCount)
  for (let facet = 0; facet < facetCount; facet += 1) {
    const a3 = point(facet * 9), b3 = point(facet * 9 + 3), c3 = point(facet * 9 + 6)
    const normal = cross3(subtract3(b3, a3), subtract3(c3, a3))
    const centroid = [(a3[0] + b3[0] + c3[0]) / 3, (a3[1] + b3[1] + c3[1]) / 3, (a3[2] + b3[2] + c3[2]) / 3]
    const sideDirection = perspective ? subtract3(cameraPosition, centroid) : cameraOut
    const localSide = dot3(normal, sideDirection) >= 0 ? 0 : 1
    observedSideIndices[facet] = facet * 2 + localSide
  }
  const rasterizeSurfaceIndices = (projectVertex) => {
    const depthBuffer = new Float64Array(pixelCount)
    depthBuffer.fill(-Infinity)
    const surfaceIndexImage = new Int32Array(pixelCount)
    surfaceIndexImage.fill(-1)
    let visiblePixels = 0
    for (let facet = 0; facet < facetCount; facet += 1) {
      const valueIndex = observedSideIndices[facet]
      const a = projectVertex(point(facet * 9))
      const b = projectVertex(point(facet * 9 + 3))
      const c = projectVertex(point(facet * 9 + 6))
      if (!a.valid || !b.valid || !c.valid) continue
      const area = edge(a, b, c.x, c.y)
      if (Math.abs(area) < 1e-10) continue
      const minPixelX = Math.max(0, Math.floor(Math.min(a.x, b.x, c.x)))
      const maxPixelX = Math.min(width - 1, Math.ceil(Math.max(a.x, b.x, c.x)) - 1)
      const minPixelY = Math.max(0, Math.floor(Math.min(a.y, b.y, c.y)))
      const maxPixelY = Math.min(height - 1, Math.ceil(Math.max(a.y, b.y, c.y)) - 1)
      for (let y = minPixelY; y <= maxPixelY; y += 1) {
        for (let x = minPixelX; x <= maxPixelX; x += 1) {
          const sampleX = x + 0.5, sampleY = y + 0.5
          const weightA = edge(b, c, sampleX, sampleY) / area
          const weightB = edge(c, a, sampleX, sampleY) / area
          const weightC = 1 - weightA - weightB
          if (weightA < -1e-7 || weightB < -1e-7 || weightC < -1e-7) continue
          const depth = weightA * a.depth + weightB * b.depth + weightC * c.depth
          const pixel = y * width + x
          if (depth <= depthBuffer[pixel]) continue
          if (!Number.isFinite(depthBuffer[pixel])) visiblePixels += 1
          depthBuffer[pixel] = depth
          surfaceIndexImage[pixel] = valueIndex
        }
      }
    }
    return { surfaceIndexImage, visiblePixels }
  }
  // Match VoxelRT: first render the requested oblique observation, then map
  // each nadir ground pixel back into that source image.  Directly rerendering
  // visible facets from nadir changes crown displacement and occlusion.
  const observedImage = rasterizeSurfaceIndices(viewVertex)
  const orthSurfaceIndexImage = new Int32Array(pixelCount)
  orthSurfaceIndexImage.fill(-1)
  let orthVisiblePixels = 0
  const rasterWidth = Math.max(1, width - 1)
  const rasterHeight = Math.max(1, height - 1)
  for (let y = 0; y < height; y += 1) {
    const worldX = orthMaximumX - y / rasterHeight * orthRangeNorth
    if (worldX < -sceneWidth / 2 || worldX > sceneWidth / 2) continue
    for (let x = 0; x < width; x += 1) {
      const worldZ = orthMinimumZ + x / rasterWidth * orthRangeEast
      if (worldZ < -sceneDepth / 2 || worldZ > sceneDepth / 2) continue
      const source = viewVertex([worldX, 0, worldZ])
      if (!source.valid) continue
      const sourceX = Math.trunc(source.x)
      const sourceY = Math.trunc(source.y)
      if (sourceX < 0 || sourceX >= width || sourceY < 0 || sourceY >= height) continue
      const side = observedImage.surfaceIndexImage[sourceY * width + sourceX]
      if (side < 0) continue
      orthSurfaceIndexImage[y * width + x] = side
      orthVisiblePixels += 1
    }
  }
  const outputSurfaceIndexImage = perspective
    ? observedImage.surfaceIndexImage : orthSurfaceIndexImage
  const skyZenithImage = new Float32Array(pixelCount)
  skyZenithImage.fill(Number.NaN)
  if (perspective) {
    const verticalScale = perspectiveTanHalfFov
    const horizontalScale = perspectiveTanHalfFov * width / height
    for (let y = 0; y < height; y += 1) {
      const screenY = (1 - 2 * (y + 0.5) / height) * verticalScale
      for (let x = 0; x < width; x += 1) {
        const screenX = (2 * (x + 0.5) / width - 1) * horizontalScale
        const ray = normalize3([
          cameraForward[0] + cameraRight[0] * screenX + cameraUp[0] * screenY,
          cameraForward[1] + cameraRight[1] * screenX + cameraUp[1] * screenY,
          cameraForward[2] + cameraRight[2] * screenX + cameraUp[2] * screenY
        ], cameraForward)
        if (ray[1] > 0) {
          skyZenithImage[y * width + x] = Math.acos(Math.max(0, Math.min(1, ray[1]))) * 180 / Math.PI
        }
      }
    }
  }

  const startNode = step ? step.node : Math.max(0, Math.round(Number(meteo.start) || 0))
  const endNode = step ? step.node + 1 : Math.max(startNode + 1, Math.round(Number(meteo.end) || startNode + 1))
  const dTime = Math.max(1, Number(meteo.dTime) || 1800)
  const clockToken = (seconds, dayOfYear = null) => {
    let totalMinutes = Math.round(seconds / 60)
    let day = dayOfYear
    if (totalMinutes >= 1440) {
      if (day != null) day += Math.floor(totalMinutes / 1440)
      totalMinutes %= 1440
    }
    const hour = String(Math.floor(totalMinutes / 60)).padStart(2, '0')
    const minute = String(totalMinutes % 60).padStart(2, '0')
    return (day == null ? '' : 'DOY' + day + '_') + hour + '-' + minute
  }
  const configuredMeteo = String(meteo.path || '')
  const meteoFile = configuredMeteo && configuredMeteo !== 'defined/meteo.txt' && configuredMeteo !== 'HiStream 内置气象数据'
    ? (isAbsolute(configuredMeteo) ? configuredMeteo : resolve(dirname(inputPath), configuredMeteo))
    : join(dirname(executable()), 'defined', 'meteo.txt')
  const meteoRows = meteoFile && existsSync(meteoFile)
    ? readFileSync(meteoFile, 'utf8').split(/\r?\n/).slice(1).filter((line) => line.trim())
    : []
  const timeForNode = (node) => {
    if (step && node === step.node) return { token: step.token, julianTime: step.julianTime }
    const julianTime = Number(String(meteoRows[node] || '').trim().split(/\s+/)[0])
    if (Number.isFinite(julianTime)) {
      const day = Math.floor(julianTime)
      return { token: clockToken((julianTime - day) * 86400, day), julianTime }
    }
    return { token: clockToken(node * dTime), julianTime: node * dTime / 86400 }
  }
  const angleToken = (value) => {
    const number = Number(value)
    return Number.isFinite(number) ? number.toFixed(2) : '0.00'
  }
  const vza = angleToken(namingViewAngles[0])
  const vaa = angleToken(namingViewAngles[1])
  const staticRadiosity = metrics.find(([id]) => id === 'radiosity')?.[2]
  const surfaceValueCount = facetCount * 2
  const staticBandValues = (band) => bandRadiosity.length >= surfaceValueCount * wavelengths.length
    ? bandRadiosity.slice(band * surfaceValueCount, (band + 1) * surfaceValueCount)
    : staticRadiosity
  const coupledFacetEb = String(backend || '').includes('RT-EB coupled')
  // FacetEB step files can be hundreds of MiB each.  Keep only the active
  // node: retaining all time nodes grows memory linearly with the simulation.
  let activeStep = null
  const stepData = (time) => {
    if (!coupledFacetEb) return null
    if (activeStep?.token === time.token) return activeStep.data
    activeStep = null
    const stepPath = join(dirname(jsonPath), '.facet_steps', 'energy_T=' + time.token + '.bin')
    if (!existsSync(stepPath)) throw new Error('缺少 FacetEB 耦合节点结果：' + basename(stepPath))
    const buffer = readFileSync(stepPath)
    const surfaceCount = facetCount * 2
    const recordFloats = buffer.length / (surfaceCount * 4)
    if (!Number.isInteger(recordFloats) || recordFloats < 9) {
      throw new Error('FacetEB 节点结果不完整：' + basename(stepPath))
    }
    const data = { buffer, recordFloats, recordBytes: recordFloats * 4, path: stepPath }
    activeStep = { token: time.token, data }
    return data
  }
  const releaseStepData = (time) => {
    if (activeStep?.token !== time.token) return ''
    const stepPath = activeStep.data.path
    activeStep = null
    return stepPath
  }
  const rasterizeStepField = (time, fieldIndex) => {
    const { buffer, recordFloats, recordBytes, path } = stepData(time)
    if (fieldIndex >= recordFloats) {
      throw new Error('FacetEB 节点结果缺少逐波段反射率/辐亮度，请重新运行模拟：' + basename(path))
    }
    const image = new Float32Array(pixelCount)
    image.fill(Number.NaN)
    const fieldOffset = Math.max(0, Math.min(recordFloats - 1, fieldIndex)) * 4
    for (let pixel = 0; pixel < pixelCount; pixel += 1) {
      const side = outputSurfaceIndexImage[pixel]
      if (side >= 0) image[pixel] = buffer.readFloatLE(side * recordBytes + fieldOffset)
    }
    return image
  }
  const rasterizeValues = (values) => {
    const image = new Float32Array(pixelCount)
    image.fill(Number.NaN)
    for (let pixel = 0; pixel < pixelCount; pixel += 1) {
      const side = outputSurfaceIndexImage[pixel]
      if (side >= 0) image[pixel] = Number(values[side])
    }
    return image
  }

  const tifPaths = []
  if (sensor.image !== false) {
    const imageNodes = isEnergyBalanceMode(mode)
      ? Array.from({ length: endNode - startNode }, (_, index) => startNode + index)
      : [startNode]
    for (const node of imageNodes) {
      const time = timeForNode(node)
      let completed = false
      try {
        const imageValues = new Float32Array(pixelCount * imageBandCount)
        for (let band = 0; band < imageBandCount; band += 1) {
          const rasterized = coupledFacetEb
            ? rasterizeStepField(time, temperatureOutput ? 0 : 9 + band)
            : rasterizeValues(staticBandValues(band))
          imageValues.set(rasterized, band * pixelCount)
          const wavelengthValue = temperatureOutput ? thermalOutputWavelength : wavelengths[band]
          const wavelengthNm = wavelengthValue > 0 && wavelengthValue <= 50 ? wavelengthValue * 1000 : wavelengthValue
          const correction = atmosphereTable
            ? sampleAtmosphereLut(atmosphereTable, atmosphereModel, atmosphereAerosol,
              atmosphereWaterVapor, atmosphereVisibility,
              atmosphereAltitude, namingViewAngles[0], wavelengthNm)
            : null
          const offset = band * pixelCount
          for (let pixel = 0; pixel < pixelCount; pixel += 1) {
            const index = offset + pixel
            const value = imageValues[index]
            if (!Number.isFinite(value)) {
              if (Number.isFinite(skyZenithImage[pixel])) {
                imageValues[index] = skyOutputValue(
                  wavelengthNm, skyZenithImage[pixel], temperatureOutput)
              }
              continue
            }
            if (!correction) continue
            if (temperatureOutput) {
              const corrected = planckRadiance(wavelengthNm, value) * correction.transmittance + correction.pathRadiance
              imageValues[index] = corrected > 0 ? inversePlanckTemperature(wavelengthNm, corrected) : value
            } else {
              imageValues[index] = value * correction.transmittance + correction.pathRadiance
            }
          }
        }
        const tifName = isEnergyBalanceMode(mode)
          ? outputTiffName(mode, { timeToken: time.token, vza, vaa, atmosphere: atmosphereEnabled })
          : outputTiffName(mode, { sza: sunAngles[0], saa: sunAngles[1], vza, vaa, atmosphere: atmosphereEnabled })
        const tifPath = join(dirname(jsonPath), tifName)
        writeFloatTiff(tifPath, width, height, imageBandCount, imageValues, bandNames)
        tifPaths.push(tifPath)
        completed = true
      } finally {
        const stepPath = releaseStepData(time)
        if (completed && removeStepFiles && stepPath && existsSync(stepPath)) {
          try { unlinkSync(stepPath) } catch { /* TIFF 已生成，临时节点文件可稍后清理。 */ }
        }
      }
    }
  }

  const processPaths = []
  if (includeProcess && processOutputEnabled(project, mode) && isEnergyBalanceMode(mode)) {
    const processDirectory = join(dirname(jsonPath), 'process')
    mkdirSync(processDirectory, { recursive: true })
    if (coupledFacetEb) {
      const selected = processOutputSelection(project, mode)
      const processTypes = [selected.radiation && 'radiation', selected.energy && 'energy'].filter(Boolean)
      for (let node = startNode; node < endNode; node += 1) {
        for (const type of processTypes) {
          const processModel = type === 'radiation' ? 'facetrt' : 'faceteb'
          const metadataPath = join(processDirectory, processModel + '_T=' + timeForNode(node).token + '.json')
          const legacyPath = join(processDirectory, type + '_T=' + timeForNode(node).token + '_f.json')
          if (existsSync(metadataPath)) processPaths.push(metadataPath)
          else if (existsSync(legacyPath)) processPaths.push(legacyPath)
        }
      }
    } else {
      const startTime = timeForNode(startNode).token
      const endTime = timeForNode(endNode - 1).token
      const processDataPath = join(processDirectory, 'faceteb_T=' + (startTime === endTime ? startTime : startTime + '_to_' + endTime) + '.json')
      copyFileSync(jsonPath, processDataPath)
      for (let node = startNode; node < endNode; node += 1) {
        const time = timeForNode(node)
        const metadataPath = join(processDirectory, 'faceteb_T=' + time.token + '.meta.json')
        writeFileSync(metadataPath, JSON.stringify({
          kind: 'facet-energy-process',
          model: 'faceteb',
          node,
          julianTime: time.julianTime,
          time: time.token,
          facetCount,
          dataFile: basename(processDataPath),
          fields: metrics.map(([id]) => id),
          note: 'FacetRT is a static radiosity solve; requested times reference the same solved field.'
        }, null, 2) + '\n', 'utf8')
        processPaths.push(metadataPath)
      }
    }
  }
  return { tifPath: tifPaths[0] || '', tifPaths, processPaths, width, height, bands: imageBandCount, bandNames, visiblePixels: orthVisiblePixels }
}

function writeRadiosityTiff(jsonPath, inputPath, mode, step = null) {
  if (!inputPath || !existsSync(inputPath)) throw new Error('找不到生成影像所需的 project.json')
  const project = projectFromSource(readFileSync(inputPath, 'utf8'))
  const { sensor, light } = project.configuration
  const perspective = ['perspective', 'central', 'center'].includes(String(sensor.projection || 'parallel').trim().toLowerCase())
  const configuredAngles = sensorViewAngles(sensor, light.azimuth)
  const angles = perspective ? [null] : (configuredAngles.length ? configuredAngles : [[0, 0]])
  const sharedResult = step ? readRadiosityResult(jsonPath, true) : null
  const results = angles.map((angle, index) => writeRadiosityTiffForAngle(
    jsonPath, inputPath, mode, angle, index === 0, !step && index === angles.length - 1, step, sharedResult))
  // Delete only this completed node, after every direction succeeds. On export
  // failure retain its binary so results can be recovered/retried externally.
  if (step) {
    const stepPath = join(dirname(jsonPath), '.facet_steps', 'energy_T=' + step.token + '.bin')
    if (existsSync(stepPath)) unlinkSync(stepPath)
  }
  const first = results[0]
  return {
    ...first,
    tifPath: results.flatMap((result) => result.tifPaths)[0] || '',
    tifPaths: results.flatMap((result) => result.tifPaths),
    processPaths: [...new Set(results.flatMap((result) => result.processPaths))],
    visiblePixels: results.reduce((sum, result) => sum + result.visiblePixels, 0),
    angleCount: results.length
  }
}

function exportFacetStep(running, jsonPath, inputPath, step) {
  return new Promise((resolveStep, rejectStep) => {
    const worker = new Worker(new URL(import.meta.url), {
      workerData: { kind: 'facet-step', jsonPath, inputPath, step },
      execArgv: process.execArgv.filter(arg => !arg.startsWith('--input-type'))
    })
    running.facetWorker = worker
    let result, failure
    worker.on('message', (message) => { result = message })
    worker.on('error', (error) => { failure = error })
    // Wait for teardown, not just a message: the old node's JS heap and array
    // buffers must be gone before the engine is allowed to compute again.
    worker.on('exit', (code) => {
      if (running.facetWorker === worker) running.facetWorker = null
      if (failure || code !== 0 || !result) rejectStep(failure || new Error('节点出图线程异常结束'))
      else resolveStep(result)
    })
  })
}

function projectJsonPath(inputPath) {
  return basename(inputPath).toLowerCase() === 'project.json' ? inputPath : join(dirname(inputPath), 'project.json')
}

function runtimePaths(baseDir, project = null) {
  const sourceRoot = process.env.HISTREAM_ROOT || join(MODEL_ROOT, "histream")
  const configuredOutput = normalizeHostPath(project?.configuration?.outDir || '')
  const projectOutputPath = configuredOutput
    ? (isAbsolute(configuredOutput) ? resolve(configuredOutput) : resolve(baseDir, configuredOutput))
    : join(baseDir, 'output')
  const configuredMeteoRaw = String(project?.configuration?.meteo?.path || "").trim()
  const configuredMeteo = /\/[A-Za-z]:/.test(configuredMeteoRaw) ? "" : normalizeHostPath(configuredMeteoRaw)
  const projectMeteoPath = configuredMeteo && configuredMeteo !== "defined/meteo.txt" && configuredMeteo !== "HiStream 内置气象数据"
    ? (isAbsolute(configuredMeteo) ? resolve(configuredMeteo) : resolve(baseDir, configuredMeteo))
    : ""
  return {
    outputDir: histreamRuntimePath(projectOutputPath),
    definedDir: histreamRuntimePath(join(sourceRoot, "defined")),
    meteoPath: histreamRuntimePath(projectMeteoPath || join(sourceRoot, "defined", "meteo.txt")),
    atmosphereLutPath: histreamRuntimePath(join(RESOURCE_ROOT, 'assets', 'atmosphere', 'simple_modtran_lut.csv')),
    projectDir: histreamRuntimePath(baseDir),
    resolveProjectPath: (value) => {
      const hostPath = normalizeHostPath(value)
      if (!hostPath) return ''
      if (isAbsolute(hostPath)) return histreamRuntimePath(hostPath)
      const localPath = resolve(baseDir, hostPath)
      const sharedAssetPath = /^assets[\\/]/i.test(hostPath) ? resolve(RESOURCE_ROOT, hostPath) : ''
      return histreamRuntimePath(sharedAssetPath && !existsSync(localPath) && existsSync(sharedAssetPath)
        ? sharedAssetPath : localPath)
    }
  }
}

function writeProject(path, value) {
  const project = normalizeStoredProjectPaths(normalizeProject(value), dirname(path))
  project.updatedAt = new Date().toISOString()
  writeFileSync(path, `${JSON.stringify(project, null, 2)}\n`, 'utf8')
  return project
}

function readProject(inputPath) {
  const path = projectJsonPath(inputPath)
  if (!existsSync(path)) return { project: null, projectPath: '' }
  let project
  try {
    project = normalizeProject(JSON.parse(readFileSync(path, 'utf8')))
  } catch (error) {
    throw new Error(`project.json 格式错误：${error.message}`)
  }
  return { project, projectPath: path }
}

function ensureDefaultSceneAsset(project, baseDir) {
  const items = project?.configuration?.objects?.items
  if (!Array.isArray(items) || !items.length) return false
  const first = items.find((item) => !String(item.fileName || '').trim())
  if (!first || !existsSync(defaultSceneAsset)) return false
  first.fileName = DEFAULT_SCENE_MODEL
  const destination = join(baseDir, DEFAULT_SCENE_MODEL)
  mkdirSync(dirname(destination), { recursive: true })
  if (!existsSync(destination)) copyFileSync(defaultSceneAsset, destination)
  return true
}
function migrateProject(inputPath, project) {
  if (!project) return project
  const originalPaths = JSON.stringify({
    outDir: project.configuration?.outDir,
    meteo: project.configuration?.meteo?.path,
    dem: project.configuration?.scene?.demFile,
    objects: (project.configuration?.objects?.items || []).map((item) => [item.fileName, item.positionFile]),
    spectra: (project.configuration?.spectra || []).map((item) => [item.fileName, item.physicalTexture?.fileName])
  })
  const meteoPath = String(project.configuration?.meteo?.path || "")
  const meteoPathRepaired = /\/[A-Za-z]:/.test(meteoPath)
  const sceneMigrated = ensureDefaultSceneAsset(project, dirname(inputPath))
  if (meteoPathRepaired) project.configuration.meteo.path = "defined/meteo.txt"
  normalizeStoredProjectPaths(project, dirname(inputPath))
  const normalizedPaths = JSON.stringify({
    outDir: project.configuration?.outDir,
    meteo: project.configuration?.meteo?.path,
    dem: project.configuration?.scene?.demFile,
    objects: (project.configuration?.objects?.items || []).map((item) => [item.fileName, item.positionFile]),
    spectra: (project.configuration?.spectra || []).map((item) => [item.fileName, item.physicalTexture?.fileName])
  })
  if (!sceneMigrated && !meteoPathRepaired && originalPaths === normalizedPaths) return project
  return writeProject(projectJsonPath(inputPath), project)
}

function openProject(value) {
  projectFile = resolveProjectFile(value)
  projectDir = dirname(projectFile)
  const stored = readProject(projectFile)
  const migrated = migrateProject(projectFile, stored.project)
  return {
    path: windowsPath(projectFile),
    projectDir: windowsPath(projectDir),
    content: stringifyProject(migrated || stored.project),
    project: migrated || stored.project,
    projectPath: windowsPath(stored.projectPath)
  }
}

function chooseProjectFile() {
  if (process.platform !== 'win32' && !existsSync('/mnt/c/WINDOWS/System32/WindowsPowerShell/v1.0/powershell.exe')) throw new Error('找不到 Windows 文件选择器')
  return new Promise((resolveChoice, reject) => {
    const script = [
      "$ErrorActionPreference = 'Stop'",
      "Add-Type -AssemblyName System.Windows.Forms",
      "$dialog = New-Object System.Windows.Forms.OpenFileDialog",
      "$dialog.Title = '打开 StreamSim 工程'",
      "$dialog.Filter = 'StreamSim 工程 (project.json)|project.json|JSON 文件 (*.json)|*.json'",
      "$dialog.FileName = 'project.json'",
      "$dialog.Multiselect = $false",
      "$dialog.CheckFileExists = $true",
      "if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { Write-Output $dialog.FileName }"
    ].join('; ')
    execFile('powershell.exe', ['-NoProfile', '-STA', '-Command', script], {
      windowsHide: false, encoding: 'utf8', maxBuffer: 1024 * 1024
    }, (error, stdout, stderr) => {
      if (error) return reject(new Error(stderr.trim() || error.message))
      resolveChoice(String(stdout || '').trim())
    })
  })
}

function createProject(data) {
  const directory = normalizeHostPath(data.directory).replace(/^"|"$/g, '')
  const name = String(data.name || '').trim()
  if (!directory) throw new Error('请输入工程保存目录')
  if (!isAbsolute(directory)) throw new Error('工程保存目录必须是绝对路径')
  if (!name || name === '.' || name === '..' || /[<>:"/\\|?*\x00-\x1f]/.test(name)) throw new Error('工程名称包含无效字符')
  if (!modes.has(data.mode)) throw new Error('不支持的计算模式')

  const target = join(resolve(directory), name)
  if (existsSync(target) && readdirSync(target).length) throw new Error(`工程目录已存在且不为空：${target}`)
  mkdirSync(target, { recursive: true })
  for (const folder of ['models', 'positions', 'spectral', 'meteorology', 'terrain', 'output']) mkdirSync(join(target, folder), { recursive: true })

  const project = createDefaultProject({
    name,
    mode: data.mode,
    sceneX: data.sceneX,
    sceneY: data.sceneY,
    sceneHeight: data.sceneHeight,
    voxelSize: data.voxelSize
  })
  ensureDefaultSceneAsset(project, target)
  project.configuration.outDir = join(target, 'output')
  const jsonPath = join(target, 'project.json')
  const saved = writeProject(jsonPath, project)
  const content = stringifyProject(saved)
  return { path: jsonPath, projectDir: target, projectPath: jsonPath, project: saved, content }
}

function saveProjectAs(data) {
  const directory = normalizeHostPath(data.directory).replace(/^"|"$/g, '')
  const name = String(data.name || '').trim()
  if (!directory) throw new Error('请输入工程保存目录')
  if (!isAbsolute(directory)) throw new Error('工程保存目录必须是绝对路径')
  if (!name || name === '.' || name === '..' || /[<>:"/\\|?*\x00-\x1f]/.test(name)) throw new Error('工程名称包含无效字符')

  const sourceFile = resolveProjectFile(data.sourcePath || projectFile)
  const sourceDirectory = dirname(sourceFile)
  const target = join(resolve(directory), name)
  const targetWithinSource = relative(sourceDirectory, target)
  if (!targetWithinSource || (!targetWithinSource.startsWith('..\\') && !targetWithinSource.startsWith('../') && !isAbsolute(targetWithinSource))) {
    throw new Error('新工程不能保存在当前工程目录内部')
  }
  if (existsSync(target)) {
    if (!statSync(target).isDirectory()) throw new Error(`目标路径不是目录：${target}`)
    if (readdirSync(target).length) throw new Error(`工程目录已存在且不为空：${target}`)
  }

  const validation = validateProject(data.project)
  if (!validation.valid) throw new Error(validation.errors.join('；'))
  const project = normalizeStoredProjectPaths(JSON.parse(JSON.stringify(validation.project)), sourceDirectory)
  const now = new Date().toISOString()
  project.name = name
  project.createdAt = now
  project.updatedAt = now
  project.configuration.outDir = 'output'

  mkdirSync(target, { recursive: true })
  for (const folder of ['models', 'positions', 'spectral', 'meteorology', 'terrain']) {
    const source = join(sourceDirectory, folder)
    const destination = join(target, folder)
    if (existsSync(source) && statSync(source).isDirectory()) cpSync(source, destination, { recursive: true, errorOnExist: true })
    else mkdirSync(destination, { recursive: true })
  }
  mkdirSync(join(target, 'output'), { recursive: true })

  const jsonPath = join(target, 'project.json')
  const saved = writeProject(jsonPath, project)
  const content = stringifyProject(saved)
  return { path: jsonPath, projectDir: target, projectPath: jsonPath, project: saved, content }
}

function resolveAsset(value) {
  const source = normalizeHostPath(value).replace(/^"|"$/g, '')
  if (!source) throw new Error('资源路径为空')
  return isAbsolute(source) ? resolve(source) : resolve(projectDir, source)
}

function csvCell(value) {
  return `"${String(value ?? '').replaceAll('"', '""')}"`
}

function appendRasterStatistics(path, width, height, bands, values, bandNames = []) {
  const name = basename(path)
  const isTimeSeries = name.startsWith('T=')
  const mode = name.includes('_f.') ? (isTimeSeries ? 'FacetEB' : 'FacetRT')
    : name.includes('_v.') ? (isTimeSeries ? 'VoxelEB' : 'VoxelRT') : 'Raytracing'
  const statisticsPath = join(dirname(path), `result_statistics_${mode.toLowerCase()}.csv`)
  const pixelCount = width * height
  const rows = []
  for (let band = 0; band < bands; band += 1) {
    let count = 0, mean = 0, moment = 0, minimum = Infinity, maximum = -Infinity
    const offset = band * pixelCount
    for (let pixel = 0; pixel < pixelCount; pixel += 1) {
      const value = Number(values[offset + pixel])
      if (!Number.isFinite(value)) continue
      count += 1
      const delta = value - mean
      mean += delta / count
      moment += delta * (value - mean)
      minimum = Math.min(minimum, value); maximum = Math.max(maximum, value)
    }
    rows.push([
      csvCell(name), mode, csvCell(filenameToken(name, 'T')), filenameToken(name, 'VZA'), filenameToken(name, 'VAA'),
      band, csvCell(bandNames[band] || `Band ${band + 1}`), count,
      count ? minimum : '', count ? maximum : '', count ? mean : '', count ? Math.sqrt(moment / count) : ''
    ].join(','))
  }
  if (!existsSync(statisticsPath)) appendFileSync(statisticsPath, 'file,mode,time,vza,vaa,band_index,band_name,count,min,max,mean,stddev\n', 'utf8')
  appendFileSync(statisticsPath, rows.join('\n') + '\n', 'utf8')
  return statisticsPath
}

function writeFloatTiff(path, width, height, bands, values, bandNames = []) {
  if (![width, height, bands].every((value) => Number.isInteger(value) && value > 0)) throw new Error('TIFF 尺寸或波段数无效')
  const pixelCount = width * height
  if (!values || values.length < pixelCount * bands) throw new Error('TIFF 波段数据不完整')

  const entries = bands > 1 ? 13 : 12
  const ifdOffset = 8
  let dataOffset = ifdOffset + 2 + entries * 12 + 4
  const extraData = []
  const allocate = (buffer, alignment = 2) => {
    const padding = (alignment - dataOffset % alignment) % alignment
    if (padding) { extraData.push({ offset: dataOffset, buffer: Buffer.alloc(padding) }); dataOffset += padding }
    const offset = dataOffset
    extraData.push({ offset, buffer })
    dataOffset += buffer.length
    return offset
  }
  const shortArrayValue = (items) => {
    if (items.length <= 2) return items.reduce((value, item, index) => value | ((item & 0xffff) << (index * 16)), 0) >>> 0
    const buffer = Buffer.alloc(items.length * 2)
    items.forEach((item, index) => buffer.writeUInt16LE(item, index * 2))
    return allocate(buffer)
  }
  const bits = Array(bands).fill(32)
  const sampleFormats = Array(bands).fill(3)
  const extraSamples = Array(Math.max(0, bands - 1)).fill(0)
  const bitsValue = shortArrayValue(bits)
  const sampleFormatsValue = shortArrayValue(sampleFormats)
  const extraSamplesValue = extraSamples.length ? shortArrayValue(extraSamples) : 0
  const names = Array.from({ length: bands }, (_, index) => String(bandNames[index] || 'Band ' + (index + 1)))
  const description = Buffer.from('HiStream image bands: {' + names.join(', ') + '}\0', 'ascii')
  const descriptionOffset = allocate(description, 1)
  const imagePadding = (4 - dataOffset % 4) % 4
  if (imagePadding) { extraData.push({ offset: dataOffset, buffer: Buffer.alloc(imagePadding) }); dataOffset += imagePadding }
  const imageOffset = dataOffset
  const imageBytes = pixelCount * bands * 4
  if (!Number.isSafeInteger(imageBytes) || imageOffset + imageBytes > 0xffffffff) throw new Error('TIFF 超过 4 GiB，请降低单幅影像分辨率或波段数')

  const header = Buffer.alloc(imageOffset)
  header.write('II', 0, 'ascii'); header.writeUInt16LE(42, 2); header.writeUInt32LE(ifdOffset, 4)
  header.writeUInt16LE(entries, ifdOffset)
  let entry = ifdOffset + 2
  const writeEntry = (tag, type, count, value) => {
    header.writeUInt16LE(tag, entry); header.writeUInt16LE(type, entry + 2); header.writeUInt32LE(count, entry + 4); header.writeUInt32LE(value >>> 0, entry + 8)
    entry += 12
  }
  writeEntry(256, 4, 1, width); writeEntry(257, 4, 1, height)
  writeEntry(258, 3, bands, bitsValue); writeEntry(259, 3, 1, 1); writeEntry(262, 3, 1, 1)
  writeEntry(270, 2, description.length, descriptionOffset)
  writeEntry(273, 4, 1, imageOffset); writeEntry(277, 3, 1, bands); writeEntry(278, 4, 1, height)
  writeEntry(279, 4, 1, imageBytes); writeEntry(284, 3, 1, 1)
  if (bands > 1) writeEntry(338, 3, extraSamples.length, extraSamplesValue)
  writeEntry(339, 3, bands, sampleFormatsValue)
  header.writeUInt32LE(0, entry)
  for (const part of extraData) part.buffer.copy(header, part.offset)
  // No full-image interleaved Buffer or Buffer.concat copy. Publish only a
  // complete TIFF; bounded chunks keep memory independent of file size.
  const temporaryPath = path + '.partial'
  const fd = openSync(temporaryPath, 'w')
  try {
    const writeAll = buffer => {
      let offset = 0
      while (offset < buffer.length) {
        const written = writeSync(fd, buffer, offset, buffer.length - offset)
        if (!written) throw new Error('TIFF 写入未完成')
        offset += written
      }
    }
    writeAll(header)
    const chunk = Buffer.allocUnsafe(Math.min(imageBytes, 64 * 1024))
    for (let first = 0; first < pixelCount * bands;) {
      const count = Math.min(chunk.length / 4, pixelCount * bands - first)
      for (let index = 0; index < count; index++) {
        const outputIndex = first + index
        const value = Number(values[(outputIndex % bands) * pixelCount + Math.floor(outputIndex / bands)])
        chunk.writeFloatLE(Number.isFinite(value) ? value : Number.NaN, index * 4)
      }
      writeAll(chunk.subarray(0, count * 4))
      first += count
    }
  } finally { closeSync(fd) }
  renameSync(temporaryPath, path)
  appendRasterStatistics(path, width, height, bands, values, bandNames)
  return path
}

function filenameToken(stem, key) {
  const source = String(stem || '')
  const marker = String(key || '').toUpperCase() + '='
  const upper = source.toUpperCase()
  const start = upper.indexOf(marker)
  if (start < 0) return ''
  const value = source.slice(start + marker.length)
  const boundaries = ['_H=', '_T=', '_TIME=', '_SZA=', '_SAA=', '_VZA=', '_VAA=', '_F', '_V', '_A']
  let end = value.length
  for (const boundary of boundaries) {
    const index = value.toUpperCase().indexOf(boundary)
    if (index >= 0) end = Math.min(end, index)
  }
  return value.slice(0, end)
}

function convertEnviToTiff(headerPath, mode) {
  const metadata = enviMetadata(readFileSync(headerPath, 'utf8'))
  const width = Number(metadata.samples), height = Number(metadata.lines), bands = Number(metadata.bands)
  const type = Number(metadata['data type'] || 4)
  const imagePath = headerPath.slice(0, -extname(headerPath).length) + '.img'
  if (![width, height, bands].every((value) => Number.isInteger(value) && value > 0) || type !== 4 || !existsSync(imagePath)) return null
  const source = readFileSync(imagePath)
  const expected = width * height * bands * 4
  if (source.length < expected) return null
  const values = new Float32Array(width * height * bands)
  for (let index = 0; index < values.length; index += 1) values[index] = source.readFloatLE(index * 4)
  const names = String(metadata['band names'] || '').replace(/[{}]/g, '').split(',').map((value) => value.trim()).filter(Boolean)
  const stem = basename(headerPath, extname(headerPath))
  const julianTime = Number(filenameToken(stem, 'T'))
  const vza = filenameToken(stem, 'VZA')
  const vaa = filenameToken(stem, 'VAA')
  const sza = filenameToken(stem, 'SZA')
  const saa = filenameToken(stem, 'SAA')
  let standardName = ''
  if (isEnergyBalanceMode(mode) && Number.isFinite(julianTime) && vza !== '' && vaa !== '') {
    standardName = outputTiffName(mode, { julianTime, vza, vaa })
  } else if (!isEnergyBalanceMode(mode) && sza !== '' && saa !== '' && vza !== '' && vaa !== '') {
    standardName = outputTiffName(mode, { sza, saa, vza, vaa })
  }
  if (!standardName) {
    const suffix = outputGeometrySuffix(mode)
    standardName = stem.endsWith(suffix) ? stem + '.tif' : stem + suffix + '.tif'
  }
  const tiffPath = join(dirname(headerPath), standardName)
  writeFloatTiff(tiffPath, width, height, bands, values, names)
  if (mode === "eVoxelRT" || mode === "eVoxelEB" || mode === "eRaytracing") {
    unlinkSync(imagePath)
    unlinkSync(headerPath)
  }
  return tiffPath
}

function convertEnviResults(directory, mode) {
  if (!existsSync(directory) || !statSync(directory).isDirectory()) return []
  const energyBalance = isEnergyBalanceMode(mode)
  return readdirSync(directory, { withFileTypes: true })
    .filter((entry) => entry.isFile() && extname(entry.name).toLowerCase() === '.hdr')
    .filter((entry) => {
      const stem = basename(entry.name, extname(entry.name))
      const hasTime = filenameToken(stem, 'T') !== ''
      return energyBalance ? hasTime : !hasTime
    })
    .map((entry) => convertEnviToTiff(join(directory, entry.name), mode))
    .filter(Boolean)
}

function tiffObservation(name) {
  const stem = basename(name, extname(name))
  const outputHeight = filenameToken(stem, 'H')
  const time = filenameToken(stem, 'T') || filenameToken(stem, 'TIME')
  const sza = filenameToken(stem, 'SZA')
  const saa = filenameToken(stem, 'SAA')
  const vza = filenameToken(stem, 'VZA')
  const vaa = filenameToken(stem, 'VAA')
  const parts = [
    outputHeight && 'H=' + outputHeight,
    time && 'T=' + time,
    sza && 'SZA=' + sza + '°',
    saa && 'SAA=' + saa + '°',
    vza && 'VZA=' + vza + '°',
    vaa && 'VAA=' + vaa + '°'
  ].filter(Boolean)
  return {
    outputHeight,
    simulationTime: time,
    solarZenith: sza,
    solarAzimuth: saa,
    viewZenith: vza,
    viewAzimuth: vaa,
    observationLabel: parts.join(' · ')
  }
}

function deleteTiffResults(value) {
  const directory = resolveAsset(value)
  if (!existsSync(directory) || !statSync(directory).isDirectory()) throw new Error(`找不到模拟结果目录：` + directory)
  if (dirname(directory) === directory) throw new Error(`不能删除磁盘根目录中的结果`)
  const processDirectory = join(directory, `process`)
  const directories = [directory, ...(existsSync(processDirectory) && statSync(processDirectory).isDirectory() ? [processDirectory] : [])]
  const paths = directories.flatMap((currentDirectory) => readdirSync(currentDirectory, { withFileTypes: true })
    .filter((entry) => entry.isFile() && ([`.tif`, `.tiff`].includes(extname(entry.name).toLowerCase()) || /^result_statistics(?:_[a-z0-9-]+)?\.csv$/i.test(entry.name)))
    .map((entry) => join(currentDirectory, entry.name)))
  for (const path of paths) unlinkSync(path)
  return { directory: windowsPath(directory), deleted: paths.length }
}

function isThreeDimensionalResultFile(path) {
  if (extname(path).toLowerCase() !== ".json") return false
  if (isRadiosityResultFile(path)) return true
  return processResultMetadata(path) != null
}

function deleteThreeDimensionalResults(value) {
  const directory = resolveAsset(value)
  if (!existsSync(directory) || !statSync(directory).isDirectory()) throw new Error("找不到模拟结果目录：" + directory)
  if (dirname(directory) === directory) throw new Error("不能删除磁盘根目录中的结果")
  const processDirectory = join(directory, "process")
  const directories = [directory, ...(existsSync(processDirectory) && statSync(processDirectory).isDirectory() ? [processDirectory] : [])]
  const metadataPaths = directories.flatMap((currentDirectory) => readdirSync(currentDirectory, { withFileTypes: true })
    .filter((entry) => entry.isFile() && extname(entry.name).toLowerCase() === ".json")
    .map((entry) => join(currentDirectory, entry.name)))
    .filter(isThreeDimensionalResultFile)
  const paths = [...metadataPaths]
  for (const metadataPath of metadataPaths) {
    const metadata = processResultMetadata(metadataPath)
    if (!metadata?.dataFile) continue
    const dataPath = resolve(dirname(metadataPath), String(metadata.dataFile))
    if (dirname(dataPath) === dirname(metadataPath) && existsSync(dataPath) && statSync(dataPath).isFile()) paths.push(dataPath)
  }
  for (const path of paths) unlinkSync(path)
  return { directory: windowsPath(directory), deleted: paths.length }
}

function listResults(value) {
  const directory = resolveAsset(value)
  if (!existsSync(directory) || !statSync(directory).isDirectory()) throw new Error('找不到模拟结果目录：' + directory)
  const processDirectory = join(directory, 'process')
  const directories = [directory, ...(existsSync(processDirectory) && statSync(processDirectory).isDirectory() ? [processDirectory] : [])]
  const files = directories.flatMap((currentDirectory) => readdirSync(currentDirectory, { withFileTypes: true })
    .filter((entry) => entry.isFile())
    .filter((entry) => ['.tif', '.tiff', '.json', '.csv'].includes(extname(entry.name).toLowerCase()))
    .map((entry) => {
      const path = join(currentDirectory, entry.name)
      const info = statSync(path)
      const extension = extname(entry.name).toLowerCase()
      let kind = ['.tif', '.tiff'].includes(extension) ? 'tiff' : extension === '.csv' ? 'text' : extension === '.json' && isRadiosityResultFile(path) ? 'facet' : 'file'
      let process = {}
      if (extension === '.json') {
        process = processResultMetadata(path) || {}
        if (process.kind) kind = 'process'
      }
      const observation = kind === 'tiff' ? tiffObservation(entry.name) : {}
      const resultType = kind === 'tiff' && /^fluid_wind_/i.test(entry.name) ? 'wind' : ''
      const inferredProcessModel = process.processType === 'fluid'
        ? 'voxelfluid'
        : process.processType === 'radiation'
          ? (process.geometry === 'voxel' ? 'voxelrt' : 'facetrt')
          : (process.geometry === 'voxel' ? 'voxeleb' : 'faceteb')
      // 兼容旧结果：旧流体过程曾错误写作 voxeleb，列表中仍按流体过程命名。
      const processModel = String(process.processType === 'fluid'
        ? inferredProcessModel : (process.model || inferredProcessModel)).toLowerCase()
      const processModelLabels = {
        facetrt: '面元辐射传输', voxelrt: '体元辐射传输',
        faceteb: '面元能量平衡', voxeleb: '体元能量平衡',
        voxelfluid: '体元流体力学'
      }
      const facetModelLabel = basename(path).toLowerCase().includes('faceteb')
        ? '面元能量平衡' : '面元辐射传输'
      const displayName = kind === 'text' && /^result_statistics(?:_[a-z0-9-]+)?\.csv$/i.test(entry.name)
        ? '统计结果 · ' + entry.name
        : kind === 'facet'
        ? facetModelLabel + ' · ' + (facetModelLabel.includes('能量平衡') ? '最终时刻' : '静态')
        : kind === 'process'
          ? (process.processType === 'photovoltaic' ? '光伏热电耦合' : (processModelLabels[processModel] || processModel)) + ' · ' + String(process.time || '静态')
          : entry.name
      return { name: displayName, path, size: info.size, modifiedAt: info.mtime.toISOString(), kind, resultType, ...observation, processTime: process.time, processType: process.processType, processModel, node: process.node }
    }))
    .filter((entry) => entry.kind !== 'file')
    .sort((a, b) => b.modifiedAt.localeCompare(a.modifiedAt))
  const hasFacetTimeSeries = files.some((entry) =>
    entry.kind === 'process' && ['facetrt', 'faceteb'].includes(entry.processModel) && entry.processTime)
  // faceteb.json is the shared full-resolution geometry and final-state
  // backing store for all per-time FacetRT/FacetEB process files. When the
  // time series exists, listing it as another "final time" is redundant.
  const latestFacet = files.find((entry) => entry.kind === 'facet' && !(
    hasFacetTimeSeries && basename(entry.path).toLowerCase() === 'faceteb.json'))
  return {
    directory: windowsPath(directory),
    files: [
      ...files.filter((entry) => entry.kind === 'tiff'),
      ...files.filter((entry) => entry.kind === 'text'),
      ...files.filter((entry) => entry.kind === 'process'),
      ...(latestFacet ? [latestFacet] : [])
    ].sort((a, b) => b.modifiedAt.localeCompare(a.modifiedAt))
      .map((entry) => ({ ...entry, path: windowsPath(entry.path) }))
  }
}

function enviMetadata(content) {
  const result = {}
  for (const match of content.matchAll(/^\s*([^=\r\n]+?)\s*=\s*([^\r\n{][^\r\n]*)/gm)) result[match[1].trim().toLowerCase()] = match[2].trim()
  return result
}

function enviValue(buffer, offset, type, littleEndian) {
  if (type === 1) return buffer.readUInt8(offset)
  if (type === 2) return littleEndian ? buffer.readInt16LE(offset) : buffer.readInt16BE(offset)
  if (type === 3) return littleEndian ? buffer.readInt32LE(offset) : buffer.readInt32BE(offset)
  if (type === 4) return littleEndian ? buffer.readFloatLE(offset) : buffer.readFloatBE(offset)
  if (type === 5) return littleEndian ? buffer.readDoubleLE(offset) : buffer.readDoubleBE(offset)
  if (type === 12) return littleEndian ? buffer.readUInt16LE(offset) : buffer.readUInt16BE(offset)
  if (type === 13) return littleEndian ? buffer.readUInt32LE(offset) : buffer.readUInt32BE(offset)
  throw new Error(`暂不支持 ENVI data type ${type}`)
}

function enviTypeSize(type) {
  return ({ 1: 1, 2: 2, 3: 4, 4: 4, 5: 8, 12: 2, 13: 4 })[type] || 0
}

function tiffTypeSize(type) {
  return ({ 1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 6: 1, 7: 1, 8: 2, 9: 4, 10: 8, 11: 4, 12: 8 })[type] || 0
}

function unpackTiffLzw(source) {
  let bitOffset = 0
  let codeSize = 9
  let nextCode = 258
  let previous = null
  const dictionary = []
  const output = []
  const readCode = () => {
    if (bitOffset + codeSize > source.length * 8) return null
    let code = 0
    for (let bit = 0; bit < codeSize; bit += 1) code = (code << 1) | ((source[Math.floor((bitOffset + bit) / 8)] >> (7 - ((bitOffset + bit) % 8))) & 1)
    bitOffset += codeSize
    return code
  }
  const reset = () => { codeSize = 9; nextCode = 258; previous = null; dictionary.length = 0 }
  while (true) {
    const code = readCode()
    if (code == null || code === 257) break
    if (code === 256) { reset(); continue }
    let entry
    if (code < 256) entry = [code]
    else if (code < nextCode && dictionary[code]) entry = dictionary[code]
    else if (code === nextCode && previous) entry = previous.concat(previous[0])
    else throw new Error('TIFF LZW 字典无效')
    output.push(...entry)
    if (previous) {
      dictionary[nextCode] = previous.concat(entry[0])
      nextCode += 1
      if (nextCode >= (1 << codeSize) && codeSize < 12) codeSize += 1
    }
    previous = entry
  }
  return Buffer.from(output)
}

function twoPercentStretch(samples, count, fallbackMinimum, fallbackMaximum) {
  if (count < 1) return { minimum: fallbackMinimum, maximum: fallbackMaximum }
  const sorted = samples.subarray(0, count)
  sorted.sort()
  const percentile = (fraction) => {
    const position = (count - 1) * fraction
    const lower = Math.floor(position)
    const upper = Math.ceil(position)
    if (lower === upper) return sorted[lower]
    const weight = position - lower
    return sorted[lower] * (1 - weight) + sorted[upper] * weight
  }
  const minimum = percentile(.02)
  const maximum = percentile(.98)
  return Number.isFinite(minimum) && Number.isFinite(maximum) && maximum > minimum
    ? { minimum, maximum }
    : { minimum: fallbackMinimum, maximum: fallbackMaximum }
}

function thermalSurfaceStretch(samples, count, zeroCount, totalCount, fallbackMinimum, fallbackMaximum) {
  const ordinary = twoPercentStretch(samples, count, fallbackMinimum, fallbackMaximum)
  if (count < 64 || zeroCount / Math.max(1, totalCount) < .01) return { ...ordinary, mode: 'percentile' }

  // A finite-scene horizon commonly contains three populations: invalid zero,
  // surface temperature and a narrow, warmer sky-temperature plateau. Locate
  // the strong surface/sky gap so surface structure is not compressed into a
  // few grey levels. The raw range and high-temperature overlay stay intact.
  const sorted = samples.subarray(0, count)
  sorted.sort()
  const first = Math.floor(count * .30)
  const last = Math.min(count - 2, Math.floor(count * .92))
  let split = -1
  let largestGap = 0
  for (let index = first; index <= last; index += 1) {
    const gap = sorted[index + 1] - sorted[index]
    if (gap > largestGap) { largestGap = gap; split = index }
  }
  const centralRange = sorted[Math.floor((count - 1) * .90)] - sorted[Math.floor((count - 1) * .10)]
  if (split < 0 || largestGap < Math.max(2, centralRange * .12)) {
    return { ...ordinary, mode: 'percentile' }
  }
  const surfaceCount = split + 1
  const upperCount = count - surfaceCount
  const detail = twoPercentStretch(sorted, surfaceCount, sorted[0], sorted[split])
  const upper98 = sorted[surfaceCount + Math.floor(Math.max(0, upperCount - 1) * .98)]
  return {
    ...detail,
    mode: 'thermal-surface',
    excludedMinimum: sorted[surfaceCount],
    excludedCeiling: upper98
  }
}

function stretchedByte(value, minimum, maximum, inverted = false) {
  if (!Number.isFinite(value)) return 0
  const range = maximum - minimum
  const normalized = range > 0 ? (value - minimum) / range : .5
  const displayed = inverted ? 1 - normalized : normalized
  return Math.max(0, Math.min(255, Math.round(displayed * 255)))
}

function readTiff(path, requestedBand = 0, includeValues = false) {
  const data = readFileSync(path)
  if (data.length < 8) throw new Error('TIFF 文件头不完整')
  const littleEndian = data.toString('ascii', 0, 2) === 'II'
  const bigEndian = data.toString('ascii', 0, 2) === 'MM'
  if (!littleEndian && !bigEndian) throw new Error('不是有效的 TIFF 文件')
  const u16 = (offset) => littleEndian ? data.readUInt16LE(offset) : data.readUInt16BE(offset)
  const u32 = (offset) => littleEndian ? data.readUInt32LE(offset) : data.readUInt32BE(offset)
  if (u16(2) !== 42) throw new Error('暂不支持 BigTIFF 格式，请导出标准 TIFF')
  const ifdOffset = u32(4)
  if (ifdOffset + 2 > data.length) throw new Error('TIFF IFD 偏移无效')
  const entryCount = u16(ifdOffset)
  const tags = new Map()
  const readValue = (entryOffset, type, count) => {
    const size = tiffTypeSize(type)
    const total = size * count
    if (!size || !Number.isSafeInteger(total)) throw new Error(`不支持 TIFF 字段类型 ${type}`)
    const valueOffset = total <= 4 ? entryOffset + 8 : (entryOffset + 12 > data.length ? -1 : u32(entryOffset + 8))
    if (valueOffset < 0 || valueOffset + total > data.length) throw new Error('TIFF 字段数据越界')
    const readNumber = (offset) => {
      if (type === 1 || type === 7 || type === 2) return data[offset]
      if (type === 3) return u16(offset)
      if (type === 4) return u32(offset)
      if (type === 5) return u32(offset) / (u32(offset + 4) || 1)
      if (type === 6) return data.readInt8(offset)
      if (type === 8) return littleEndian ? data.readInt16LE(offset) : data.readInt16BE(offset)
      if (type === 9) return littleEndian ? data.readInt32LE(offset) : data.readInt32BE(offset)
      if (type === 10) {
        const denominator = littleEndian ? data.readInt32LE(offset + 4) : data.readInt32BE(offset + 4)
        return (littleEndian ? data.readInt32LE(offset) : data.readInt32BE(offset)) / (denominator || 1)
      }
      if (type === 11) return littleEndian ? data.readFloatLE(offset) : data.readFloatBE(offset)
      if (type === 12) return littleEndian ? data.readDoubleLE(offset) : data.readDoubleBE(offset)
      throw new Error(`不支持 TIFF 字段类型 ${type}`)
    }
    if (type === 2) return data.subarray(valueOffset, valueOffset + total).toString('ascii').replace(/\0+$/, '')
    return Array.from({ length: count }, (_, index) => readNumber(valueOffset + index * size))
  }
  for (let index = 0; index < entryCount; index += 1) {
    const offset = ifdOffset + 2 + index * 12
    if (offset + 12 > data.length) throw new Error('TIFF IFD 不完整')
    tags.set(u16(offset), readValue(offset, u16(offset + 2), u32(offset + 4)))
  }
  const tagValue = (tag, fallback) => tags.has(tag) ? tags.get(tag) : fallback
  const width = Number(tagValue(256, [0])[0])
  const height = Number(tagValue(257, [0])[0])
  const bitsPerSample = tagValue(258, [8]).map(Number)
  const compression = Number(tagValue(259, [1])[0])
  const photometric = Number(tagValue(262, [1])[0])
  const samples = Math.max(1, Number(tagValue(277, [1])[0]))
  const planar = Number(tagValue(284, [1])[0])
  const sampleFormats = tagValue(339, [1]).map(Number)
  const orientation = Number(tagValue(274, [1])[0])
  const predictor = Number(tagValue(317, [1])[0])
  const noDataText = String(tagValue(42113, '') || '').trim()
  const noData = noDataText && Number.isFinite(Number(noDataText)) ? Number(noDataText) : null
  const pixelScale = tagValue(33550, []).map(Number)
  const tiePoint = tagValue(33922, []).map(Number)
  const geoKeys = tagValue(34735, []).map(Number)
  const geoAscii = String(tagValue(34737, '') || '').replace(/\|+$/, '').trim()
  const geoKey = (key) => {
    for (let index = 4; index + 3 < geoKeys.length; index += 4) {
      if (geoKeys[index] === key && geoKeys[index + 1] === 0) return geoKeys[index + 3]
    }
    return null
  }
  const modelType = geoKey(1024)
  const projectedCode = geoKey(3072)
  const linearUnit = geoKey(3076)
  if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1) throw new Error('TIFF 缺少有效的宽度或高度')
  if (![1, 2].includes(planar)) throw new Error(`暂不支持 TIFF planar configuration ${planar}`)
  if (![1, 5, 8, 32773, 32946].includes(compression)) throw new Error(`暂不支持 TIFF compression ${compression}`)
  const tiled = tags.has(322) && tags.has(323) && tags.has(324) && tags.has(325)
  const tileWidth = tiled ? Number(tagValue(322, [width])[0]) : width
  const tileHeight = tiled ? Number(tagValue(323, [height])[0]) : Number(tagValue(278, [height])[0])
  const offsets = (tiled ? tagValue(324, []) : tagValue(273, [])).map(Number)
  const byteCounts = (tiled ? tagValue(325, []) : tagValue(279, [])).map(Number)
  if (!offsets.length || offsets.length !== byteCounts.length) throw new Error('TIFF 缺少有效的图像数据分块')
  if (!bitsPerSample.every((bits) => [1, 8, 16, 32, 64].includes(bits))) throw new Error(`暂不支持 TIFF bits per sample ${bitsPerSample.join(',')}`)
  if (!sampleFormats.every((format) => [1, 2, 3].includes(format))) throw new Error(`暂不支持 TIFF sample format ${sampleFormats.join(',')}`)

  const chunksPerRow = tiled ? Math.ceil(width / tileWidth) : 1
  const chunksPerColumn = Math.ceil(height / tileHeight)
  const chunksPerPlane = chunksPerRow * chunksPerColumn
  const decodedChunks = new Map()
  const getChunk = (index) => {
    if (decodedChunks.has(index)) return decodedChunks.get(index)
    if (!Number.isInteger(offsets[index]) || !Number.isInteger(byteCounts[index]) || offsets[index] < 0 || byteCounts[index] < 0 || offsets[index] + byteCounts[index] > data.length) throw new Error('TIFF 图像数据分块越界')
    const source = data.subarray(offsets[index], offsets[index] + byteCounts[index])
    let decoded = source
    if (compression === 5) decoded = unpackTiffLzw(source)
    if (compression === 8 || compression === 32946) decoded = inflateSync(source)
    if (compression === 32773) {
      const output = []
      for (let offset = 0; offset < source.length;) {
        const control = source[offset++]
        if (control <= 127) for (let count = 0; count < control + 1 && offset < source.length; count += 1) output.push(source[offset++])
        else if (control >= 129 && offset < source.length) { const value = source[offset++]; for (let count = 0; count < 257 - control; count += 1) output.push(value) }
      }
      decoded = Buffer.from(output)
    }
    if (predictor === 2) {
      const bits = Number(bitsPerSample[0])
      const sampleBytes = bits / 8
      const rowSamples = (tiled ? tileWidth : width) * (planar === 2 ? 1 : samples)
      const rowBytes = rowSamples * sampleBytes
      for (let row = 0; row < Math.floor(decoded.length / rowBytes); row += 1) {
        const rowStart = row * rowBytes
        for (let offset = sampleBytes * (planar === 2 ? 1 : samples); offset < rowBytes; offset += 1) decoded[rowStart + offset] = (decoded[rowStart + offset] + decoded[rowStart + offset - sampleBytes * (planar === 2 ? 1 : samples)]) & 255
      }
    }
    decodedChunks.set(index, decoded)
    return decoded
  }
  const readSample = (chunk, bitOffset, bits, format) => {
    if (bits === 1) return (chunk[Math.floor(bitOffset / 8)] >> (7 - (bitOffset % 8))) & 1
    const offset = Math.floor(bitOffset / 8)
    if (offset + bits / 8 > chunk.length) return Number.NaN
    if (bits === 8) return format === 2 ? chunk.readInt8(offset) : chunk[offset]
    if (bits === 16) return format === 2 ? (littleEndian ? chunk.readInt16LE(offset) : chunk.readInt16BE(offset)) : (littleEndian ? chunk.readUInt16LE(offset) : chunk.readUInt16BE(offset))
    if (bits === 32) return format === 3 ? (littleEndian ? chunk.readFloatLE(offset) : chunk.readFloatBE(offset)) : format === 2 ? (littleEndian ? chunk.readInt32LE(offset) : chunk.readInt32BE(offset)) : (littleEndian ? chunk.readUInt32LE(offset) : chunk.readUInt32BE(offset))
    return format === 3 ? (littleEndian ? chunk.readDoubleLE(offset) : chunk.readDoubleBE(offset)) : Number.NaN
  }
  const sourceValueAt = (x, y, band) => {
    const bits = Number(bitsPerSample[band] || bitsPerSample[0])
    const format = Number(sampleFormats[band] || sampleFormats[0] || 1)
    const chunkColumn = tiled ? Math.floor(x / tileWidth) : 0
    const chunkRow = Math.floor(y / tileHeight)
    const chunkInPlane = chunkRow * chunksPerRow + chunkColumn
    const chunkIndex = planar === 2 ? band * chunksPerPlane + chunkInPlane : chunkInPlane
    const chunk = getChunk(chunkIndex)
    const localX = tiled ? x % tileWidth : x
    const localY = y % tileHeight
    const pixelSamples = planar === 2 ? 1 : samples
    const sampleIndex = (localY * (tiled ? tileWidth : width) + localX) * pixelSamples + (planar === 2 ? 0 : band)
    return readSample(chunk, sampleIndex * bits, bits, format)
  }
  const rotated = [5, 6, 7, 8].includes(orientation)
  const outputWidth = rotated ? height : width
  const outputHeight = rotated ? width : height
  const sourceCoordinates = (x, y) => {
    if (orientation === 2) return [width - 1 - x, y]
    if (orientation === 3) return [width - 1 - x, height - 1 - y]
    if (orientation === 4) return [x, height - 1 - y]
    if (orientation === 5) return [y, x]
    if (orientation === 6) return [y, height - 1 - x]
    if (orientation === 7) return [width - 1 - y, height - 1 - x]
    if (orientation === 8) return [width - 1 - y, x]
    return [x, y]
  }
  const band = Math.max(0, Math.min(samples - 1, Number(requestedBand) || 0))
  const description = typeof tagValue(270, '') === 'string' ? tagValue(270, '') : ''
  const describedBands = description.match(/(?:band names|image bands|metrics)\s*[:=]\s*\{?([^}\r\n]+)\}?/i)?.[1]
  const bandNames = describedBands ? describedBands.split(',').map((value) => value.trim()).filter(Boolean) : samples === 3 && photometric === 2 ? ['红色', '绿色', '蓝色'] : Array.from({ length: samples }, (_, index) => `波段 ${index + 1}`)
  const activeBandName = String(bandNames[band] || '')
  const wavelengthNanometres = Number(activeBandName.match(/@\s*([0-9.]+)\s*nm/i)?.[1])
  const thermalBand = /temperature|热红外|thermal/i.test(activeBandName) || (Number.isFinite(wavelengthNanometres) && wavelengthNanometres >= 3000)
  let minimum = Infinity, maximum = -Infinity, elevationSum = 0, elevationCount = 0
  const values = includeValues ? new Float64Array(outputWidth * outputHeight) : null
  const valueAt = (x, y) => {
    const [sourceX, sourceY] = sourceCoordinates(x, y)
    const value = sourceValueAt(sourceX, sourceY, band)
    return noData != null && value === noData ? Number.NaN : value
  }
  for (let y = 0; y < outputHeight; y += 1) for (let x = 0; x < outputWidth; x += 1) {
    const value = valueAt(x, y)
    if (values) values[y * outputWidth + x] = value
    if (Number.isFinite(value)) {
      minimum = Math.min(minimum, value); maximum = Math.max(maximum, value)
      elevationSum += value; elevationCount += 1
    }
  }
  if (!Number.isFinite(minimum)) { minimum = 0; maximum = 0 }
  const scale = Math.min(1, 768 / Math.max(outputWidth, outputHeight))
  const previewWidth = Math.max(1, Math.round(outputWidth * scale))
  const previewHeight = Math.max(1, Math.round(outputHeight * scale))
  const pixels = Buffer.alloc(previewWidth * previewHeight)
  const previewValues = new Float64Array(previewWidth * previewHeight)
  const finiteSamples = new Float64Array(previewWidth * previewHeight)
  let finiteSampleCount = 0
  let zeroSampleCount = 0
  for (let y = 0; y < previewHeight; y += 1) for (let x = 0; x < previewWidth; x += 1) {
    const value = valueAt(Math.min(outputWidth - 1, Math.floor(x / scale)), Math.min(outputHeight - 1, Math.floor(y / scale)))
    const index = y * previewWidth + x
    previewValues[index] = value
    if (Number.isFinite(value)) {
      if (thermalBand && value <= 0) zeroSampleCount += 1
      else finiteSamples[finiteSampleCount++] = value
    }
  }
  const stretch = thermalBand
    ? thermalSurfaceStretch(finiteSamples, finiteSampleCount, zeroSampleCount,
      previewWidth * previewHeight, minimum, maximum)
    : twoPercentStretch(finiteSamples, finiteSampleCount, minimum, maximum)
  for (let index = 0; index < pixels.length; index += 1) {
    pixels[index] = stretchedByte(previewValues[index], stretch.minimum, stretch.maximum, photometric === 0)
  }
  let hotspotPixels = null
  let hotspotCount = 0
  let hotspotThreshold = null
  if (thermalBand && maximum > stretch.maximum) {
    const stretchRange = Math.max(0, stretch.maximum - stretch.minimum)
    const ordinaryThreshold = stretch.maximum + Math.max(stretchRange * 2, Math.abs(stretch.maximum) * .05, 1e-9)
    const excludedThreshold = Number.isFinite(stretch.excludedCeiling)
      ? stretch.excludedCeiling + Math.max(Math.abs(stretch.excludedCeiling) * .05, 2) : -Infinity
    hotspotThreshold = Math.max(ordinaryThreshold, excludedThreshold)
    if (maximum > hotspotThreshold) {
      hotspotPixels = Buffer.alloc(previewWidth * previewHeight)
      const hotspotRange = maximum - hotspotThreshold
      for (let index = 0; index < hotspotPixels.length; index += 1) {
        const value = previewValues[index]
        if (!Number.isFinite(value) || value <= hotspotThreshold) continue
        const normalized = Math.sqrt(Math.max(0, Math.min(1, (value - hotspotThreshold) / hotspotRange)))
        hotspotPixels[index] = Math.max(48, Math.round(normalized * 255))
        hotspotCount += 1
      }
    }
  }
  const bits = Number(bitsPerSample[0] || 8)
  const sampleFormat = Number(sampleFormats[0] || 1)
  const dataType = `${sampleFormat === 3 ? 'Float' : sampleFormat === 2 ? 'Int' : 'UInt'}${bits}`
  const coordinateType = modelType === 1 ? '投影坐标' : modelType === 2 ? '经纬度坐标' : '未声明坐标系'
  const crs = projectedCode && projectedCode !== 32767 ? `EPSG:${projectedCode}` : geoAscii || coordinateType
  const unit = linearUnit === 9001 ? 'm' : modelType === 2 ? '°' : ''
  const terrainScale = Math.min(1, 96 / Math.max(outputWidth, outputHeight))
  const terrainWidth = Math.max(2, Math.round(outputWidth * terrainScale))
  const terrainHeight = Math.max(2, Math.round(outputHeight * terrainScale))
  const terrainValues = []
  for (let y = 0; y < terrainHeight; y += 1) for (let x = 0; x < terrainWidth; x += 1) {
    const sourceX = Math.min(outputWidth - 1, Math.round(x * (outputWidth - 1) / Math.max(1, terrainWidth - 1)))
    const sourceY = Math.min(outputHeight - 1, Math.round(y * (outputHeight - 1) / Math.max(1, terrainHeight - 1)))
    const value = valueAt(sourceX, sourceY)
    terrainValues.push(Number.isFinite(value) ? value : null)
  }
  return {
    kind: 'tiff', path: windowsPath(path), width: outputWidth, height: outputHeight,
    bands: samples, band, bandNames, previewWidth, previewHeight, minimum, maximum,
    stretchMinimum: stretch.minimum, stretchMaximum: stretch.maximum, stretchPercent: 2,
    stretchMode: stretch.mode || 'percentile',
    stretchExcludedMinimum: Number.isFinite(stretch.excludedMinimum) ? stretch.excludedMinimum : null,
    pixels: pixels.toString('base64'), noData, dataType, coordinateType, crs, unit,
    thermalBand, wavelengthNanometres: Number.isFinite(wavelengthNanometres) ? wavelengthNanometres : null,
    hotspotThreshold, hotspotCount,
    hotspotPixels: hotspotPixels && hotspotCount ? hotspotPixels.toString('base64') : null,
    pixelSizeX: Number.isFinite(pixelScale[0]) ? pixelScale[0] : null,
    pixelSizeY: Number.isFinite(pixelScale[1]) ? pixelScale[1] : null,
    originX: tiePoint.length >= 6 ? tiePoint[3] : null,
    originY: tiePoint.length >= 6 ? tiePoint[4] : null,
    georeferenced: pixelScale.length >= 2 && tiePoint.length >= 6,
    mean: elevationCount ? elevationSum / elevationCount : 0,
    terrainWidth, terrainHeight, terrainValues,
    ...(values ? { values } : {})
  }
}

function readWindTiff(path) {
  const speed = readTiff(path, 0, true)
  if (speed.bands < 4) throw new Error('风速 TIFF 必须包含速度、X、垂直和 Y 四个波段')
  const componentX = readTiff(path, 1, true)
  const componentY = readTiff(path, 3, true)
  const targetArrowsAcross = 28
  const step = Math.max(1, Math.ceil(Math.max(speed.width, speed.height) / targetArrowsAcross))
  const firstX = Math.min(speed.width - 1, Math.floor(step / 2))
  const firstY = Math.min(speed.height - 1, Math.floor(step / 2))
  const arrows = []
  for (let y = firstY; y < speed.height; y += step) {
    for (let x = firstX; x < speed.width; x += step) {
      const index = y * speed.width + x
      const u = Number(componentX.values[index])
      const v = Number(componentY.values[index])
      const magnitude = Number(speed.values[index])
      if (Number.isFinite(u) && Number.isFinite(v) && Number.isFinite(magnitude)) {
        arrows.push([x, y, u, v, magnitude])
      }
    }
  }
  const result = { ...speed }
  delete result.values
  return {
    ...result,
    kind: 'wind',
    band: 0,
    bandNames: ['水平风速大小', 'X 分量', '垂直分量', 'Y 分量'],
    arrows,
    arrowStep: step,
    adaptive: true
  }
}

function compareTiffResults(firstPath, secondPath, requestedBand = 0, requestedScatterPoints = 4000) {
  const band = Math.max(0, Math.floor(Number(requestedBand) || 0))
  const first = readTiff(firstPath, band, true)
  const second = readTiff(secondPath, band, true)
  const commonBands = Math.min(first.bands, second.bands)
  if (band >= commonBands) throw new Error(`所选波段超出共同波段范围（共 ${commonBands} 个）`)
  if (first.width !== second.width || first.height !== second.height) {
    throw new Error(`图像尺寸不一致：A 为 ${first.width} × ${first.height}，B 为 ${second.width} × ${second.height}`)
  }
  if (first.georeferenced && second.georeferenced) {
    const different = first.crs !== second.crs || ['pixelSizeX', 'pixelSizeY', 'originX', 'originY']
      .some((key) => Math.abs(Number(first[key]) - Number(second[key])) > 1e-8)
    if (different) throw new Error('两幅图像的坐标系、原点或像元大小不一致，无法逐像元比较')
  }

  let validPixels = 0, meanDifference = 0, sumAbsolute = 0, sumSquared = 0
  let differenceMinimum = Infinity, differenceMaximum = -Infinity
  let meanFirst = 0, meanSecond = 0, covariance = 0, varianceFirst = 0, varianceSecond = 0
  for (let index = 0; index < first.values.length; index += 1) {
    const a = first.values[index], b = second.values[index]
    if (!Number.isFinite(a) || !Number.isFinite(b)) continue
    const difference = b - a
    validPixels += 1
    meanDifference += (difference - meanDifference) / validPixels
    sumAbsolute += Math.abs(difference)
    sumSquared += difference * difference
    differenceMinimum = Math.min(differenceMinimum, difference)
    differenceMaximum = Math.max(differenceMaximum, difference)
    const deltaFirst = a - meanFirst
    meanFirst += deltaFirst / validPixels
    const deltaSecond = b - meanSecond
    meanSecond += deltaSecond / validPixels
    covariance += deltaFirst * (b - meanSecond)
    varianceFirst += deltaFirst * (a - meanFirst)
    varianceSecond += deltaSecond * (b - meanSecond)
  }
  if (!validPixels) throw new Error('两幅图像没有可共同比较的有效像元')

  const maxScatterPoints = Math.max(100, Math.min(10000, Math.floor(Number(requestedScatterPoints) || 4000)))
  const scatterTarget = Math.min(validPixels, maxScatterPoints)
  const scatter = []
  const differenceSamples = new Float64Array(validPixels)
  let validIndex = 0
  for (let index = 0; index < first.values.length; index += 1) {
    const a = first.values[index], b = second.values[index]
    if (!Number.isFinite(a) || !Number.isFinite(b)) continue
    if (validIndex >= Math.floor(scatter.length * validPixels / scatterTarget)) scatter.push([a, b])
    differenceSamples[validIndex] = b - a
    validIndex += 1
  }
  differenceSamples.sort()
  const percentile = (fraction) => {
    const position = (differenceSamples.length - 1) * fraction
    const lower = Math.floor(position), upper = Math.ceil(position)
    if (lower === upper) return differenceSamples[lower]
    const weight = position - lower
    return differenceSamples[lower] * (1 - weight) + differenceSamples[upper] * weight
  }
  const differencePercentileMinimum = percentile(.02)
  const differencePercentileMaximum = percentile(.98)

  const previewScale = Math.min(1, 768 / Math.max(first.width, first.height))
  const previewWidth = Math.max(1, Math.round(first.width * previewScale))
  const previewHeight = Math.max(1, Math.round(first.height * previewScale))
  const differenceLimit = Math.max(Math.abs(differencePercentileMinimum), Math.abs(differencePercentileMaximum), Number.EPSILON)
  const differencePixels = Buffer.alloc(previewWidth * previewHeight * 4)
  const negative = [36, 111, 211], neutral = [255, 255, 255], positive = [227, 74, 66]
  for (let y = 0; y < previewHeight; y += 1) for (let x = 0; x < previewWidth; x += 1) {
    const sourceX = Math.min(first.width - 1, Math.floor(x / previewScale))
    const sourceY = Math.min(first.height - 1, Math.floor(y / previewScale))
    const sourceIndex = sourceY * first.width + sourceX
    const a = first.values[sourceIndex], b = second.values[sourceIndex]
    const offset = (y * previewWidth + x) * 4
    if (!Number.isFinite(a) || !Number.isFinite(b)) {
      differencePixels[offset + 3] = 0
      continue
    }
    const difference = b - a
    const ratio = differenceLimit > 0 ? Math.min(1, Math.abs(difference) / differenceLimit) : 0
    const target = difference < 0 ? negative : positive
    for (let channel = 0; channel < 3; channel += 1) differencePixels[offset + channel] = Math.round(neutral[channel] + (target[channel] - neutral[channel]) * ratio)
    differencePixels[offset + 3] = 255
  }

  const bandNames = Array.from({ length: commonBands }, (_, index) => {
    const firstName = first.bandNames[index] || `波段 ${index + 1}`
    const secondName = second.bandNames[index] || `波段 ${index + 1}`
    return firstName === secondName ? firstName : `${firstName} / ${secondName}`
  })
  const correlationDenominator = Math.sqrt(varianceFirst * varianceSecond)
  return {
    kind: 'change', firstPath: windowsPath(firstPath), secondPath: windowsPath(secondPath),
    width: first.width, height: first.height, band, bands: commonBands, bandNames,
    previewWidth, previewHeight, differencePixels: differencePixels.toString('base64'),
    differenceMinimum, differenceMaximum, differencePercentileMinimum, differencePercentileMaximum, differenceLimit,
    validPixels, meanDifference, mae: sumAbsolute / validPixels,
    rmse: Math.sqrt(sumSquared / validPixels),
    correlation: correlationDenominator > 0 ? covariance / correlationDenominator : null,
    meanFirst, meanSecond, scatter
  }
}

function inspectAsciiGrid(data) {
  const text = data.toString('utf8').replace(/^\uFEFF/, '')
  const lines = text.split(/\r?\n/)
  const header = new Map()
  let dataStart = 0
  const keys = new Set(['ncols', 'nrows', 'xllcorner', 'xllcenter', 'yllcorner', 'yllcenter', 'cellsize', 'nodata_value'])
  for (let index = 0; index < lines.length; index += 1) {
    const match = lines[index].trim().match(/^(\S+)\s+(.+)$/)
    const key = match?.[1]?.toLowerCase()
    if (!match || !keys.has(key)) { dataStart = index; break }
    header.set(key, Number(match[2]))
    dataStart = index + 1
  }
  const width = Number(header.get('ncols'))
  const height = Number(header.get('nrows'))
  const cellSize = Number(header.get('cellsize'))
  if (![width, height].every((value) => Number.isInteger(value) && value > 0) || !(cellSize > 0))
    throw new Error('ASC 缺少有效的 NCOLS、NROWS 或 CELLSIZE')
  if (width * height > 25_000_000) throw new Error('DEM 栅格超过 2500 万像元，请先裁剪或降采样')
  const values = lines.slice(dataStart).join(' ').trim().split(/\s+/).filter(Boolean).map(Number)
  if (values.length !== width * height || values.some((value) => !Number.isFinite(value)))
    throw new Error(`ASC 高程数据数量无效，应为 ${width * height} 个数值`)
  const noDataValue = header.has('nodata_value') ? Number(header.get('nodata_value')) : null
  let minimum = Infinity
  let maximum = -Infinity
  let elevationSum = 0
  let elevationCount = 0
  for (const value of values) {
    if (noDataValue != null && value === noDataValue) continue
    minimum = Math.min(minimum, value)
    maximum = Math.max(maximum, value)
    elevationSum += value
    elevationCount += 1
  }
  if (!Number.isFinite(minimum)) throw new Error('ASC 中没有有效高程像元')
  const terrainScale = Math.min(1, 96 / Math.max(width, height))
  const terrainWidth = Math.max(2, Math.round(width * terrainScale))
  const terrainHeight = Math.max(2, Math.round(height * terrainScale))
  const terrainValues = []
  for (let y = 0; y < terrainHeight; y += 1) for (let x = 0; x < terrainWidth; x += 1) {
    const sourceX = Math.min(width - 1, Math.round(x * (width - 1) / Math.max(1, terrainWidth - 1)))
    const sourceY = Math.min(height - 1, Math.round(y * (height - 1) / Math.max(1, terrainHeight - 1)))
    const value = values[sourceY * width + sourceX]
    terrainValues.push(noDataValue != null && value === noDataValue ? null : value)
  }
  return {
    format: 'ESRI ASCII Grid', width, height, bands: 1, dataType: '文本数值',
    pixelSizeX: cellSize, pixelSizeY: cellSize, unit: 'm', noData: noDataValue,
    minimum, maximum, relief: maximum - minimum, coordinateType: '局部/投影坐标',
    crs: 'ASC 未内嵌 CRS', georeferenced: header.has('xllcorner') || header.has('xllcenter'),
    originX: header.get('xllcorner') ?? header.get('xllcenter') ?? null,
    originY: header.get('yllcorner') ?? header.get('yllcenter') ?? null,
    mean: elevationCount ? elevationSum / elevationCount : 0,
    terrainWidth, terrainHeight, terrainValues
  }
}

function inspectDemFile(path, extension) {
  if (extension === '.asc') return inspectAsciiGrid(readFileSync(path))
  const raster = readTiff(path, 0)
  if (raster.bands !== 1) throw new Error(`DEM 必须是单波段栅格，当前文件有 ${raster.bands} 个波段`)
  return {
    format: 'GeoTIFF', width: raster.width, height: raster.height, bands: raster.bands,
    dataType: raster.dataType, pixelSizeX: raster.pixelSizeX, pixelSizeY: raster.pixelSizeY,
    unit: raster.unit, noData: raster.noData, minimum: raster.minimum, maximum: raster.maximum,
    relief: raster.maximum - raster.minimum, coordinateType: raster.coordinateType,
    crs: raster.crs, georeferenced: raster.georeferenced,
    originX: raster.originX, originY: raster.originY, mean: raster.mean,
    terrainWidth: raster.terrainWidth, terrainHeight: raster.terrainHeight,
    terrainValues: raster.terrainValues
  }
}

function readEnvi(path, requestedBand = 0) {
  const header = readFileSync(path, 'utf8')
  const metadata = enviMetadata(header)
  const width = Number(metadata.samples)
  const height = Number(metadata.lines)
  const bands = Number(metadata.bands)
  const dataType = Number(metadata['data type'])
  const headerOffset = Number(metadata['header offset'] || 0)
  const interleave = String(metadata.interleave || 'bsq').toLowerCase().replace('bsp', 'bsq')
  const littleEndian = Number(metadata['byte order'] || 0) === 0
  const band = Math.max(0, Math.min(bands - 1, Number(requestedBand) || 0))
  const bandNamesText = header.match(/band names\s*=\s*\{([^}]*)\}/i)?.[1] || metadata['band names'] || ''
  const bandNames = String(bandNamesText).split(',').map((value) => value.trim()).filter(Boolean)
  if (![width, height, bands].every((value) => Number.isInteger(value) && value > 0)) throw new Error('ENVI 头文件缺少有效的 samples、lines 或 bands')
  const typeSize = enviTypeSize(dataType)
  if (!typeSize) throw new Error(`暂不支持 ENVI data type ${dataType}`)
  const imagePath = path.slice(0, -extname(path).length) + '.img'
  if (!existsSync(imagePath)) throw new Error(`找不到 ENVI 影像数据：${imagePath}`)
  const data = readFileSync(imagePath)
  const expectedSize = headerOffset + width * height * bands * typeSize
  if (data.length < expectedSize) throw new Error(`ENVI 数据不完整：需要 ${expectedSize} 字节，实际 ${data.length} 字节`)

  const valueAt = (x, y) => {
    let index
    if (interleave === 'bil') index = y * width * bands + band * width + x
    else if (interleave === 'bip') index = (y * width + x) * bands + band
    else index = band * width * height + y * width + x
    return enviValue(data, headerOffset + index * typeSize, dataType, littleEndian)
  }
  let minimum = Infinity, maximum = -Infinity
  for (let y = 0; y < height; y += 1) for (let x = 0; x < width; x += 1) {
    const value = valueAt(x, y)
    if (Number.isFinite(value)) { minimum = Math.min(minimum, value); maximum = Math.max(maximum, value) }
  }
  if (!Number.isFinite(minimum)) { minimum = 0; maximum = 0 }
  const scale = Math.min(1, 768 / Math.max(width, height))
  const previewWidth = Math.max(1, Math.round(width * scale))
  const previewHeight = Math.max(1, Math.round(height * scale))
  const pixels = Buffer.alloc(previewWidth * previewHeight)
  const previewValues = new Float64Array(previewWidth * previewHeight)
  const finiteSamples = new Float64Array(previewWidth * previewHeight)
  let finiteSampleCount = 0
  for (let y = 0; y < previewHeight; y += 1) for (let x = 0; x < previewWidth; x += 1) {
    const value = valueAt(Math.min(width - 1, Math.floor(x / scale)), Math.min(height - 1, Math.floor(y / scale)))
    const index = y * previewWidth + x
    previewValues[index] = value
    if (Number.isFinite(value)) finiteSamples[finiteSampleCount++] = value
  }
  const stretch = twoPercentStretch(finiteSamples, finiteSampleCount, minimum, maximum)
  for (let index = 0; index < pixels.length; index += 1) {
    pixels[index] = stretchedByte(previewValues[index], stretch.minimum, stretch.maximum)
  }
  return { kind: 'envi', path: windowsPath(path), width, height, bands, band, bandNames, previewWidth, previewHeight, minimum, maximum, stretchMinimum: stretch.minimum, stretchMaximum: stretch.maximum, stretchPercent: 2, pixels: pixels.toString('base64') }
}

async function api(request, response, url) {
  if (request.method === 'GET' && url.pathname === '/api/defaults') {
    const path = executable()
    return json(response, 200, { executable: windowsPath(path), executableExists: existsSync(path), radiosityExecutable: windowsPath(path), radiosityExecutableExists: existsSync(path), projectFile: windowsPath(projectFile), platform: process.platform, versions: { node: process.versions.node, three: '0.185.1' } })
  }
  if (request.method === 'GET' && url.pathname === '/api/events') {
    response.writeHead(200, { 'Content-Type': 'text/event-stream', 'Cache-Control': 'no-cache', Connection: 'keep-alive' })
    response.write(': connected\n\n')
    clients.add(response)
    request.on('close', () => clients.delete(response))
    return
  }
  if (request.method === 'POST' && url.pathname === '/api/project/open') {
    try {
      const data = await body(request)
      return json(response, 200, openProject(data.path))
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/pick') {
    try {
      const selected = await chooseProjectFile()
      if (!selected) return json(response, 200, { cancelled: true })
      return json(response, 200, openProject(selected))
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/create') {
    try {
      const result = createProject(await body(request))
      projectFile = result.path
      projectDir = result.projectDir
      return json(response, 201, { ...result, path: windowsPath(result.path), projectDir: windowsPath(result.projectDir), projectPath: windowsPath(result.projectPath) })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/save-as') {
    try {
      const result = saveProjectAs(await body(request))
      projectFile = result.path
      projectDir = result.projectDir
      return json(response, 201, { ...result, path: windowsPath(result.path), projectDir: windowsPath(result.projectDir), projectPath: windowsPath(result.projectPath) })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/import-obj') {
    try {
      const data = await body(request)
      if (!projectDir || !projectFile) throw new Error('请先新建或打开工程')
      const name = basename(String(data.name || '').trim())
      if (!name || extname(name).toLowerCase() !== '.obj') throw new Error('只能导入 OBJ 文件')
      const content = sanitizeObjContent(data.content)
      if (!content.trim()) throw new Error('OBJ 文件为空')
      if (Buffer.byteLength(content, 'utf8') > 80 * 1024 * 1024) throw new Error('OBJ 文件超过 80MB，请使用简化模型')
      const modelsDir = join(projectDir, 'models')
      mkdirSync(modelsDir, { recursive: true })
      const path = join(modelsDir, name)
      const positionsDir = join(projectDir, 'positions')
      mkdirSync(positionsDir, { recursive: true })
      const positionPath = join(positionsDir, `${name.slice(0, -4)}_position.txt`)
      writeFileSync(path, content, 'utf8')
      if (!existsSync(positionPath)) writeFileSync(positionPath, '0 0 0 1 0\n', 'utf8')
      return json(response, 201, { path: windowsPath(path), name, positionPath: windowsPath(positionPath), relativePath: join('models', name).replaceAll('\\', '/') })
    } catch (error) { return json(response, 400, { error: error.message }) }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/import-meteo') {
    try {
      const data = await body(request)
      if (!projectDir || !projectFile) throw new Error('请先新建或打开工程')
      const name = basename(String(data.name || '').trim())
      const allowedExtensions = new Set(['.txt', '.meteo', '.dat', '.csv'])
      if (!name || !allowedExtensions.has(extname(name).toLowerCase())) throw new Error('只能导入 TXT、METEO、DAT 或 CSV 气象文件')
      const content = String(data.content || '')
      if (!content.trim()) throw new Error('气象驱动文件为空')
      if (Buffer.byteLength(content, 'utf8') > 20 * 1024 * 1024) throw new Error('气象驱动文件超过 20MB')
      const meteorologyDir = join(projectDir, 'meteorology')
      mkdirSync(meteorologyDir, { recursive: true })
      const path = join(meteorologyDir, name)
      writeFileSync(path, content, 'utf8')
      return json(response, 201, { path: windowsPath(path), name, relativePath: join('meteorology', name).replaceAll('\\', '/') })
    } catch (error) { return json(response, 400, { error: error.message }) }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/import-dem') {
    let importedPath = ''
    try {
      const data = await body(request)
      const targetProjectFile = data.projectPath ? resolveProjectFile(data.projectPath) : projectFile
      const targetProjectDir = targetProjectFile ? dirname(targetProjectFile) : projectDir
      if (!targetProjectDir || !targetProjectFile) throw new Error('请先新建或打开工程')
      const sourceName = basename(String(data.name || '').trim())
      const extension = extname(sourceName).toLowerCase()
      if (!sourceName || !new Set(['.tif', '.tiff', '.asc']).has(extension))
        throw new Error('DEM 仅支持单波段 GeoTIFF（TIF/TIFF）或 ESRI ASCII Grid（ASC）')
      const encoded = String(data.contentBase64 || '')
      if (!encoded || encoded.length > 90 * 1024 * 1024) throw new Error('DEM 文件为空或超过 64MB')
      const content = Buffer.from(encoded, 'base64')
      if (!content.length || content.length > 64 * 1024 * 1024) throw new Error('DEM 文件为空或超过 64MB')
      const terrainDir = join(targetProjectDir, 'terrain')
      mkdirSync(terrainDir, { recursive: true })
      const stem = sourceName.slice(0, -extension.length).replace(/[^\p{L}\p{N}_-]+/gu, '_').replace(/^_+|_+$/g, '') || 'terrain'
      let name = `${stem}${extension}`
      for (let suffix = 2; existsSync(join(terrainDir, name)); suffix += 1) name = `${stem}_${suffix}${extension}`
      importedPath = join(terrainDir, name)
      writeFileSync(importedPath, content)
      const metadata = inspectDemFile(importedPath, extension)
      const warnings = []
      if (metadata.coordinateType === '经纬度坐标') warnings.push('源文件为经纬度坐标；建议重投影到米制坐标后再模拟')
      if (!metadata.georeferenced) warnings.push('文件未包含完整地理参考；HiStream 仍会把整幅栅格映射到场景范围')
      if (!['Float32', 'Int16'].includes(metadata.dataType)) warnings.push(`当前像元类型为 ${metadata.dataType}；建议使用 Float32 或 Int16`)
      return json(response, 201, {
        path: windowsPath(importedPath), name,
        relativePath: join('terrain', name).replaceAll('\\', '/'), metadata, warnings
      })
    } catch (error) {
      if (importedPath && existsSync(importedPath)) unlinkSync(importedPath)
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/dem-info') {
    try {
      const data = await body(request)
      const targetProjectFile = data.projectPath ? resolveProjectFile(data.projectPath) : projectFile
      const targetProjectDir = targetProjectFile ? dirname(targetProjectFile) : projectDir
      if (!targetProjectDir) throw new Error('请先新建或打开工程')
      const storedPath = normalizeHostPath(data.path).replace(/^"|"$/g, '')
      if (!storedPath) throw new Error('DEM 路径为空')
      const demPath = isAbsolute(storedPath) ? resolve(storedPath) : resolve(targetProjectDir, storedPath)
      const extension = extname(demPath).toLowerCase()
      if (!new Set(['.tif', '.tiff', '.asc']).has(extension))
        throw new Error('DEM 仅支持 TIF、TIFF 或 ASC')
      if (!existsSync(demPath) || !statSync(demPath).isFile()) throw new Error(`找不到 DEM：${demPath}`)
      return json(response, 200, { path: windowsPath(demPath), metadata: inspectDemFile(demPath, extension) })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/import-spectrum') {
    try {
      const data = await body(request)
      if (!projectDir || !projectFile) throw new Error('请先新建或打开工程')
      const name = basename(String(data.name || '').trim())
      const allowedExtensions = new Set(['.txt', '.dat', '.csv'])
      if (!name || !allowedExtensions.has(extname(name).toLowerCase())) throw new Error('只能导入 TXT、DAT 或 CSV 波谱文件')
      if (Buffer.byteLength(String(data.content || ''), 'utf8') > 5 * 1024 * 1024) throw new Error('波谱文件超过 5MB')
      const content = normalizeSpectrumContent(data.content)
      const spectralDir = join(projectDir, 'spectral')
      mkdirSync(spectralDir, { recursive: true })
      const path = join(spectralDir, name)
      writeFileSync(path, content, 'utf8')
      return json(response, 201, { path: windowsPath(path), name, relativePath: join('spectral', name).replaceAll('\\', '/') })
    } catch (error) { return json(response, 400, { error: error.message }) }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/distribution') {
    try {
      const data = await body(request)
      if (!projectDir || !projectFile) throw new Error('请先新建或打开工程')
      if (!Array.isArray(data.instances) || !data.instances.length) throw new Error('实例分布不能为空')
      if (data.instances.length > 100000) throw new Error('单个 OBJ 最多支持 100000 个实例')
      const positionsDir = resolve(projectDir, 'positions')
      mkdirSync(positionsDir, { recursive: true })
      const safeName = basename(String(data.name || 'object')).replace(/\.[^.]+$/, '').replace(/[^\p{L}\p{N}_-]+/gu, '_') || 'object'
      const path = data.path ? resolveAsset(data.path) : join(positionsDir, `${safeName}_position.txt`)
      if (dirname(path) !== positionsDir) throw new Error('实例分布文件必须位于当前工程 positions 目录')
      const lines = data.instances.map((item, index) => {
        const values = [item.x, item.y, item.z ?? 0, item.scale ?? 1, item.rotation ?? 0].map(Number)
        if (!values.every(Number.isFinite) || values[3] <= 0) throw new Error(`第 ${index + 1} 个实例参数无效`)
        return values.join(' ')
      })
      writeFileSync(path, `${lines.join('\n')}\n`, 'utf8')
      return json(response, 200, { path: windowsPath(path), relativePath: projectStoredPath(path, projectDir), count: lines.length })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/read') {
    try {
      const data = await body(request)
      const path = projectAssetPath(data.path, projectDir)
      if (!existsSync(path) || !statSync(path).isFile()) throw new Error(`找不到场景资源：${path}`)
      if (statSync(path).size > 80 * 1024 * 1024) throw new Error('预览资源超过 80MB，请使用简化模型')
      return json(response, 200, { path: windowsPath(path), content: readFileSync(path, 'utf8') })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/save') {
    try {
      const data = await body(request)
      const path = resolveProjectFile(data.path || projectFile)
      let candidate = data.project
      if (!candidate) {
        try { candidate = JSON.parse(String(data.content || '')) }
        catch (error) { throw new Error(`project.json 格式错误：${error.message}`) }
      }
      const validation = validateProject(candidate)
      if (!validation.valid) throw new Error(validation.errors.join('；'))
      let project = validation.project
      ensureDefaultSceneAsset(project, dirname(path))
      project = writeProject(path, project)
      const content = stringifyProject(project)
      projectFile = path
      projectDir = dirname(path)
      return json(response, 200, { path: windowsPath(path), content, project, projectPath: windowsPath(path) })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/run') {
    let runtimeScene = null
    try {
      const data = await body(request)
      if (resettingProcesses) throw new Error('模拟环境正在重置，请稍后再运行')
      if (child) throw new Error('已有模拟任务正在运行')
      if (!modes.has(data.mode)) throw new Error('不支持的计算模式')
      const input = resolveProjectFile(data.inputPath || projectFile)
      projectFile = input
      projectDir = dirname(input)
      const stored = readProject(input)
      const project = stored.project ? migrateProject(input, stored.project) : null
      if (!project) throw new Error('无法读取工程配置')
      runtimeScene = prepareRuntimeSceneProject(input, project, {
        resolveAssetPath: (value) => projectAssetPath(value, projectDir)
      })
      const runInput = runtimeScene.inputPath
      emit({
        type: 'stdout',
        text: `实例边界筛选：原始 ${runtimeScene.total.toLocaleString('zh-CN')} 个，保留 ${runtimeScene.kept.toLocaleString('zh-CN')} 个，完整包围盒在域外跳过 ${runtimeScene.excluded.toLocaleString('zh-CN')} 个；跨界对象仅计算域内部分${runtimeScene.clippedObjects ? `；预裁切 ${runtimeScene.clippedObjects} 个单实例 OBJ，三角面 ${runtimeScene.sourceTriangles.toLocaleString('zh-CN')} → ${runtimeScene.retainedTriangles.toLocaleString('zh-CN')}` : ''}${runtimeScene.invalid ? `，忽略无效记录 ${runtimeScene.invalid.toLocaleString('zh-CN')} 条` : ''}`
      })
      const facetMode = data.mode === 'eFacetRT' || data.mode === 'eFacetEB'
      if (facetMode) {
        const estimate = assertFacetScale(runtimeScene.project, projectDir, data.mode)
        emit({ type: 'stdout', text: `面元规模预检：约 ${estimate.total.toLocaleString('zh-CN')} 个三角面` })
      }
      const projectSource = readFileSync(input, "utf8")
      const referencedMaterials = new Set([
        project.configuration.scene?.background?.materialName,
        ...(project.configuration.objects?.items || []).flatMap(object =>
          [object.materialName, ...(object.meshes || []).map(mesh => mesh.materialName)])
      ])
      // PV process files are unconditional and also need persistent geometry,
      // even when the optional radiation/energy process switches are off.
      const keepThreeDimensionalResults = processOutputEnabled(projectSource, data.mode)
        || (data.mode === 'eFacetEB' && project.configuration.materials.some(material =>
          material.energyModel === 'photovoltaic' && referencedMaterials.has(material.name)))
      let engine, args, radiosityJsonPath = ''
      engine = normalizeHostPath(data.executable || executable())
      if (!existsSync(engine)) throw new Error(`找不到 HiStream：${windowsPath(engine)}`)
      if (facetMode) {
        const outputDirectory = join(projectDir, 'output')
        mkdirSync(outputDirectory, { recursive: true })
        const resultName = data.mode === "eFacetRT" ? "facetrt.json" : "faceteb.json"
        radiosityJsonPath = join(outputDirectory, keepThreeDimensionalResults ? resultName : ".streamsim-transient-" + data.mode + ".json")
        args = [data.mode, histreamRuntimePath(runInput, engine),
          histreamRuntimePath(radiosityJsonPath, engine)]
      } else {
        args = [data.mode, histreamRuntimePath(runInput, engine)]
      }
      const startedAt = Date.now()
      const workingDirectory = facetMode ? dirname(engine) : projectDir
      const streamFacetSteps = data.mode === 'eFacetEB'
      child = spawn(engine, args, { cwd: workingDirectory, windowsHide: true,
        env: { ...process.env, STREAMSIM_FACET_STEP_SYNC: streamFacetSteps ? '1' : '0' },
        stdio: [streamFacetSteps ? 'pipe' : 'ignore', 'pipe', 'pipe'] })
      childExecutable = engine
      const running = child
      emit({ type: 'started', pid: running.pid, command: `"${windowsPath(engine)}" ${args.map((item) => `"${windowsPath(item)}"`).join(' ')}` })
      running.stdout.on('data', (chunk) => emit({ type: 'stdout', text: chunk.toString() }))
      running.stderr.on('data', (chunk) => emit({ type: 'stderr', text: chunk.toString() }))
      let streamedSteps = 0, stepOutputError = null, stepPending = false
      if (streamFacetSteps) {
        // readline handles split/coalesced stdout chunks and CRLF safely.
        const lines = createInterface({ input: running.stdout, crlfDelay: Infinity })
        running.stdin.on('error', () => { /* Engine may be stopped during output. */ })
        lines.on('line', async (line) => {
          if (!line.startsWith('FACET_STEP\t') || running.streamsimReset || running.streamsimStopped) return
          try {
            const match = /^FACET_STEP\t(\d+)\t(DOY\d+_\d{2}-\d{2})\t([\d.eE+-]+)$/.exec(line)
            if (!match || stepPending) throw new Error('面元节点输出协议异常')
            const step = { node: Number(match[1]), token: match[2], julianTime: Number(match[3]) }
            if (!Number.isFinite(step.julianTime)) throw new Error('面元节点时间无效')
            stepPending = true
            emit({ type: 'stdout', text: `节点 ${step.node + 1} 已计算，正在逐方向保存图像：${step.token}` })
            const result = await exportFacetStep(running, radiosityJsonPath, input, step)
            if (running.streamsimReset || running.streamsimStopped || running.exitCode !== null) return
            streamedSteps += 1
            stepPending = false
            emit({ type: 'stdout', text: `节点 ${step.node + 1} 已保存 ${result.tifPaths.length} 个 TIFF，输出缓存已释放：${step.token}` })
            running.stdin.write(`FACET_ACK\t${step.node}\n`)
          } catch (error) {
            if (running.streamsimReset || running.streamsimStopped) return
            stepOutputError = error
            emit({ type: 'error', text: `节点影像生成失败（已保留节点文件）：${error.message}` })
            running.stdin.end('FACET_ERROR\n')
          }
        })
      }
      running.on('error', (error) => {
        runtimeScene?.cleanup()
        if (!running.streamsimReset) emit({ type: 'error', text: error.message })
      })
      running.on('close', (code, signal) => {
        if (running.facetWorker) void running.facetWorker.terminate()
        if (running.streamsimReset) {
          runtimeScene?.cleanup()
          if (child === running) child = null
          if (!child) childExecutable = ''
          return
        }
        let finalCode = stepOutputError ? 1 : code
        if (code === 0 && radiosityJsonPath && !streamedSteps && !stepOutputError) {
          try {
            const tif = writeRadiosityTiff(radiosityJsonPath, input, data.mode)
            if (tif.tifPaths.length) {
              const timing = isEnergyBalanceMode(data.mode) ? ' 个逐时间 TIFF' : ' 个 TIFF'
              emit({ type: 'stdout', text: '已生成 ' + tif.tifPaths.length + timing + '（' + tif.width + ' × ' + tif.height + '，' + tif.bands + ' 波段）' })
            }
            if (tif.processPaths.length) emit({ type: 'stdout', text: '已保存 ' + tif.processPaths.length + ' 个时间节点索引' })
          } catch (error) {
            finalCode = 1
            emit({ type: 'error', text: `结果影像生成失败：${error.message}` })
          } finally {
            if (!keepThreeDimensionalResults && existsSync(radiosityJsonPath)) unlinkSync(radiosityJsonPath)
          }
        }
        if (streamedSteps && code === 0 && !keepThreeDimensionalResults && existsSync(radiosityJsonPath)) unlinkSync(radiosityJsonPath)
        emit({ type: 'closed', code: finalCode, signal, elapsed: Date.now() - startedAt })
        runtimeScene?.cleanup()
        if (child === running) { child = null; childExecutable = '' }
      })
      return json(response, 200, { pid: running.pid, inputPath: windowsPath(input) })
    } catch (error) {
      runtimeScene?.cleanup()
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/stop') {
    if (!child) return json(response, 200, { stopped: false })
    child.streamsimStopped = true
    if (child.facetWorker) void child.facetWorker.terminate()
    if (process.platform === 'win32') spawn('taskkill', ['/pid', String(child.pid), '/T', '/F'], { windowsHide: true })
    else child.kill('SIGTERM')
    return json(response, 200, { stopped: true })
  }
  if (request.method === 'POST' && url.pathname === '/api/reset') {
    try {
      return json(response, 200, await resetSimulationProcesses())
    } catch (error) {
      return json(response, 500, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/results/delete-all') {
    try {
      if (child) throw new Error('模拟运行中，不能删除结果')
      const data = await body(request)
      return json(response, 200, data.kind === "3d" ? deleteThreeDimensionalResults(data.path) : deleteTiffResults(data.path))
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/results/list') {
    try {
      const data = await body(request)
      return json(response, 200, listResults(data.path))
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/results/read') {
    try {
      const data = await body(request)
      const path = resolveAsset(data.path)
      if (!existsSync(path) || !statSync(path).isFile()) throw new Error(`找不到模拟结果：${path}`)
      const extension = extname(path).toLowerCase()
      if (extension === '.hdr') return json(response, 200, readEnvi(path, data.band))
      if (extension === '.tif' || extension === '.tiff') {
        const result = /^fluid_wind_/i.test(basename(path))
          ? readWindTiff(path)
          : readTiff(path, data.band)
        return json(response, 200, result)
      }
      if (extension === '.json' && isRadiosityResultFile(path)) {
        const result = sampleRadiosityResult(path)
        return json(response, 200, { ...result, path: windowsPath(result.path) })
      }
      if (extension === '.json' && processResultMetadata(path)) {
        const result = readProcessResult(path)
        return json(response, 200, { ...result, path: windowsPath(result.path) })
      }
      if (resultImageMime[extension]) {
        if (statSync(path).size > 32 * 1024 * 1024) throw new Error('图像结果超过 32MB，请在资源管理器中打开')
        return json(response, 200, { kind: 'image', path: windowsPath(path), mimeType: resultImageMime[extension], data: readFileSync(path).toString('base64') })
      }
      if (['.txt', '.csv', '.dat', '.log', '.json'].includes(extension)) {
        if (statSync(path).size > 4 * 1024 * 1024) throw new Error('文本结果超过 4MB，请在外部程序中打开')
        return json(response, 200, { kind: 'text', path: windowsPath(path), content: readFileSync(path, 'utf8') })
      }
      return json(response, 200, { kind: 'file', path: windowsPath(path) })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/results/change') {
    try {
      const data = await body(request)
      const firstPath = resolveAsset(data.firstPath)
      const secondPath = resolveAsset(data.secondPath)
      for (const path of [firstPath, secondPath]) {
        if (!existsSync(path) || !statSync(path).isFile()) throw new Error(`找不到模拟结果：${path}`)
        if (!['.tif', '.tiff'].includes(extname(path).toLowerCase())) throw new Error('变化分析目前仅支持 TIFF 图像')
      }
      return json(response, 200, compareTiffResults(firstPath, secondPath, data.band, data.maxScatterPoints))
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/open') {
    const data = await body(request)
    const path = resolve(normalizeHostPath(data.path || projectDir || process.env.STREAMSIM_DATA_ROOT || PROJECT_ROOT))
    if (!existsSync(path)) return json(response, 400, { error: `找不到路径：${path}` })
    if (process.platform === 'win32') spawn('explorer.exe', statSync(path).isFile() ? ['/select,', path] : [path], { detached: true, windowsHide: true })
    return json(response, 200, { ok: true, path: windowsPath(path) })
  }
  return false
}

const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url, 'http://127.0.0.1')
    if (url.pathname.startsWith('/api/')) {
      const handled = await api(request, response, url)
      if (handled !== false) return
      return json(response, 404, { error: 'API 不存在' })
    }
    if (apiOnly) return json(response, 404, { error: '开发模式请访问 http://127.0.0.1:5173' })
    let path = resolve(GUI_OUTPUT_DIR, `.${decodeURIComponent(url.pathname)}`)
    if (url.pathname === '/' || !existsSync(path)) path = join(GUI_OUTPUT_DIR, 'index.html')
    if (!path.startsWith(GUI_OUTPUT_DIR) || !existsSync(path)) return json(response, 404, { error: '文件不存在' })
    response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' })
    createReadStream(path).pipe(response)
  } catch (error) {
    json(response, 500, { error: error.message })
  }
})

const port = Number(process.env.STREAMSIM_PORT || 4173)
if (isMainThread) server.listen(port, '127.0.0.1', () => console.log(`STREAMSIM: http://127.0.0.1:${apiOnly ? 5173 : port}`))
else if (workerData?.kind === 'facet-step') {
  parentPort.postMessage(writeRadiosityTiff(workerData.jsonPath, workerData.inputPath, 'eFacetEB', workerData.step))
  parentPort.close()
}

export { server }

function shutdown() {
  if (child?.facetWorker) void child.facetWorker.terminate()
  if (child) child.kill('SIGTERM')
  server.close(() => process.exit(0))
}
process.on('SIGINT', shutdown)
process.on('SIGTERM', shutdown)
