// Serial Vulkan regressions with frozen inputs and independent flux identities.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { runEnergyEngineRegression } from '../tests/energy_core/engine.mjs';

const root = path.resolve(import.meta.dirname, '..');
const destination = path.join(root, '.test/energy-final-state-20261005');
const args = process.argv.slice(2);
const option = (key, fallback) => args.includes(key) ? args[args.indexOf(key) + 1] : fallback;
const label = option('--label', 'candidate');
const executable = path.resolve(option('--engine', path.join(root, 'models/bin_x64/Release/histream.exe')));
const suite = option('--suite', 'fixtures');
const hash = file => createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const json = file => JSON.parse(fs.readFileSync(file, 'utf8'));
fs.mkdirSync(destination, { recursive: true });
const manifestPath = path.join(destination, 'inputs.json');

if (!fs.existsSync(manifestPath)) {
  const prepared = runEnergyEngineRegression(root, { prepareOnly: true });
  const template = json(prepared.cases[0].project);
  const fixtures = [];
  for (const [name, method, night, exports] of [
    ['method0-day', 0, false, true], ['method1-night', 1, true, true],
    ['method2-day', 2, false, true], ['exports-disabled', 0, false, false],
  ]) {
    const project = structuredClone(template);
    const directory = path.join(destination, 'inputs', name);
    fs.mkdirSync(directory, { recursive: true });
    const c = project.configuration;
    c.control.radiationMaxSteps = 256;
    c.control.soilTemperatureMethod = method;
    c.control.shortwaveScatteringOrders = 3;
    c.control.vegetationTemperatureMethod = 1;
    c.sensor.radiationProcess = exports;
    c.sensor.energyProcess = exports;
    c.sensor.process = exports;
    for (const material of c.materials) {
      if (['Soil', 'Building'].includes(material.type)) material.params.method = method;
    }
    const count = exports ? 3 : 1;
    c.meteo.start = 0;
    c.meteo.end = count;
    c.meteo.dTime = 1800;
    c.meteo.path = path.join(directory, 'meteo.txt');
    fs.writeFileSync(c.meteo.path, `${count}\n` + Array.from({ length: count }, (_, i) =>
      `${(night ? 215 : 214.5) + i / 48} 25 15 1013 2 ${night ? 0 : 700} 350\n`).join(''));
    const input = path.join(directory, 'project.json');
    fs.writeFileSync(input, JSON.stringify(project, null, 2));
    fixtures.push({ name, method, night, exports, count, input, inputSha256: hash(input) });
  }
  const examples = [];
  for (const [file, names] of [
    ['benchmark-1791106902349', ['auto', 'forest', 'beijing']],
    ['benchmark-1791106966694', ['auto']],
  ]) {
    const old = json(path.join(root, '.test/scattering-20261004', file, 'manifest.json'));
    for (const plan of old.plans.filter(p => names.includes(p.name) && p.applicable)) {
      const run = plan.runs.find(r => r.variant === 'new-order-3' || r.name === 'new-order-3' || r.orders === 3 && !r.legacy);
      // The manifest schema predates this regression; identify by recorded input path if needed.
      const chosen = run || plan.runs.find(r => r.input?.includes('new-order-3'));
      assert(chosen && fs.existsSync(chosen.input), `Missing frozen input: ${plan.name}/${file}`);
      const input = chosen.input;
      examples.push({ name: `${plan.name}-${plan.profile}`, input, inputSha256: hash(input),
        sourceProject: plan.sourceProject, modifications: plan.modifications });
    }
  }
  fs.writeFileSync(manifestPath, JSON.stringify({ fixtures, examples }, null, 2));
}
if (args.includes('--prepare')) { console.log(manifestPath); process.exit(0); }

function processFiles(output) {
  const directory = path.join(output, 'process');
  if (!fs.existsSync(directory)) return [];
  return fs.readdirSync(directory).filter(f => f.endsWith('.json')).map(f => {
    const metadata = json(path.join(directory, f));
    const data = fs.readFileSync(path.join(directory, metadata.dataFile));
    assert.equal(data.length, metadata.voxelCount * metadata.recordFloats * 4);
    const offsets = Object.fromEntries(metadata.fields.map(field => [field.id, field.offset]));
    return { metadata, data, get: (i, id) => data.readFloatLE((i * metadata.recordFloats + offsets[id]) * 4), directory };
  });
}

