import assert from 'node:assert/strict'
import { createServer } from 'node:http'
import { startDesktopServer } from '../desktop/local-server.mjs'

// Occupy the old desktop port unless the user's web service already owns it.
const blocker = createServer((request, response) => response.end('existing service'))
const ownsBlocker = await new Promise((resolve, reject) => {
  blocker.once('error', error => error.code === 'EADDRINUSE' ? resolve(false) : reject(error))
  blocker.listen(4173, '127.0.0.1', () => resolve(true))
})
const servers = []
try {
  const urls = []
  for (let i = 0; i < 2; i++) {
    const server = createServer((request, response) => response.end(`desktop ${i}`))
    servers.push(server)
    const url = await startDesktopServer(server)
    assert.notEqual(new URL(url).port, '4173')
    assert.equal(await (await fetch(url)).text(), `desktop ${i}`)
    urls.push(url)
  }
  assert.notEqual(urls[0], urls[1])
  assert.equal((await fetch('http://127.0.0.1:4173/')).status, 200)
  console.log('PASS: port 4173 occupied; two isolated desktop servers work; existing service preserved')
} finally {
  for (const server of servers) {
    server.closeAllConnections()
    await new Promise(resolve => server.close(resolve))
  }
  if (ownsBlocker) {
    blocker.closeAllConnections()
    await new Promise(resolve => blocker.close(resolve))
  }
}
