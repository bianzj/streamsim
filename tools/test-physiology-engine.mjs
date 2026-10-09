// Small, serial GPU fixtures: absolute PAR, two biochemical methods, and failure reporting.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { runEnergyEngineRegression } from '../tests/energy_core/engine.mjs';

const root = path.resolve(import.meta.dirname, '..');
const prepared = runEnergyEngineRegression(root, { prepareOnly: true });
const template = JSON.parse(fs.readFileSync(prepared.cases[0].project));
const destination = path.join(root, '.test', 'physiology-engine', String(Date.now()));
const executable = path.resolve(process.env.STREAMSIM_ENGINE || path.join(root, 'models/bin_x64/Release/histream.exe'));
fs.mkdirSync(destination, { recursive: true });
const cases = [];
function run(name, method, scenario, invalid = false, medium = false, solver = 'traditional', leafType = 3, fluid = false) {
  const project = structuredClone(template);
  const directory = path.join(destination, name);
  fs.mkdirSync(directory, { recursive: true });
  const c = project.configuration;
  c.outDir = path.join(directory, 'output');
  c.control.vegetationTemperatureMethod = method;
  c.control.radiationSolver = solver;
  if (fluid) {
    c.fluid.enabled = true;
    c.fluid.voxelSize = 1;
    c.fluid.timeStep = 10;
    c.meteo.dTime = 10;
  }
  c.control.spectralAccelerationWidth = 8; // deliberately crosses the former 700 nm group boundary
  if (scenario === 'diffuse') c.control.depth = 1;
  c.sensor.image = false;
  c.sensor.radiationProcess = false;
  c.sensor.energyProcess = !invalid;
  c.sensor.process = !invalid;
  c.meteo.start = 0;
  c.meteo.end = 1;
  c.meteo.path = path.join(directory, 'meteo.txt');
  c.light.direct = scenario === 'diffuse' ? 0 : 0.8;
  c.light.diffuse = 1 - c.light.direct;
  fs.writeFileSync(c.meteo.path, `1\n${scenario === 'night' ? '214.0' : '214.5'} 25 15 1013 2 ${scenario === 'night' ? 0 : 700} 350\n`);
  if (invalid) c.materials.find(m => m.name === 'leaf_c3').params.qLs = 2;
  c.materials.find(m => m.name === 'leaf_c3').params.Type = leafType;
  if (medium) Object.assign(c.canopies.find(m => m.name === 'canopy_default'), {
    structureType: 'fog', density: 0, extinction: 0.08,
    scatteringAlbedo: 0.95, asymmetry: 0.85, fixedTemperature: 288,
  });
  const input = path.join(directory, 'project.json');
  fs.writeFileSync(input, JSON.stringify(project, null, 2));
  const execution = spawnSync(executable, ['eVoxelEB', input], {
    cwd: path.dirname(executable), encoding: 'utf8', windowsHide: true,
    timeout: 180000, maxBuffer: 32 * 1024 * 1024,
  });
  fs.writeFileSync(path.join(directory, 'run.log'), `${execution.stdout || ''}\n${execution.stderr || ''}`);
  assert(!execution.error, `${name}: ${execution.error}`);
  assert.equal(execution.status, invalid ? 4 : 0, `${name}: ${directory}/run.log`);
  const diagnosticsDirectory = path.join(c.outDir, 'diagnostics');
  const metadataFile = fs.readdirSync(diagnosticsDirectory).find(f => f.startsWith('physiology_node=') && f.endsWith('.json'));
  assert(metadataFile, `${name}: missing mandatory physiology diagnostics`);
  const diagnostics = JSON.parse(fs.readFileSync(path.join(diagnosticsDirectory, metadataFile)));
  assert.equal(diagnostics.valid, !invalid);
  assert.equal(diagnostics.vegetationCount > 0, !medium);
  const fields = Object.fromEntries(diagnostics.fields.map(f => [f.id, f]));
  if (invalid) {
    assert(diagnostics.invalidVoxelCount > 0);
    assert(fields.gppSunlit.invalidCount > 0);
    assert.match(execution.stderr, /Non-finite or invalid VoxelEB state/);
  } else if (medium) {
    assert.equal(diagnostics.invalidVoxelCount, 0);
    assert.equal(fields.rssSunlit.mean, null, 'A participating medium has no leaf physiology');
    assert.equal(fields.gppSunlit.mean, null);
    const binary = fs.readFileSync(path.join(diagnosticsDirectory, diagnostics.dataFile));
    assert.equal(binary.length, diagnostics.voxelCount * diagnostics.recordFloats * 4);
  } else {
    assert.equal(diagnostics.invalidVoxelCount, 0);
    for (const field of diagnostics.fields) {
      assert.equal(field.invalidCount, 0, `${name}: ${field.id}`);
      assert(Number.isFinite(field.mean));
    }
    assert.equal(fields.directPar.unit, 'umol photons m-2 leaf s-1');
    if (scenario === 'night') {
      assert.equal(fields.directPar.max, 0);
      assert.equal(fields.diffusePar.max, 0);
      assert.equal(fields.gppSunlit.max, 0);
      assert.equal(fields.gppShaded.max, 0);
      assert(fields.netAssimilationSunlit.mean <= 0);
    } else {
      const par = fields.directPar.mean + fields.diffusePar.mean;
      assert(par > 100 && par < 10000, `${name}: daylight PAR absolute scale ${par}`);
      assert(fields.gppSunlit.mean > 1, `${name}: daylight leaf GPP absolute scale`);
      if (scenario === 'diffuse') {
        assert.equal(fields.directPar.max, 0);
        assert(fields.diffusePar.mean > 100);
        assert(fields.gppShaded.mean > 1);
      }
    }
    const binary = fs.readFileSync(path.join(diagnosticsDirectory, diagnostics.dataFile));
    assert.equal(binary.length, diagnostics.voxelCount * diagnostics.recordFloats * 4);
  }
  cases.push({ name, method, scenario, solver, leafType, fluid, passed: true, directory, invalidVoxelCount: diagnostics.invalidVoxelCount,
    parMean: fields.directPar.mean + fields.diffusePar.mean,
    gppSunlitMean: fields.gppSunlit.mean, gppShadedMean: fields.gppShaded.mean });
}
for (const method of [0, 1]) {
  for (const scenario of ['day', 'diffuse', 'night']) run(`method-${method}-${scenario}`, method, scenario);
}
run('invalid-leaf-with-export-disabled', 1, 'day', true);
run('participating-medium', 1, 'day', false, true);
run('accelerated-c3-day', 1, 'day', false, false, 'accelerated');
run('accelerated-c4-day', 1, 'day', false, false, 'accelerated', 4);
run('fluid-coupled-c3-day', 1, 'day', false, false, 'traditional', 3, true);
for (const [name, row] of [
  ['bad-numeric', '214.5 25 wrong 1013 2 700 350'],
  ['zero-pressure', '214.5 25 15 0 2 700 350'],
  ['nan-input', '214.5 NaN 15 1013 2 700 350'],
]) {
  const directory = path.join(destination, name);
  fs.mkdirSync(directory, { recursive: true });
  const project = structuredClone(template);
  project.configuration.outDir = path.join(directory, 'output');
  project.configuration.meteo.path = path.join(directory, 'meteo.txt');
  project.configuration.meteo.start = 0;
  project.configuration.meteo.end = 1;
  fs.writeFileSync(project.configuration.meteo.path, `1\n${row}\n`);
  const input = path.join(directory, 'project.json');
  fs.writeFileSync(input, JSON.stringify(project));
  const execution = spawnSync(executable, ['eVoxelEB', input], {
    cwd: path.dirname(executable), encoding: 'utf8', windowsHide: true, timeout: 30000,
  });
  fs.writeFileSync(path.join(directory, 'run.log'), `${execution.stdout || ''}\n${execution.stderr || ''}`);
  assert(!execution.error);
  assert.equal(execution.status, 4);
  assert.match(execution.stderr, /Meteorology node 0/);
  cases.push({ name, passed: true, rejectedBeforeSimulation: true, directory });
}
const result = { passed: true, executable, engineSha256: createHash('sha256').update(fs.readFileSync(executable)).digest('hex'), cases };
fs.writeFileSync(path.join(destination, 'result.json'), JSON.stringify(result, null, 2));
console.log(JSON.stringify(result, null, 2));
