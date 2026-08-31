import { spawn } from 'node:child_process'

const options = { stdio: 'inherit', shell: true }
const bridge = spawn(process.execPath, ['server.mjs', '--api-only'], options)
const vite = spawn(process.platform === 'win32' ? 'vite.cmd' : 'vite', [], options)

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
