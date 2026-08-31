import { createServer } from 'node:http';
import { createReadStream, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { randomUUID } from 'node:crypto';
import { dirname, extname, join, normalize, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';

const guiDir = dirname(fileURLToPath(import.meta.url));
const projectRoot = resolve(guiDir, '..');
const runtimeDir = join(guiDir, '.runtime');
mkdirSync(runtimeDir, { recursive: true });

const sourceTreeFile = join(projectRoot, 'data', 'example', 'single_tree_LAI_4.obj');
const forestSceneFile = join(runtimeDir, 'forest_plot_6x6.obj');
const forestRows = 6;
const forestColumns = 6;
const forestTreeCount = forestRows * forestColumns;

function buildForestScene() {
  const vertices = [];
  const faces = [];
  for (const rawLine of readFileSync(sourceTreeFile, 'utf8').split(/\r?\n/)) {
    const parts = rawLine.trim().split(/\s+/);
    if (parts[0] === 'v') {
      vertices.push(parts.slice(1, 4).map(Number));
    } else if (parts[0] === 'f') {
      faces.push(parts.slice(1).map((token) => {
        const raw = Number(token.split('/')[0]);
        return raw > 0 ? raw - 1 : vertices.length + raw;
      }));
    }
  }
  if (!vertices.length || !faces.length) {
    throw new Error('单树 OBJ 场景为空');
  }

  const minimumY = Math.min(...vertices.map((vertex) => vertex[1]));
  const spacing = 4.8;
  const output = [
    '# FacetLab deterministic forest plot',
    `# facetlab-tree-count ${forestTreeCount}`,
  ];
  let vertexOffset = 0;
  for (let row = 0; row < forestRows; ++row) {
    for (let column = 0; column < forestColumns; ++column) {
      const tree = row * forestColumns + column;
      const x = (column - (forestColumns - 1) * 0.5) * spacing +
                0.32 * Math.sin((tree + 1) * 2.17);
      const z = (row - (forestRows - 1) * 0.5) * spacing +
                0.32 * Math.cos((tree + 1) * 1.73);
      const scale = 0.90 + 0.18 * (0.5 + 0.5 * Math.sin((tree + 1) * 1.31));
      const angle = ((tree * 47 + row * 13) % 360) * Math.PI / 180;
      const cosine = Math.cos(angle);
      const sine = Math.sin(angle);

      output.push(`o tree_${String(tree + 1).padStart(2, '0')}`);
      for (const vertex of vertices) {
        const localX = vertex[0] * scale;
        const localZ = vertex[2] * scale;
        const px = localX * cosine - localZ * sine + x;
        const py = (vertex[1] - minimumY) * scale;
        const pz = localX * sine + localZ * cosine + z;
        output.push(`v ${px.toFixed(5)} ${py.toFixed(5)} ${pz.toFixed(5)}`);
      }
      for (const face of faces) {
        output.push(`f ${face.map((index) => index + vertexOffset + 1).join(' ')}`);
      }
      vertexOffset += vertices.length;
    }
  }
  writeFileSync(forestSceneFile, `${output.join('\n')}\n`, 'utf8');
}

buildForestScene();

const port = Number(process.env.PORT || 43177);
const host = process.env.HOST || '127.0.0.1';

const mime = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.obj': 'text/plain; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
};

function nativeArgument(path) {
  if (process.platform !== 'linux') return path;
  const match = path.match(/^\/mnt\/([a-zA-Z])\/(.*)$/);
  return match ? `${match[1].toUpperCase()}:/${match[2].replaceAll('\\', '/')}` : path;
}

function runnerCandidates() {
  const configured = process.env.RADIOSITY_RUNNER;
  return [
    configured,
    join(projectRoot, 'cmake-build-gui', 'radiosity_web_runner.exe'),
    join(projectRoot, 'cmake-build-vulkan', 'radiosity_web_runner.exe'),
    join(projectRoot, 'cmake-build-release', 'radiosity_web_runner.exe'),
    join(projectRoot, 'cmake-build-debug', 'radiosity_web_runner.exe'),
  ].filter(Boolean);
}

function findRunner() {
  return runnerCandidates().find((candidate) => existsSync(candidate));
}

function sendFile(response, fileName) {
  if (!existsSync(fileName)) {
    response.writeHead(404).end('Not found');
    return;
  }
  response.writeHead(200, {
    'content-type': mime[extname(fileName)] || 'application/octet-stream',
    'cache-control': fileName.includes(`${sep}node_modules${sep}`)
      ? 'public, max-age=86400'
      : 'no-cache',
  });
  createReadStream(fileName).pipe(response);
}

async function readBody(request) {
  const chunks = [];
  for await (const chunk of request) chunks.push(chunk);
  return Buffer.concat(chunks).toString('utf8');
}

function clamp(value, minimum, maximum) {
  return Math.max(minimum, Math.min(maximum, Number(value)));
}

function energyBalance(radiosityResult, input = {}) {
  const settings = {
    airTemperatureC: clamp(input.airTemperatureC ?? 25, -30, 60),
    relativeHumidity: clamp(input.relativeHumidity ?? 60, 1, 100),
    windSpeed: clamp(input.windSpeed ?? 2, 0.1, 30),
    incomingLongwave: clamp(input.incomingLongwave ?? 400, 100, 700),
    leafSurfaceResistance: clamp(input.leafSurfaceResistance ?? 100, 10, 1000),
    soilSurfaceResistance: clamp(input.soilSurfaceResistance ?? 2000, 10, 10000),
  };
  const sigma = 5.670374419e-8;
  const rhoCp = 1205;
  const gamma = 66;
  const airK = settings.airTemperatureC + 273.15;
  const saturationKPa = 0.6108 * Math.exp(
    17.27 * settings.airTemperatureC / (settings.airTemperatureC + 237.3)
  );
  const slope = 4098 * saturationKPa /
                ((settings.airTemperatureC + 237.3) ** 2) * 1000;
  const vpdPa = saturationKPa * (1 - settings.relativeHumidity / 100) * 1000;
  const count = radiosityResult.facetCount;
  const result = {
    settings,
    absorbedShortwave: new Array(count),
    temperatureC: new Array(count),
    netRadiation: new Array(count),
    sensibleHeat: new Array(count),
    latentHeat: new Array(count),
    groundHeat: new Array(count),
    residual: new Array(count),
  };

  for (let facet = 0; facet < count; ++facet) {
    const leaf = facet < radiosityResult.leafFacetCount;
    const reflectance = leaf ? 0.10 : 0.20;
    const transmittance = leaf ? 0.05 : 0;
    const b0 = radiosityResult.radiosity[facet * 2] || 0;
    const b1 = radiosityResult.radiosity[facet * 2 + 1] || 0;
    const determinant = reflectance * reflectance -
                        transmittance * transmittance;
    const h0 = determinant !== 0
      ? (reflectance * b0 - transmittance * b1) / determinant
      : b0 / Math.max(reflectance, 1e-6);
    const h1 = determinant !== 0
      ? (reflectance * b1 - transmittance * b0) / determinant
      : b1 / Math.max(reflectance, 1e-6);
    const absorbed = Math.max(0, 1 - reflectance - transmittance) *
                     Math.max(0, h0, h1);
    const emissivity = leaf ? 0.96 : 0.95;
    const aerodynamicResistance = leaf
      ? 80 / Math.sqrt(settings.windSpeed)
      : 120 / settings.windSpeed;
    const surfaceResistance = leaf
      ? settings.leafSurfaceResistance
      : settings.soilSurfaceResistance;

    const fluxes = (temperatureK) => {
      const netRadiation = absorbed + emissivity *
        (settings.incomingLongwave - sigma * temperatureK ** 4);
      const groundHeat = leaf ? 0 : 0.10 * netRadiation;
      const latentHeat = (
        slope * (netRadiation - groundHeat) +
        rhoCp * vpdPa / aerodynamicResistance
      ) / (
        slope + gamma * (1 + surfaceResistance / aerodynamicResistance)
      );
      const sensibleHeat = rhoCp * (temperatureK - airK) /
                           aerodynamicResistance;
      return {
        netRadiation,
        groundHeat,
        latentHeat,
        sensibleHeat,
        residual: netRadiation - groundHeat - latentHeat - sensibleHeat,
      };
    };

    let low = 243.15;
    let high = 353.15;
    for (let iteration = 0; iteration < 48; ++iteration) {
      const middle = 0.5 * (low + high);
      if (fluxes(middle).residual > 0) low = middle;
      else high = middle;
    }
    const temperatureK = 0.5 * (low + high);
    const flux = fluxes(temperatureK);
    result.absorbedShortwave[facet] = absorbed;
    result.temperatureC[facet] = temperatureK - 273.15;
    result.netRadiation[facet] = flux.netRadiation;
    result.sensibleHeat[facet] = flux.sensibleHeat;
    result.latentHeat[facet] = flux.latentHeat;
    result.groundHeat[facet] = flux.groundHeat;
    result.residual[facet] = flux.residual;
  }
  return result;
}

async function runModel(request, response) {
  let body;
  try {
    body = JSON.parse(await readBody(request));
  } catch {
    response.writeHead(400).end('Invalid JSON');
    return;
  }
  if (body.backend !== 'cpu' && body.backend !== 'gpu') {
    response.writeHead(400).end('backend must be cpu or gpu');
    return;
  }

  const runner = findRunner();
  if (!runner) {
    response.writeHead(503, { 'content-type': 'text/plain; charset=utf-8' });
    response.end(
      '未找到原生运行器。请先构建 CMake 目标 radiosity_web_runner，' +
      '或设置 RADIOSITY_RUNNER。'
    );
    return;
  }

  const resultFile = join(runtimeDir, `result-${randomUUID()}.json`);
  const shaderDirectory = join(dirname(runner), 'shader', 'vulkanradiosity');
  const args = [
    nativeArgument(shaderDirectory),
    nativeArgument(forestSceneFile),
    '--web',
    body.backend,
    nativeArgument(resultFile),
  ];

  response.writeHead(200, {
    'content-type': 'application/x-ndjson; charset=utf-8',
    'cache-control': 'no-cache',
    'x-content-type-options': 'nosniff',
  });
  const send = (value) => response.write(`${JSON.stringify(value)}\n`);
  send({ type: 'progress', value: 1, stage: '原生运行器已启动' });

  const child = spawn(runner, args, { cwd: projectRoot, windowsHide: true });
  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (chunk) => { stderr += chunk; });

  const lines = createInterface({ input: child.stdout });
  lines.on('line', (line) => {
    const [kind, value, ...stage] = line.split('\t');
    if (kind === 'PROGRESS') {
      send({
        type: 'progress',
        value: Math.round(Number(value) * 0.84),
        stage: stage.join(' '),
      });
    }
  });

  child.on('error', (error) => {
    send({ type: 'error', message: error.message });
    response.end();
  });
  child.on('close', (code) => {
    try {
      if (code !== 0) {
        throw new Error(stderr.trim() || `原生运行器退出，代码 ${code}`);
      }
      const result = JSON.parse(readFileSync(resultFile, 'utf8'));
      send({ type: 'progress', value: 90, stage: '整理逐面元双面辐射度' });
      if (body.energy) {
        const energyStart = performance.now();
        result.energy = energyBalance(result, body.energy);
        result.timing.energyBalanceMs = performance.now() - energyStart;
        result.timing.combinedMs = result.timing.totalMs + result.timing.energyBalanceMs;
      }
      send({ type: 'result', result });
    } catch (error) {
      send({ type: 'error', message: error.message });
    } finally {
      rmSync(resultFile, { force: true });
      response.end();
    }
  });
}

