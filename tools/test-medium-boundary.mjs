// Serial GPU checks for participating-medium entries in VoxelEB SW/LW.
// Closure is diagnostic only: an unfinished budget must not stop time nodes.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { runEnergyEngineRegression } from '../tests/energy_core/engine.mjs';
import { normalizeProject } from '../src/renderer/src/project-schema.js';

const root = path.resolve(import.meta.dirname, '..');
const args = process.argv.slice(2);
const option = (key, fallback) => args.includes(key) ? args[args.indexOf(key) + 1] : fallback;
const label = option('--label', 'candidate');
const executable = path.resolve(option('--engine', path.join(root, 'models/bin_x64/Release/histream.exe')));
const taskDirectory = path.join(root, '.test/medium-boundary-20261006');
const inspectOnly = args.includes('--inspect');
const destination = path.resolve(option('--inspect', path.join(taskDirectory, `${label}-${Date.now()}`)));
const read = file => JSON.parse(fs.readFileSync(file));
const sha256 = file => createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const buildManifest = read(path.join(path.dirname(executable), 'build-manifest.json'));
assert.equal(buildManifest.engine.sha256, sha256(executable));
const artifactSha256 = createHash('sha256').update(JSON.stringify({
  engine: buildManifest.engine, shaderBinaries: buildManifest.shaderBinaries })).digest('hex');
for (const shader of buildManifest.shaderBinaries)
  assert.equal(sha256(path.join(path.dirname(executable), shader.path)), shader.sha256);

fs.mkdirSync(destination, { recursive: true });
function createTemplate() {
  const prepared = runEnergyEngineRegression(root, { prepareOnly: true });
  const project = read(prepared.cases[0].project);
  const c = project.configuration;
  c.scene.height = 8;
  c.scene.background.angularEffectStrength = 0;
  c.canopies.push({ ...structuredClone(c.canopies[0]), name: 'boundary_fog',
    structureType: 'fog', lai: 0, density: 0, extinction: 0.08,
    scatteringAlbedo: 0, asymmetry: 0, emissionScale: 0, fixedTemperature: 298.15 });
  const fog = structuredClone(c.objects.items.find(item => item.type === 'Vegetation'));
  Object.assign(fog, { name: 'boundary_fog', canopyName: 'boundary_fog',
    angularEffectStrength: 0, dimensions: [8, 6, 6], instanceCount: 1,
    positionFile: path.join(destination, 'fog_position.txt') });
  fs.writeFileSync(fog.positionFile, '2 1 0 1 0\n');
  fog.meshes = fog.meshes.map(mesh => ({ ...mesh, name: 'boundary_fog',
    canopyName: 'boundary_fog', angularEffectStrength: 0 }));
  c.objects.items = [fog];
  c.objects.count = 1;
  c.objects.names = [fog.name];
  return normalizeProject(project);
}
const template = inspectOnly ? read(path.join(destination, 'clear-sky/project.json')) : createTemplate();
const cases = [
  { name: 'clear-sky', extinction: 0, rli: 350, emission: 0 },
  { name: 'thin-sky', extinction: 0.08, rli: 350, emission: 0 },
  { name: 'thick-sky', extinction: 0.5, rli: 350, emission: 0 },
  { name: 'dark-zero-emission', extinction: 0.08, rli: 0, emission: 0 },
  { name: 'dark-full-emission', extinction: 0.08, rli: 0, emission: 1 },
  { name: 'dark-half-emission', extinction: 0.08, rli: 0, emission: 0.5 },
  { name: 'dark-zero-extinction', extinction: 0, rli: 0, emission: 1 },
  { name: 'fire-zero-emission', extinction: 0.08, rli: 0, emission: 0, structure: 'fire', temperature: 1100 },
  { name: 'clear-shortwave', extinction: 0, rli: 0, emission: 0, rin: 700 },
  { name: 'thick-shortwave', extinction: 0.5, rli: 0, emission: 0, rin: 700 },
  { name: 'isothermal-radiation', extinction: 0.5, rli: 5.6704e-8 * 298.15 ** 4, emission: 1 },
  { name: 'finite-budget-sequence', extinction: 0, rli: 350, emission: 0, rin: 10000, nodes: 3 },
];

