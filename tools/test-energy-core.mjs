// Compile the production GLSL scalar conduction/dispatch code as C++ float32.
// No GPU or full engine build is required.  Run: node tools/test-energy-core.mjs
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const destination = path.join(root, '.test', 'energy-core');
fs.mkdirSync(destination, { recursive: true });
const functions = fs.readFileSync(path.join(root, 'models/histream/shader/functions.glsl'), 'utf8');
function extract(name) {
  const match = new RegExp(`(?:float|void) ${name}\\(`).exec(functions);
  assert(match, `${name} is missing`);
  const open = functions.indexOf('{', match.index);
  let level = 1, end = open + 1;
  while (level && end < functions.length) {
    const character = functions[end++];
    if (character === '{') ++level;
    if (character === '}') --level;
  }
  assert.equal(level, 0, `${name} has an incomplete body`);
  return functions.slice(match.index, end)
    .replace(/\binout float (\w+)(?!\w)(?=\[)/g, 'float $1')
    .replace(/\binout float (\w+)/g, 'float &$1')
    .replace(/\bin float /g, 'const float ')
    // GLSL unsuffixed real constants are float, whereas C++ defaults to double.
    .replace(/\b\d+(?:\.\d*(?:e[+-]?\d+)?|e[+-]?\d+)(?![\w.])/gi, '$&f');
}
const production = [
  '#pragma once',
  '#define TLASTNUM 8',
  'inline float max(float a, float b) { return a > b ? a : b; }',
  'inline float clamp(float a, float low, float high) { return a < low ? low : (a > high ? high : a); }',
  ...['soilMoistureFraction', 'Soilheatflux', 'soilHeatFluxSurfaceDerivative'].map(extract),
].join('\n');
fs.writeFileSync(path.join(destination, 'production_soil.hpp'), production);
const scene = fs.readFileSync(path.join(root, 'models/histream/src/base/scene.cpp'), 'utf8');
const buildingStart = scene.indexOf('void preparePrimitiveBuilding(');
assert(buildingStart >= 0, 'Primitive-building input validation is missing');
const buildingEnd = scene.indexOf('\n}', buildingStart) + 2;
assert(buildingEnd > buildingStart);
fs.writeFileSync(path.join(destination, 'production_building.hpp'), scene.slice(buildingStart, buildingEnd));

// Execute the actual zero-carbon/material-resistance branch with bounds-checked
// buffers; a building/water bioId deliberately lies beyond the leaf buffer.
const biochemical = fs.readFileSync(path.join(root, 'models/histream/shader/voxeleb/biochemical_m12.comp'), 'utf8');
const dispatchStart = biochemical.indexOf('voxelHeatFlux[bufferId0].GPPsunlit = 0.0;');
const dispatchEnd = biochemical.indexOf('qLs = leafBios[bioId].qLs;', dispatchStart);
assert(dispatchStart >= 0 && dispatchEnd > dispatchStart);
let dispatch = biochemical.slice(dispatchStart, dispatchEnd)
  .replace(/if\(isParticipatingMedium\(canopy\)\) return;/, 'if(isParticipatingMedium(canopy)) return false;')
  .replace(/\b0\.0\b/g, '0.0f');
assert.match(dispatch, /else if\s*\(typeId == TYPE_VEGETATION\)/,
  'Only vegetation may enter the leaf-material branch');
fs.writeFileSync(path.join(destination, 'production_biochemical.hpp'),
  `bool productionMaterialDispatch(unsigned typeId, unsigned bioId, unsigned bufferId0, unsigned canopyId = 0) {\nfloat rss;\nCanopy canopy;\n${dispatch}\nreturn true;\n}\nreturn false;\n}\n`);

const source = path.join(root, 'tests', 'energy_core', 'main.cpp');
const executable = path.join(destination, process.platform === 'win32' ? 'energy_core.exe' : 'energy_core');
function execute(command, arguments_, options = {}) {
  const result = spawnSync(command, arguments_, { cwd: destination, encoding: 'utf8', ...options });
  if (result.error || result.status !== 0) {
    process.stderr.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    throw result.error || new Error(`${command} exited with ${result.status}`);
  }
  return result.stdout;
}
if (process.platform === 'win32') {
  const installer = path.join(process.env['ProgramFiles(x86)'] || 'C:/Program Files (x86)',
    'Microsoft Visual Studio', 'Installer', 'vswhere.exe');
  const installation = execute(installer, ['-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath']).trim();
  assert(installation, 'MSVC C++ build tools are required for the native test');
  const vcvars = path.join(installation, 'VC', 'Auxiliary', 'Build', 'vcvars64.bat');
  const quote = value => `"${value}"`;
  const compilerArguments = ['/nologo', '/std:c++17', '/EHsc', '/O2', '/fp:strict', '/utf-8',
    `/I${quote(destination)}`, `/Fe:${quote(executable)}`, '/Fo:energy_core.obj', quote(source)];
  const compileScript = path.join(destination, 'compile.cmd');
  fs.writeFileSync(compileScript, `@echo off\r\ncall ${quote(vcvars)} >nul\r\nif errorlevel 1 exit /b 1\r\ncl.exe ${compilerArguments.join(' ')}\r\nexit /b %errorlevel%\r\n`);
  execute(process.env.ComSpec || 'cmd.exe', ['/d', '/c', compileScript]);
} else {
  execute(process.env.CXX || 'c++', ['-std=c++17', '-O2', '-ffp-contract=off',
    '-I', destination, source, '-o', executable]);
}
const output = execute(executable, []);
process.stdout.write(output);
fs.writeFileSync(path.join(destination, 'result.json'), output);
if (process.argv.includes('--prepare-engine') || process.argv.includes('--engine')) {
  const { runEnergyEngineRegression } = await import('../tests/energy_core/engine.mjs');
  const result = runEnergyEngineRegression(root, { prepareOnly: !process.argv.includes('--engine') });
  process.stdout.write(`${JSON.stringify(result)}\n`);
}
