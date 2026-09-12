import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawnSync } from 'node:child_process'
import { createDefaultProject, validateProject } from '../src/renderer/src/project-schema.js'

const root = fileURLToPath(new URL('../', import.meta.url))
const destination = fs.mkdtempSync(path.join(root, 'tmp/component-output-'))
const exe = path.join(root, 'models/bin_x64/Release/histream.exe')
for (const mode of ['eFacetEB', 'eVoxelEB']) {
  const project = createDefaultProject({ mode })
  const c = project.configuration
  c.outDir = path.join(destination, mode)
  fs.mkdirSync(c.outDir)
  Object.assign(c.scene, { x: 8, y: 8, height: 4, voxel: 1 })
  Object.assign(c.control, { samples: 16, depth: 2, neighbors: 0 })
  Object.assign(c.sensor, { image: false, process: true, radiationProcess: true, energyProcess: true, bands: '560,10500' })
  Object.assign(c.meteo, { path: path.join(root, 'assets/examples/photovoltaic_maize/meteo.txt'), start: 24, end: 25 })
  c.fluid.enabled = false
  const types = mode === 'eFacetEB' ? ['Vegetation', 'Building', 'PV'] : ['Vegetation', 'Building']
  c.objects.items = types.map((type, index) => {
    const positionFile = path.join(c.outDir, type + '.txt')
    fs.writeFileSync(positionFile, `${1.5 + index * 2} 3 0 1 0\n`)
    const materialName = type === 'PV' ? 'pv_panel' : type === 'Building' ? 'building_surface' : 'leaf_c3'
    const spectralName = type === 'PV' ? 'pv_opaque' : type === 'Building' ? 'concrete' : 'green_leaf'
    return { name: type, type: type === 'PV' ? 'Building' : type, materialName, spectralName,
      thermalName: 'building_temperature', canopyName: type === 'Vegetation' ? 'canopy_default' : 'rigid_body',
      fileName: path.join(root, 'assets/obj-library/solar/solar_panel_single_tilted.obj'), positionFile, meshes: [] }
  })
  c.objects.count = c.objects.items.length
  c.objects.names = c.objects.items.map(item => item.name)
  const validation = validateProject(project)
  assert.equal(validation.valid, true, validation.errors.join(';'))
  const input = path.join(c.outDir, 'project.json')
  fs.writeFileSync(input, JSON.stringify(project))
  const result = spawnSync(exe, [mode, input], { cwd: path.dirname(exe), windowsHide: true,
    env: { ...process.env, STREAMSIM_FACET_STEP_SYNC: '0' }, encoding: 'utf8', timeout: 120000, maxBuffer: 4 * 1024 * 1024 })
  fs.writeFileSync(path.join(c.outDir, 'run.log'), String(result.stdout) + '\n' + String(result.stderr))
  assert.equal(result.status, 0, result.error?.message || result.stderr)
  const processDirectory = path.join(c.outDir, 'process')
  for (const file of fs.readdirSync(processDirectory).filter(name => name.endsWith('.json'))) {
    const metadata = JSON.parse(fs.readFileSync(path.join(processDirectory, file)))
    if (!['radiation', 'energy'].includes(metadata.processType)) continue
    const data = fs.readFileSync(path.join(processDirectory, metadata.dataFile))
    const componentIds = mode === 'eFacetEB'
      ? fs.readFileSync(path.resolve(processDirectory, metadata.componentFile))
      : Array.from({ length: metadata.voxelCount }, (_, index) => data.readFloatLE((index * metadata.recordFloats + metadata.componentOffset) * 4))
    for (const id of [1, 2, 3]) assert.ok(componentIds.includes(id), mode + ' must contain component ' + id)
    if (mode === 'eFacetEB') assert.ok(componentIds.includes(4), 'PV must remain separate')
    if (metadata.processType === 'energy') {
      const field = metadata.fields.find(field => field.id === 'temperature')
      assert.ok(field, 'temperature is stored per node')
      assert.ok(data.readFloatLE(field.offset * 4) > 150, 'native temperature is K')
    }
    assert.equal(data.length, (metadata.geometry === 'voxel' ? metadata.voxelCount : metadata.surfaceCount) * metadata.recordFloats * 4)
  }
  console.log(mode + ': component labels, temperature, record layout passed')
}
console.log(destination)
