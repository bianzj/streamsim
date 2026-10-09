// Compile the actual production token parser without Vulkan/GDAL dependencies.
// Run: node tools/test-text-input.mjs
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const destination = path.join(root, '.test', 'text-input');
fs.mkdirSync(destination, { recursive: true });
const utils = fs.readFileSync(path.join(root, 'models/histream/src/base/utils.cpp'), 'utf8');
const signature = 'std::vector<std::string> Utils::splitt(';
const start = utils.indexOf(signature);
assert(start >= 0, 'Production text parser is missing');
const open = utils.indexOf('{', start);
let level = 1, end = open + 1;
while (level && end < utils.length) {
  if (utils[end] === '{') ++level;
  if (utils[end] === '}') --level;
  ++end;
}
assert.equal(level, 0, 'Production text parser has an incomplete body');
const declaration = 'namespace Utils { std::vector<std::string> splitt(std::string&, std::string&); }';
fs.writeFileSync(path.join(destination, 'production_split.hpp'),
  `#pragma once\n${declaration}\n${utils.slice(start, end)}\n`);
const source = path.join(root, 'tests/text_input/main.cpp');
const executable = path.join(destination, process.platform === 'win32' ? 'text_input.exe' : 'text_input');
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
    `if errorlevel 1 exit /b 1\r\ncl.exe /nologo /std:c++17 /EHsc /O2 /utf-8 ` +
    `/I${quote(destination)} /Fe:${quote(executable)} /Fo:text_input.obj ${quote(source)}\r\n` +
    'exit /b %errorlevel%\r\n');
  execute(process.env.ComSpec || 'cmd.exe', ['/d', '/c', script]);
} else {
  execute(process.env.CXX || 'c++', ['-std=c++17', '-O2', '-I', destination, source, '-o', executable]);
}
const output = execute(executable, []);
process.stdout.write(output);
fs.writeFileSync(path.join(destination, 'result.json'), output);
