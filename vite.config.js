import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vite'

const PROJECT_ROOT = dirname(fileURLToPath(import.meta.url))
const GUI_SOURCE_DIR = join(PROJECT_ROOT, 'src', 'renderer')
const GUI_OUTPUT_DIR = join(PROJECT_ROOT, 'gui')

export default defineConfig({
  root: GUI_SOURCE_DIR,
  server: {
    host: '127.0.0.1',
    port: 5173,
    proxy: {
      '/api': 'http://127.0.0.1:4173'
    }
  },
  build: {
    outDir: GUI_OUTPUT_DIR,
    emptyOutDir: true
  }
})
