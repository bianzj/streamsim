// Real Vulkan regression for truncated diffuse paths. Run serially after build.
// --prepare-only writes fixtures without starting the engine/GPU.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { runEnergyEngineRegression } from '../tests/energy_core/engine.mjs';
import { normalizeProject, validateProject } from '../src/renderer/src/project-schema.js';

const root = path.resolve(import.meta.dirname, '..');
const prepared = runEnergyEngineRegression(root, { prepareOnly: true });
const template = JSON.parse(fs.readFileSync(prepared.cases[0].project, 'utf8'));
const destination = path.join(root, '.test', 'sky-truncation', String(Date.now()));
const executable = path.resolve(process.env.STREAMSIM_ENGINE || path.join(root, 'models/bin_x64/Release/histream.exe'));
fs.mkdirSync(destination, { recursive: true });

const inputs = [
  { name: 'sky-cap1', steps: 1, rin: 700, rli: 350 },
  { name: 'sky-cap256', steps: 256, rin: 700, rli: 350 },
  { name: 'dark-cap256', steps: 256, rin: 0, rli: 0 },
].map(specification => {
  const directory = path.join(destination, specification.name);
  fs.mkdirSync(directory, { recursive: true });
  const project = structuredClone(template);
  const c = project.configuration;
  Object.assign(c.scene, { height: 4 });
  c.scene.background.angularEffectStrength = 0;
  c.outDir = path.join(directory, 'output');
  Object.assign(c.sensor, { image: false, process: true, energyProcess: true, radiationProcess: true });
  Object.assign(c.control, { radiationSolver: 'traditional', shortwaveScatteringOrders: 1,
    radiationMaxSteps: specification.steps, couplingIterations: 2,
    vegetationTemperatureMethod: 0, soilTemperatureMethod: 2 });
  Object.assign(c.light, { direct: 0, diffuse: 1 });
  Object.assign(c.meteo, { start: 0, end: 1, path: path.join(directory, 'meteo.txt') });
  fs.writeFileSync(c.meteo.path, `1\n214.5 25 15 1013 2 ${specification.rin} ${specification.rli}\n`);

  // Pure horizontal ground receives the sky. The three synthetic OBJ layers
  // sit entirely above the state domain: Scene retains their TLAS integration
  // geometry but clips every corresponding NanoVDB/energy state. Thus these
  // hits continue tracing without a scattering or thermal source. They are
  // test instrumentation, not physical buildings or a participating medium.
  // OBJ voxelization makes each layer a closed one-voxel shell. cap=1 permits
  // two hits in the legacy <=maxStep loop, then leaves an unresolved path;
  // cap=256 traverses all layers and reaches an actual sky miss.
  // Ground thermal emission is downward of the receiving upper hemisphere.
  for(const spectrum of c.spectra) {
    spectrum.reflectance = '0';
    spectrum.transmittance = '0';
    spectrum.refTir = 0.05;
    spectrum.tauTir = 0;
  }
  const ghostName = 'synthetic_continuation_planes';
  const planeHeights = [10, 12, 14];
  assert(planeHeights.every(height => height > c.scene.height));
  const objFile = path.join(directory, 'synthetic_continuation_planes.obj');
  const positionsFile = path.join(directory, 'synthetic_continuation_planes_position.txt');
  const margin = 4;
  const x0 = -margin, x1 = c.scene.x + margin;
  const z0 = -margin, z1 = c.scene.y + margin;
  const objLines = ['# Synthetic TLAS-only continuation layers; no physical surface states.', `o ${ghostName}`];
  for(const [index, height] of planeHeights.entries()) {
    objLines.push(`v ${x0} ${height} ${z0}`, `v ${x1} ${height} ${z0}`,
      `v ${x1} ${height} ${z1}`, `v ${x0} ${height} ${z1}`);
    const first = index*4 + 1;
    objLines.push(`f ${first} ${first + 1} ${first + 2}`, `f ${first} ${first + 2} ${first + 3}`);
  }
  fs.writeFileSync(objFile, objLines.join('\n') + '\n');
  fs.writeFileSync(positionsFile, '0 0 0 1 0\n');
  c.objects.items = [{ name: ghostName, type: 'Building', materialName: 'test_building',
    spectralName: 'solid_optics', canopyName: 'rigid_body', thermalName: 'soil_temperature',
    angularEffectStrength: 0, fileName: objFile, positionFile: positionsFile,
    shape: 'cube', dimensions: [x1 - x0, 1, z1 - z0], instanceCount: 1,
    meshes: [{ name: ghostName, materialName: 'test_building', spectralName: 'solid_optics',
      canopyName: 'rigid_body', thermalName: 'soil_temperature', angularEffectStrength: 0 }] }];
  c.objects.count = 1;
  c.objects.names = [ghostName];
  const normalized = normalizeProject(project);
  assert(validateProject(normalized).valid, JSON.stringify(validateProject(normalized).errors));
  const input = path.join(directory, 'project.json');
  fs.writeFileSync(input, JSON.stringify(normalized, null, 2));
  const expectedStateCount = c.scene.x*c.scene.y/(c.scene.voxel*c.scene.voxel);
  assert(Number.isInteger(expectedStateCount) && expectedStateCount > 0,
    'Fixture requires an integral horizontal ground grid');
  return { ...specification, directory, input, outDir: normalized.configuration.outDir,
    fixture: { kind: 'synthetic TLAS-only continuation planes', domainHeight: c.scene.height,
      planeHeights, objSha256: createHash('sha256').update(fs.readFileSync(objFile)).digest('hex'),
      expectedStateCount } };
});
fs.writeFileSync(path.join(destination, 'inputs.json'), JSON.stringify(inputs, null, 2));
if(process.argv.includes('--prepare-only')) {
  console.log(JSON.stringify({ prepared: true, destination, executable, inputs }, null, 2));
  process.exit(0);
}