const server = createServer(async (request, response) => {
  const url = new URL(request.url, `http://${request.headers.host || host}`);
  if (request.method === 'GET' && url.pathname === '/api/health') {
    response.writeHead(200, {
      'content-type': 'application/json; charset=utf-8',
      'cache-control': 'no-store',
    });
    response.end(JSON.stringify({ app: 'radiosity-three-gui', ready: true }));
    return;
  }
  if (request.method === 'POST' && url.pathname === '/api/run') {
    await runModel(request, response);
    return;
  }
  if (request.method === 'GET' && url.pathname === '/api/scene') {
    sendFile(response, forestSceneFile);
    return;
  }

  let fileName;
  if (url.pathname.startsWith('/three/')) {
    fileName = join(guiDir, 'node_modules', 'three', url.pathname.slice('/three/'.length));
  } else {
    const requested = url.pathname === '/' ? 'index.html' : url.pathname.slice(1);
    fileName = join(guiDir, requested);
  }
  const safeRoot = url.pathname.startsWith('/three/')
    ? join(guiDir, 'node_modules', 'three')
    : guiDir;
  const normalizedFile = normalize(fileName);
  if (!normalizedFile.startsWith(normalize(safeRoot + sep))) {
    response.writeHead(403).end('Forbidden');
    return;
  }
  sendFile(response, normalizedFile);
});

server.listen(port, host, () => {
  console.log(`Three.js GUI: http://${host}:${port}`);
  const runner = findRunner();
  console.log(runner ? `Native runner: ${runner}` : 'Native runner: not built');
});
