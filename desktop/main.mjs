import { app, BrowserWindow, dialog, screen, shell } from 'electron'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { startDesktopServer } from './local-server.mjs'

const sourceRoot = dirname(dirname(fileURLToPath(import.meta.url)))
const smokeTest = process.argv.includes('--smoke-test')
let mainWindow = null

const hasSingleInstanceLock = app.requestSingleInstanceLock()

if (!hasSingleInstanceLock) {
  app.quit()
} else {
  app.on('second-instance', () => {
    if (!mainWindow) return
    if (mainWindow.isMinimized()) mainWindow.restore()
    mainWindow.show()
    mainWindow.focus()
  })
  app.whenReady().then(createWindow).catch((error) => {
    const message = error?.stack || String(error)
    console.error(message)
    if (!smokeTest) dialog.showErrorBox('StreamSim 启动失败', message)
    app.quit()
  })
  app.on('window-all-closed', () => app.quit())
}

function resourceRoot() {
  return app.isPackaged ? process.resourcesPath : sourceRoot
}

async function waitForServer(url, timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs
  let lastError
  while (Date.now() < deadline) {
    try {
      const response = await fetch(`${url}/api/defaults`)
      if (response.ok) return
    } catch (error) {
      lastError = error
    }
    await new Promise((resolve) => setTimeout(resolve, 150))
  }
  throw lastError || new Error('StreamSim 本地服务启动超时')
}

async function createWindow() {
  const root = resourceRoot()
  const dataRoot = process.env.LOCALAPPDATA
    ? join(process.env.LOCALAPPDATA, 'StreamSim')
    : app.getPath('userData')
  process.env.STREAMSIM_RESOURCE_ROOT = root
  process.env.STREAMSIM_DATA_ROOT = dataRoot
  process.env.STREAMSIM_SERVER_AUTOSTART = '0'
  process.env.HISTREAM_ROOT = join(root, 'engine')
  process.env.PATH = `${join(root, 'engine')};${process.env.PATH || ''}`

  const { server } = await import('../server.mjs')
  const url = await startDesktopServer(server)
  await waitForServer(url)

  const workArea = screen.getPrimaryDisplay().workArea
  // Use a conventional 16:9 startup window while keeping it inside the
  // usable display area. Users can still resize it after launch.
  const aspectRatio = 16 / 9
  let height = workArea.height
  let width = Math.floor(height * aspectRatio)
  if (width > workArea.width) {
    width = workArea.width
    height = Math.floor(width / aspectRatio)
  }

  mainWindow = new BrowserWindow({
    width,
    height,
    x: workArea.x + Math.round((workArea.width - width) / 2),
    y: workArea.y + Math.round((workArea.height - height) / 2),
    minWidth: Math.min(1100, workArea.width),
    minHeight: Math.min(720, workArea.height),
    icon: join(root, 'branding', 'streamsim.ico'),
    show: false,
    backgroundColor: '#0b1118',
    autoHideMenuBar: true,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  })

  mainWindow.webContents.setWindowOpenHandler(({ url: target }) => {
    if (/^https?:/i.test(target)) shell.openExternal(target)
    return { action: 'deny' }
  })
  mainWindow.webContents.on('will-navigate', (event, target) => {
    if (!target.startsWith(url)) {
      event.preventDefault()
      if (/^https?:/i.test(target)) shell.openExternal(target)
    }
  })
  mainWindow.once('ready-to-show', () => {
    if (!smokeTest) mainWindow.show()
  })
  mainWindow.on('closed', () => { mainWindow = null })
  await mainWindow.loadURL(url)
  if (smokeTest) {
    console.log(`STREAMSIM_SMOKE_TEST_OK ${url}`)
    app.quit()
  }
}