const results = [];
function loadRadiation(directory) {
  const processDirectory = path.join(directory, 'output/process');
  const metadata = read(path.join(processDirectory, fs.readdirSync(processDirectory).find(f => /^voxelrt_T=.*\.json$/.test(f))));
  const data = fs.readFileSync(path.join(processDirectory, metadata.dataFile));
  const linksFile = path.join(processDirectory, metadata.sceneLinks.dataFile);
  const links = fs.readFileSync(linksFile);
  assert.equal(data.length, metadata.voxelCount * metadata.recordFloats * 4);
  const offsets = Object.fromEntries(metadata.fields.map(f => [f.id, f.offset]));
  const rows = [];
  for (let voxel = 0; voxel < metadata.voxelCount; ++voxel) {
    const base = voxel * metadata.recordFloats;
    const type = links.readInt32LE((voxel * metadata.sceneLinks.recordInts + 4) * 4);
    const component = data.readFloatLE((base + metadata.componentOffset) * 4);
    if (type !== 1 || !component) continue;
    const row = {
      voxel, position: metadata.positionOffsets.map(offset => data.readFloatLE((base + offset) * 4)),
      sw: data.readFloatLE((base + offsets.shortwaveRadiation) * 4),
      lw: data.readFloatLE((base + offsets.longwaveRadiation) * 4),
    };
    assert(Number.isFinite(row.lw) && row.lw >= 0, `Nonfinite/negative ground LW: ${voxel}`);
    assert(Number.isFinite(row.sw) && row.sw >= 0, `Nonfinite/negative ground SW: ${voxel}`);
    rows.push(row);
  }
  assert(rows.length > 0, 'Require real soil receivers, not medium Rn placeholders');
  return { rows, linksSha256: sha256(linksFile), groundCount: rows.length,
    lwMin: Math.min(...rows.map(r => r.lw)), lwMax: Math.max(...rows.map(r => r.lw)),
    swMin: Math.min(...rows.map(r => r.sw)), swMax: Math.max(...rows.map(r => r.sw)) };
}

for (const specification of cases) {
  const project = structuredClone(template);
  const c = project.configuration;
  const directory = path.join(destination, specification.name);
  fs.mkdirSync(directory, { recursive: true });
  c.outDir = path.join(directory, 'output');
  c.fluid.enabled = false;
  Object.assign(c.sensor, { x: 16, y: 16, image: false, process: true, energyProcess: true, radiationProcess: true });
  Object.assign(c.control, { shortwaveScatteringOrders: 1, radiationMaxSteps: 256, radiationSolver: 'traditional',
    vegetationTemperatureMethod: 0, soilTemperatureMethod: 2 });
  Object.assign(c.light, { direct: 0, diffuse: 1 });
  const nodes = specification.nodes || 1;
  Object.assign(c.meteo, { start: 0, end: nodes, dTime: 1800, path: path.join(directory, 'meteo.txt') });
  if (!inspectOnly) fs.writeFileSync(c.meteo.path, `${nodes}\n` + Array.from({ length: nodes }, (_, node) =>
    `${214.5 + node / 48} 25 15 1013 2 ${specification.rin || 0} ${specification.rli}\n`).join(''));
  // A black horizontal ground absorbs sky LW; other ground optics are zero.
  for (const spectrum of c.spectra) Object.assign(spectrum, {
    reflectance: '0', transmittance: '0', refTir: 0, tauTir: 0,
  });
  Object.assign(c.canopies.find(canopy => canopy.name === 'boundary_fog'), {
    structureType: specification.structure || 'fog', density: 0,
    extinction: specification.extinction, scatteringAlbedo: 0, asymmetry: 0,
    fixedTemperature: specification.temperature || 298.15, emissionScale: specification.emission,
  });
  const input = path.join(directory, 'project.json');
  if (!inspectOnly) fs.writeFileSync(input, JSON.stringify(project, null, 2));
  if (args.includes('--prepare-only')) continue;
  const started = performance.now();
  if (!inspectOnly) {
    const run = spawnSync(executable, ['eVoxelEB', input], {
      cwd: path.dirname(executable), windowsHide: true, encoding: 'utf8',
      timeout: 180000, maxBuffer: 32 * 1024 * 1024,
      env: { ...process.env, STREAMSIM_NEW_SCATTERING: '0' },
    });
    fs.writeFileSync(path.join(directory, 'run.log'), `${run.stdout || ''}\n${run.stderr || ''}`);
    assert(!run.error, String(run.error));
    assert.equal(run.status, 0, `Engine failed: ${directory}/run.log`);
  }
  const result = { ...specification, directory, elapsedMs: inspectOnly ? null : performance.now() - started, ...loadRadiation(directory) };
  results.push(result);
  console.log(JSON.stringify({ name: result.name, lwMin: result.lwMin, lwMax: result.lwMax,
    swMin: result.swMin, swMax: result.swMax, elapsedMs: result.elapsedMs }));
}
if (args.includes('--prepare-only')) { console.log(destination); process.exit(0); }

