// Real example geometry; paired orders use identical frozen inputs and run serially.
// Preparation is the default. GPU execution requires --run --plan=<manifest.json>.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { spawn } from 'node:child_process';
import { prepareRuntimeSceneProject } from './runtime-scene.mjs';

const root = path.resolve(import.meta.dirname, '..');
const destination = path.join(root, '.test/scattering-20261004');
const argumentsMap = Object.fromEntries(process.argv.slice(2).map(argument => {
  const [key, ...value] = argument.replace(/^--/, '').split('=');
  return [key, value.length ? value.join('=') : true];
}));
const executable = path.resolve(argumentsMap.engine || process.env.STREAMSIM_ENGINE ||
  path.join(root, 'models/bin_x64/Release/histream.exe'));
const hashFile = file => createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const json = (file, value) => fs.writeFileSync(file, JSON.stringify(value, null, 2) + '\n');
const readJson = file => JSON.parse(fs.readFileSync(file, 'utf8'));
const fileIdentity = file => ({ path: path.resolve(file), size: fs.statSync(file).size, sha256: hashFile(file) });
const exampleNames = ['auto', 'forest', 'beijing', 'photovoltaic', 'ship'];
fs.mkdirSync(destination, { recursive: true });

function prepare() {
  const profile = String(argumentsMap.profile || 'day');
  assert(['day', 'diurnal', 'full'].includes(profile), 'Profile must be day, diurnal, or full');
  const selected = String(argumentsMap.case || (profile === 'diurnal' ? 'auto' : exampleNames.join(','))).split(',');
  const orders = String(argumentsMap.orders || '1,3').split(',').map(Number);
  assert(orders.length && orders.every(value => [1, 2, 3].includes(value)), 'Orders must be 1, 2, or 3');
  const benchmarkDirectory = path.join(destination, `benchmark-${Date.now()}`);
  fs.mkdirSync(benchmarkDirectory, { recursive: true });
  const plans = [];
  for (const name of selected) {
    assert(exampleNames.includes(name), `Unknown example: ${name}`);
    const sourceDirectory = path.resolve(argumentsMap.examples || 'D:/examples', name);
    const originalPath = path.join(sourceDirectory, 'project.json');
    const original = readJson(originalPath);
    const originalIdentity = fileIdentity(originalPath);
    const caseDirectory = path.join(benchmarkDirectory, `${name}-${profile}`);
    const frozenDirectory = path.join(caseDirectory, 'source');
    fs.mkdirSync(frozenDirectory, { recursive: true });
    fs.copyFileSync(originalPath, path.join(frozenDirectory, 'original-project.json'));
    const project = structuredClone(original);
    const c = project.configuration;
    const assets = [];
    const copied = new Map();
    function freezeAsset(value, fallbackDirectory = sourceDirectory, required = true) {
      if (!value) return value;
      const source = path.isAbsolute(value) ? path.resolve(value) : path.resolve(fallbackDirectory, value);
      if (!fs.existsSync(source)) {
        if (required) throw new Error(`Missing source asset: ${source}`);
        return value;
      }
      if (copied.has(source)) return copied.get(source);
      const snapshot = path.join(frozenDirectory, `asset-${assets.length}-${path.basename(source)}`);
      fs.copyFileSync(source, snapshot);
      const identity = fileIdentity(source);
      assert.equal(identity.sha256, hashFile(snapshot), `Snapshot mismatch: ${source}`);
      assets.push({ ...identity, snapshot });
      copied.set(source, snapshot);
      return snapshot;
    }
    for (const item of c.objects.items) {
      item.fileName = freezeAsset(item.fileName);
      item.positionFile = freezeAsset(item.positionFile);
    }
    if (c.scene.demFile) c.scene.demFile = freezeAsset(c.scene.demFile);
    for (const spectrum of c.spectra || []) {
      if (spectrum.fileName) spectrum.fileName = freezeAsset(spectrum.fileName);
      if (spectrum.physicalTexture?.enabled && spectrum.physicalTexture.fileName) {
        spectrum.physicalTexture.fileName = freezeAsset(spectrum.physicalTexture.fileName,
          fs.existsSync(path.resolve(sourceDirectory, spectrum.physicalTexture.fileName)) ? sourceDirectory : root);
      }
    }
    if (c.atmosphere?.enabled && c.atmosphere.lutFile) {
      c.atmosphere.lutFile = freezeAsset(c.atmosphere.lutFile,
        fs.existsSync(path.resolve(sourceDirectory, c.atmosphere.lutFile)) ? sourceDirectory : root);
    }
    const internalMeteo = !c.meteo.path || ['defined/meteo.txt', 'HiStream 内置气象数据'].includes(c.meteo.path);
    const sourceMeteo = internalMeteo ? path.join(path.dirname(executable), 'defined/meteo.txt') :
      path.resolve(sourceDirectory, c.meteo.path);
    c.meteo.path = freezeAsset(sourceMeteo);
    const meteoRows = fs.readFileSync(c.meteo.path, 'utf8').trim().split(/\r?\n/).slice(1)
      .filter(line => line.trim()).map(line => line.trim().split(/\s+/).map(Number));
    assert(meteoRows.length && meteoRows.every(row => row.length >= 7 && row.every(Number.isFinite)), 'Invalid meteorology');
    const firstDay = Math.floor(meteoRows[0][0]);
    const dayIndices = meteoRows.map((row, index) => ({ row, index })).filter(({ row }) => Math.floor(row[0]) === firstDay);
    const maximumSolar = dayIndices.reduce((best, value) => value.row[5] > best.row[5] ? value : best);
    const applicable = ['auto', 'forest', 'beijing'].includes(name);
    const modifications = [];
    if (applicable && project.mode !== 'eVoxelEB') {
      modifications.push({ field: 'mode', before: project.mode, after: 'eVoxelEB',
        reason: 'Exercise the new VoxelEB shortwave algorithm with original example geometry and materials' });
      project.mode = 'eVoxelEB';
    }
    if (name === 'beijing' && !argumentsMap['full-domain']) {
      const roi = String(argumentsMap.roi || '4500,3350,300,300').split(',').map(Number);
      assert(roi.length === 4 && roi.every(Number.isFinite) && roi[2] > 0 && roi[3] > 0, 'ROI is x,z,width,height');
      const before = structuredClone(c.scene);
      Object.assign(c.scene, { offsetX: roi[0], offsetZ: roi[1], x: roi[2], y: roi[3] });
      modifications.push({ field: 'scene.domain', before, after: structuredClone(c.scene),
        reason: 'Explicit city ROI; original voxel size, material parameters, height and traversal budget retained' });
    }
    const sensorBefore = structuredClone(c.sensor);
    Object.assign(c.sensor, { x: 128, y: 128, bands: '550 10500', continuousBands: false,
      principalPlane: false, hemisphere: false, cruiseEnabled: false,
      image: true, process: applicable, radiationProcess: applicable, energyProcess: applicable });
    modifications.push({ field: 'sensor', before: sensorBefore, after: structuredClone(c.sensor),
      reason: 'Reduce unrelated imaging/I/O; use identical image and process settings in both orders' });
    const timeBefore = { start: c.meteo.start, end: c.meteo.end };
    if (profile === 'day' || !applicable) Object.assign(c.meteo, { start: maximumSolar.index, end: maximumSolar.index + 1 });
    if (profile === 'diurnal' && applicable) Object.assign(c.meteo, { start: dayIndices[0].index, end: dayIndices.at(-1).index + 1 });
    modifications.push({ field: 'meteo.nodes', before: timeBefore, after: { start: c.meteo.start, end: c.meteo.end },
      reason: profile === 'day' ? 'Peak daylight source node' : profile === 'diurnal' ? 'Contiguous complete first source day' : 'Original node interval' });
    assert(c.meteo.start >= 0 && c.meteo.end <= meteoRows.length && c.meteo.end > c.meteo.start);
    const runs = [];
    const variants = applicable ? [
      { order: 1, legacy: true, label: 'legacy-order-1' },
      ...orders.map(order => ({ order, legacy: false, label: `new-order-${order}` })),
    ] : [{ order: 1, legacy: false, label: 'original-mode-smoke' }];
    for (const { order, legacy, label } of variants) {
      const runDirectory = path.join(caseDirectory, label);
      fs.mkdirSync(runDirectory, { recursive: true });
      const runProject = structuredClone(project);
      runProject.configuration.outDir = path.join(runDirectory, 'output');
      fs.mkdirSync(runProject.configuration.outDir, { recursive: true });
      runProject.configuration.control.shortwaveScatteringOrders = order;
      const candidatePath = path.join(runDirectory, 'frozen-project.json');
      json(candidatePath, runProject);
      const runtime = prepareRuntimeSceneProject(candidatePath, runProject, {
        resolveAssetPath: value => path.isAbsolute(value) ? value : path.resolve(sourceDirectory, value),
      });
      // Keep the runtime input and clipped assets as reproducible evidence.
      runtime.project.configuration.control.shortwaveScatteringOrders = order;
      json(runtime.inputPath, runtime.project);
      runs.push({ order: applicable ? order : null, legacy, label, applicable, mode: runProject.mode,
        environment: applicable && !legacy ? { STREAMSIM_NEW_SCATTERING: '1' } : {},
        input: runtime.inputPath, inputSha256: hashFile(runtime.inputPath), runDirectory,
        outDir: runProject.configuration.outDir,
        retained: { total: runtime.total, kept: runtime.kept, excluded: runtime.excluded, invalid: runtime.invalid,
          clippedObjects: runtime.clippedObjects, sourceTriangles: runtime.sourceTriangles, retainedTriangles: runtime.retainedTriangles } });
    }
    const estimate = Math.ceil(c.scene.x / c.scene.voxel) * Math.ceil(c.scene.y / c.scene.voxel) * Math.ceil(c.scene.height / c.scene.voxel);
    plans.push({ name, profile, originalMode: original.mode, testedMode: project.mode, applicable,
      applicability: applicable ? 'VoxelEB shortwave order comparison' : 'Original-mode smoke only; VoxelEB shortwave orders do not apply',
      sourceProject: originalIdentity, assets, modifications, scene: c.scene, control: c.control,
      sensor: c.sensor, meteorology: { originalReference: original.configuration.meteo.path, internalMeteo,
        source: fileIdentity(sourceMeteo), frozen: c.meteo.path, start: c.meteo.start, end: c.meteo.end,
        nodeCount: c.meteo.end - c.meteo.start, dTime: c.meteo.dTime,
        rows: meteoRows.slice(c.meteo.start, c.meteo.end) }, denseGridCellEstimate: estimate, runs });
  }
  const manifestPath = path.join(benchmarkDirectory, 'manifest.json');
  const manifest = { schemaVersion: 1, createdAt: new Date().toISOString(), benchmarkDirectory,
    preparedEngine: fileIdentity(executable), orders, plans, timeoutMs: Number(argumentsMap['timeout-ms'] || 900000),
    limitations: ['Paired tests use example geometry/materials with explicitly documented observation/time/domain changes.',
      'Forest and Beijing are converted from VoxelRT to VoxelEB; these results do not benchmark their original runtime mode.',
      'Legacy order 1 is reported separately: the new order 1 also restores sky first scattering absent from the legacy solver.',
      'No validation against measured observations; whole-GPU memory includes other desktop applications.',
      'WDDM can report process GPU memory as unavailable; CPU time is a sampled lower bound.'] };
  json(manifestPath, manifest);
  console.log(JSON.stringify({ prepared: true, manifest: manifestPath, plans: plans.map(plan => ({
    name: plan.name, applicable: plan.applicable, mode: plan.testedMode, scene: plan.scene,
    nodeCount: plan.meteorology.nodeCount, denseGridCellEstimate: plan.denseGridCellEstimate, runs: plan.runs.length })) }, null, 2));
  return manifestPath;
}