function readRadiation(directory) {
  const files = fs.readdirSync(directory).filter(file => /^voxelrt_T=.*\.json$/.test(file));
  assert.equal(files.length, 1, `Expected one radiation node in ${directory}`);
  const metadata = JSON.parse(fs.readFileSync(path.join(directory, files[0]), 'utf8'));
  const binary = fs.readFileSync(path.join(directory, metadata.dataFile));
  const links = fs.readFileSync(path.join(directory, metadata.sceneLinks.dataFile));
  assert.equal(binary.length, metadata.voxelCount*metadata.recordFloats*4);
  assert.equal(links.length, metadata.voxelCount*metadata.sceneLinks.recordInts*4);
  const typeOffset = metadata.sceneLinks.fields.indexOf('type');
  assert(typeOffset >= 0, 'Missing shader material type in scene links');
  const fieldOffsets = Object.fromEntries(metadata.fields.map(field => [field.id, field.offset]));
  const rows = [];
  for(let voxel = 0; voxel < metadata.voxelCount; ++voxel) {
    assert.equal(links.readInt32LE((voxel*metadata.sceneLinks.recordInts + typeOffset)*4), 1,
      'Only receiving ground may have a physical state; continuation layers must be clipped');
    const record = voxel*metadata.recordFloats;
    const position = metadata.positionOffsets.map(offset => binary.readFloatLE((record + offset)*4));
    const values = {};
    for(const field of metadata.fields) {
      const value = binary.readFloatLE((record + field.offset)*4);
      assert(Number.isFinite(value), `Nonfinite ${field.id}, voxel ${voxel}`);
      values[field.id] = value;
    }
    for(const field of ['shortwaveRadiation', 'longwaveRadiation']) {
      assert(fieldOffsets[field] !== undefined, `Missing ${field}`);
      assert(values[field] >= 0, `Negative ${field}, voxel ${voxel}`);
    }
    rows.push({ position, values });
  }
  return { metadata, linksSha256: createHash('sha256').update(links).digest('hex'), rows };
}

