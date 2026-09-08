import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from 'node:fs'
import { dirname, extname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const sourceInput = join(repoRoot, 'validation', 'urban_tree_building', 'tests', 'physical_faceteb', 'Input.xml')
const lut = join(repoRoot, 'assets', 'atmosphere', 'simple_modtran_lut.csv')
const testRoot = join(repoRoot, '.test', 'atmosphere-facet-smoke')
const server = 'http://127.0.0.1:4173'

for (const path of [sourceInput, lut]) if (!existsSync(path)) throw new Error(`缺少测试文件：${path}`)

const api = async (path, options = {}) => {
  const response = await fetch(server + path, options)
  const result = await response.json()
  if (!response.ok) throw new Error(result.error || `${path} 请求失败`)
  return result
}

const waitForRun = async (mode, inputPath) => {
  const controller = new AbortController()
  const response = await fetch(server + '/api/events', { signal: controller.signal })
  if (!response.ok || !response.body) throw new Error('无法连接模拟事件流')
  const reader = response.body.getReader()
  let timeoutId
  const completed = (async () => {
    const decoder = new TextDecoder()
    let pending = ''
    while (true) {
      const { value, done } = await reader.read()
      if (done) throw new Error('模拟事件流提前关闭')
      pending += decoder.decode(value, { stream: true })
      const blocks = pending.split('\n\n')
      pending = blocks.pop() || ''
      for (const block of blocks) {
        const line = block.split('\n').find((item) => item.startsWith('data: '))
        if (!line) continue
        const event = JSON.parse(line.slice(6))
        if (event.type === 'error') process.stderr.write(`${event.text}\n`)
        if (event.type === 'closed') return event
      }
    }
  })()
  try {
    await api('/api/run', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mode, inputPath })
    })
    const timeout = new Promise((_, reject) => {
      timeoutId = setTimeout(() => reject(new Error(`${mode} 测试超时`)), 180000)
    })
    const closed = await Promise.race([completed, timeout])
    if (closed.code !== 0) throw new Error(`${mode} 返回代码 ${closed.code}`)
  } finally {
    clearTimeout(timeoutId)
    controller.abort()
  }
}

const tiffFiles = (directory) => readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
  const path = join(directory, entry.name)
  if (entry.isDirectory()) return tiffFiles(path)
  return ['.tif', '.tiff'].includes(extname(entry.name).toLowerCase()) ? [path] : []
}).sort()

const run = async (mode, enabled) => {
  const label = enabled ? 'enabled' : 'disabled'
  const caseRoot = join(testRoot, mode, label)
  const output = join(caseRoot, 'output')
  const input = join(caseRoot, 'Input.xml')
  rmSync(caseRoot, { recursive: true, force: true })
  mkdirSync(output, { recursive: true })
  let xml = readFileSync(sourceInput, 'utf8')
  xml = xml.replace(/mode="[^"]*"/, `mode="${mode}"`)
  xml = xml.replace(/<outDir>[\s\S]*?<\/outDir>/, `<outDir>${output}</outDir>`)
  xml = xml.replace(/<pixelResolutionX>[^<]*<\/pixelResolutionX>/, '<pixelResolutionX>24</pixelResolutionX>')
  xml = xml.replace(/<pixelResolutionY>[^<]*<\/pixelResolutionY>/, '<pixelResolutionY>18</pixelResolutionY>')
  xml = xml.replace(/<viewAngles>[^<]*<\/viewAngles>/, '<viewAngles>15,0</viewAngles>')
  if (!/<sensorPosition>/i.test(xml)) xml = xml.replace('<viewAngle>', '<sensorPosition>25,20,2000</sensorPosition>\n        <viewAngle>')
  xml = xml.replace(/<Atmosphere>[\s\S]*?<\/Atmosphere>\s*/i, '')
  xml = xml.replace('<Scene>', `<Atmosphere><enabled>${enabled ? 1 : 0}</enabled><model>midlatitude-summer</model><waterVapor>2.8</waterVapor><aerosol>urban</aerosol><visibility>31</visibility><lutFile>${lut}</lutFile></Atmosphere>\n  <Scene>`)
  writeFileSync(input, xml, 'utf8')
  await waitForRun(mode, input)
  const outputs = tiffFiles(output)
  if (!outputs.length) throw new Error(`${mode} 没有生成 TIFF`)
  const newest = outputs.sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs)[0]
  return newest
}

const results = []
for (const mode of ['eFacetRT', 'eFacetEB']) {
  const disabled = await run(mode, false)
  const enabled = await run(mode, true)
  if (!enabled.toLowerCase().endsWith('_a.tif')) throw new Error(`${mode} 有大气结果缺少 _a.tif 后缀`)
  if (disabled.toLowerCase().endsWith('_a.tif')) throw new Error(`${mode} 无大气结果错误使用 _a.tif 后缀`)
  if (readFileSync(disabled).equals(readFileSync(enabled))) throw new Error(`${mode} 启用大气查找表后 TIFF 没有发生变化`)
  results.push({ mode, model: 'midlatitude-summer', waterVapor: 2.8, aerosol: 'urban', visibility: 31, disabled, enabled, outputChanged: true })
}

console.log(JSON.stringify(results, null, 2))