function query(command, args) {
  return new Promise(resolve => {
    const child = spawn(command, args, { windowsHide: true });
    let stdout = '', stderr = '';
    child.stdout.on('data', value => { stdout += value; });
    child.stderr.on('data', value => { stderr += value; });
    const timeout = setTimeout(() => child.kill(), 4000);
    child.on('error', error => { clearTimeout(timeout); resolve({ stdout, stderr, error: error.message }); });
    child.on('close', status => { clearTimeout(timeout); resolve({ stdout, stderr, status }); });
  });
}
async function sampleResources(pid) {
  const commands = [
    query('nvidia-smi', ['--query-gpu=index,name,memory.used,memory.total,utilization.gpu', '--format=csv,noheader,nounits']),
    query('nvidia-smi', ['--query-compute-apps=pid,used_gpu_memory', '--format=csv,noheader,nounits']),
    process.platform === 'win32' ? query('powershell', ['-NoProfile', '-Command',
      `$taskProcess = Get-Process -Id ${pid} -ErrorAction SilentlyContinue; if ($taskProcess) { Write-Output ($taskProcess.TotalProcessorTime.TotalMilliseconds.ToString([Globalization.CultureInfo]::InvariantCulture) + ',' + $taskProcess.WorkingSet64.ToString([Globalization.CultureInfo]::InvariantCulture)) }`]) : Promise.resolve({ stdout: '' }),
  ];
  const [queried, apps, queriedProcess] = await Promise.all(commands);
  const gpu = String(queried.stdout || '').trim().split(/\r?\n/).filter(Boolean).map(line => {
    const [index, name, used, total, utilization] = line.split(',').map(value => value.trim());
    return { index: Number(index), name, usedMiB: Number(used), totalMiB: Number(total), utilizationPercent: Number(utilization) };
  });
  const processRows = String(apps.stdout || '').trim().split(/\r?\n/).filter(Boolean).map(line => line.split(',').map(value => value.trim()));
  const memory = Number(processRows.find(values => Number(values[0]) === pid)?.[1]);
  let processCpuMs = null, workingSetMiB = null;
  if (process.platform === 'win32') {
    const values = String(queriedProcess.stdout || '').trim().split(',').map(Number);
    if (values.length === 2 && values.every(Number.isFinite)) [processCpuMs, workingSetMiB] = [values[0], values[1] / 1048576];
  }
  return { timestamp: new Date().toISOString(), gpu, processGpuMiB: Number.isFinite(memory) ? memory : null,
    processCpuMs, workingSetMiB, gpuQueryError: queried.error || (queried.status ? queried.stderr : null) };
}

