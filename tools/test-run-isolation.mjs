import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import os from 'node:os'
import { createHash } from 'node:crypto'
import vm from 'node:vm'
import { spawnSync } from 'node:child_process'
import { createDefaultProject } from '../src/renderer/src/project-schema.js'

process.env.STREAMSIM_SERVER_AUTOSTART = '0'
const { server } = await import('../server.mjs')
const root = fs.mkdtempSync(path.join(os.tmpdir(), 'streamsim-run-isolation-'))
const sha = file => createHash('sha256').update(fs.readFileSync(file)).digest('hex')
const fixture = createDefaultProject({ name: 'isolation', mode: 'eRaytracing' })
fixture.configuration.outDir = 'output'
fs.mkdirSync(path.join(root, 'output'))
fs.writeFileSync(path.join(root, 'output', 'historic.tif'), 'history stays')
fs.writeFileSync(path.join(root, 'model.obj'), 'g test\nv 0 0 0\nv 0.1 0 0\nv 0 0.1 0\nf 1 2 3\n')
fs.writeFileSync(path.join(root, 'positions.txt'), '1 1 0 1 0\n')
fixture.configuration.objects = { count: 1, names: ['test'], items: [{ name: 'test', type: 'Building', fileName: 'model.obj', positionFile: 'positions.txt', dimensions: [0.1, 0.1, 0.1], meshes: [] }] }
fs.writeFileSync(path.join(root, 'project.json'), JSON.stringify(fixture, null, 2))
// Node acts as a deterministic CLI fixture, with no renderer/GPU invocation.
fs.writeFileSync(path.join(root, 'eRaytracing'), `const fs=require('fs'),path=require('path');
const p=JSON.parse(fs.readFileSync(process.argv[2]));const d=p.configuration.outDir;
const stem=path.join(d,'SZA=30.00_SAA=135.00_VZA=0.00_VAA=0.00');
fs.writeFileSync(stem+'.hdr','ENVI\\nsamples = 1\\nlines = 1\\nbands = 1\\ndata type = 4\\ninterleave = bsq\\nbyte order = 0\\nband names = {Reflectance}\\n');
const a=Buffer.alloc(4);a.writeFloatLE(0.25);fs.writeFileSync(stem+'.img',a);
`)
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve))
const url = 'http://127.0.0.1:' + server.address().port
const post = async (endpoint, body) => { const r = await fetch(url + endpoint, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }); const result = await r.json(); assert.equal(r.status, 200, JSON.stringify(result)); return result }
try {
  const original = sha(path.join(root, 'project.json'))
  const runs = []
  for (let index = 0; index < 2; index++) {
    const result = await post('/api/run', { inputPath: path.join(root, 'project.json'), mode: 'eRaytracing', executable: process.execPath })
    assert.ok(result.runId && result.outputDir)
    let manifest
    for (let attempt = 0; attempt < 100; attempt++) {
      manifest = JSON.parse(fs.readFileSync(path.join(result.outputDir, 'run-manifest.json')))
      if (!['running', 'preparing'].includes(manifest.status)) break
      await new Promise(resolve => setTimeout(resolve, 20))
    }
    assert.equal(manifest.status, 'completed', JSON.stringify(manifest))
    assert.equal(manifest.sourceInput.sha256, original)
    assert.equal(manifest.engine.sha256, sha(process.execPath))
    assert.equal(manifest.runtimeInput.sha256, sha(manifest.runtimeInput.path))
    const runtime = JSON.parse(fs.readFileSync(manifest.runtimeInput.path))
    for (const object of runtime.configuration.objects.items) {
      assert.ok(fs.existsSync(path.resolve(path.dirname(manifest.runtimeInput.path), object.positionFile)), 'placements survive cleanup')
    }
    assert.ok(manifest.completed.products.some(file => file.endsWith('.tif')))
    assert.equal(sha(path.join(root, 'project.json')), original, 'run outputDir does not overwrite source')
    runs.push(result)
  }
  assert.notEqual(runs[0].runId, runs[1].runId)
  assert.notEqual(runs[0].outputDir, runs[1].outputDir)
  assert.equal(fs.readFileSync(path.join(root, 'output', 'historic.tif'), 'utf8'), 'history stays')
  if (process.platform === 'win32') {
    // Exercise the same ASCII junction -> Chinese folder used by Stream3D.
    const canonical = path.join(root, '北京内城.stream3d'), alias = path.join(root, 'native-alias')
    fs.mkdirSync(canonical)
    for (const name of ['project.json', 'model.obj', 'positions.txt', 'eRaytracing'])
      fs.copyFileSync(path.join(root, name), path.join(canonical, name))
    const junction = spawnSync('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command',
      'New-Item -ItemType Junction -Path $env:COMPAT_ALIAS -Value $env:COMPAT_TARGET -ErrorAction Stop | Out-Null'],
      { env: { ...process.env, COMPAT_ALIAS: alias, COMPAT_TARGET: canonical }, windowsHide: true, encoding: 'utf8' })
    assert.equal(junction.status, 0, junction.stderr)
    const result = await post('/api/run', { inputPath: path.join(alias, 'project.json'), mode: 'eRaytracing', executable: process.execPath })
    let manifest
    for (let attempt = 0; attempt < 100; attempt++) {
      manifest = JSON.parse(fs.readFileSync(path.join(result.outputDir, 'run-manifest.json')))
      if (!['running', 'preparing'].includes(manifest.status)) break
      await new Promise(resolve => setTimeout(resolve, 20))
    }
    assert.equal(manifest.status, 'completed', JSON.stringify(manifest))
    assert.equal(manifest.exitCode, 0)
    assert.ok(manifest.completed.products.some(file => file.endsWith('.tif')),
      'Manifest and converted TIFF must both publish through the junction')
    assert.equal(sha(path.join(canonical, 'project.json')), original)
    console.log('Stream3D ASCII junction / Chinese project manifest and TIFF publication passed')
  }
  const listed = await post('/api/results/list', { path: path.join(root, 'output') })
  for (const run of runs) assert.ok(listed.files.some(file => file.runId === run.runId && file.kind === 'tiff'))
  assert.ok(!listed.files.some(file => /source-project|input[\\/]|run-manifest/.test(file.path)))

  // Re-export statistics for the same observation; only its latest rows stay.
  const source = fs.readFileSync(new URL('../server.mjs', import.meta.url), 'utf8')
  const functionSource = source.slice(source.indexOf('function writeRasterStatistics('), source.indexOf('\nfunction writeFloatTiff(', source.indexOf('function writeRasterStatistics(')))
  const context = { ...path, existsSync: fs.existsSync, readFileSync: fs.readFileSync, writeFileSync: fs.writeFileSync,
    csvCell: v => '"' + String(v ?? '').replaceAll('"', '""') + '"', filenameToken: () => '' }
  vm.createContext(context)
  vm.runInContext(functionSource + '\nthis.writeStats=writeRasterStatistics', context)
  const raster = path.join(root, 'observation_v.tif')
  context.writeStats(raster, 1, 1, 1, [0.1], ['old'])
  context.writeStats(raster, 1, 1, 1, [0.3], ['new'])
  const rows = fs.readFileSync(path.join(root, 'result_statistics_voxelrt.csv'), 'utf8').trim().split(/\r?\n/)
  assert.equal(rows.length, 2)
  assert.ok(rows[1].includes('"new"'))
  console.log('Run isolation, persistent inputs, recursive result listing and statistics replacement passed')
} finally { await new Promise(resolve => server.close(resolve)) }