function inspect(output, specification) {
  const processes = processFiles(output);
  const energies = processes.filter(p => p.metadata.processType === 'energy').sort((a, b) => a.metadata.node - b.metadata.node);
  const radiations = processes.filter(p => p.metadata.processType === 'radiation');
  const diagnosticsDirectory = path.join(output, 'diagnostics');
  const balances = fs.readdirSync(diagnosticsDirectory).filter(f => f.startsWith('energy_balance_node=') && f.endsWith('.json'))
    .map(f => json(path.join(diagnosticsDirectory, f))).sort((a, b) => a.node - b.node);
  const summary = { nodeCount: energies.length, balanceDiagnosticCount: balances.length,
    finalStatesOutsideTolerance: balances.map(d => d.budgetStatesOutsideTolerance),
    maxMethod0GError: 0, maxIndependentSolidRnError: 0, maxWaterStorageError: 0,
    maxLeafFinalTemperatureLEError: 0, checkedGround: 0, checkedWater: 0, checkedLeaves: 0 };
  if (label !== 'baseline') {
    assert(balances.length > 0, 'Energy diagnostics must exist even when exports are disabled');
    for (const balance of balances) {
      assert.equal(balance.finalStateRefreshed, true);
      assert(balance.temperatureIterations >= 3 && balance.temperatureIterations <= 50);
      assert.equal(balance.budgetResidualToleranceWm2, 2);
      assert.equal(balance.budgetResidualWithinTolerance, balance.budgetStatesOutsideTolerance === 0);
      if (balance.budgetStatesOutsideTolerance === 0) {
        const residual = balance.fields.find(f => f.id === 'energyResidual');
        if (residual.count) assert(Math.max(Math.abs(residual.min), Math.abs(residual.max)) < 2.002,
          'GPU zero-count must agree with independently reconstructed mixed residuals');
      }
    }
  }
  if (specification && !specification.exports) {
    assert.equal(energies.length, 0);
    assert.equal(radiations.length, 0);
    return summary;
  }
  let previousThermal;
  for (const energy of energies) {
    const { metadata: m, directory } = energy;
    const radiation = radiations.find(p => p.metadata.node === m.node);
    assert(radiation, `Missing radiation for node ${m.node}`);
    const thermal = fs.readFileSync(path.join(directory, m.thermalState.dataFile));
    const links = fs.readFileSync(path.join(directory, m.sceneLinks.dataFile));
    const diagName = fs.readdirSync(diagnosticsDirectory).find(f => f.startsWith(`physiology_node=${m.node}_`) && f.endsWith('.json'));
    const physiology = json(path.join(diagnosticsDirectory, diagName));
    const physiologyData = fs.readFileSync(path.join(diagnosticsDirectory, physiology.dataFile));
    const rssOffsets = physiology.fields.filter(f => f.id.startsWith('rss')).map(f => f.offset);
    for (let i = 0; i < m.voxelCount; ++i) {
      const component = energy.data.readFloatLE((i * m.recordFloats + m.componentOffset) * 4);
      if (!component) continue;
      for (const field of m.fields) assert(Number.isFinite(energy.get(i, field.id)), `${field.id}/${i}`);
      for (const field of radiation.metadata.fields) assert(Number.isFinite(radiation.get(i, field.id)), `${field.id}/${i}`);
      if (!specification) continue;
      const type = links.readInt32LE((i * m.sceneLinks.recordInts + 4) * 4);
      const ts = thermal.readFloatLE((i * 3) * 4);
      const th = thermal.readFloatLE((i * 3 + 1) * 4);
      const fraction = thermal.readFloatLE((i * 3 + 2) * 4);
      const rn = radiation.get(i, 'netRadiation');
      const g = energy.get(i, 'surfaceHeatFlux');
      if ([1, 3, 4].includes(type)) {
        const emissivity = type === 1 ? 0.95 : type === 3 ? 0.92 : 0.98;
        const expected = radiation.get(i, 'shortwaveRadiation') + radiation.get(i, 'longwaveRadiation')
          - emissivity * 5.6704e-8 * (fraction * ts ** 4 + (1 - fraction) * th ** 4);
        summary.maxIndependentSolidRnError = Math.max(summary.maxIndependentSolidRnError, Math.abs(rn - expected));
      }
      if ([1, 3].includes(type) && specification.method === 0) {
        ++summary.checkedGround;
        summary.maxMethod0GError = Math.max(summary.maxMethod0GError, Math.abs(g - 0.35 * rn));
      }
      if (type === 4) {
        ++summary.checkedWater;
        assert.equal(ts, th, 'Water retains one shared temperature');
        const oldTemperature = previousThermal ? previousThermal.readFloatLE(i * 3 * 4) : 298.15;
        const expected = 4186000 * 0.5 * (ts - oldTemperature) / 1800;
        summary.maxWaterStorageError = Math.max(summary.maxWaterStorageError, Math.abs(g - expected));
      }
      if (type === 2) {
        const h = energy.get(i, 'sensibleHeat');
        const meanT = fraction * ts + (1 - fraction) * th;
        const raa = 1.2047 * 1004 * (meanT - 273.15 - 25) / h;
        if (!Number.isFinite(raa) || !(raa >= 1) || Math.abs(meanT - 298.15) < 0.1) continue;
        const rss = rssOffsets.map(offset => physiologyData.readFloatLE((i * physiology.recordFloats + offset) * 4));
        const latent = (t, resistance) => {
          const tc = t - 273.15;
          const es = 6.107 * 10 ** (7.5 * tc / (237.3 + tc));
          return 1.2047 / (raa + resistance) * (2.501 - 0.002361 * tc) * 1e6 * (es - 15) * (18 / 28.96 / 1013);
        };
        const expected = fraction * latent(ts, rss[0]) + (1 - fraction) * latent(th, rss[1]);
        summary.maxLeafFinalTemperatureLEError = Math.max(summary.maxLeafFinalTemperatureLEError, Math.abs(energy.get(i, 'latentHeat') - expected));
        ++summary.checkedLeaves;
      }
    }
    previousThermal = thermal;
  }
  if (specification && label !== 'baseline') {
    assert.equal(energies.length, specification.count);
    assert(summary.maxIndependentSolidRnError < 0.002, JSON.stringify(summary));
    assert(summary.maxMethod0GError < 0.002, JSON.stringify(summary));
    assert(summary.checkedWater > 0);
    assert(summary.maxWaterStorageError < 0.1, JSON.stringify(summary));
    assert(summary.checkedLeaves > 0);
    assert(summary.maxLeafFinalTemperatureLEError < 0.002, JSON.stringify(summary));
    if (specification.method === 0) assert(summary.checkedGround > 0);
  }
  return summary;
}