function jsonFiles(directory) {
  if (!fs.existsSync(directory)) return [];
  return fs.readdirSync(directory, { withFileTypes: true }).flatMap(entry => entry.isDirectory() ?
    jsonFiles(path.join(directory, entry.name)) : entry.name.endsWith('.json') ? [path.join(directory, entry.name)] : []);
}
function collectOutputs(run) {
  const diagnostics = [], processes = [], scattering = [];
  for (const file of jsonFiles(run.outDir)) {
    const metadata = readJson(file);
    if (metadata.kind === 'voxel-physiology-diagnostics') diagnostics.push({ file, ...metadata });
    else if (metadata.geometry === 'voxel' && metadata.dataFile && metadata.fields && metadata.positionOffsets) processes.push({ file, ...metadata });
    if (/scatter|shortwave/i.test(path.basename(file)) || /scatter/i.test(String(metadata.kind))) scattering.push({ file, ...metadata });
  }
  return { diagnostics, processes, scattering };
}

function semanticChecks(run, diagnostics) {
  if (run.mode !== 'eVoxelEB') return { applicable: false, passed: null };
  const configuration = readJson(run.input).configuration;
  const rows = fs.readFileSync(configuration.meteo.path, 'utf8').trim().split(/\r?\n/).slice(1)
    .filter(line => line.trim()).map(line => line.trim().split(/\s+/).map(Number));
  const zeroShortwave = diagnostics.filter(metadata => rows[metadata.node]?.[5] === 0);
  const checked = zeroShortwave.filter(metadata => metadata.vegetationCount > 0).map(metadata => {
    const fields = Object.fromEntries(metadata.fields.map(field => [field.id, field]));
    return { node: metadata.node, julianTime: metadata.julianTime,
      parZero: fields.directPar.max === 0 && fields.diffusePar.max === 0,
      grossAssimilationZero: fields.gppSunlit.max === 0 && fields.gppShaded.max === 0,
      netAssimilationNonpositive: fields.netAssimilationSunlit.max <= 0 && fields.netAssimilationShaded.max <= 0 };
  });
  return { applicable: true, zeroShortwaveNodes: zeroShortwave.length, testedVegetationNodes: checked.length,
    nodes: checked, passed: checked.every(value => value.parZero && value.grossAssimilationZero && value.netAssimilationNonpositive) };
}

