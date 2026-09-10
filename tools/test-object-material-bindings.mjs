import assert from 'node:assert/strict'
import fs from 'node:fs'
import vm from 'node:vm'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawnSync } from 'node:child_process'
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js'
import { createMaterialPresets, createDefaultProject, validateProject, normalizeProject, projectToXml, simplifySolarSpectra } from '../src/renderer/src/project-schema.js'

// Exercise the actual UI functions without a browser or a running simulation.
const source = fs.readFileSync(new URL('../src/renderer/src/main.js', import.meta.url), 'utf8')
function functionSource(name) {
  const start = source.indexOf(`function ${name}(`)
  assert.ok(start >= 0, name)
  const next = source.slice(start + 1).search(/\n(?:async )?function /)
  return source.slice(start, next < 0 ? undefined : start + 1 + next)
}
const presets = createMaterialPresets()
assert.deepEqual(presets.spectra.filter((s) => s.name.startsWith('pv_')).map((s) => s.name), ['pv_opaque'])
const legacySpectrum = { name: 'pv_monocrystalline', label: '单晶硅太阳能板光谱', model: 'file',
  fileName: 'assets/spectral-library/pv_monocrystalline_silicon.txt', reflectance: '0.06', transmittance: '0',
  refTir: 0.1, tauTir: 0, previewTexture: { enabled: true, preset: 'solar-panel', repeatSize: 0.18 } }
assert.equal(simplifySolarSpectra([legacySpectrum]).length, 0)
for (const configuration of [
  { scene: { background: { spectralName: legacySpectrum.name } } },
  { objects: { items: [{ spectralName: legacySpectrum.name }] } },
  { objects: { items: [{ meshes: [{ spectralName: legacySpectrum.name }] }] } }
]) assert.deepEqual(simplifySolarSpectra([legacySpectrum], configuration), [legacySpectrum])
assert.equal(simplifySolarSpectra([{ ...legacySpectrum, reflectance: '0.2' }]).length, 1)
assert.equal(simplifySolarSpectra([{ ...legacySpectrum, label: 'My panel' }]).length, 1)
const legacyProject = createDefaultProject()
legacyProject.configuration.spectra.push(legacySpectrum)
assert.equal(normalizeProject(legacyProject).configuration.spectra.some((s) => s.name === legacySpectrum.name), false)
const fields = new Map()
const field = (key) => {
  if (!fields.has(key)) fields.set(key, { value: '', innerHTML: '' })
  return fields.get(key)
}
const context = vm.createContext({
  state: { config: presets, pendingObject: { meshNames: ['tree_leaves', 'tree_trunk'] } },
  $: field, document: { querySelector: field },
  escapeHtml: (value) => String(value ?? ''),
  optionList: (items, label, selected) => `${selected}:${items.some((item) => item.name === selected)}`,
  bindingOptionList: (items, selected) => selected
})
for (const name of ['physicalTypeForObject', 'materialMatchesObjectType', 'objectClassification', 'objectTypeFields', 'recommendedVegetationMeshBinding', 'renderMeshMaterialRows']) {
  vm.runInContext(functionSource(name), context)
}
context.pv = presets.materials.find((m) => m.name === 'pv_panel_mono')
assert.equal(vm.runInContext("materialMatchesObjectType(pv, 'Vegetation')", context), true)
assert.equal(vm.runInContext("materialMatchesObjectType(pv, 'Building')", context), true)
assert.equal(vm.runInContext("materialMatchesObjectType(pv, 'Fog')", context), false)
field('#materialObjectType').value = 'Vegetation'
field('#spectralPreset').value = 'pv_opaque'
field('#thermalPreset').value = 'pv_temperature'
field('#physicalMaterialPreset').value = 'pv_panel_mono'
vm.runInContext('renderMeshMaterialRows({ recommendVegetation: true })', context)
assert.equal((field('#meshMaterialRows').innerHTML.match(/pv_panel_mono:true/g) || []).length, 2)
assert.doesNotMatch(field('#meshMaterialRows').innerHTML, /tree_trunk_wood|green_leaf/)

// Explicit plant choices must also survive names such as "trunk".
field('#physicalMaterialPreset').value = 'leaf_c4'
field('#spectralPreset').value = 'maize_leaf'
vm.runInContext('renderMeshMaterialRows()', context)
assert.equal((field('#meshMaterialRows').innerHTML.match(/leaf_c4:true/g) || []).length, 2)

