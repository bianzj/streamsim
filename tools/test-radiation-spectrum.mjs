// Compile the production spectrum helper without external engine dependencies.
// Run: node tools/test-radiation-spectrum.mjs
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const destination = path.join(root, '.test', 'radiation-spectrum');
fs.mkdirSync(destination, { recursive: true });
const fixtures = path.join(destination, 'fixtures');
fs.mkdirSync(fixtures, { recursive: true });
// Extract the real reader's normalization loop so reversing the fractions or
// missing trapezoidal endpoint weights cannot pass through a copied formula.
const fileio = fs.readFileSync(path.join(root, 'models/histream/src/base/fileio.cpp'), 'utf8');
const fractionStart = fileio.indexOf('const float directFraction = m_pVoxelebXml->lightxml.direct;');
assert(fractionStart >= 0, 'Configured shortwave fraction is missing from the reader');
const loopStart = fileio.indexOf('for (int i = 0; i < b1; i++)', fractionStart);
assert(loopStart >= 0, 'Production shortwave normalization loop is missing');
const loopOpen = fileio.indexOf('{', loopStart);
let level = 1, loopEnd = loopOpen + 1;
while (level && loopEnd < fileio.length) {
  if (fileio[loopEnd] === '{') ++level;
  if (fileio[loopEnd] === '}') --level;
  ++loopEnd;
}
assert.equal(level, 0, 'Production shortwave normalization loop is incomplete');
fs.writeFileSync(path.join(destination, 'production_normalization.hpp'),
  '#pragma once\nvoid productionShortwaveCoefficients(const std::vector<float>& esun_,\n' +
  'const std::vector<float>& esky_, float TsEsun, float TsEsky, float directFraction,\n' +
  'std::vector<float>& fesun_, std::vector<float>& fesky_) {\n' +
  'int b1 = static_cast<int>(esun_.size());\n' + fileio.slice(loopStart, loopEnd) + '\n}\n');
const source = path.join(root, 'tests/radiation_spectrum/main.cpp');
const includeDirectory = path.join(root, 'models/histream/src/base');
const executable = path.join(destination, process.platform === 'win32' ? 'radiation_spectrum.exe' : 'radiation_spectrum');
function execute(command, args) {
  const result = spawnSync(command, args, { cwd: destination, encoding: 'utf8' });
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
    `/I${quote(includeDirectory)} /I${quote(destination)} /Fe:${quote(executable)} /Fo:radiation_spectrum.obj ${quote(source)}\r\n` +
    'exit /b %errorlevel%\r\n');
  execute(process.env.ComSpec || 'cmd.exe', ['/d', '/c', script]);
} else {
  execute(process.env.CXX || 'c++', ['-std=c++17', '-O2', '-ffp-contract=off', '-I',
    includeDirectory, '-I', destination, source, '-o', executable]);
}
const output = execute(executable, [fixtures]);
process.stdout.write(output);
fs.writeFileSync(path.join(destination, 'result.json'), output);
