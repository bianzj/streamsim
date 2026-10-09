// CPU-only analytic checks of the production energy-output helper.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const root = path.resolve(import.meta.dirname, '..');
const destination = path.join(root, '.test', 'energy-output');
fs.mkdirSync(destination, { recursive: true });
const source = path.join(root, 'tests/energy_output/main.cpp');
const include = path.join(root, 'models/histream/src/base');
const executable = path.join(destination, process.platform === 'win32' ? 'energy_output.exe' : 'energy_output');
function execute(command, args) {
  const result = spawnSync(command, args, { cwd: destination, encoding: 'utf8', windowsHide: true });
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
  const script = path.join(destination, 'compile.cmd');
  fs.writeFileSync(script, `@echo off\r\ncall ${quote(path.join(installation, 'VC/Auxiliary/Build/vcvars64.bat'))} >nul\r\n` +
    `if errorlevel 1 exit /b 1\r\ncl.exe /nologo /std:c++17 /EHsc /O2 /fp:strict /utf-8 ` +
    `/I${quote(include)} /Fe:${quote(executable)} /Fo:energy_output.obj ${quote(source)}\r\n` +
    'exit /b %errorlevel%\r\n');
  execute(process.env.ComSpec || 'cmd.exe', ['/d', '/c', script]);
} else {
  execute(process.env.CXX || 'c++', ['-std=c++17', '-O2', '-ffp-contract=off', '-I', include, source, '-o', executable]);
}
const output = execute(executable, []);
fs.writeFileSync(path.join(destination, 'result.json'), output);
process.stdout.write(output);
