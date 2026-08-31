import { createServer } from 'node:http'
import { execFile, spawn } from 'node:child_process'
import { copyFileSync, createReadStream, existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs'
import { inflateSync } from 'node:zlib'
import { basename, dirname, extname, isAbsolute, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createDefaultProject, DEFAULT_SCENE_MODEL, normalizeProject, PROJECT_MODES, projectToXml } from './src/renderer/src/project-schema.js'

const root = dirname(fileURLToPath(import.meta.url))
const dist = join(root, 'dist')
const apiOnly = process.argv.includes('--api-only')
const modes = new Set(PROJECT_MODES)
const modelRoot = join(root, 'models')
const defaultSceneAsset = join(root, 'assets', 'obj-library', 'house_a.obj')
const histreamCandidates = [
  join(modelRoot, 'histream', 'bin', 'Debug', 'histream.exe'),
  join(modelRoot, 'histream', 'bin', 'Release', 'histream.exe')
]
const mime = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.svg': 'image/svg+xml', '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.gif': 'image/gif', '.bmp': 'image/bmp' }
const resultImageMime = { '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.gif': 'image/gif', '.bmp': 'image/bmp' }
let child = null
let projectFile = ''
let projectDir = ''
const clients = new Set()

function executable() {
  return histreamCandidates.find(existsSync) || histreamCandidates[0]
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

function resolveProjectFile(value) {
  let path = String(value || '').trim().replace(/^"|"$/g, '')
  if (!path) throw new Error('请输入已有工程目录、project.json 或 Input.xml 完整路径')
  path = resolve(path)
  if (!existsSync(path)) throw new Error(`找不到已有工程：${path}`)

  if (statSync(path).isDirectory()) {
    const manifest = join(path, 'project.json')
    path = existsSync(manifest) ? manifest : join(path, 'Input.xml')
    if (!existsSync(path)) throw new Error(`工程目录中找不到 project.json 或 Input.xml：${dirname(path)}`)
  }

  if (!statSync(path).isFile()) throw new Error(`工程入口不是文件：${path}`)
  if (extname(path).toLowerCase() === '.json') {
    if (basename(path).toLowerCase() !== 'project.json') throw new Error('工程描述文件必须名为 project.json')
    let project
    try {
      project = JSON.parse(readFileSync(path, 'utf8'))
    } catch (error) {
      throw new Error(`project.json 格式错误：${error.message}`)
    }
    const inputFile = String(project.inputFile || 'Input.xml').trim()
    if (!inputFile) throw new Error('project.json 中的 inputFile 为空')
    const inputPath = isAbsolute(inputFile) ? resolve(inputFile) : resolve(dirname(path), inputFile)
    if (!existsSync(inputPath) || !statSync(inputPath).isFile()) throw new Error(`找不到 project.json 指定的配置：${inputPath}`)
    if (extname(inputPath).toLowerCase() !== '.xml') throw new Error('project.json 的 inputFile 必须指向 XML 文件')
    return inputPath
  }

  if (extname(path).toLowerCase() !== '.xml') throw new Error('工程入口必须是 project.json 或 XML 文件')
  return path
}

function decodeXmlText(value) {
  return String(value || '')
    .replaceAll('&amp;', '&')
    .replaceAll('&lt;', '<')
    .replaceAll('&gt;', '>')
    .replaceAll('&quot;', '"')
    .replaceAll('&apos;', "'")
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

function readRadiosityResult(jsonPath) {
  if (!existsSync(jsonPath)) throw new Error(`找不到面元结果：${jsonPath}`)
  let result
  try { result = JSON.parse(readFileSync(jsonPath, 'utf8')) } catch (error) { throw new Error(`面元结果格式错误：${error.message}`) }
  const facetCount = Number(result.facetCount)
  if (!Number.isInteger(facetCount) || facetCount < 1) throw new Error('面元结果缺少有效的 facetCount')
  const metrics = [
    ['radiosity', 'Radiosity', result.radiosity],
    ['lightEnhancement', 'Light enhancement', result.lightEnhancement],
    ['sunlit', 'Sunlit fraction', result.sunlit]
  ]
  for (const [, name, values] of metrics) if (!Array.isArray(values) || values.length < facetCount * 2) throw new Error(`${name} 数据不完整`)
  return { facetCount, metrics, vertexPositions: Array.isArray(result.vertexPositions) ? result.vertexPositions : [], backend: result.backend, iterations: result.iterations, maxDelta: result.maxDelta }
}

function isRadiosityResultFile(path) {
  try { readRadiosityResult(path); return true } catch { return false }
}

function writeRadiosityTiff(jsonPath) {
  const { facetCount, metrics } = readRadiosityResult(jsonPath)
  const faceValueCount = facetCount * 2
  const width = Math.ceil(Math.sqrt(faceValueCount))
  const height = Math.ceil(faceValueCount / width)
  const entries = 12
  const ifdOffset = 8
  const bitsOffset = ifdOffset + 2 + entries * 12 + 4
  const sampleFormatOffset = bitsOffset + 6
  const description = Buffer.from('HiStream facet metrics: Radiosity, Light enhancement, Sunlit fraction\0', 'ascii')
  const descriptionOffset = sampleFormatOffset + 6
  const imageOffset = descriptionOffset + description.length
  const pixelCount = width * height
  const image = Buffer.alloc(pixelCount * metrics.length * 4)
  for (let index = 0; index < pixelCount * metrics.length; index += 1) image.writeFloatLE(Number.NaN, index * 4)
  for (let face = 0; face < faceValueCount; face += 1) for (let band = 0; band < metrics.length; band += 1) image.writeFloatLE(Number(metrics[band][2][face]), (face * metrics.length + band) * 4)
  const header = Buffer.alloc(imageOffset)
  header.write('II', 0, 'ascii'); header.writeUInt16LE(42, 2); header.writeUInt32LE(ifdOffset, 4)
  header.writeUInt16LE(entries, ifdOffset)
  let entry = ifdOffset + 2
  const writeEntry = (tag, type, count, value) => {
    header.writeUInt16LE(tag, entry); header.writeUInt16LE(type, entry + 2); header.writeUInt32LE(count, entry + 4)
    if (type === 3 && count === 1) header.writeUInt16LE(value, entry + 8)
    else header.writeUInt32LE(value, entry + 8)
    entry += 12
  }
  writeEntry(256, 4, 1, width)
  writeEntry(257, 4, 1, height)
  writeEntry(270, 2, description.length, descriptionOffset)
  writeEntry(258, 3, 3, bitsOffset)
  writeEntry(259, 3, 1, 1)
  writeEntry(262, 3, 1, 2)
  writeEntry(273, 4, 1, imageOffset)
  writeEntry(277, 3, 1, metrics.length)
  writeEntry(278, 4, 1, height)
  writeEntry(279, 4, 1, image.length)
  writeEntry(284, 3, 1, 1)
  writeEntry(339, 3, 3, sampleFormatOffset)
  header.writeUInt16LE(32, bitsOffset); header.writeUInt16LE(32, bitsOffset + 2); header.writeUInt16LE(32, bitsOffset + 4)
  header.writeUInt16LE(3, sampleFormatOffset); header.writeUInt16LE(3, sampleFormatOffset + 2); header.writeUInt16LE(3, sampleFormatOffset + 4)
  description.copy(header, descriptionOffset)
  const tifPath = jsonPath.replace(/\.json$/i, '.tif')
  writeFileSync(tifPath, Buffer.concat([header, image]))
  return { tifPath, width, height, bands: metrics.length, faceValueCount }
}

function projectJsonPath(inputPath) {
  return join(dirname(inputPath), 'project.json')
}

function runtimePaths(baseDir) {
  const sourceRoot = process.env.HISTREAM_ROOT || join(modelRoot, 'histream')
  return {
    outputDir: join(baseDir, 'output'),
    definedDir: join(sourceRoot, 'defined'),
    meteoPath: join(sourceRoot, 'defined', 'meteo.txt')
  }
}

function writeProject(path, value) {
  const project = normalizeProject(value)
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
  if (!project || !ensureDefaultSceneAsset(project, dirname(inputPath))) return project
  const saved = writeProject(projectJsonPath(inputPath), project)
  writeFileSync(inputPath, projectToXml(saved, runtimePaths(dirname(inputPath))), 'utf8')
  return saved
}

function openProject(value) {
  projectFile = resolveProjectFile(value)
  projectDir = dirname(projectFile)
  const stored = readProject(projectFile)
  const migrated = migrateProject(projectFile, stored.project)
  return {
    path: projectFile,
    projectDir,
    content: migrated ? readFileSync(projectFile, 'utf8') : (stored.content || readFileSync(projectFile, 'utf8')),
    project: migrated || stored.project,
    projectPath: stored.projectPath
  }
}

function chooseProjectFile() {
  if (process.platform !== 'win32') throw new Error('工程文件选择器仅支持 Windows')
  return new Promise((resolveChoice, reject) => {
    const script = [
      "$ErrorActionPreference = 'Stop'",
      "Add-Type -AssemblyName System.Windows.Forms",
      "$dialog = New-Object System.Windows.Forms.OpenFileDialog",
      "$dialog.Title = '打开 StreamSim 工程'",
      "$dialog.Filter = 'HiStream 工程 (*.json;*.xml)|project.json;Input.xml|project.json|project.json|Input.xml|Input.xml|所有文件 (*.*)|*.*'",
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
  const directory = String(data.directory || '').trim().replace(/^"|"$/g, '')
  const name = String(data.name || '').trim()
  if (!directory) throw new Error('请输入工程保存目录')
  if (!isAbsolute(directory)) throw new Error('工程保存目录必须是绝对路径')
  if (!name || name === '.' || name === '..' || /[<>:"/\\|?*\x00-\x1f]/.test(name)) throw new Error('工程名称包含无效字符')
  if (!modes.has(data.mode)) throw new Error('不支持的计算模式')

  const target = join(resolve(directory), name)
  if (existsSync(target) && readdirSync(target).length) throw new Error(`工程目录已存在且不为空：${target}`)
  mkdirSync(target, { recursive: true })
  for (const folder of ['models', 'positions', 'spectral', 'meteorology', 'output']) mkdirSync(join(target, folder), { recursive: true })

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
  const inputPath = join(target, 'Input.xml')
  const saved = writeProject(jsonPath, project)
  const content = projectToXml(saved, runtimePaths(target))
  writeFileSync(inputPath, content, 'utf8')
  return { path: inputPath, projectDir: target, projectPath: jsonPath, project: saved, content }
}

function resolveAsset(value) {
  const source = String(value || '').trim().replace(/^"|"$/g, '')
  if (!source) throw new Error('资源路径为空')
  return isAbsolute(source) ? resolve(source) : resolve(projectDir, source)
}

function listResults(value) {
  const directory = resolveAsset(value)
  if (!existsSync(directory) || !statSync(directory).isDirectory()) throw new Error(`找不到模拟结果目录：${directory}`)
  const names = new Set(readdirSync(directory))
  const files = readdirSync(directory, { withFileTypes: true })
    .filter((entry) => entry.isFile())
    .filter((entry) => !['.hdr', '.img'].includes(extname(entry.name).toLowerCase()))
    .map((entry) => {
      const path = join(directory, entry.name)
      const info = statSync(path)
      const extension = extname(entry.name).toLowerCase()
      return {
        name: entry.name,
        path,
        size: info.size,
        modifiedAt: info.mtime.toISOString(),
        kind: extension === '.hdr' ? 'envi' : ['.tif', '.tiff'].includes(extension) ? 'tiff' : extension === '.json' && isRadiosityResultFile(path) ? 'facet' : resultImageMime[extension] ? 'image' : ['.txt', '.csv', '.dat', '.log', '.json'].includes(extension) ? 'text' : 'file'
      }
    })
    .sort((a, b) => b.modifiedAt.localeCompare(a.modifiedAt))
  return { directory, files }
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

function readTiff(path, requestedBand = 0) {
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
  let minimum = Infinity, maximum = -Infinity
  const valueAt = (x, y) => {
    const [sourceX, sourceY] = sourceCoordinates(x, y)
    return sourceValueAt(sourceX, sourceY, band)
  }
  for (let y = 0; y < outputHeight; y += 1) for (let x = 0; x < outputWidth; x += 1) {
    const value = valueAt(x, y)
    if (Number.isFinite(value)) { minimum = Math.min(minimum, value); maximum = Math.max(maximum, value) }
  }
  if (!Number.isFinite(minimum)) { minimum = 0; maximum = 0 }
  const scale = Math.min(1, 768 / Math.max(outputWidth, outputHeight))
  const previewWidth = Math.max(1, Math.round(outputWidth * scale))
  const previewHeight = Math.max(1, Math.round(outputHeight * scale))
  const pixels = Buffer.alloc(previewWidth * previewHeight)
  const range = maximum - minimum
  for (let y = 0; y < previewHeight; y += 1) for (let x = 0; x < previewWidth; x += 1) {
    const value = valueAt(Math.min(outputWidth - 1, Math.floor(x / scale)), Math.min(outputHeight - 1, Math.floor(y / scale)))
    const normalized = Number.isFinite(value) ? (range ? (value - minimum) / range : .5) : 0
    pixels[y * previewWidth + x] = Math.max(0, Math.min(255, Math.round((photometric === 0 ? 1 - normalized : normalized) * 255)))
  }
  const description = typeof tagValue(270, '') === 'string' ? tagValue(270, '') : ''
  const describedBands = description.match(/(?:band names|metrics)\s*[:=]\s*\{?([^}\r\n]+)\}?/i)?.[1]
  const bandNames = describedBands ? describedBands.split(',').map((value) => value.trim()).filter(Boolean) : samples === 3 && photometric === 2 ? ['红色', '绿色', '蓝色'] : Array.from({ length: samples }, (_, index) => `波段 ${index + 1}`)
  return { kind: 'tiff', path, width: outputWidth, height: outputHeight, bands: samples, band, bandNames, previewWidth, previewHeight, minimum, maximum, pixels: pixels.toString('base64') }
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
  const range = maximum - minimum
  for (let y = 0; y < previewHeight; y += 1) for (let x = 0; x < previewWidth; x += 1) {
    const value = valueAt(Math.min(width - 1, Math.floor(x / scale)), Math.min(height - 1, Math.floor(y / scale)))
    pixels[y * previewWidth + x] = Number.isFinite(value) ? Math.round((range ? (value - minimum) / range : 0.5) * 255) : 0
  }
  return { kind: 'envi', path, width, height, bands, band, bandNames, previewWidth, previewHeight, minimum, maximum, pixels: pixels.toString('base64') }
}

async function api(request, response, url) {
  if (request.method === 'GET' && url.pathname === '/api/defaults') {
    const path = executable()
    return json(response, 200, { executable: path, executableExists: existsSync(path), radiosityExecutable: path, radiosityExecutableExists: existsSync(path), projectFile, platform: process.platform, versions: { node: process.versions.node, three: '0.185.1' } })
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
      return json(response, 201, result)
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
      return json(response, 201, { path, name, positionPath, relativePath: join('models', name) })
    } catch (error) { return json(response, 400, { error: error.message }) }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/distribution') {
    try {
      const data = await body(request)
      if (!projectDir || !projectFile) throw new Error('请先新建或打开工程')
      if (!Array.isArray(data.instances) || !data.instances.length) throw new Error('实例分布不能为空')
      if (data.instances.length > 10000) throw new Error('单个 OBJ 最多支持 10000 个实例')
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
      return json(response, 200, { path, count: lines.length })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/read') {
    try {
      const data = await body(request)
      const path = resolveAsset(data.path)
      if (!existsSync(path) || !statSync(path).isFile()) throw new Error(`找不到场景资源：${path}`)
      if (statSync(path).size > 80 * 1024 * 1024) throw new Error('预览资源超过 80MB，请使用简化模型')
      return json(response, 200, { path, content: readFileSync(path, 'utf8') })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/project/save') {
    try {
      const data = await body(request)
      const path = resolveProjectFile(data.path || projectFile)
      let content = String(data.content || '')
      let project = null
      let savedProjectPath = ''
      if (data.project) {
        savedProjectPath = projectJsonPath(path)
        project = writeProject(savedProjectPath, data.project)
        ensureDefaultSceneAsset(project, dirname(path))
        project = writeProject(savedProjectPath, project)
        content = projectToXml(project, runtimePaths(dirname(path)))
      }
      writeFileSync(path, content, 'utf8')
      projectFile = path
      projectDir = dirname(path)
      return json(response, 200, { path, content, project, projectPath: savedProjectPath })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/run') {
    try {
      const data = await body(request)
      if (child) throw new Error('已有模拟任务正在运行')
      if (!modes.has(data.mode)) throw new Error('不支持的计算模式')
      const input = resolveProjectFile(data.inputPath || projectFile)
      projectFile = input
      projectDir = dirname(input)
      const stored = readProject(input)
      if (stored.project) migrateProject(input, stored.project)
      let engine, args, radiosityJsonPath = ''
      engine = data.executable || executable()
      if (!existsSync(engine)) throw new Error(`找不到 HiStream：${engine}`)
      const facetMode = data.mode === 'eFacetRT' || data.mode === 'eFacetEB'
      if (facetMode) {
        const outputDirectory = join(projectDir, 'output')
        mkdirSync(outputDirectory, { recursive: true })
        radiosityJsonPath = join(outputDirectory,
          data.mode === 'eFacetRT' ? 'radiosity_gpu.json' : 'faceteb.json')
        args = [data.mode, input, radiosityJsonPath]
      } else {
        args = [data.mode, input]
      }
      const startedAt = Date.now()
      const workingDirectory = facetMode ? dirname(engine) : projectDir
      child = spawn(engine, args, { cwd: workingDirectory, windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] })
      const running = child
      emit({ type: 'started', pid: running.pid, command: `"${engine}" ${args.map((item) => `"${item}"`).join(' ')}` })
      running.stdout.on('data', (chunk) => emit({ type: 'stdout', text: chunk.toString() }))
      running.stderr.on('data', (chunk) => emit({ type: 'stderr', text: chunk.toString() }))
      running.on('error', (error) => emit({ type: 'error', text: error.message }))
      running.on('close', (code, signal) => {
        let finalCode = code
        if (code === 0 && radiosityJsonPath) {
          try {
            const tif = writeRadiosityTiff(radiosityJsonPath)
            emit({ type: 'stdout', text: `已生成三波段 TIFF 面元结果：${tif.tifPath}` })
          } catch (error) {
            finalCode = 1
            emit({ type: 'error', text: `结果影像生成失败：${error.message}` })
          }
        }
        emit({ type: 'closed', code: finalCode, signal, elapsed: Date.now() - startedAt })
        if (child === running) child = null
      })
      return json(response, 200, { pid: running.pid, inputPath: input })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/stop') {
    if (!child) return json(response, 200, { stopped: false })
    if (process.platform === 'win32') spawn('taskkill', ['/pid', String(child.pid), '/T', '/F'], { windowsHide: true })
    else child.kill('SIGTERM')
    return json(response, 200, { stopped: true })
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
      if (extension === '.tif' || extension === '.tiff') return json(response, 200, readTiff(path, data.band))
      if (extension === '.json' && isRadiosityResultFile(path)) {
        const result = readRadiosityResult(path)
        return json(response, 200, { kind: 'facet', path, facetCount: result.facetCount, vertexPositions: result.vertexPositions, metrics: result.metrics.map(([id, label, values]) => ({ id, label, values })), backend: result.backend, iterations: result.iterations, maxDelta: result.maxDelta })
      }
      if (resultImageMime[extension]) {
        if (statSync(path).size > 32 * 1024 * 1024) throw new Error('图像结果超过 32MB，请在资源管理器中打开')
        return json(response, 200, { kind: 'image', path, mimeType: resultImageMime[extension], data: readFileSync(path).toString('base64') })
      }
      if (['.txt', '.csv', '.dat', '.log', '.json'].includes(extension)) {
        if (statSync(path).size > 4 * 1024 * 1024) throw new Error('文本结果超过 4MB，请在外部程序中打开')
        return json(response, 200, { kind: 'text', path, content: readFileSync(path, 'utf8') })
      }
      return json(response, 200, { kind: 'file', path })
    } catch (error) {
      return json(response, 400, { error: error.message })
    }
  }
  if (request.method === 'POST' && url.pathname === '/api/open') {
    const data = await body(request)
    const path = resolve(String(data.path || projectDir || root))
    if (!existsSync(path)) return json(response, 400, { error: `找不到路径：${path}` })
    if (process.platform === 'win32') spawn('explorer.exe', statSync(path).isFile() ? ['/select,', path] : [path], { detached: true, windowsHide: true })
    return json(response, 200, { ok: true, path })
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
    let path = resolve(dist, `.${decodeURIComponent(url.pathname)}`)
    if (url.pathname === '/' || !existsSync(path)) path = join(dist, 'index.html')
    if (!path.startsWith(dist) || !existsSync(path)) return json(response, 404, { error: '文件不存在' })
    response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' })
    createReadStream(path).pipe(response)
  } catch (error) {
    json(response, 500, { error: error.message })
  }
})

server.listen(4173, '127.0.0.1', () => console.log(`STREAMSIM: http://127.0.0.1:${apiOnly ? 5173 : 4173}`))

function shutdown() {
  if (child) child.kill('SIGTERM')
  server.close(() => process.exit(0))
}
process.on('SIGINT', shutdown)
process.on('SIGTERM', shutdown)
