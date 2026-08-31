import { createServer } from 'node:http'
import { spawn } from 'node:child_process'
import { createReadStream, existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs'
import { basename, dirname, extname, isAbsolute, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createDefaultProject, normalizeProject, PROJECT_MODES, projectToXml } from './src/renderer/src/project-schema.js'

const root = dirname(fileURLToPath(import.meta.url))
const dist = join(root, 'dist')
const apiOnly = process.argv.includes('--api-only')
const modes = new Set(PROJECT_MODES)
const histreamCandidates = process.platform === 'win32'
  ? ['C:\\work\\bin_x64\\Debug\\histream.exe', 'C:\\work\\bin_x64\\Release\\histream.exe']
  : ['/mnt/c/work/bin_x64/Debug/histream.exe', '/mnt/c/work/bin_x64/Release/histream.exe']
const radiosityCandidates = process.platform === 'win32'
  ? ['C:\\work\\radiosity\\cmake-build-gui\\radiosity_web_runner.exe']
  : ['/mnt/c/work/radiosity/cmake-build-gui/radiosity_web_runner.exe']
const radiosityShaderCandidates = process.platform === 'win32'
  ? ['C:\\work\\radiosity\\cmake-build-gui\\shader\\vulkanradiosity']
  : ['/mnt/c/work/radiosity/cmake-build-gui/shader/vulkanradiosity']
const mime = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.svg': 'image/svg+xml', '.png': 'image/png' }
let child = null
let projectFile = ''
let projectDir = ''
const clients = new Set()

function executable() {
  return histreamCandidates.find(existsSync) || histreamCandidates[0]
}

function radiosityExecutable() {
  return radiosityCandidates.find(existsSync) || radiosityCandidates[0]
}

function radiosityShaderDirectory() {
  return radiosityShaderCandidates.find(existsSync) || radiosityShaderCandidates[0]
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

function radiosityScene(inputPath) {
  const xml = readFileSync(inputPath, 'utf8')
  for (const match of xml.matchAll(/<fileName>([\s\S]*?)<\/fileName>/gi)) {
    const source = decodeXmlText(match[1].trim())
    const path = isAbsolute(source) ? resolve(source) : resolve(dirname(inputPath), source)
    if (existsSync(path) && statSync(path).isFile() && extname(path).toLowerCase() === '.obj') return path
  }
  const modelsDir = join(dirname(inputPath), 'models')
  if (existsSync(modelsDir) && statSync(modelsDir).isDirectory()) {
    const entry = readdirSync(modelsDir, { withFileTypes: true }).find((item) => item.isFile() && extname(item.name).toLowerCase() === '.obj')
    if (entry) return join(modelsDir, entry.name)
  }
  throw new Error('面元辐射传输需要先在当前工程中导入至少一个 OBJ')
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

  const width = Math.ceil(Math.sqrt(facetCount))
  const height = Math.ceil(facetCount / width)
  const pixelCount = width * height
  const image = Buffer.alloc(pixelCount * sources.length * 4)
  for (let index = 0; index < pixelCount * sources.length; index += 1) image.writeFloatLE(Number.NaN, index * 4)
  for (let band = 0; band < sources.length; band += 1) {
    const values = sources[band][1]
    for (let facet = 0; facet < facetCount; facet += 1) {
      const sides = [Number(values[facet * 2]), Number(values[facet * 2 + 1])].filter(Number.isFinite)
      image.writeFloatLE(sides.length ? Math.max(...sides) : Number.NaN, (band * pixelCount + facet) * 4)
    }
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
  return { headerPath, imagePath, width, height, bands: sources.length }
}

function projectJsonPath(inputPath) {
  return join(dirname(inputPath), 'project.json')
}

function runtimePaths(baseDir) {
  const sourceRoot = process.env.HISTREAM_ROOT || (process.platform === 'win32' ? 'C:\\work\\histream' : '/mnt/c/work/histream')
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
    .filter((entry) => extname(entry.name).toLowerCase() !== '.img' || !names.has(`${entry.name.slice(0, -4)}.hdr`))
    .map((entry) => {
      const path = join(directory, entry.name)
      const info = statSync(path)
      const extension = extname(entry.name).toLowerCase()
      return {
        name: entry.name,
        path,
        size: info.size,
        modifiedAt: info.mtime.toISOString(),
        kind: extension === '.hdr' ? 'envi' : ['.txt', '.csv', '.dat', '.log', '.json'].includes(extension) ? 'text' : 'file'
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
  return { kind: 'envi', path, width, height, bands, band, previewWidth, previewHeight, minimum, maximum, pixels: pixels.toString('base64') }
}

async function api(request, response, url) {
  if (request.method === 'GET' && url.pathname === '/api/defaults') {
    const path = executable()
    return json(response, 200, { executable: path, executableExists: existsSync(path), radiosityExecutable: radiosityExecutable(), radiosityExecutableExists: existsSync(radiosityExecutable()), projectFile, platform: process.platform, versions: { node: process.versions.node, three: '0.185.1' } })
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
      projectFile = resolveProjectFile(data.path)
      projectDir = dirname(projectFile)
      const stored = readProject(projectFile)
      return json(response, 200, { path: projectFile, projectDir, content: stored.content || readFileSync(projectFile, 'utf8'), project: stored.project, projectPath: stored.projectPath })
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
      let engine, args, radiosityJsonPath = ''
      if (data.mode === 'eFacetRT') {
        engine = radiosityExecutable()
        const shaderDirectory = radiosityShaderDirectory()
        if (!existsSync(engine)) throw new Error(`找不到 GPU Radiosity：${engine}`)
        if (!existsSync(shaderDirectory)) throw new Error(`找不到 GPU Radiosity 着色器：${shaderDirectory}`)
        const sceneObj = radiosityScene(input)
        const outputDirectory = join(projectDir, 'output')
        mkdirSync(outputDirectory, { recursive: true })
        radiosityJsonPath = join(outputDirectory, 'radiosity_gpu.json')
        args = [shaderDirectory, sceneObj, '--web', 'gpu', radiosityJsonPath]
      } else {
        engine = data.executable || executable()
        if (!existsSync(engine)) throw new Error(`找不到 HiStream：${engine}`)
        args = [data.mode, input]
      }
      const startedAt = Date.now()
      child = spawn(engine, args, { cwd: dirname(engine), windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] })
      const running = child
      emit({ type: 'started', pid: running.pid, command: `"${engine}" ${args.map((item) => `"${item}"`).join(' ')}` })
      running.stdout.on('data', (chunk) => emit({ type: 'stdout', text: chunk.toString() }))
      running.stderr.on('data', (chunk) => emit({ type: 'stderr', text: chunk.toString() }))
      running.on('error', (error) => emit({ type: 'error', text: error.message }))
      running.on('close', (code, signal) => {
        let finalCode = code
        if (code === 0 && radiosityJsonPath) {
          try {
            const output = writeRadiosityEnvi(radiosityJsonPath)
            emit({ type: 'stdout', text: `已生成三波段结果影像：${output.headerPath}\n` })
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

server.listen(4173, '127.0.0.1', () => console.log(`STREAMFIELD: http://127.0.0.1:${apiOnly ? 5173 : 4173}`))

function shutdown() {
  if (child) child.kill('SIGTERM')
  server.close(() => process.exit(0))
}
process.on('SIGINT', shutdown)
process.on('SIGTERM', shutdown)
