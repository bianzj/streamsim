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
  chooseExecutable: async () => window.prompt('HiStream 可执行文件路径：', localStorage.getItem('histreamExecutable') || 'C:\\work\\bin_x64\\Debug\\histream.exe'),
  chooseXml: async () => {
    const path = window.prompt('已有工程目录、project.json 或 Input.xml 完整路径：', localStorage.getItem('histreamProject') || '')
    if (!path) return null
    const result = await post('/api/project/open', { path })
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
  saveDistribution: (data) => post("/api/project/distribution", data),
  readText: (path) => post('/api/project/read', { path }),
  saveXml: async ({ path, content, project }) => {
    const result = await post('/api/project/save', { path, content, project })
    localStorage.setItem('histreamProject', result.path)
    return result
  },
  openPath: async (path) => { await post('/api/open', { path }); return '' },
  listResults: (path) => post('/api/results/list', { path }),
  readResult: (path, band = 0) => post('/api/results/read', { path, band }),
  run: (data) => post('/api/run', data),
  stop: () => post('/api/stop'),
  onSimulationEvent: (callback) => {
    const events = new EventSource('/api/events')
    events.onmessage = (event) => callback(JSON.parse(event.data))
    return () => events.close()
  }
}