async function runEngine(manifest, run) {
  assert.equal(hashFile(run.input), run.inputSha256, 'Frozen runtime configuration changed');
  const logPath = path.join(run.runDirectory, 'run.log');
  const logStream = fs.createWriteStream(logPath);
  const start = performance.now();
  let logText = '', timedOut = false;
  const environment = { ...process.env };
  delete environment.STREAMSIM_NEW_SCATTERING;
  Object.assign(environment, run.environment);
  const child = spawn(executable, [run.mode, run.input], { cwd: path.dirname(executable), windowsHide: true, env: environment });
  const samples = [];
  const capture = chunk => { logStream.write(chunk); logText += chunk.toString(); };
  child.stdout.on('data', capture); child.stderr.on('data', capture);
  let sampling = null;
  const sample = () => {
    if (!child.pid) return;
    if (sampling) return;
    sampling = sampleResources(child.pid).then(resources => {
      const value = { elapsedMs: performance.now() - start, ...resources };
      samples.push(value);
      fs.appendFileSync(path.join(run.runDirectory, 'resources.jsonl'), JSON.stringify(value) + '\n');
    }).finally(() => { sampling = null; });
  };
  sample();
  const timer = setInterval(sample, 1500);
  const timeout = setTimeout(() => { timedOut = true; child.kill(); }, manifest.timeoutMs);
  const status = await new Promise(resolve => {
    child.on('error', error => resolve({ error: error.message, code: null }));
    child.on('close', (code, signal) => resolve({ code, signal }));
  });
  clearInterval(timer); clearTimeout(timeout);
  const wallMs = performance.now() - start;
  if (sampling) await sampling;
  await new Promise(resolve => logStream.end(resolve));
  const outputs = collectOutputs(run);
  const maximum = property => samples.reduce((value, item) => item[property] == null ? value : Math.max(value ?? 0, item[property]), null);
  const inputProject = readJson(run.input);
  const expectedNodes = inputProject.configuration.meteo.end - inputProject.configuration.meteo.start;
  const physicalChecks = semanticChecks(run, outputs.diagnostics);
  const scatteringValid = !run.applicable || outputs.scattering.length === expectedNodes && outputs.scattering.every(metadata =>
    metadata.format === 'streamsim-shortwave-scattering-v1' && metadata.valid === true &&
    metadata.requestedOrders === run.order && metadata.completedOrders === run.order && metadata.legacy === run.legacy &&
    (run.legacy || metadata.statsAvailable === true && metadata.perOrder.every(order =>
      ['maxStepTruncations', 'transmittanceStops', 'tracedRays', 'invalidValues'].every(field =>
        Number.isSafeInteger(order[field]) && order[field] >= 0) && order.invalidValues === 0 &&
      ['absorbedShortwave', 'absorbedPar', 'transmittanceResidual', 'truncatedResidual'].every(field => Number.isFinite(order[field])))));
  const result = { ...run, ...status, timedOut, wallMs, expectedNodes, logPath, executable: fileIdentity(executable),
    ran: true, deviceLost: /VK_ERROR_DEVICE_LOST|device lost/i.test(logText),
    cpuTimeMsSampledLowerBound: maximum('processCpuMs'), peakWorkingSetMiBSampled: maximum('workingSetMiB'),
    peakProcessGpuMiBSampled: maximum('processGpuMiB'), peakGpuMiBSampled: samples.reduce((value, item) =>
      Math.max(value, ...item.gpu.map(gpu => gpu.usedMiB)), 0), sampleCount: samples.length,
    scatteringLog: logText.split(/\r?\n/).filter(line => /SCATTER|SHORTWAVE|RADIANCE_BUFFER/.test(line)),
    ...outputs, physicalChecks, valid: status.code === 0 && !timedOut && physicalChecks.passed !== false && (run.mode !== 'eVoxelEB' ||
      outputs.diagnostics.length === expectedNodes && outputs.diagnostics.every(value => value.valid && value.invalidVoxelCount === 0)) &&
      scatteringValid };
  json(path.join(run.runDirectory, 'result.json'), result);
  return result;
}

