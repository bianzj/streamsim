// Record the exact source and deployed shader binaries of a completed build.
import fs from 'node:fs'
import path from 'node:path'
import crypto from 'node:crypto'
import { execFileSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const runtime = path.resolve(root, process.argv[2] || 'models/bin_x64/Release')
const sha256 = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex')
function inventory(directory, predicate) {
  const files = []
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const file = path.join(directory, entry.name)
    if (entry.isDirectory()) files.push(...inventory(file, predicate))
    else if (entry.isFile() && predicate(file)) files.push(file)
  }
  return files.sort()
}
const sourceFiles = [
  path.join(root, 'models/histream/CMakeLists.txt'),
  ...inventory(path.join(root, 'models/histream/src'), p => /\.(cpp|h|inl)$/.test(p)),
  ...inventory(path.join(root, 'models/histream/shader'), p => /\.(glsl|h|comp|rgen|rchit|rmiss|vert|frag)$/.test(p)),
  path.join(root, 'server.mjs'), path.join(root, 'tools/runtime-scene.mjs'),
  path.join(root, 'src/renderer/src/project-schema.js'),
  path.join(root, 'src/renderer/src/main.js'), path.join(root, 'package.json')
]
const sources = sourceFiles.map(file => ({ path: path.relative(root, file).replaceAll('\\', '/'), sha256: sha256(file) }))
const shaderBinaries = inventory(path.join(runtime, 'shader'), p => p.endsWith('.spv'))
  .map(file => ({ path: path.relative(runtime, file).replaceAll('\\', '/'), sha256: sha256(file) }))
let commit = '', dirty = true
try {
  commit = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' }).trim()
  dirty = Boolean(execFileSync('git', ['status', '--porcelain=v1'], { cwd: root, encoding: 'utf8' }).trim())
} catch { /* A packaged checkout can lack Git metadata. */ }
const engine = path.join(runtime, process.platform === 'win32' ? 'histream.exe' : 'histream')
const manifest = {
  kind: 'streamsim-build', schemaVersion: 1,
  applicationVersion: JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8')).version,
  createdAt: new Date().toISOString(), commit, dirty,
  sourceTreeSha256: crypto.createHash('sha256').update(JSON.stringify(sources)).digest('hex'),
  engine: { path: path.basename(engine), sha256: sha256(engine) },
  sources, shaderBinaries
}
fs.writeFileSync(path.join(runtime, 'build-manifest.json'), JSON.stringify(manifest, null, 2) + '\n')
console.log(JSON.stringify({ version: manifest.applicationVersion, engineSHA256: manifest.engine.sha256,
  sourceTreeSHA256: manifest.sourceTreeSha256, shaderCount: shaderBinaries.length }))
