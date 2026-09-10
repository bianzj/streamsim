import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawn } from 'node:child_process'
import { createInterface } from 'node:readline'
import { Worker } from 'node:worker_threads'

const root = fileURLToPath(new URL('../', import.meta.url))
const example = path.join(root, 'assets/examples/photovoltaic_maize')
const base = JSON.parse(fs.readFileSync(path.join(example, 'project.json'), 'utf8'))
const output = fs.mkdtempSync(path.join(root, 'tmp/facet-step-stream-'))
// GPU reduction order can differ by a few ulps between runs. Avoid assert's
// enormous per-byte diff for full binary buffers on failure.
function compareFloats(a, b, label, tolerance = 0.5) {
  assert.equal(a.length, b.length, label + ' length')
  let maximum = 0
  for (let i = 0; i < a.length; i++) maximum = Math.max(maximum, Math.abs(a[i] - b[i]))
  assert.ok(Number.isFinite(maximum) && maximum <= tolerance, `${label}: max delta ${maximum}`)
}
function binaryFloats(buffer) {
  return new Float32Array(buffer.buffer, buffer.byteOffset, buffer.length / 4)
}
for (const object of base.configuration.objects.items) {
  for (const key of ['fileName', 'positionFile']) object[key] = path.join(example, object[key])
}
// A small single panel keeps this lifecycle regression independent of the
// much larger crop example and its GPU raster reduction variability.
const positions = path.join(output, 'positions.txt')
fs.writeFileSync(positions, '2 2 0 1 0\n')
const panel = base.configuration.objects.items[0]
Object.assign(panel, { fileName: path.join(root, 'assets/obj-library/solar/solar_panel_single_tilted.obj'),
  positionFile: positions, spectralName: 'pv_flat', thermalName: 'vegetation_temperature', canopyName: 'rigid_body', meshes: [] })
base.configuration.scene.voxel = 1
Object.assign(base.configuration.meteo, { path: path.join(example, 'meteo.txt'), start: 24, end: 26 })
Object.assign(base.configuration.sensor, { x: 8, y: 8, image: true, temperature: false,
  bands: '560,10500', continuousBands: false, principalPlane: true, hemisphere: false,
  process: true, energyProcess: true, radiationProcess: true })

function exportStep(jsonPath, inputPath, step) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(new URL('../server.mjs', import.meta.url), {
      workerData: { kind: 'facet-step', jsonPath, inputPath, step }
    })
    let result
    worker.on('message', message => { result = message })
    worker.on('error', reject)
    worker.on('exit', code => code === 0 && result ? resolve(result) : reject(new Error('worker exit ' + code)))
  })
}

async function run(name, streaming, abort = false, sensorOverrides = {}) {
  const directory = path.join(output, name)
  fs.mkdirSync(directory)
  const project = structuredClone(base)
  Object.assign(project.configuration.sensor, sensorOverrides)
  project.configuration.outDir = directory
  const input = path.join(directory, 'project.json')
  const resultPath = path.join(directory, 'faceteb.json')
  fs.writeFileSync(input, JSON.stringify(project))
  const exe = path.join(root, 'models/bin_x64/Release/histream.exe')
  const child = spawn(exe, ['eFacetEB', input, resultPath], { cwd: path.dirname(exe), windowsHide: true,
    env: { ...process.env, STREAMSIM_FACET_STEP_SYNC: streaming ? '1' : '0' }, stdio: ['pipe', 'pipe', 'pipe'] })
  let log = '', failure, count = 0, pending = false
  const timeout = setTimeout(() => { failure = new Error('engine timeout'); child.kill() }, 180000)
  child.stderr.on('data', chunk => { log += chunk })
  child.stdout.on('data', chunk => { log += chunk })
  child.stdin.on('error', () => {})
  createInterface({ input: child.stdout }).on('line', async line => {
    if (!line.startsWith('FACET_STEP\t')) return
    try {
      assert.equal(pending, false)
      pending = true
      const [, node, token, julianTime] = line.split('\t')
      const step = { node: Number(node), token, julianTime: Number(julianTime) }
      const stepPath = path.join(directory, '.facet_steps', 'energy_T=' + token + '.bin')
      assert.ok(fs.existsSync(stepPath))
      assert.equal(fs.readdirSync(path.dirname(stepPath)).filter(f => f.endsWith('.bin')).length, 1)
      // Without ACK the next node cannot start, even when this consumer pauses.
      await new Promise(resolve => setTimeout(resolve, 100))
      assert.equal((log.match(/FACET_STEP\t/g) || []).length, count + 1)
      if (abort) { child.stdin.end(); return }
      compareFloats(binaryFloats(fs.readFileSync(stepPath)), binaryFloats(fs.readFileSync(path.join(output, 'baseline', '.facet_steps', path.basename(stepPath)))), 'node binary')
      const result = await exportStep(resultPath, input, step)
      if (project.configuration.sensor.image) assert.ok(result.tifPaths.length > 1, 'all requested directions output now')
      else assert.equal(result.tifPaths.length, 0)
      assert.equal(fs.existsSync(stepPath), false)
      for (const file of result.tifPaths) assert.ok(fs.statSync(file).size > 0)
      assert.equal(child.exitCode, null, 'TIFFs exist while engine is waiting')
      count++
      pending = false
      child.stdin.write('FACET_ACK\t' + node + '\n')
    } catch (error) { failure = error; child.stdin.end('FACET_ERROR\n') }
  })
  const code = await new Promise((resolve, reject) => { child.on('error', reject); child.on('close', resolve) })
  clearTimeout(timeout)
  fs.writeFileSync(path.join(directory, 'run.log'), log)
  if (failure) throw failure
  if (abort) {
    assert.notEqual(code, 0)
    assert.ok(fs.readdirSync(path.join(directory, '.facet_steps')).some(f => f.endsWith('.bin')))
    return
  }
  assert.equal(code, 0, log)
  if (streaming) assert.equal(count, 2)
  return { directory, input, resultPath, result: JSON.parse(fs.readFileSync(resultPath, 'utf8')) }
}