function compareRecords(left, right) {
  assert.equal(left.voxelCount, right.voxelCount, 'Voxel count differs across paired inputs');
  assert.equal(left.recordFloats, right.recordFloats, 'Binary layouts differ');
  const a = fs.readFileSync(path.join(path.dirname(left.file), left.dataFile));
  const b = fs.readFileSync(path.join(path.dirname(right.file), right.dataFile));
  assert.equal(a.length, left.voxelCount * left.recordFloats * 4);
  assert.equal(a.length, b.length);
  for (let index = 0; index < left.voxelCount; ++index) {
    for (const offset of left.positionOffsets) {
      assert.equal(a.readFloatLE((index * left.recordFloats + offset) * 4),
        b.readFloatLE((index * right.recordFloats + offset) * 4), 'Voxel positions differ');
    }
    assert.equal(Number.isInteger(left.componentOffset), Number.isInteger(right.componentOffset), 'Component layout differs');
    if (Number.isInteger(left.componentOffset)) {
      assert.equal(a.readFloatLE((index * left.recordFloats + left.componentOffset) * 4),
        b.readFloatLE((index * right.recordFloats + right.componentOffset) * 4), 'Voxel components differ');
    }
  }
  const fields = [];
  for (const field of left.fields) {
    const matching = right.fields.find(value => value.id === field.id);
    assert(matching, `Missing comparison field: ${field.id}`);
    let count = 0, sumA = 0, sumB = 0, sumDifference = 0, sumSquaredDifference = 0, maxAbsoluteDifference = 0;
    for (let index = 0; index < left.voxelCount; ++index) {
      const offsetA = (index * left.recordFloats + field.offset) * 4;
      const offsetB = (index * right.recordFloats + matching.offset) * 4;
      const va = a.readFloatLE(offsetA), vb = b.readFloatLE(offsetB);
      if (!Number.isFinite(va) || !Number.isFinite(vb)) continue;
      ++count; sumA += va; sumB += vb;
      const difference = vb - va;
      sumDifference += difference; sumSquaredDifference += difference * difference;
      maxAbsoluteDifference = Math.max(maxAbsoluteDifference, Math.abs(difference));
    }
    fields.push({ id: field.id, count, meanOrder1: count ? sumA / count : null, meanHigherOrder: count ? sumB / count : null,
      meanDifference: count ? sumDifference / count : null, rmseDifference: count ? Math.sqrt(sumSquaredDifference / count) : null,
      maxAbsoluteDifference });
  }
  return { node: left.node, julianTime: left.julianTime, kind: left.kind, voxelCount: left.voxelCount, fields };
}
function compareRuns(baseline, higher) {
  const comparisons = [];
  for (const metadata of baseline.processes) {
    const matching = higher.processes.find(value => value.node === metadata.node && value.processType === metadata.processType);
    assert(matching, `Missing higher-order process node: ${metadata.node}/${metadata.processType}`);
    comparisons.push(compareRecords(metadata, matching));
  }
  const physiology = baseline.diagnostics.map(metadata => {
    const matching = higher.diagnostics.find(value => value.node === metadata.node);
    assert(matching, `Missing higher-order diagnostic node: ${metadata.node}`);
    return { node: metadata.node, julianTime: metadata.julianTime, vegetationCount: metadata.vegetationCount,
      fields: metadata.fields.map(field => {
        const other = matching.fields.find(value => value.id === field.id);
        return { id: field.id, unit: field.unit, meanOrder1: field.mean, meanHigherOrder: other.mean,
          meanDifference: field.mean == null || other.mean == null ? null : other.mean - field.mean };
      }) };
  });
  const scattering = baseline.scattering.map(metadata => {
    const matching = higher.scattering.find(value => value.node === metadata.node);
    assert(matching, `Missing higher-order scattering node: ${metadata.node}`);
    return { node: metadata.node, baselineElapsedMs: metadata.elapsedMs, higherElapsedMs: matching.elapsedMs,
      stageRatio: metadata.elapsedMs > 0 ? matching.elapsedMs / metadata.elapsedMs : null,
      baselineCompletedOrders: metadata.completedOrders, higherCompletedOrders: matching.completedOrders,
      orderContributionEarlyStop: matching.orderContributionEarlyStop,
      perOrder: matching.perOrder, counterAggregation: matching.counterAggregation,
      fluxAggregation: matching.fluxAggregation, residualAggregation: matching.residualAggregation };
  });
  const baselineStageMs = baseline.scattering.reduce((sum, value) => sum + value.elapsedMs, 0);
  const higherStageMs = higher.scattering.reduce((sum, value) => sum + value.elapsedMs, 0);
  return { baselineOrder: baseline.order, baselineLegacy: baseline.legacy,
    higherOrder: higher.order, higherLegacy: higher.legacy, wallRatio: higher.wallMs / baseline.wallMs,
    wallDifferenceMs: higher.wallMs - baseline.wallMs, baselineStageMs, higherStageMs,
    stageRatio: baselineStageMs > 0 ? higherStageMs / baselineStageMs : null, scattering, comparisons, physiology };
}

