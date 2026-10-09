// Serial, real Vulkan fixtures. Run only after building the current engine.
// node tools/test-scattering-core.mjs --prepare-only creates inputs without GPU work.
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
const destination = path.join(root, '.test', 'scattering-core', String(Date.now()));
const executable = path.resolve(process.env.STREAMSIM_ENGINE || path.join(root, 'models/bin_x64/Release/histream.exe'));
const prepareOnly = process.argv.includes('--prepare-only');
const selectedCase = process.argv.find(argument => argument.startsWith('--case='))?.slice('--case='.length);
const additional = process.argv.includes('--additional');
fs.mkdirSync(destination, { recursive: true });

const specifications = [
  { name: 'black-new1', orders: 1, black: true },
  { name: 'black-new3', orders: 3, black: true },
  { name: 'night-new3', orders: 3, night: true },
  { name: 'mixed-rin350-new3', orders: 3, rin: 350 },
  { name: 'mixed-rin700-new3', orders: 3, rin: 700 },
  { name: 'mixed-rin700-repeat-new3', orders: 3, rin: 700 },
  { name: 'sun-only-new3', orders: 3, direct: 1 },
  { name: 'sky-only-new3', orders: 3, direct: 0 },
  { name: 'leaf-fog-solid-water-new3', orders: 3, medium: true },
  { name: 'reflective-new1', orders: 1, reflective: true },
  { name: 'reflective-new3', orders: 3, reflective: true },
];
const additionalSpecifications = [
  { name: 'maxstep1-new3', orders: 3, raySteps: 1, expectStepTruncation: true },
  { name: 'maxstep64-new3', orders: 3, raySteps: 64 },
  { name: 'dense-canopy-new3', orders: 3, raySteps: 64, denseCanopy: true },
  { name: 'narrow8-night-new3', orders: 3, width: 8, night: true },
  { name: 'narrow8-day-new3', orders: 3, width: 8 },
];

function prepare(specification) {
  const directory = path.join(destination, specification.name);
  fs.mkdirSync(directory, { recursive: true });
  const project = structuredClone(template);
  const c = project.configuration;
  c.outDir = path.join(directory, 'output');
  Object.assign(c.sensor, { image: false, process: true, energyProcess: true,
    radiationProcess: true, x: 16, y: 16, bands: '550 10500' });
  Object.assign(c.control, { shortwaveScatteringOrders: specification.orders,
    spectralAccelerationWidth: specification.width ?? 100,
    radiationMaxSteps: specification.raySteps ?? 256,
    vegetationTemperatureMethod: 1, soilTemperatureMethod: 2 });
  Object.assign(c.meteo, { start: 0, end: 1, dTime: 1800,
    path: path.join(directory, 'meteo.txt') });
  c.light.direct = specification.direct ?? 0.8;
  c.light.diffuse = 1 - c.light.direct;
  const rin = specification.night ? 0 : specification.rin ?? 700;
  fs.writeFileSync(c.meteo.path,
    `1\n${specification.night ? '214.0' : '214.5'} 25 15 1013 2 ${rin} 350\n`);
  if(specification.black) {
    for(const material of c.spectra) {
      material.reflectance = '0';
      material.transmittance = '0';
    }
    // A Fresnel water BRDF can reflect even when its diffuse spectrum is zero.
    for(const material of c.materials) material.params.brdfModel = 0;
    for(const object of c.objects.items) {
      object.angularEffectStrength = 0;
      for(const mesh of object.meshes) mesh.angularEffectStrength = 0;
    }
  }
  if(specification.reflective) {
    for(const material of c.spectra) {
      material.reflectance = material.name === 'leaf_optics' ? '0.35' : '0.6';
      material.transmittance = material.name === 'leaf_optics' ? '0.25' : '0';
    }
  }
  if(specification.denseCanopy)
    c.canopies.find(canopy => canopy.name === 'canopy_default').density = 25;
  if(specification.medium) {
    c.canopies.push({ ...structuredClone(c.canopies[0]), name: 'test_fog',
      structureType: 'fog', density: 0, extinction: 0.08,
      scatteringAlbedo: 0.95, asymmetry: 0.5, fixedTemperature: 288 });
    const fog = structuredClone(c.objects.items.find(item => item.type === 'Vegetation'));
    fog.name = 'test_fog_object';
    fog.canopyName = 'test_fog';
    fog.positionFile = path.join(directory, 'fog_position.txt');
    fs.writeFileSync(fog.positionFile, '6 1 0 1 0\n');
    fog.meshes = fog.meshes.map(mesh => ({ ...mesh, name: 'fog', canopyName: 'test_fog' }));
    c.objects.items.push(fog);
    c.objects.count = c.objects.items.length;
    c.objects.names = c.objects.items.map(item => item.name);
  }
  const normalized = normalizeProject(project);
  const validation = validateProject(normalized);
  assert(validation.valid, JSON.stringify(validation.errors));
  const input = path.join(directory, 'project.json');
  fs.writeFileSync(input, JSON.stringify(normalized, null, 2));
  return { ...specification, directory, input, outDir: normalized.configuration.outDir };
}