const manifest = json(manifestPath);
const results = [];
for (const plan of manifest[suite]) {
  assert.equal(hash(plan.input), plan.inputSha256, 'Frozen input changed');
  const project = json(plan.input);
  // Native runtime projects can reference generated OBJ files beside the input.
  // Relocating only project.json must retain these exact frozen resources.
  function resolveResources(value) {
    if (!value || typeof value !== 'object') return;
    for (const [key, item] of Object.entries(value)) {
      if (typeof item === 'string' && item && ['fileName', 'positionFile', 'path'].includes(key)) {
        const absolute = path.resolve(path.dirname(plan.input), item);
        if (fs.existsSync(absolute)) value[key] = absolute;
      } else resolveResources(item);
    }
  }
  resolveResources(project);
  const directory = path.join(destination, label, suite, plan.name);
  fs.mkdirSync(directory, { recursive: true });
  project.configuration.outDir = path.join(directory, 'output');
  const input = path.join(directory, 'project.json');
  fs.writeFileSync(input, JSON.stringify(project, null, 2));
  const started = performance.now();
  let elapsedMs = null;
  if (!args.includes('--inspect-only')) {
    const execution = spawnSync(executable, ['eVoxelEB', input], { cwd: path.dirname(executable),
      encoding: 'utf8', windowsHide: true, timeout: 300000, maxBuffer: 64 * 1024 * 1024 });
    elapsedMs = performance.now() - started;
    fs.writeFileSync(path.join(directory, 'run.log'), `${execution.stdout || ''}\n${execution.stderr || ''}`);
    assert(!execution.error, String(execution.error));
    assert.equal(execution.status, 0, `Engine failed: ${directory}/run.log`);
  }
  const result = { name: plan.name, input: plan.input, directory, elapsedMs,
    ...inspect(project.configuration.outDir, suite === 'fixtures' ? plan : undefined) };
  results.push(result);
  const suffix = args.includes('--inspect-only') ? '-inspection' : '';
  fs.writeFileSync(path.join(destination, `${label}-${suite}${suffix}-result.json`), JSON.stringify({ executable, engineSha256: hash(executable), results }, null, 2));
  console.log(JSON.stringify(result));
}
if (label === 'baseline' && suite === 'fixtures') {
  const method0 = results.find(r => r.name === 'method0-day');
  assert(method0.maxMethod0GError > 10, 'Baseline must reproduce the method0 G defect');
  assert(method0.maxWaterStorageError > 0.1, 'Baseline must reproduce storage/final-temperature inconsistency');
}
