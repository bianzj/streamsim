async function request(path, options) {
  const response = await fetch(path, options)
  const result = await response.json()
  if (!response.ok) throw new Error(result.error || `请求失败：${response.status}`)
  return result
}

function post(path, value = {}) {
  return request(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(value) })
}

function chooseFile(accept) {
  return new Promise((resolve) => {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = accept
    input.oncancel = () => resolve(null)
    input.onchange = async () => {
      const file = input.files?.[0]
      resolve(file ? { path: file.name, content: await file.text() } : null)
    }
    input.click()
  })
}

export const webApi = {
  defaults: () => request('/api/defaults'),
  openProject: (path) => post('/api/project/open', { path }),
  chooseExecutable: async () => window.prompt('HiStream 可执行文件路径：', localStorage.getItem('histreamExecutable') || ''),
  chooseXml: async () => {
    const result = await post('/api/project/pick')
    if (!result?.path || result.cancelled) return null
    localStorage.setItem('histreamProject', result.path)
    return result
  },
  createProject: async (project) => {
    const result = await post('/api/project/create', project)
    localStorage.setItem('histreamProject', result.path)
    return result
  },
  chooseObj: () => chooseFile('.obj,text/plain'),
  importObj: ({ name, content }) => post('/api/project/import-obj', { name, content }),
  importMeteo: ({ name, content }) => post('/api/project/import-meteo', { name, content }),
  importDem: ({ name, contentBase64, projectPath }) => post('/api/project/import-dem', { name, contentBase64, projectPath }),
  inspectDem: ({ path, projectPath }) => post('/api/project/dem-info', { path, projectPath }),
  importSpectrum: ({ name, content }) => post('/api/project/import-spectrum', { name, content }),
  saveDistribution: (data) => post("/api/project/distribution", data),
  readText: (path) => post('/api/project/read', { path }),
  saveXml: async ({ path, content, project }) => {
    const result = await post('/api/project/save', { path, content, project })
    localStorage.setItem('histreamProject', result.path)
    return result
  },
  saveProjectAs: async (project) => {
    const result = await post('/api/project/save-as', project)
    localStorage.setItem('histreamProject', result.path)
    return result
  },
  openPath: async (path) => { await post('/api/open', { path }); return '' },
  listResults: (path) => post('/api/results/list', { path }),
  deleteResults: (path, kind = 'image') => post('/api/results/delete-all', { path, kind }),
  readResult: (path, band = 0) => post('/api/results/read', { path, band }),
  compareResults: (firstPath, secondPath, band = 0, maxScatterPoints = 4000) => post('/api/results/change', { firstPath, secondPath, band, maxScatterPoints }),
  run: (data) => post('/api/run', data),
  stop: () => post('/api/stop'),
  reset: () => post('/api/reset'),
  onSimulationEvent: (callback) => {
    const events = new EventSource('/api/events')
    events.onmessage = (event) => callback(JSON.parse(event.data))
    return () => events.close()
  }
}