function writeReport(manifest, results) {
  const report = ['# 三阶散射案例测试', '',
    '冻结同条件对比：旧一次散射、新算法一次、新算法三次。legacy→new1包括天空一次散射补入、相函数归一化、参与介质吸收及边界/sky截断处理修正；纯增阶成本使用new1→new3，不把这些修正归入二、三阶贡献。', '',
    '| 案例 | 模式与范围 | 旧一次总秒 | 新一次总秒 | 新三次总秒 | 新3/旧1总耗时 | 新3/旧1散射阶段 | 新3/新1总耗时 | 新3/新1散射阶段 |',
    '|---|---|---:|---:|---:|---:|---:|---:|---:|'];
  const number = (value, decimals = 3) => value == null || !Number.isFinite(value) ? '—' : value.toFixed(decimals);
  for (const result of results) {
    const plan = manifest.plans.find(value => value.name === result.name);
    const legacy = result.runs.find(value => value.legacy);
    const first = result.runs.find(value => !value.legacy && value.order === 1);
    const third = result.runs.find(value => !value.legacy && value.order === 3);
    const comparison = result.comparisons.find(value => value.higherOrder === 3);
    const upgrade = result.upgradeComparison;
    const domain = `${plan.scene.x}×${plan.scene.y}×${plan.scene.height} m，体元 ${plan.scene.voxel} m`;
    report.push(`| ${result.name} | ${plan.testedMode}，${domain} | ${number(legacy?.wallMs / 1000)} | ${number(first?.wallMs / 1000)} | ${number(third?.wallMs / 1000)} | ${number(upgrade?.wallRatio)} | ${number(upgrade?.stageRatio)} | ${number(comparison?.wallRatio)} | ${number(comparison?.stageRatio)} |`);
  }
  report.push('', 'forest、beijing原案例为VoxelRT，本次明确临时转换为VoxelEB测试新算法。北京仅origin(x=4500,z=3350)的300×300m ROI，不能推断完整5km城市场景速度。原fluid配置完整保留；sensorPeriodicTraversalCount=20控制观察路径，不控制本次中央计算域中的辐射传播。', '',
    '阶段耗时来自引擎 scattering JSON；总耗时包括场景加载、能量迭代、成像及输出。一次冷启动/组，启动开销及噪声会稀释单节点比值；不能把单次总时间较短解释为算法提速。', '',
    '| 案例：新3 − 新1 | 漫射PAR均值差 μmol m⁻²叶 s⁻¹ | 温度均值差 K | 温度空间RMSE K | 温度最大绝对差 K | 短波均值差 W m⁻² |',
    '|---|---:|---:|---:|---:|---:|');
  for (const result of results) {
    const comparison = result.comparisons.find(value => value.higherOrder === 3);
    if (!comparison) continue;
    const temperatures = comparison.comparisons.filter(value => value.kind === 'voxel-energy-process')
      .flatMap(value => value.fields.filter(field => field.id === 'temperature'));
    const shortwave = comparison.comparisons.filter(value => value.kind === 'voxel-radiation-process')
      .flatMap(value => value.fields.filter(field => field.id === 'shortwaveRadiation'));
    const par = comparison.physiology.flatMap(value => value.fields.filter(field => field.id === 'diffusePar'))
      .filter(value => value.meanDifference != null);
    const average = values => values.length ? values.reduce((sum, value) => sum + value, 0) / values.length : null;
    const count = temperatures.reduce((sum, value) => sum + value.count, 0);
    const rmse = count ? Math.sqrt(temperatures.reduce((sum, value) => sum + value.count * value.rmseDifference ** 2, 0) / count) : null;
    report.push(`| ${result.name} | ${number(average(par.map(value => value.meanDifference)))} | ${number(average(temperatures.map(value => value.meanDifference)))} | ${number(rmse)} | ${number(temperatures.length ? Math.max(...temperatures.map(value => value.maxAbsoluteDifference)) : null)} | ${number(average(shortwave.map(value => value.meanDifference)))} |`);
  }
  report.push('', '多节点均值差为节点均值差的简单平均；空间RMSE按体元记录数汇总。PAR仅聚合叶冠层体元。', '',
    '| 案例/组 | 完成节点 | 实际完成阶数 | maxStep 截断 | 透过率阈值停止 | 残余透过率和 | GPU总显存峰值 MiB | 结果有效 |',
    '|---|---:|---|---:|---:|---:|---:|---|');
  for (const result of results) for (const run of result.runs) {
    const stats = run.scattering.flatMap(value => value.perOrder || []);
    const sum = field => stats.reduce((value, order) => value + order[field], 0);
    const completed = [...new Set(run.scattering.map(value => value.completedOrders))].join(',');
    report.push(`| ${result.name}/${run.label} | ${run.diagnosticCount} | ${completed || '—'} | ${run.legacy ? '未统计' : sum('maxStepTruncations')} | ${run.legacy ? '未统计' : sum('transmittanceStops')} | ${run.legacy ? '未统计' : number(sum('transmittanceResidual'))} | ${number(run.peakGpuMiBSampled, 0)} | ${run.valid} |`);
  }
  report.push('', '| 案例/组 | 无入射短波节点 | PAR=0/GPP=0/叶片净同化≤0 |', '|---|---:|---|');
  for (const result of results) for (const run of result.runs) {
    report.push(`| ${result.name}/${run.label} | ${run.physicalChecks?.zeroShortwaveNodes ?? '—'} | ${run.physicalChecks?.passed ?? '—'} |`);
  }
  report.push('', '| 案例/组/阶 | 路径数 | maxStep 截断率 % | 透过率阈值停止率 % |',
    '|---|---:|---:|---:|');
  for (const result of results) for (const run of result.runs.filter(value => !value.legacy)) {
    const aggregated = new Map();
    for (const metadata of run.scattering) for (const stats of metadata.perOrder) {
      const total = aggregated.get(stats.order) || { tracedRays: 0, maxStepTruncations: 0, transmittanceStops: 0 };
      for (const field of Object.keys(total)) total[field] += stats[field];
      aggregated.set(stats.order, total);
    }
    for (const [order, stats] of [...aggregated.entries()].sort((a, b) => a[0] - b[0])) {
      if (!stats.tracedRays) continue;
      report.push(`| ${result.name}/${run.label}/${order} | ${stats.tracedRays} | ${number(stats.maxStepTruncations / stats.tracedRays * 100, 6)} | ${number(stats.transmittanceStops / stats.tracedRays * 100, 6)} |`);
    }
  }
  report.push('', 'maxStep 与透过率阈值是单条射线路径的截断；当前没有按散射阶贡献提前停止。CPU uint64 汇总每个谱组计数，读取时检查整数未丢失精度。残余量为无量纲 ray throughput 求和，不是遗漏辐射能量。', '',
    'perOrder absorbedShortwave/absorbedPar 是体元通量密度之和，不能解释为区域总功率。PAR/GPP均值是未按叶面积加权的体元状态均值；本测试比较数值敏感性，不验证实测精度。', '',
    'GPU显存为整卡用量，含桌面程序；WDDM不可读的进程显存保留 null。所有源哈希、配置修改、原始输出和逐体元差异均在 manifest/result.json 中。', '',
    `数据目录：${manifest.benchmarkDirectory}`, '');
  fs.writeFileSync(path.join(manifest.benchmarkDirectory, 'REPORT.md'), report.join('\n'));
}