const allSpecifications = [...specifications, ...additionalSpecifications];
assert(!selectedCase || allSpecifications.some(specification => specification.name === selectedCase), 'Unknown --case');
const selectedSpecifications = selectedCase ? allSpecifications.filter(specification => specification.name === selectedCase)
  : additional ? additionalSpecifications : specifications;
const inputs = selectedSpecifications.map(prepare);
fs.writeFileSync(path.join(destination, 'inputs.json'), JSON.stringify(inputs, null, 2));
if(prepareOnly) {
  console.log(JSON.stringify({ prepared: true, destination, executable, inputs }, null, 2));
  process.exit(0);
}

function onlyMetadata(directory, pattern) {
  const names = fs.readdirSync(directory).filter(name => pattern.test(name));
  assert.equal(names.length, 1, `Expected one metadata file in ${directory}: ${names}`);
  const file = path.join(directory, names[0]);
  return { file, metadata: JSON.parse(fs.readFileSync(file, 'utf8')) };
}

function readProcess(directory, pattern) {
  const { file, metadata } = onlyMetadata(directory, pattern);
  const binary = fs.readFileSync(path.join(directory, metadata.dataFile));
  assert.equal(binary.length, metadata.voxelCount * metadata.recordFloats * 4);
  const links = fs.readFileSync(path.join(directory, metadata.sceneLinks.dataFile));
  const typeOffset = metadata.sceneLinks.fields.indexOf('type');
  assert(typeOffset >= 0);
  const values = [];
  const fieldSums = Object.fromEntries(metadata.fields.map(field => [field.id, 0]));
  const typeCounts = {};
  for(let voxel = 0; voxel < metadata.voxelCount; ++voxel) {
    const type = links.readInt32LE((voxel*metadata.sceneLinks.recordInts + typeOffset)*4);
    typeCounts[type] = (typeCounts[type] || 0) + 1;
    for(const field of metadata.fields) {
      const value = binary.readFloatLE((voxel*metadata.recordFloats + field.offset)*4);
      assert(Number.isFinite(value), `${file}: nonfinite ${field.id}, voxel=${voxel}, type=${type}`);
      if(field.id === 'gpp') assert(value >= 0, `${file}: negative GPP`);
      if(type !== 2 && ['gpp', 'npp'].includes(field.id))
        assert.equal(value, 0, `${file}: nonvegetation ${field.id}`);
      values.push(value);
      fieldSums[field.id] += value;
    }
  }
  return { file, voxelCount: metadata.voxelCount, fields: metadata.fields.map(field => field.id),
    typeCounts, values, fieldSums };
}

