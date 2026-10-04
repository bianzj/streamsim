import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { createDefaultProject, normalizeProject, validateProject } from '../../src/renderer/src/project-schema.js';

export function runEnergyEngineRegression(root, { prepareOnly = false } = {}) {
  const destination = path.join(root, '.test', 'energy-core', `mixed-${Date.now()}`);
  fs.mkdirSync(destination, { recursive: true });
  const base = createDefaultProject({ name: 'Farquhar mixed material regression',
    mode: 'eVoxelEB', sceneX: 12, sceneY: 8, sceneHeight: 4, voxelSize: 1 });
  const c = base.configuration;
  Object.assign(c.sensor, { x: 16, y: 16, bands: '550 10500', image: false,
    process: true, energyProcess: true, radiationProcess: false });
  Object.assign(c.control, { depth: 2, samples: 4, couplingIterations: 8,
    vegetationTemperatureMethod: 1, soilTemperatureMethod: 2 });
  Object.assign(c.meteo, { path: path.join(root, 'models/bin_x64/Release/defined/meteo.txt'),
    start: 24, end: 25, dTime: 1800 });
  c.spectra = [
    { name: 'soil', model: 'custom', reflectance: '0.2', transmittance: '0', refTir: 0.05, tauTir: 0, params: {} },
    { name: 'leaf_optics', model: 'custom', reflectance: '0.1', transmittance: '0.05', refTir: 0.03, tauTir: 0, params: {} },
    { name: 'solid_optics', model: 'custom', reflectance: '0.3', transmittance: '0', refTir: 0.08, tauTir: 0, params: {} },
    { name: 'water_optics', model: 'custom', reflectance: '0.04', transmittance: '0', refTir: 0.02, tauTir: 0, params: {} },
  ];
  c.thermals = [{ name: 'soil_temperature', sunlitTemperature: 298.15, shadedTemperature: 298.15 }];
  c.canopies = [
    { name: 'canopy_default', structureType: 'canopy', lai: 2, density: 1, hc: 2, G: 0.5,
      leafwidth: 0.1, hspot: 0.2, LIDFa: -0.35, LIDFb: -0.15 },
    { name: 'rigid_body', structureType: 'rigid', lai: 0, density: 1, hc: 2, G: 0.5 },
  ];
  const soil = { method: 2, rss: 2000, cs: 1180, rhos: 1800, lambdas: 0.8, Tsoil: 25, SMC: 25, Satwater: 0.45 };
  const water = { rss: 0, heatCapacity: 4186000, mixingDepth: 0.5, evaporationCoefficient: 1 };
  c.materials = [
    { name: 'soilset', type: 'Soil', params: soil },
    { name: 'test_building', type: 'Building', params: { ...soil, rss: 1e9, cs: 900, rhos: 2300, lambdas: 1.5, SMC: 0, Satwater: 0 } },
    { name: 'unused_water', type: 'Water', params: water },
    { name: 'test_water', type: 'Water', params: water },
    { name: 'leaf_c3', type: 'Vegetation', params: { Vcmax: 80, m: 9, BallBerry: 0.01, Type: 3,
      kV: 0.6396, Rdparam: 0.015, Tparam: '0.2,0.3,288,313,328', Tyear: 25,
      beta: 0.507, kNPQs: 0, qLs: 1, stressfactor: 1, Tcor: 0 } },
  ];
  const specifications = [
    ['vegetation', 'Vegetation', 'leaf_c3', 'leaf_optics', 'canopy_default', 3, 3],
    ['building', 'Building', 'test_building', 'solid_optics', 'rigid_body', 8, 3],
    ['water', 'Water', 'test_water', 'water_optics', 'rigid_body', 8, 6],
  ];
  c.objects.items = specifications.map(([name, type, materialName, spectralName, canopyName, x, z]) => {
    const positionFile = path.join(destination, `${name}_position.txt`);
    fs.writeFileSync(positionFile, `${x} ${z} 0 1 0\n` + (name === 'building' ? '2 6 0 1 0\n' : ''));
    return { name, type, materialName, spectralName, canopyName, thermalName: 'soil_temperature',
      fileName: '', positionFile, shape: 'cube', dimensions: [2, 2, 2], instanceCount: name === 'building' ? 2 : 1,
      meshes: [{ name, materialName, spectralName, canopyName, thermalName: 'soil_temperature' }] };
  });
  c.objects.count = c.objects.items.length;
  c.objects.names = c.objects.items.map(item => item.name);
  const executable = path.join(root, 'models/bin_x64/Release/histream.exe');
  const cases = [];
  for (const solver of ['traditional', 'accelerated']) {
    const project = normalizeProject(base);
    const caseDirectory = path.join(destination, solver);
    project.configuration.outDir = path.join(caseDirectory, 'output');
    project.configuration.control.radiationSolver = solver;
    fs.mkdirSync(caseDirectory, { recursive: true });
    const input = path.join(caseDirectory, 'project.json');
    fs.writeFileSync(input, JSON.stringify(project, null, 2));
    assert(validateProject(project).valid, JSON.stringify(validateProject(project).errors));
    if (prepareOnly) { cases.push({ solver, project: input }); continue; }
    const execution = spawnSync(executable, ['eVoxelEB', input], { cwd: path.dirname(executable),
      encoding: 'utf8', windowsHide: true, timeout: 180000, maxBuffer: 64 * 1024 * 1024 });
    fs.writeFileSync(path.join(caseDirectory, 'run.log'), (execution.stdout || '') + '\n' + (execution.stderr || ''));
    assert(!execution.error, String(execution.error));
    assert.equal(execution.status, 0, `Engine failed: ${caseDirectory}/run.log`);
    const processDirectory = path.join(project.configuration.outDir, 'process');
    const metadataFiles = fs.readdirSync(processDirectory).filter(file => file.startsWith('voxeleb_T=') && file.endsWith('.json'));
    assert.equal(metadataFiles.length, 1, 'Expected the sole requested energy node');
    const metadata = JSON.parse(fs.readFileSync(path.join(processDirectory, metadataFiles[0]), 'utf8'));
    const data = fs.readFileSync(path.join(processDirectory, metadata.dataFile));
    const links = fs.readFileSync(path.join(processDirectory, metadata.sceneLinks.dataFile));
    const typeOffset = metadata.sceneLinks.fields.indexOf('type');
    assert(typeOffset >= 0, 'Process scene links must identify actual shader material type');
    assert.equal(data.length, metadata.voxelCount * metadata.recordFloats * 4);
    assert.equal(links.length, metadata.voxelCount * metadata.sceneLinks.recordInts * 4);
    const counts = {};
    let positiveLeafGpp = 0;
    const buildingPlacements = [0, 0];
    for (let voxel = 0; voxel < metadata.voxelCount; ++voxel) {
      const type = links.readInt32LE((voxel * metadata.sceneLinks.recordInts + typeOffset) * 4);
      counts[type] = (counts[type] || 0) + 1;
      const values = {};
      for (const field of metadata.fields) {
        const value = data.readFloatLE((voxel * metadata.recordFloats + field.offset) * 4);
        assert(Number.isFinite(value), `Nonfinite ${field.id}, type ${type}, voxel ${voxel}`);
        values[field.id] = value;
      }
      if (type === 2) {
        assert(values.gpp >= 0, 'Gross leaf photosynthesis must be nonnegative');
        if (values.gpp > 0) ++positiveLeafGpp;
      } else {
        assert.equal(values.gpp, 0, `Nonvegetation GPP, type ${type}, voxel ${voxel}`);
        assert.equal(values.npp, 0, `Nonvegetation NPP, type ${type}, voxel ${voxel}`);
      }
      if (type === 3) {
        assert.equal(values.latentHeat, 0, 'Impermeable building must have zero latent heat');
        const x = data.readFloatLE((voxel * metadata.recordFloats + metadata.positionOffsets[0]) * 4);
        const z = data.readFloatLE((voxel * metadata.recordFloats + metadata.positionOffsets[2]) * 4);
        if (x >= 8 && x <= 10 && z >= 3 && z <= 5) ++buildingPlacements[0];
        else if (x >= 2 && x <= 4 && z >= 6 && z <= 8) ++buildingPlacements[1];
        else assert.fail(`Building ignored its position file: x=${x}, z=${z}`);
      }
    }
    for (const type of [1, 2, 3, 4]) assert(counts[type] > 0, `Missing actual material type ${type}`);
    assert(positiveLeafGpp > 0, 'The vegetation Farquhar branch must produce photosynthesis in daylight');
    assert(buildingPlacements.every(count => count > 0), 'Both primitive building placements must retain valid scene links');
    cases.push({ solver, passed: true, voxelCount: metadata.voxelCount, typeCounts: counts,
      positiveLeafGpp, project: input, metadata: path.join(processDirectory, metadataFiles[0]) });
  }
  const result = { prepared: prepareOnly, passed: !prepareOnly, destination, cases };
  if (!prepareOnly) result.engineSha256 = createHash('sha256').update(fs.readFileSync(executable)).digest('hex');
  fs.writeFileSync(path.join(destination, 'result.json'), JSON.stringify(result, null, 2));
  return result;
}
