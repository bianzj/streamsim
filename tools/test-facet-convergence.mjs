import fs from 'node:fs'
import path from 'node:path'
import assert from 'node:assert/strict'
import { spawnSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'
import { createHash } from 'node:crypto'
import { createDefaultProject } from '../src/renderer/src/project-schema.js'

const root = fileURLToPath(new URL('../', import.meta.url))
const output = fs.mkdtempSync(path.join(root, 'tmp/facet-convergence-'))
const exe = path.join(root, 'models/bin_x64/Release/histream.exe')
function fixture(name, iterations, tolerance, image = false) {
  const directory = path.join(output, name); fs.mkdirSync(directory)
  const p = createDefaultProject({ name, mode: 'eFacetEB', sceneX: 2, sceneY: 2, sceneHeight: 1, voxelSize: 2 })
  p.configuration.outDir = directory
  Object.assign(p.configuration.control, { facetBackend: 'cpu', couplingIterations: iterations, temperatureTolerance: tolerance })
  Object.assign(p.configuration.sensor, { image, process: true, energyProcess: true, radiationProcess: false, bands: '560', x: 2, y: 2 })
  const meteo = path.join(directory, 'meteo.txt'); fs.writeFileSync(meteo, '1\n214.5 20 15 1000 1 0 400\n')
  Object.assign(p.configuration.meteo, { path: meteo, start: 0, end: 1 })
  const input = path.join(directory, 'project.json'); fs.writeFileSync(input, JSON.stringify(p))
  // Old files remain on disk, but must not be claimed by the new run index.
  fs.writeFileSync(path.join(directory, 'photovoltaic_node_999.csv'), 'stale')
  return { directory, input, meteo, p }
}
function run(name, iterations, tolerance) {
  const { directory, input } = fixture(name, iterations, tolerance)
  const r = spawnSync(exe, ['eFacetEB', input], { cwd: path.dirname(exe), windowsHide: true, encoding: 'utf8', timeout: 180000 })
  fs.writeFileSync(path.join(directory, 'run.log'), r.stdout + '\n' + r.stderr)
  assert.ok([0, 2].includes(r.status), r.stdout + '\n' + r.stderr)
  const index = JSON.parse(fs.readFileSync(path.join(directory, 'faceteb_run.json')))
  const result = JSON.parse(fs.readFileSync(path.join(directory, 'faceteb.json')))
  assert.equal(index.completed.length, 1)
  assert.equal(index.completed[0].converged, result.converged)
  assert.ok(Number.isFinite(result.energyResidual) && Number.isFinite(result.energyResidualTemperatureEquivalent))
  assert.equal(index.products.includes('photovoltaic_node_999.csv'), false)
  const processName = fs.readdirSync(path.join(directory, 'process')).find(n => n.endsWith('.json'))
  const process = JSON.parse(fs.readFileSync(path.join(directory, 'process', processName)))
  assert.equal(process.converged, result.converged)
  return { r, index, result }
}
const incomplete = run('iteration-budget', 1, 0.001)
assert.equal(incomplete.r.status, 2)
assert.equal(incomplete.index.status, 'non_converged')
assert.equal(incomplete.result.converged, false)
const complete = run('converged', 100, 0.05)
assert.equal(complete.r.status, 0)
assert.equal(complete.index.status, 'completed')
assert.equal(complete.result.converged, true)
assert.ok(complete.result.temperatureDelta <= 0.05)
assert.ok(complete.result.energyResidualTemperatureEquivalent <= 0.05)
console.log('CPU FacetEB convergence, exhausted budget, metadata and stale-node exclusion passed:', output)

if (process.argv.includes('--api')) {
  process.env.STREAMSIM_SERVER_AUTOSTART = '0'
  const { server } = await import('../server.mjs')
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve))
  const url = 'http://127.0.0.1:' + server.address().port
  const abort = new AbortController(), events = []
  const response = await fetch(url + '/api/events', { signal: abort.signal })
  const reader = response.body.getReader(), decoder = new TextDecoder()
  const stream = (async () => {
    let buffered = ''
    try {
      for (;;) {
        const { value, done } = await reader.read(); if (done) break
        buffered += decoder.decode(value, { stream: true })
        let end
        while ((end = buffered.indexOf('\n\n')) >= 0) {
          const event = buffered.slice(0, end); buffered = buffered.slice(end + 2)
          if (event.startsWith('data: ')) events.push(JSON.parse(event.slice(6)))
        }
      }
    } catch (error) { if (error.name !== 'AbortError') throw error }
  })()
  const sha = file => createHash('sha256').update(fs.readFileSync(file)).digest('hex')
  const post = async (endpoint, data) => {
    const r = await fetch(url + endpoint, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data) })
    const result = await r.json(); assert.equal(r.status, 200, JSON.stringify(result)); return result
  }
  try {
    for (const [name, iterations, tolerance, expectedStatus, expectedCode] of [
      ['api-budget', 1, 0.001, 'non_converged', 2], ['api-converged', 100, 0.05, 'completed', 0]
    ]) {
      const { directory, input, meteo } = fixture(name, iterations, tolerance, true)
      const inputSha = sha(input), driverSha = sha(meteo)
      const start = await post('/api/run', { inputPath: input, mode: 'eFacetEB', executable: exe })
      const deadline = Date.now() + 180000
      let closed
      while (!closed && Date.now() < deadline) {
        closed = events.find(event => event.type === 'closed' && event.runId === start.runId)
        if (!closed) await new Promise(resolve => setTimeout(resolve, 25))
      }
      assert.ok(closed, 'API run did not close within deadline')
      assert.equal(closed.status, expectedStatus)
      assert.equal(closed.code, expectedCode)
      assert.equal(closed.outputDir, start.outputDir)
      assert.ok(events.some(e => e.type === 'started' && e.runId === start.runId && e.outputDir === start.outputDir))
      const manifest = JSON.parse(fs.readFileSync(path.join(start.outputDir, 'run-manifest.json')))
      assert.equal(manifest.status, expectedStatus)
      assert.equal(manifest.exitCode, expectedCode)
      assert.equal(manifest.completed.nodeCount, 1)
      assert.equal(manifest.completed.nodes[0].converged, expectedCode === 0)
      assert.equal(manifest.sourceInput.sha256, inputSha)
      assert.equal(sha(input), inputSha)
      assert.equal(manifest.engine.sha256, sha(exe))
      assert.equal(manifest.meteorology.sha256, driverSha)
      assert.equal(manifest.meteorology.snapshot.sha256, driverSha)
      assert.equal(sha(manifest.meteorology.snapshot.path), driverSha)
      assert.equal(sha(manifest.runtimeInput.path), manifest.runtimeInput.sha256)
      assert.ok(manifest.runtimeInput.path.startsWith(start.outputDir))
      const runtime = JSON.parse(fs.readFileSync(manifest.runtimeInput.path))
      assert.equal(runtime.configuration.outDir, start.outputDir)
      assert.equal(runtime.configuration.meteo.path, manifest.meteorology.snapshot.path)
      assert.ok(manifest.completed.products.some(file => file.endsWith('.tif')), 'actual TIFF exported even when unconverged')
      const listed = await post('/api/results/list', { path: start.outputDir })
      assert.ok(listed.files.some(file => file.kind === 'tiff' && file.runId === start.runId && file.runStatus === expectedStatus))
      assert.ok(listed.files.every(file => !/source-project|run-manifest|input[\\/]/.test(file.path)))
      assert.ok(fs.existsSync(path.join(directory, 'photovoltaic_node_999.csv')), 'history remains')
      fs.writeFileSync(path.join(start.outputDir, 'api-events.json'), JSON.stringify(events.filter(event => event.runId === start.runId), null, 2))
    }
    console.log('Real CPU API FacetEB non-convergence/completion, SSE, TIFF, frozen meteorology and persistent input passed')
  } finally {
    abort.abort(); await stream
    await new Promise(resolve => server.close(resolve))
  }
}
