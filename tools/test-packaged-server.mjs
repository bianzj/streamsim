// node tools/test-packaged-server.mjs release/<build>/win-unpacked
// Uses an isolated port; does not stop the user's running web service.
import { spawn } from 'node:child_process'
import fs from 'node:fs'
import path from 'node:path'
import assert from 'node:assert/strict'
import { createServer } from 'node:net'

const base = path.resolve(process.argv[2] || 'release/win-unpacked')
const resources = path.join(base, 'resources')
fs.mkdirSync('tmp', { recursive: true })
const scratch = fs.mkdtempSync(path.resolve('tmp/package-smoke-'))
const probe = createServer()
await new Promise(resolve => probe.listen(0, '127.0.0.1', resolve))
const port = probe.address().port
await new Promise(resolve => probe.close(resolve))
for (const file of ['engine/histream.exe', 'statistic/streamsim_statistics.py',
  'statistic/scene_example_report.py', 'statistic/run_scene_examples.mjs',
  'src/renderer/src/project-schema.js', 'branding/streamsim.ico']) {
  assert.ok(fs.existsSync(path.join(resources, file)), file)
}
const child = spawn(path.join(base, 'StreamSim.exe'), [path.join(resources, 'app.asar/server.mjs')], {
  cwd: base, windowsHide: true,
  env: { ...process.env, ELECTRON_RUN_AS_NODE: '1', STREAMSIM_RESOURCE_ROOT: resources,
    STREAMSIM_DATA_ROOT: scratch, STREAMSIM_PORT: String(port) }
})
let log = '', spawnError
child.stdout.on('data', data => { log += data })
child.stderr.on('data', data => { log += data })
child.on('error', error => { spawnError = error })
try {
  let ready = false
  const url = `http://127.0.0.1:${port}`
  for (let attempt = 0; attempt < 60; attempt++) {
    if (spawnError) throw spawnError
    if (child.exitCode !== null) throw new Error(log)
    if (log.includes(`STREAMSIM: ${url}`)) { ready = true; break }
    await new Promise(resolve => setTimeout(resolve, 250))
  }
  assert.ok(ready, log)
  const response = await fetch(`${url}/api/defaults`, { signal: AbortSignal.timeout(10000) })
  assert.equal(response.status, 200)
  assert.ok(JSON.stringify(await response.json()).includes('engine'), 'packaged engine path')
  const page = await fetch(url, { signal: AbortSignal.timeout(10000) })
  assert.equal(page.status, 200)
  const html = await page.text()
  assert.ok(html.includes('saveAsDialog'))
  const script = html.match(/src="([^"]+\.js)"/)
  assert.ok(script)
  const bundleResponse = await fetch(new URL(script[1], url), { signal: AbortSignal.timeout(10000) })
  assert.equal(bundleResponse.status, 200)
  const bundle = await bundleResponse.text()
  assert.ok(!/saveAsDialog.{0,8}addEventListener/.test(bundle), 'no backdrop dismissal')
  console.log('PASS: packaged Electron runtime, server API, GUI, engine path, statistics tools, S icon')
} finally {
  child.kill()
  fs.writeFileSync(path.join(scratch, 'server.log'), log)
}