const checks = [];
const check = (name, callback) => {
  try { callback(); checks.push({ name, passed: true }); }
  catch (error) { checks.push({ name, passed: false, error: String(error.stack || error) }); }
};
const byName = Object.fromEntries(results.map(r => [r.name, r]));
const tolerance = (a, b) => 2e-4 + 2e-5 * Math.max(a, b);
check('unchanged receiver geometry', () => {
  for (const result of results) {
    // Fire voxelization has a distinct volume shape. Its ground receivers
    // remain identical; extinction/emission comparisons share the fog volume.
    if (result.structure !== 'fire') assert.equal(result.linksSha256, results[0].linksSha256);
    assert.equal(result.groundCount, results[0].groundCount);
    result.rows.forEach((row, i) => assert.deepEqual(row.position, results[0].rows[i].position));
  }
});
check('zero extinction admits the external sky', () => {
  for (const row of byName['clear-sky'].rows) assert(Math.abs(row.lw - 350) < 0.01, `${row.voxel}: ${row.lw}`);
});
check('increasing extinction cannot increase sky LW', () => {
  let reduced = 0;
  const low = byName['thin-sky'].rows, high = byName['thick-sky'].rows;
  low.forEach((row, i) => {
    assert(row.lw <= 350.01);
    assert(high[i].lw <= row.lw + tolerance(high[i].lw, row.lw));
    if (row.lw - high[i].lw > tolerance(row.lw, high[i].lw)) ++reduced;
  });
  assert(reduced > 0, 'The fixture must include nonzero medium path lengths');
});
check('zero source means zero fog/fire LW', () => {
  for (const name of ['dark-zero-emission', 'dark-zero-extinction', 'fire-zero-emission'])
    assert(byName[name].lwMax <= 1e-5, `${name}: ${byName[name].lwMax}`);
});
check('finite homogeneous thermal source obeys its blackbody bound', () => {
  const limit = 5.6704e-8 * 298.15 ** 4;
  const full = byName['dark-full-emission'];
  assert(full.lwMax > 1e-4);
  assert(full.lwMax <= limit + 0.02, `${full.lwMax} > ${limit}`);
});
check('medium thermal source scales linearly with emissionScale', () => {
  byName['dark-half-emission'].rows.forEach((row, i) => {
    const expected = 0.5 * byName['dark-full-emission'].rows[i].lw;
    assert(Math.abs(row.lw - expected) < tolerance(row.lw, expected));
  });
});
check('clear medium transmits shortwave and extinction attenuates it', () => {
  let reduced = 0;
  const clear = byName['clear-shortwave'].rows, thick = byName['thick-shortwave'].rows;
  clear.forEach((row, i) => {
    assert(Math.abs(row.sw - 700) < 0.02, `${row.voxel}: ${row.sw}`);
    assert(thick[i].sw <= row.sw + tolerance(thick[i].sw, row.sw));
    if (row.sw - thick[i].sw > tolerance(row.sw, thick[i].sw)) ++reduced;
  });
  assert(reduced > 0);
});
check('thin medium balances thermal source and transmitted sky', () => {
  const expected = 5.6704e-8 * 298.15 ** 4;
  byName['thin-sky'].rows.forEach((row, i) => {
    const combined = row.lw * expected / 350 + byName['dark-full-emission'].rows[i].lw;
    assert(Math.abs(combined - expected) < 0.02, `${row.voxel}: ${combined} versus ${expected}`);
  });
});
check('isothermal radiation obeys the existing opacity-cutoff error bound', () => {
  const expected = 5.6704e-8 * 298.15 ** 4;
  // For this uniform, purely absorbing medium and equal-temperature sky,
  // full-path I=B. Stopping at throughput<0.01 omits at most 0.01*B.
  // This bound does not apply to arbitrary sources or maxStep truncation.
  for (const row of byName['isothermal-radiation'].rows) {
    assert(row.lw <= expected + 0.02, `${row.voxel}: extra thermal source ${row.lw}`);
    assert(row.lw >= expected * 0.99 - 0.02, `${row.voxel}: lost more than the opacity-cutoff bound`);
  }
});
check('finite unfinished budgets continue to the next time node', () => {
  const directory = byName['finite-budget-sequence'].directory;
  const diagnosticDirectory = path.join(directory, 'output/diagnostics');
  const balanceFiles = fs.readdirSync(diagnosticDirectory).filter(f => /^energy_balance_node=.*\.json$/.test(f));
  assert.equal(balanceFiles.length, 3);
  const balances = balanceFiles.map(f => read(path.join(diagnosticDirectory, f))).sort((a, b) => a.node - b.node);
  assert.deepEqual(balances.map(b => b.node), [0, 1, 2]);
  assert(balances[0].budgetStatesOutsideTolerance > 0, 'Stress input must reproduce an unfinished first node');
  assert.equal(balances[0].temperatureIterations, 50);
  const processDirectory = path.join(directory, 'output/process');
  const files = fs.readdirSync(processDirectory).filter(f => /^voxeleb_T=.*\.json$/.test(f));
  assert.equal(files.length, 3);
  for (const file of files) {
    const metadata = read(path.join(processDirectory, file));
    const data = fs.readFileSync(path.join(processDirectory, metadata.dataFile));
    for (let voxel = 0; voxel < metadata.voxelCount; ++voxel) {
      const base = voxel * metadata.recordFloats;
      if (!data.readFloatLE((base + metadata.componentOffset) * 4)) continue;
      for (const field of metadata.fields)
        assert(Number.isFinite(data.readFloatLE((base + field.offset) * 4)), `Nonfinite sequence field ${field.id}`);
    }
  }
});
const result = { passed: checks.every(c => c.passed), label, destination, executable,
  engineSha256: sha256(executable), artifactSha256, sourceTreeSha256: buildManifest.sourceTreeSha256,
  scope: 'VoxelEB participating-medium entry classification; no requirement of budget closure',
  cases: results.map(({ rows, ...record }) => record), checks };
fs.writeFileSync(path.join(destination, inspectOnly ? 'inspection-result.json' : 'result.json'), JSON.stringify(result, null, 2));
fs.writeFileSync(path.join(taskDirectory, `${label}-result.json`), JSON.stringify(result, null, 2));
if (label === 'baseline') {
  assert(byName['dark-zero-emission'].lwMax > 10, 'Baseline must reproduce the erroneous opaque medium surface source');
  assert(!result.passed, 'Baseline must fail the physics contract');
  console.log(JSON.stringify({ expectedFailureReproduced: true, failedChecks: checks.filter(c => !c.passed).map(c => c.name) }));
} else {
  console.log(JSON.stringify({ passed: result.passed, destination, checks }));
  process.exit(result.passed ? 0 : 1);
}