// The existing-object material picker must update actual mesh bindings.
const handlers = new Map()
const selects = [{ value: 'leaf_c3' }, { value: 'tree_trunk_wood' }]
const canopies = [{ value: 'tree_leaf_canopy' }, { value: 'tree_trunk_rigid' }]
field('#objectAttributeMaterial').value = 'pv_panel_mono'
field('#objectAttributeMaterial').addEventListener = (event, handler) => handlers.set(event, handler)
field('#objectAttributeForm').elements = { canopyName: { value: 'tree_leaf_canopy' } }
context.$$ = (selector) => selector.includes('attribute-physical') ? selects : canopies
context.renderObjectAttributeSummary = () => {}
const eventStart = source.indexOf("$('#objectAttributeMaterial').addEventListener")
const eventEnd = source.indexOf("$('#closeObjectBtn')", eventStart)
vm.runInContext(source.slice(eventStart, eventEnd), context)
handlers.get('change')()
assert.ok(selects.every((s) => s.value === 'pv_panel_mono'))
assert.ok(canopies.every((s) => s.value === 'rigid_body'))

// Solar Panel is a persistent UI classification using the existing solid engine type.
const solarFields = JSON.parse(vm.runInContext("JSON.stringify(objectTypeFields('SolarPanel'))", context))
assert.deepEqual(solarFields, { type: 'Building', classification: 'SolarPanel' })
const solarProject = createDefaultProject({ mode: 'eFacetEB' })
solarProject.configuration.objects.items = [{ name: 'panel', ...solarFields, fileName: 'panel.obj',
  materialName: 'pv_panel', spectralName: 'pv_opaque', thermalName: 'pv_temperature', canopyName: 'rigid_body' }]
context.solarObject = normalizeProject(JSON.parse(JSON.stringify(solarProject))).configuration.objects.items[0]
assert.equal(vm.runInContext('objectClassification(solarObject)', context), 'SolarPanel')
assert.equal(validateProject(solarProject).valid, true)
assert.match(projectToXml(solarProject), /<types>Building<\/types>/)
assert.equal(vm.runInContext("objectTypeFields('Vegetation').classification", context), undefined)
field('#materialObjectType').value = 'SolarPanel'
field('#spectralPreset').value = '__new__'
field('#thermalPreset').value = '__new__'
context.applySpectrumPreset = () => {}
context.applyThermalPreset = () => {}
context.renderPhysicalParameters = () => {}
for (const name of ['renderSpectrumPresetOptions', 'renderThermalPresetOptions', 'renderPhysicalPresetOptions']) {
  vm.runInContext(functionSource(name), context)
  vm.runInContext(`${name}()`, context)
}
assert.equal(field('#spectralPreset').innerHTML, 'pv_opaque:true')
assert.equal(field('#thermalPreset').innerHTML, 'pv_temperature:true')
assert.equal(field('#physicalMaterialPreset').innerHTML, 'pv_panel:true')

// New buildings use an editable dry concrete surface, not the soil preset.
field('#materialObjectType').value = 'Building'
vm.runInContext('renderPhysicalPresetOptions()', context)
assert.equal(field('#physicalMaterialPreset').innerHTML, 'building_surface:true')
context.building = presets.materials.find((m) => m.name === 'building_surface')
context.legacySoil = presets.materials.find((m) => m.name === 'soil_dry')
assert.equal(vm.runInContext("materialMatchesObjectType(building, 'Building')", context), true)
assert.equal(vm.runInContext("materialMatchesObjectType(legacySoil, 'Building')", context), true)
assert.equal(context.building.params.SMC, 0)
assert.equal(context.building.params.Satwater, 0)
const buildingProject = createDefaultProject()
buildingProject.configuration.objects.items = [{ name: 'building', type: 'Building', fileName: 'building.obj',
  materialName: 'building_surface', spectralName: 'concrete', thermalName: 'building_temperature', canopyName: 'rigid_body' }]
assert.equal(validateProject(normalizeProject(buildingProject)).valid, true)
assert.equal(normalizeProject(buildingProject).configuration.objects.items[0].materialName, 'building_surface')
assert.match(projectToXml(buildingProject), /<soilSet name="building_surface">/)

