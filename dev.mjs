import { spawn } from 'node:child_process'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'

const PROJECT_ROOT = dirname(fileURLToPath(import.meta.url))
const PROCESS_OPTIONS = { cwd: PROJECT_ROOT, stdio: 'inherit', windowsHide: true }
const bridge = spawn(process.execPath, [join(PROJECT_ROOT, 'server.mjs'), '--api-only'], PROCESS_OPTIONS)
const vite = spawn(process.execPath, [join(PROJECT_ROOT, 'node_modules', 'vite', 'bin', 'vite.js')], PROCESS_OPTIONS)

function stop() {
  bridge.kill()
  vite.kill()
}

process.on('SIGINT', stop)
process.on('SIGTERM', stop)
vite.on('exit', (code) => {
  bridge.kill()
  process.exit(code ?? 0)
})