const cases = [];
const failures = [];
for(const input of inputs) {
  try {
    console.log(`START ${input.name}`);
    const started = Date.now();
    const env = { ...process.env };
    delete env.STREAMSIM_NEW_SCATTERING;
    const run = spawnSync(executable, ['eVoxelEB', input.input], {
      cwd: path.dirname(executable), encoding: 'utf8', windowsHide: true,
      timeout: 180000, maxBuffer: 32*1024*1024, env,
    });
    fs.writeFileSync(path.join(input.directory, 'run.log'), `${run.stdout || ''}\n${run.stderr || ''}`);
    assert(!run.error, String(run.error));
    assert.equal(run.status, 0, `${input.name} engine failed; see run.log`);
    const diagnosticDirectory = path.join(input.outDir, 'diagnostics');
    const scatteringFiles = fs.readdirSync(diagnosticDirectory).filter(file => /^scattering_node=.*\.json$/.test(file));
    assert.equal(scatteringFiles.length, 1);
    const scattering = JSON.parse(fs.readFileSync(path.join(diagnosticDirectory, scatteringFiles[0]), 'utf8'));
    assert.equal(scattering.legacy, true, 'This fixture must exercise the default single-scattering shader');
    assert.equal(scattering.completedOrders, 1);
    const radiation = readRadiation(path.join(input.outDir, 'process'));
    assert.equal(radiation.metadata.voxelCount, input.fixture.expectedStateCount,
      'The synthetic continuation geometry must create no extra energy states');
    const clippedPlacement = String(run.stdout).match(/OBJ placement 0[^\n]*inserted=(\d+)[^\n]*clipped=(\d+)/);
    assert(clippedPlacement, 'Missing continuation layer clipping diagnostic');
    assert.equal(Number(clippedPlacement[1]), 0, 'Continuation layers acquired physical states');
    assert(Number(clippedPlacement[2]) > 0, 'No out-of-domain continuation layer states were clipped');
    cases.push({ name: input.name, passed: true, elapsedMs: Date.now() - started,
      fixture: input.fixture, clippedSyntheticStates: Number(clippedPlacement[2]), ...radiation });
    console.log(`PASS ${input.name}`);
  } catch(error) { failures.push({ name: input.name, error: error.stack || String(error) }); }
}

const checks = [];
function check(name, callback) {
  try { checks.push({ name, passed: true, ...callback() }); }
  catch(error) { checks.push({ name, passed: false, error: error.stack || String(error) }); }
}
if(failures.length === 0) {
  const byName = Object.fromEntries(cases.map(result => [result.name, result]));
  const low = byName['sky-cap1'];
  const full = byName['sky-cap256'];
  check('same frozen voxel geometry', () => {
    assert.equal(low.linksSha256, full.linksSha256);
    assert.equal(low.rows.length, full.rows.length);
    low.rows.forEach((row, voxel) => assert.deepEqual(row.position, full.rows[voxel].position));
    return { voxelCount: low.rows.length };
  });
  for(const field of ['shortwaveRadiation', 'longwaveRadiation']) {
    check(`a step cap cannot invent ${field}`, () => {
      let reducedVoxels = 0, maxReduction = 0, maxIncrease = 0;
      let lowSum = 0, fullSum = 0;
      for(let voxel = 0; voxel < low.rows.length; ++voxel) {
        const a = low.rows[voxel].values[field];
        const b = full.rows[voxel].values[field];
        const tolerance = 2e-4 + 2e-5*Math.max(a, b);
        assert(a <= b + tolerance, `${field}: capped path added radiation, voxel ${voxel}: ${a} > ${b}`);
        if(b - a > tolerance) ++reducedVoxels;
        maxReduction = Math.max(maxReduction, b - a);
        maxIncrease = Math.max(maxIncrease, a - b);
        lowSum += a;
        fullSum += b;
      }
      assert(reducedVoxels > 0, `${field}: fixture did not exercise an unresolved path`);
      return { reducedVoxels, maxReduction, maxIncrease, lowSum, fullSum,
        sumConvention: 'sum of per-voxel flux densities, not total domain power' };
    });
    check(`without external illumination ${field} is zero`, () => {
      const maximum = Math.max(...byName['dark-cap256'].rows.map(row => row.values[field]));
      assert(maximum <= 1e-5, `${field}: unexpected internal emission or illumination ${maximum}`);
      return { maximum };
    });
  }
}
const result = { passed: failures.length === 0 && checks.every(check => check.passed),
  destination, executable, engineSha256: createHash('sha256').update(fs.readFileSync(executable)).digest('hex'),
  scope: 'default one-order diffuse VNIR and common diffuse TIR actual-miss boundary; synthetic TLAS-only continuation geometry, not medium transport or building physics',
  cases: cases.map(({ rows, ...record }) => record), checks, failures };
fs.writeFileSync(path.join(destination, 'result.json'), JSON.stringify(result, null, 2));
console.log(JSON.stringify(result, null, 2));
process.exit(result.passed ? 0 : 1);
