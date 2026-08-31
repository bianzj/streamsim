import { resolve } from 'node:path'
import { defineConfig } from 'vite'

export default defineConfig({
  root: resolve('src/renderer'),
  server: {
    host: '127.0.0.1',
    port: 5173,
    proxy: {
      '/api': 'http://127.0.0.1:4173'
    }
  },
  build: {
    outDir: resolve('dist'),
    emptyOutDir: true
  }
})