function run(input) {
  console.log(`START ${input.name}`);
  const started = Date.now();
  const execution = spawnSync(executable, ['eVoxelEB', input.input], {
    cwd: path.dirname(executable), encoding: 'utf8', windowsHide: true,
    timeout: 180000, maxBuffer: 32*1024*1024,
    env: { ...process.env, STREAMSIM_NEW_SCATTERING: '1' },
  });
  fs.writeFileSync(path.join(input.directory, 'run.log'),
    `${execution.stdout || ''}\n${execution.stderr || ''}`);
  assert(!execution.error, `${input.name}: ${execution.error}`);
  assert.equal(execution.status, 0, `${input.name}: engine failed; ${input.directory}/run.log`);
  const diagnosticsDirectory = path.join(input.outDir, 'diagnostics');
  const { file: scatteringFile, metadata: scattering } = onlyMetadata(diagnosticsDirectory, /^scattering_node=.*\.json$/);
  assert.equal(scattering.legacy, false);
  assert.equal(scattering.requestedOrders, input.orders);
  assert.equal(scattering.completedOrders, input.orders);
  assert.equal(scattering.orderContributionEarlyStop, false);
  assert.equal(scattering.valid, true);
  assert.match(scattering.orderDefinition, /sky orders 0\.\.K plus solar orders 1\.\.K/);
  const orders = Object.fromEntries(scattering.perOrder.map(order => [order.order, order]));
  for(const order of scattering.perOrder) {
    assert.equal(order.invalidValues, 0, `${input.name}: invalid order=${order.order}`);
    for(const metric of ['absorbedShortwave', 'absorbedPar', 'truncatedResidual'])
      assert(Number.isFinite(order[metric]) && order[metric] >= 0, `${input.name}: ${metric}`);
    if(!input.expectStepTruncation)
      assert.equal(order.maxStepTruncations, 0, `${input.name}: incomplete geometric traversal`);
    if(order.order === 0 || order.order <= input.orders) assert(order.tracedRays > 0);
  }
  const { file: physiologyFile, metadata: physiology } = onlyMetadata(diagnosticsDirectory, /^physiology_node=.*\.json$/);
  assert.equal(physiology.valid, true);
  assert.equal(physiology.invalidVoxelCount, 0);
  for(const field of physiology.fields) assert.equal(field.invalidCount, 0);
  const physiological = Object.fromEntries(physiology.fields.map(field => [field.id, field]));
  const processDirectory = path.join(input.outDir, 'process');
  const energy = readProcess(processDirectory, /^voxeleb_T=.*\.json$/);
  const radiation = readProcess(processDirectory, /^voxelrt_T=.*\.json$/);
  for(const type of [1, 2, 3, 4]) assert(energy.typeCounts[type] > 0, `${input.name}: missing material type=${type}`);
  const result = { name: input.name, passed: true, orders: input.orders,
    elapsedMs: Date.now() - started, scatteringFile, physiologyFile,
    scattering, physiological, energy, radiation };
  console.log(`PASS ${input.name} ${result.elapsedMs} ms`);
  return result;
}

const completed = [];
const failures = [];
for(const input of inputs) {
  try { completed.push(run(input)); }
  catch(error) {
    failures.push({ name: input.name, error: error.stack || String(error), directory: input.directory });
    console.error(`FAIL ${input.name}: ${error.message}`);
  }
}
if(failures.length || selectedCase) {
  const partial = { passed: failures.length === 0, destination, executable, failures,
    engineSha256: createHash('sha256').update(fs.readFileSync(executable)).digest('hex'),
    cases: completed.map(({ energy, radiation, ...rest }) => ({ ...rest,
      energy: { ...energy, values: undefined }, radiation: { ...radiation, values: undefined } })) };
  fs.writeFileSync(path.join(destination, 'result.json'), JSON.stringify(partial, null, 2));
  console.log(JSON.stringify({ passed: partial.passed, destination, failures }, null, 2));
  process.exit(failures.length ? 1 : 0);
}
const byName = Object.fromEntries(completed.map(result => [result.name, result]));
const checks = [];

function recordCheck(name, callback) {
  try {
    const detail = callback();
    checks.push({ name, passed: true, ...detail });
  } catch(error) {
    checks.push({ name, passed: false, error: error.stack || String(error) });
    console.error(`FAIL CHECK ${name}: ${error.message}`);
  }
}

function close(actual, expected, relative = 1e-4, absolute = 1e-5) {
  assert(Math.abs(actual - expected) <= absolute + relative*Math.max(Math.abs(actual), Math.abs(expected)),
    `Expected ${actual} ~ ${expected}`);
}