async function execute(manifestPath) {
  const manifest = readJson(manifestPath);
  const only = argumentsMap.case ? String(argumentsMap.case).split(',') : null;
  const lockPath = path.join(destination, '.gpu-benchmark.lock');
  const lock = fs.openSync(lockPath, 'wx');
  fs.writeSync(lock, String(process.pid));
  const aggregatePath = path.join(manifest.benchmarkDirectory, 'result.json');
  const results = fs.existsSync(aggregatePath) ? readJson(aggregatePath).results : [];
  const saveResult = value => {
    const existing = results.findIndex(item => item.name === value.name && item.profile === value.profile);
    if (existing >= 0) results[existing] = value;
    else results.push(value);
    json(aggregatePath, { manifest: manifestPath, executable: fileIdentity(executable), results });
    writeReport(manifest, results);
  };
  try {
    for (const plan of manifest.plans) {
      if (only && !only.includes(plan.name)) continue;
      for (const asset of plan.assets) assert.equal(hashFile(asset.snapshot), asset.sha256, `Frozen asset changed: ${asset.snapshot}`);
      if (!plan.applicable && !argumentsMap['smoke-original']) {
        saveResult({ name: plan.name, profile: plan.profile, applicable: false, ran: false, valid: null,
          skipped: 'VoxelEB shortwave orders do not apply; original-mode smoke is optional via --smoke-original', runs: [], comparisons: [] });
        continue;
      }
      const runs = [];
      for (const run of plan.runs) {
        console.log(JSON.stringify({ event: 'start', name: plan.name, profile: plan.profile, order: run.order, mode: run.mode }));
        const result = argumentsMap['analyze-only'] ? readJson(path.join(run.runDirectory, 'result.json')) : await runEngine(manifest, run);
        if (argumentsMap['analyze-only']) {
          result.physicalChecks = semanticChecks(run, result.diagnostics);
          result.valid = result.valid && result.physicalChecks.passed !== false;
        }
        runs.push(result);
        console.log(JSON.stringify({ event: 'complete', name: plan.name, order: run.order,
          legacy: result.legacy, status: result.code, valid: result.valid, wallMs: result.wallMs, peakGpuMiB: result.peakGpuMiBSampled }));
        json(path.join(plan.runs[0].runDirectory, '../execution.json'), { name: plan.name, runs });
        if (result.deviceLost) break;
      }
      const baseline = runs.find(value => value.order === 1 && !value.legacy && value.valid);
      const legacy = runs.find(value => value.legacy && value.valid);
      const third = runs.find(value => !value.legacy && value.order === 3 && value.valid);
      const comparisons = baseline ? runs.filter(value => value.order > 1 && !value.legacy && value.valid).map(value => compareRuns(baseline, value)) : [];
      const result = { name: plan.name, profile: plan.profile, applicable: plan.applicable, ran: runs.length > 0,
        valid: runs.length === plan.runs.length && runs.every(value => value.valid),
        runs: runs.map(({ processes, diagnostics, scattering, ...value }) => ({ ...value,
        processCount: processes.length, diagnosticCount: diagnostics.length, scattering })), comparisons,
        legacyComparison: legacy && baseline ? compareRuns(legacy, baseline) : null,
        upgradeComparison: legacy && third ? compareRuns(legacy, third) : null };
      saveResult(result);
    }
  } finally { fs.closeSync(lock); fs.unlinkSync(lockPath); }
  console.log(JSON.stringify({ result: path.join(manifest.benchmarkDirectory, 'result.json'),
    cases: results.map(value => ({ name: value.name, applicable: value.applicable, ran: value.ran, valid: value.valid,
      ratios: value.comparisons.map(comparison => comparison.wallRatio) })) }, null, 2));
}

if (argumentsMap.run || argumentsMap['analyze-only']) {
  assert(argumentsMap.plan, 'Use --run --plan=<prepared manifest.json>; preparation never starts GPU jobs');
  await execute(path.resolve(argumentsMap.plan));
} else {
  prepare();
}
