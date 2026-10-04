import assert from 'node:assert/strict'
import fs from 'node:fs'
import vm from 'node:vm'

const source = fs.readFileSync(new URL('../src/renderer/src/main.js', import.meta.url), 'utf8')
function extract(name) {
  const match = new RegExp(`(?:async )?function ${name}\\(`).exec(source)
  assert.ok(match, name)
  const rest = source.slice(match.index)
  const next = rest.slice(1).search(/\n(?:async )?function /)
  return rest.slice(0, next < 0 ? undefined : next + 1)
}
const state = {
  inputPath: 'D:/examples/photovoltaic/project.json', mode: 'eFacetEB',
  config: { outDir: 'output', sensor: {} }, outputDir: 'output', xmlDirty: false
}
const outputDir = 'D:/examples/photovoltaic/output/runs/test-run'
const messages = []
const context = vm.createContext({
  state, Date,
  api: { run: async () => ({ pid: 123, runId: 'test-run', outputDir }) },
  setRunning: value => { state.running = value },
  toast: (...values) => messages.push(values),
  addLog: () => {}, t: text => text,
  renderRunProgress: () => {},
  simulationMethodSummary: () => '',
  simulationModeLabels: { eFacetEB: 'FacetEB' }
})
vm.runInContext(extract('runSimulation'), context)
vm.runInContext(extract('handleSimulationEvent'), context)
await vm.runInContext('runSimulation()', context)
assert.equal(state.outputDir, outputDir)
assert.equal(state.config.outDir, 'output', 'saved configuration remains a base directory')
vm.runInContext(`handleSimulationEvent(${JSON.stringify({ type: 'started', pid: 123, runId: 'test-run', outputDir })})`, context)
vm.runInContext(`handleSimulationEvent(${JSON.stringify({ type: 'closed', code: 0, elapsed: 5, outputDir })})`, context)
assert.equal(state.outputDir, outputDir, 'success must keep the current run directory')
assert.equal(state.progress, 100)
vm.runInContext(`handleSimulationEvent(${JSON.stringify({ type: 'closed', code: 2, status: 'non_converged', elapsed: 5, outputDir })})`, context)
assert.equal(state.outputDir, outputDir, 'unconverged results remain accessible')
assert.match(state.progressStage, /未收敛/)
assert.match(messages.at(-1)[1], /结果已保留/)
assert.equal(state.running, false)
console.log('PASS: current run output survives started/completed/non-converged UI events')
