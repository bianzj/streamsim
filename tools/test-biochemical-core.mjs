// Compile the actual Farquhar GLSL helpers and complete dispatch as C++
// float32, with checked buffers. Run: node tools/test-biochemical-core.mjs
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const destination = path.join(root, '.test', 'biochemical-core');
fs.mkdirSync(destination, { recursive: true });
const functions = fs.readFileSync(path.join(root, 'models/histream/shader/functions.glsl'), 'utf8');
function extract(source, name) {
  const match = new RegExp(`(?:float|bool|void) ${name}\\(`).exec(source);
  assert(match, `${name} is missing`);
  const open = source.indexOf('{', match.index);
  let level = 1, end = open + 1;
  while (level && end < source.length) {
    const character = source[end++];
    if (character === '{') ++level;
    if (character === '}') --level;
  }
  assert.equal(level, 0, `${name} has an incomplete body`);
  return source.slice(match.index, end)
    .replace(/\buint\b/g, 'unsigned')
    // Unsuffixed GLSL real constants are float, rather than C++ double.
    .replace(/\b\d+\.\d*(?:e[+-]?\d+)?(?![\w.])|\b\d+e[+-]?\d+(?![\w.])/gi, '$&f');
}
const helpers = ['es_fun', 'biochemicalMissing', 'biochemicalFinite',
  'biochemicalQuadraticRoot', 'biochemicalElectronTransport',
  'biochemicalIntercellularCO2', 'biochemicalResistance'].map(name => extract(functions, name));
fs.writeFileSync(path.join(destination, 'production_helpers.hpp'), helpers.join('\n'));
const shader = fs.readFileSync(path.join(root, 'models/histream/shader/voxeleb/biochemical_m12.comp'), 'utf8');
let dispatch = extract(shader, 'main').replace('void main()', 'void productionDispatch()');
if (process.argv.includes('--trace')) dispatch = dispatch.replace('if(!resultValid)',
  'if(!resultValid) std::fprintf(stderr, "solver Ag=%g A=%g Ci=%g rss=%g Rd=%g J=%g Ja=%g Ac=%g Aj=%g\\n", Ag,A,Ci,rss,Rd,J,Ja,Ac,Aj);\nif(!resultValid)');
fs.writeFileSync(path.join(destination, 'production_dispatch.hpp'), dispatch);
const collatzShader = fs.readFileSync(path.join(root, 'models/histream/shader/voxeleb/biochemical.comp'), 'utf8');
fs.writeFileSync(path.join(destination, 'production_collatz.hpp'),
  extract(collatzShader, 'main').replace('void main()', 'void productionCollatz()'));
const baseline = JSON.parse(fs.readFileSync(path.join(root,
  'tests/biochemical_core/normal-domain-baseline.json'), 'utf8'));
const floatLiteral = value => `${Number(value).toPrecision(10)}f`;
fs.writeFileSync(path.join(destination, 'normal_domain_baseline.hpp'),
  `struct BaselineCase { unsigned method; float type, rh, co2, directPar; float expected[7]; };\n` +
  `const BaselineCase normalDomainBaseline[] = {\n` + baseline.map(item =>
    `{${item.method},${[item.type, item.rh, item.co2, item.directPar].map(floatLiteral).join(',')},` +
    `{${item.expected.map(floatLiteral).join(',')}}}`).join(',\n') + '\n};\n');

function execute(command, arguments_, options = {}) {
  const result = spawnSync(command, arguments_, { cwd: destination, encoding: 'utf8', ...options });
  if (result.error || result.status !== 0) {
    process.stderr.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    throw result.error || new Error(`${command} exited with ${result.status}`);
  }
  return result.stdout;
}
const source = path.join(root, 'tests/biochemical_core/main.cpp');
const executable = path.join(destination, process.platform === 'win32' ? 'biochemical_core.exe' : 'biochemical_core');
if (process.platform === 'win32') {
  const installer = path.join(process.env['ProgramFiles(x86)'] || 'C:/Program Files (x86)',
    'Microsoft Visual Studio', 'Installer', 'vswhere.exe');
  const installation = execute(installer, ['-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath']).trim();
  assert(installation, 'MSVC C++ build tools are required');
  const vcvars = path.join(installation, 'VC/Auxiliary/Build/vcvars64.bat');
  const quote = value => `"${value}"`;
  const arguments_ = ['/nologo', '/std:c++17', '/EHsc', '/O2', '/fp:strict', '/utf-8',
    `/I${quote(destination)}`, `/Fe:${quote(executable)}`, '/Fo:biochemical_core.obj', quote(source)];
  const script = path.join(destination, 'compile.cmd');
  fs.writeFileSync(script, `@echo off\r\ncall ${quote(vcvars)} >nul\r\nif errorlevel 1 exit /b 1\r\ncl.exe ${arguments_.join(' ')}\r\nexit /b %errorlevel%\r\n`);
  execute(process.env.ComSpec || 'cmd.exe', ['/d', '/c', script]);
} else {
  execute(process.env.CXX || 'c++', ['-std=c++17', '-O2', '-ffp-contract=off',
    '-I', destination, source, '-o', executable]);
}
const output = execute(executable, []);
process.stdout.write(output);
fs.writeFileSync(path.join(destination, 'result.json'), output);