if(!additional) {
recordCheck('zero albedo annihilates every scattered source', () => {
  for(const name of ['black-new1', 'black-new3']) {
    for(const order of byName[name].scattering.perOrder.filter(order => order.order > 0)) {
      assert.equal(order.absorbedShortwave, 0);
      assert.equal(order.absorbedPar, 0);
    }
  }
  close(byName['black-new1'].radiation.fieldSums.shortwaveRadiation,
    byName['black-new3'].radiation.fieldSums.shortwaveRadiation, 0, 0);
  return { exactZero: true };
});

recordCheck('zero illumination produces zero PAR, absorbed shortwave and GPP', () => {
  const result = byName['night-new3'];
  assert.equal(result.radiation.fieldSums.shortwaveRadiation, 0);
  assert.equal(result.energy.fieldSums.gpp, 0);
  for(const metric of ['directPar', 'diffusePar', 'gppSunlit', 'gppShaded'])
    assert.equal(result.physiological[metric].max, 0);
  for(const order of result.scattering.perOrder) {
    assert.equal(order.absorbedShortwave, 0);
    assert.equal(order.absorbedPar, 0);
  }
  return { exactZero: true };
});

recordCheck('linear radiative transport under a doubled illumination', () => {
  const low = byName['mixed-rin350-new3'];
  const high = byName['mixed-rin700-new3'];
  close(high.radiation.fieldSums.shortwaveRadiation, 2*low.radiation.fieldSums.shortwaveRadiation);
  for(let index = 0; index < high.scattering.perOrder.length; ++index) {
    for(const metric of ['absorbedShortwave', 'absorbedPar'])
      close(high.scattering.perOrder[index][metric], 2*low.scattering.perOrder[index][metric]);
  }
  for(const metric of ['directPar', 'diffusePar'])
    close(high.physiological[metric].mean, 2*low.physiological[metric].mean);
  return { relativeTolerance: 1e-4, shortwaveRatio:
    high.radiation.fieldSums.shortwaveRadiation/low.radiation.fieldSums.shortwaveRadiation };
});

recordCheck('identical inputs preserve per-voxel physical fields', () => {
  const first = byName['mixed-rin700-new3'];
  const repeat = byName['mixed-rin700-repeat-new3'];
  let maximumAbsoluteDifference = 0;
  for(const product of ['energy', 'radiation']) {
    assert.deepEqual(first[product].fields, repeat[product].fields);
    assert.equal(first[product].values.length, repeat[product].values.length);
    for(let index = 0; index < first[product].values.length; ++index) {
      close(first[product].values[index], repeat[product].values[index], 2e-6, 1e-5);
      maximumAbsoluteDifference = Math.max(maximumAbsoluteDifference,
        Math.abs(first[product].values[index] - repeat[product].values[index]));
    }
  }
  return { maximumAbsoluteDifference, relativeTolerance: 2e-6, absoluteTolerance: 1e-5 };
});

recordCheck('separate Sun and sky primary sources reach all three orders', () => {
  const sun = byName['sun-only-new3'];
  const sky = byName['sky-only-new3'];
  const sunOrders = Object.fromEntries(sun.scattering.perOrder.map(order => [order.order, order]));
  const skyOrders = Object.fromEntries(sky.scattering.perOrder.map(order => [order.order, order]));
  assert.equal(sunOrders[0].absorbedShortwave, 0, 'Sun-only must not inject a sky boundary');
  assert.equal(sunOrders[0].absorbedPar, 0);
  assert.equal(sky.physiological.directPar.max, 0, 'Sky-only must not inject direct Sun');
  assert(skyOrders[0].absorbedShortwave > 0);
  for(const order of [1, 2, 3]) {
    assert(sunOrders[order].absorbedShortwave > 0, `Sun order=${order}`);
    assert(skyOrders[order].absorbedShortwave > 0, `Sky order=${order}`);
  }
  return { sunPerOrder: sun.scattering.perOrder.map(order => order.absorbedShortwave),
    skyPerOrder: sky.scattering.perOrder.map(order => order.absorbedShortwave) };
});

recordCheck('additional scattering increments increase cumulative absorption', () => {
  const single = byName['reflective-new1'];
  const third = byName['reflective-new3'];
  const orders = Object.fromEntries(third.scattering.perOrder.map(order => [order.order, order]));
  assert(orders[2].absorbedShortwave > 0 && orders[3].absorbedShortwave > 0);
  assert(orders[2].absorbedPar > 0 && orders[3].absorbedPar > 0);
  assert(third.radiation.fieldSums.shortwaveRadiation > single.radiation.fieldSums.shortwaveRadiation);
  close(single.scattering.perOrder.find(order => order.order === 1).absorbedShortwave,
    orders[1].absorbedShortwave);
  close(third.radiation.fieldSums.shortwaveRadiation - single.radiation.fieldSums.shortwaveRadiation,
    orders[2].absorbedShortwave + orders[3].absorbedShortwave, 2e-4);
  return { higherOrderIncrement: orders[2].absorbedShortwave + orders[3].absorbedShortwave,
    shortwaveRatio: third.radiation.fieldSums.shortwaveRadiation/single.radiation.fieldSums.shortwaveRadiation };
});

recordCheck('leaf, participating medium, building and water coexist without invalid state', () => {
  const result = byName['leaf-fog-solid-water-new3'];
  assert(result.energy.typeCounts[2] > byName['mixed-rin700-new3'].energy.typeCounts[2], 'Fog object missing');
  assert.equal(result.scattering.valid, true);
  assert.equal(result.physiological.gppSunlit.invalidCount, 0);
  return { typeCounts: result.energy.typeCounts, invalidVoxelCount: 0 };
});
} else {
  recordCheck('a geometric cap reports unresolved rays without asserting an energy-error bound', () => {
    const capped = byName['maxstep1-new3'];
    const reference = byName['maxstep64-new3'];
    const count = capped.scattering.perOrder.reduce((sum, order) => sum + order.maxStepTruncations, 0);
    const residual = capped.scattering.perOrder.reduce((sum, order) => sum + order.truncatedResidual, 0);
    assert(count > 0, 'One-step fixture must exercise the cap diagnostic');
    assert(residual > 0, 'Truncated rays must report their remaining throughput');
    assert.equal(reference.scattering.perOrder.reduce((sum, order) => sum + order.maxStepTruncations, 0), 0);
    return { oneStepTruncations: count, summedDimensionlessThroughput: residual,
      shortwaveAtOneStep: capped.radiation.fieldSums.shortwaveRadiation,
      shortwaveAt64Steps: reference.radiation.fieldSums.shortwaveRadiation,
      scope: 'Step-count sensitivity, not an omitted-energy estimate or error bound' };
  });
  recordCheck('an optically dense canopy exercises transmittance early termination', () => {
    const dense = byName['dense-canopy-new3'];
    const count = dense.scattering.perOrder.reduce((sum, order) => sum + order.transmittanceStops, 0);
    const residual = dense.scattering.perOrder.reduce((sum, order) => sum + order.transmittanceResidual, 0);
    assert(count > 0, 'Dense fixture must exercise the throughput cutoff');
    assert(residual > 0 && residual <= 0.01*count, 'Cutoff residual must remain below its configured throughput threshold');
    return { transmittanceStops: count, summedDimensionlessThroughput: residual,
      maximumResidualPerStoppedRay: 0.01 };
  });
  recordCheck('narrow spectral groups retain exact zero-light shortwave, PAR and GPP', () => {
    const night = byName['narrow8-night-new3'];
    assert.equal(night.radiation.fieldSums.shortwaveRadiation, 0);
    assert.equal(night.energy.fieldSums.gpp, 0);
    for(const metric of ['directPar', 'diffusePar', 'gppSunlit', 'gppShaded'])
      assert.equal(night.physiological[metric].max, 0);
    for(const order of night.scattering.perOrder) {
      assert.equal(order.absorbedShortwave, 0);
      assert.equal(order.absorbedPar, 0);
    }
    return { widthNm: 8, exactZero: true };
  });
  recordCheck('spectrally constant optics preserve broadband transport when group width changes', () => {
    const narrow = byName['narrow8-day-new3'];
    const wide = byName['maxstep64-new3'];
    close(narrow.radiation.fieldSums.shortwaveRadiation, wide.radiation.fieldSums.shortwaveRadiation, 2e-4);
    for(let index = 0; index < narrow.scattering.perOrder.length; ++index)
      close(narrow.scattering.perOrder[index].absorbedShortwave,
        wide.scattering.perOrder[index].absorbedShortwave, 2e-4);
    return { narrowWidthNm: 8, wideWidthNm: 100, relativeTolerance: 2e-4,
      shortwaveRatio: narrow.radiation.fieldSums.shortwaveRadiation/wide.radiation.fieldSums.shortwaveRadiation,
      parRatio: narrow.physiological.diffusePar.mean/wide.physiological.diffusePar.mean,
      scope: 'Broadband invariance for constant custom spectra; PAR wavelength-quadrature accuracy is observed separately' };
  });
}

const result = { passed: checks.every(check => check.passed), destination, executable,
  engineSha256: createHash('sha256').update(fs.readFileSync(executable)).digest('hex'),
  scope: 'synthetic serial GPU fixtures; radiative implementation regression, not observational accuracy or whole-domain energy closure',
  checks, cases: completed.map(({ energy, radiation, physiological, ...rest }) => ({ ...rest,
    energy: { ...energy, values: undefined }, radiation: { ...radiation, values: undefined },
    physiological })) };
fs.writeFileSync(path.join(destination, 'result.json'), JSON.stringify(result, null, 2));
console.log(JSON.stringify({ passed: result.passed, destination, checks }, null, 2));
if(!result.passed) process.exitCode = 1;
