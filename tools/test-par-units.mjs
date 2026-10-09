// Compile the production GLSL PAR conversion and consumer expressions as float32.
// Run: node tools/test-par-units.mjs (no GPU or engine build required).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const destination = path.join(root, '.test', 'par-units');
fs.mkdirSync(destination, { recursive: true });
const shader = relative => fs.readFileSync(path.join(root, 'models/histream/shader', relative), 'utf8');
const global = shader('global.glsl');
const direct = shader('voxeleb/voxelrad_direct_VNIR.comp');
const diffuse = shader('voxeleb/voxelrad_diffuse_VNIR_single.comp');
const collatz = shader('voxeleb/biochemical.comp');
const farquhar = shader('voxeleb/biochemical_m12.comp');
const constants = global.match(/^#define AHC\s+[^\r\n]+/m)?.[0];
assert(constants, 'Production photon-energy constant is missing');
function float32(text) {
  return text.replace(/\b\d+(?:\.\d*(?:e[+-]?\d+)?|e[+-]?\d+)(?![\w.])/gi, '$&f');
}
function extractFunction(source, name) {
  const match = new RegExp(`(?:float|int) ${name}\\(`).exec(source);
  assert(match, `Production helper ${name} is missing`);
  const open = source.indexOf('{', match.index);
  let level = 1, end = open + 1;
  while (level && end < source.length) {
    if (source[end] === '{') ++level;
    if (source[end] === '}') --level;
    ++end;
  }
  assert.equal(level, 0, `${name} is incomplete`);
  return float32(source.slice(match.index, end));
}
function producer(name, source, power) {
  const parBlock = source.match(/if\s*\(([^{};]*)\)\s*\{\s*(float pnet = [^;]+;)/);
  assert(parBlock, `${name} PAR write and wavelength gate are missing`);
  const [, condition, statement] = parBlock;
  assert.match(statement, /absorbedParPhotonFlux\(/,
    `${name} must share the photon-unit conversion`);
  assert.match(source, /voxelPnets\[bufferId0\]\.(?:direct|diffuse)Pnet = (?:0|0\.0);/,
    `${name} must reset PAR each dispatch`);
  assert.match(source, /kband_end = shortwaveGroupEnd\(kband_start, kstep, NUM_VNIR\)/,
    `${name} must preserve the PAR group boundary`);
  assert.match(source, /kband_center = \(kband_(?:start|end) \+ kband_(?:end|start) - 1\) \/ 2/,
    `${name} must use the same representative wavelength`);
  assert.match(source, /kband_start = kband_end;/,
    `${name} must advance from the actual split group end`);
  return `float ${name}(float ${power}, float wavelengthNm) {\n` +
    `struct { float wavelength; } atomConds[1] = {{wavelengthNm}};\n` +
    `int kband_center = 0;\nif (${float32(condition)}) {\n${float32(statement)}\n` +
    `return pnet;\n}\nreturn 0.0f;\n}`;
}
function component(name, source, condition) {
  const start = source.indexOf('Q = voxelPnets[bufferId0].diffusePnet;');
  assert(start >= 0, `${name} diffuse PAR input is missing`);
  const tail = source.slice(start);
  const end = tail.indexOf('Q += voxelPnets[bufferId0].directPnet;') +
    'Q += voxelPnets[bufferId0].directPnet;'.length;
  assert(end > 0, `${name} direct PAR input is missing`);
  let expressions = tail.slice(0, end);
  if (expressions.includes('{')) expressions += '\n}';
  return `float ${name}(float diffuse, float direct, int ${condition}) {\n` +
    `struct { float diffusePnet, directPnet; } voxelPnets[1] = {{diffuse, direct}};\n` +
    `int bufferId0 = 0; float Q = 0.0f;\n${float32(expressions)}\nreturn Q;\n}`;
}
const collatzJe = collatz.match(/^\s*Je = [^;]+;/m)?.[0];
const farquharQ2 = farquhar.match(/^\s*Q2 = [^;]+;/m)?.[0];
assert(collatzJe && farquharQ2, 'Production light-limitation expressions are missing');
assert.doesNotMatch(collatzJe, /\b1000\b/, 'Collatz cannot compensate a millimole buffer anymore');
const production = [
  '#pragma once', float32(constants),
  'inline int min(int a, int b) { return a < b ? a : b; }',
  'inline int clamp(int a, int lo, int hi) { return a < lo ? lo : (a > hi ? hi : a); }',
  extractFunction(global, 'absorbedParPhotonFlux'),
  extractFunction(global, 'shortwaveGroupEnd'),
  producer('productionDirectPar', direct, 'rad'),
  producer('productionDiffusePar', diffuse, 'rads'),
  component('productionCollatzQ', collatz, 'issunlit'),
  component('productionFarquharQ', farquhar, 'kt'),
  `float productionCollatzJe(float Q, float po0, float fPAR) {\nfloat Je;\n${float32(collatzJe)}\nreturn Je;\n}`,
  `float productionFarquharQ2(float Q, float beta, float po0) {\nfloat Q2;\n${float32(farquharQ2)}\nreturn Q2;\n}`,
].join('\n');
fs.writeFileSync(path.join(destination, 'production_par.hpp'), production);
const source = path.join(root, 'tests/par_units/main.cpp');
const executable = path.join(destination, process.platform === 'win32' ? 'par_units.exe' : 'par_units');
function execute(command, args, options = {}) {
  const result = spawnSync(command, args, { cwd: destination, encoding: 'utf8', ...options });
  if (result.error || result.status !== 0) {
    process.stderr.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    throw result.error || new Error(`${command} exited with ${result.status}`);
  }
  return result.stdout;
}
if (process.platform === 'win32') {
  const installer = path.join(process.env['ProgramFiles(x86)'] || 'C:/Program Files (x86)',
    'Microsoft Visual Studio/Installer/vswhere.exe');
  const installation = execute(installer, ['-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath']).trim();
  assert(installation, 'MSVC C++ tools are required');
  const quote = value => `"${value}"`;
  const vcvars = path.join(installation, 'VC/Auxiliary/Build/vcvars64.bat');
  const script = path.join(destination, 'compile.cmd');
  fs.writeFileSync(script, `@echo off\r\ncall ${quote(vcvars)} >nul\r\n` +
    `if errorlevel 1 exit /b 1\r\ncl.exe /nologo /std:c++17 /EHsc /O2 /fp:strict /utf-8 ` +
    `/I${quote(destination)} /Fe:${quote(executable)} /Fo:par_units.obj ${quote(source)}\r\n` +
    'exit /b %errorlevel%\r\n');
  execute(process.env.ComSpec || 'cmd.exe', ['/d', '/c', script]);
} else {
  execute(process.env.CXX || 'c++', ['-std=c++17', '-O2', '-ffp-contract=off', '-I',
    destination, source, '-o', executable]);
}
const output = execute(executable, []);
process.stdout.write(output);
fs.writeFileSync(path.join(destination, 'result.json'), output);