const baseline = await run('baseline', false)
const streamed = await run('streamed', true)
await run('images-disabled', true, false, { image: false, process: false, energyProcess: false, radiationProcess: false })
for (const key of ['temperature', 'radiosity', 'netRadiation', 'sensibleHeat', 'latentHeat', 'storageHeat', 'photovoltaicPower']) {
  compareFloats(streamed.result[key], baseline.result[key], key, key === 'temperature' ? 0.05 : 0.5)
}
const summaryNumbers = directory => fs.readFileSync(path.join(directory, 'photovoltaic_summary.csv'), 'utf8')
  .trim().split(/\r?\n/).slice(1).flatMap(row => row.split(',').filter((_, i) => i !== 1).map(Number))
compareFloats(summaryNumbers(streamed.directory), summaryNumbers(baseline.directory), 'PV cumulative summary', 0.5)
const rows = fs.readFileSync(path.join(baseline.directory, 'photovoltaic_summary.csv'), 'utf8').trim().split(/\r?\n/).slice(1)
for (const row of rows) {
  const [node, token] = row.split(',')
  const result = await exportStep(baseline.resultPath, baseline.input, { node: Number(node), token, julianTime: 214.5 })
  // TIFF binary writer appends a planar float32 payload after its metadata.
  for (const file of result.tifPaths) {
    const a = fs.readFileSync(path.join(streamed.directory, path.basename(file))), b = fs.readFileSync(file)
    assert.equal(a.length, b.length)
    const bytes = 8 * 8 * 2 * 4
    assert.ok(a.subarray(0, -bytes).equals(b.subarray(0, -bytes)), 'TIFF metadata')
    const values = buffer => Array.from({ length: bytes / 4 }, (_, i) => buffer.readFloatLE(buffer.length - bytes + i * 4))
    const av = values(a), bv = values(b)
    assert.deepEqual(av.map(Number.isNaN), bv.map(Number.isNaN))
    compareFloats(av.map(v => Number.isNaN(v) ? 0 : v), bv.map(v => Number.isNaN(v) ? 0 : v), 'TIFF pixels', 0.01)
  }
}
await run('consumer-disconnected', true, true)
// Exercise the actual HTTP/SSE orchestration on an isolated ephemeral port.
const apiProjectDir = path.join(output, 'api')
fs.mkdirSync(apiProjectDir)
const apiProject = structuredClone(base)
apiProject.configuration.outDir = 'output'
const apiInput = path.join(apiProjectDir, 'project.json')
fs.writeFileSync(apiInput, JSON.stringify(apiProject))
const service = spawn(process.execPath, ['--input-type=module', '-e',
  `const {server}=await import(${JSON.stringify(new URL('../server.mjs', import.meta.url).href)}); if(!server.listening) await new Promise(r=>server.once('listening',r)); console.log('TEST_PORT:'+server.address().port)`],
  { cwd: root, windowsHide: true, env: { ...process.env, STREAMSIM_PORT: '0' }, stdio: ['ignore', 'pipe', 'pipe'] })
let serviceErrors = ''
service.stderr.on('data', c => { serviceErrors += c })
const controller = new AbortController()
try {
  const port = await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('test server startup timeout')), 10000)
    service.on('error', reject)
    createInterface({ input: service.stdout }).on('line', line => {
      if (line.startsWith('TEST_PORT:')) { clearTimeout(timer); resolve(Number(line.slice(10))) }
    })
  })
  const url = 'http://127.0.0.1:' + port
  const events = []
  const response = await fetch(url + '/api/events', { signal: controller.signal })
  const closed = (async () => {
    let pending = ''
    for await (const text of response.body.pipeThrough(new TextDecoderStream())) {
      pending += text
      let end
      while ((end = pending.indexOf('\n\n')) >= 0) {
        const message = pending.slice(0, end); pending = pending.slice(end + 2)
        if (!message.startsWith('data: ')) continue
        const event = JSON.parse(message.slice(6)); events.push(event)
        if (event.type === 'closed') return event
      }
    }
    throw new Error('event stream ended without completion')
  })()
  const result = await fetch(url + '/api/run', { method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mode: 'eFacetEB', inputPath: apiInput }) })
  assert.equal(result.status, 200, JSON.stringify(await result.json()))
  const timer = setTimeout(() => controller.abort(), 120000)
  const completion = await closed
  clearTimeout(timer)
  assert.equal(completion.code, 0, events.map(e => e.text || '').join('\n'))
  assert.equal(events.filter(e => /输出缓存已释放/.test(e.text || '')).length, 2)
  assert.equal(events.some(e => /结果影像生成失败|节点影像生成失败/.test(e.text || '')), false)
  assert.equal(fs.readdirSync(path.join(apiProjectDir, 'output')).filter(f => f.endsWith('.tif')).length, 122)
  assert.equal(fs.readdirSync(path.join(apiProjectDir, 'output/.facet_steps')).filter(f => f.endsWith('.bin')).length, 0)
  assert.equal(serviceErrors, '')
} finally {
  controller.abort()
  service.kill()
}
console.log('FacetEB streaming: two nodes, multi-angle TIFF parity, state continuity, cache cleanup and disconnected consumer passed')
console.log('FacetEB HTTP/SSE pipeline: 122 TIFFs, two acknowledged nodes, no deferred duplicate output passed')
console.log(output)