for (const [fileName, materialName] of [['tree.obj', 'pv_panel_mono'], ['solar_panel.obj', 'leaf_c3']]) {
  const project = createDefaultProject({ mode: 'eFacetEB' })
  project.configuration.objects.items = [{ name: 'test', fileName, type: 'Vegetation', materialName,
    meshes: [{ name: 'mesh', materialName, spectralName: materialName.startsWith('pv_') ? 'pv_opaque' : 'green_leaf', thermalName: 'vegetation_temperature' }] }]
  const roundTrip = normalizeProject(JSON.parse(JSON.stringify(project)))
  assert.equal(validateProject(roundTrip).valid, true)
  assert.equal(roundTrip.configuration.objects.items[0].type, 'Vegetation')
  assert.equal(roundTrip.configuration.objects.items[0].meshes[0].materialName, materialName)
}
assert.doesNotMatch(source, /solar-panel-card|importBuiltinSolarPanel|SOLAR_PANEL_MODELS/)
console.log('OBJ geometry/material bindings: passed')

if (process.argv.includes('--engine')) {
  const root = fileURLToPath(new URL('../', import.meta.url))
  const example = path.join(root, 'assets/examples/photovoltaic_maize')
  const base = JSON.parse(fs.readFileSync(path.join(example, 'project.json'), 'utf8'))
  const output = fs.mkdtempSync(path.join(root, 'tmp/object-material-'))
  const executable = path.join(root, 'models/bin_x64/Release/histream.exe')
  const run = (name, geometry, type, physical, spectral, scale) => {
    const p = structuredClone(base)
    const c = p.configuration
    c.outDir = path.join(output, name)
    fs.mkdirSync(c.outDir)
    c.meteo.path = path.join(example, 'meteo.txt')
    c.meteo.start = 24
    c.meteo.end = 25
    const fileName = path.join(root, 'assets/obj-library', geometry)
    const object = new OBJLoader().parse(fs.readFileSync(fileName, 'utf8'))
    const meshes = []
    object.traverse((mesh) => {
      if (mesh.isMesh) meshes.push({ name: mesh.name, materialName: physical, spectralName: spectral,
        thermalName: 'vegetation_temperature', canopyName: physical === 'pv_solid' ? 'rigid_body' : 'tree_leaf_canopy' })
    })
    const positionFile = path.join(c.outDir, 'positions.txt')
    fs.writeFileSync(positionFile, `2 2 0 ${scale} 0\n`)
    c.objects = { count: 1, names: [name], items: [{ name, type, fileName, positionFile,
      materialName: physical, canopyName: meshes[0].canopyName, meshes }] }
    const validation = validateProject(p)
    assert.equal(validation.valid, true, validation.errors.join('; '))
    const input = path.join(c.outDir, 'project.json')
    fs.writeFileSync(input, JSON.stringify(p, null, 2))
    const result = spawnSync(executable, ['eFacetEB', input], {
      cwd: path.dirname(executable), encoding: 'utf8', windowsHide: true, timeout: 120000, maxBuffer: 8 * 1024 * 1024
    })
    fs.writeFileSync(path.join(c.outDir, 'run.log'), `${result.stdout}\n${result.stderr}`)
    assert.equal(result.status, 0, result.error?.message || result.stderr)
    const data = JSON.parse(fs.readFileSync(path.join(c.outDir, 'faceteb.json'), 'utf8'))
    const count = data.leafFacetCount * 2
    assert.ok(count > 0)
    assert.ok(data.temperature.every(Number.isFinite))
    const powers = data.photovoltaicPower.slice(0, count)
    const latent = data.latentHeat.slice(0, count)
    if (physical === 'pv_solid') {
      assert.ok(powers.some((v) => v > 0), 'PV geometry must generate electricity')
      assert.ok(latent.every((v) => v === 0), 'PV surfaces must not transpire')
    } else {
      assert.ok(powers.every((v) => v === 0), 'Vegetation must not generate electricity')
      assert.ok(latent.some((v) => Math.abs(v) > 0), 'Vegetation must enter latent heat calculation')
    }
    console.log(`${name}: ${data.leafFacetCount} object facets, max PV=${Math.max(...powers)}, max latent=${Math.max(...latent)}`)
    return data
  }
  const building = run('tree-pv-building-label', 'vegetation/tree_poplar.obj', 'Building', 'pv_solid', 'pv_flat', 0.15)
  const vegetation = run('tree-pv-vegetation-label', 'vegetation/tree_poplar.obj', 'Vegetation', 'pv_solid', 'pv_flat', 0.15)
  assert.equal(building.facetCount, vegetation.facetCount)
  assert.ok(building.photovoltaicPower.every((v, i) => Math.abs(v - vegetation.photovoltaicPower[i]) < 0.001))
  run('panel-vegetation', 'solar/solar_panel_single_tilted.obj', 'Vegetation', 'leaf_c4', 'maize_leaf', 1)
  console.log(`Engine geometry/material regression passed: ${output}`)
}
