import { spawnSync } from 'node:child_process'
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { dirname, extname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const engine = join(repoRoot, 'models', 'bin_x64', 'Release', 'histream.exe')
const lut = join(repoRoot, 'assets', 'atmosphere', 'simple_modtran_lut.csv')
const testRoot = join(repoRoot, '.test', 'atmosphere-smoke')
const cases = [
  { mode: 'eVoxelRT', source: 'imaging_voxelrt' },
  { mode: 'eVoxelEB', source: 'physical_voxeleb' },
  { mode: 'eRaytracing', source: 'imaging_raytracing' }
].map((item) => ({
  ...item,
  input: join(repoRoot, 'validation', 'urban_tree_building', 'tests', item.source, 'Input.xml')
}))

for (const path of [engine, lut, ...cases.map((item) => item.input)]) if (!existsSync(path)) throw new Error(`缺少测试文件：${path}`)

const run = (testCase, enabled) => {
  const label = enabled ? 'enabled' : 'disabled'
  const caseRoot = join(testRoot, testCase.mode)
  const output = join(caseRoot, label)
  const input = join(caseRoot, `Input_${label}.xml`)
  rmSync(output, { recursive: true, force: true })
  mkdirSync(output, { recursive: true })
  let xml = readFileSync(testCase.input, 'utf8')
  xml = xml.replace(/<outDir>[\s\S]*?<\/outDir>/, `<outDir>${output}</outDir>`)
  xml = xml.replace(/<viewAngles>[^<]*<\/viewAngles>/, '<viewAngles>15,0</viewAngles>')
  if (!/<sensorPosition>/i.test(xml)) xml = xml.replace('<viewAngle>', '<sensorPosition>25,20,2000</sensorPosition>\n        <viewAngle>')
  xml = xml.replace(/<Atmosphere>[\s\S]*?<\/Atmosphere>\s*/i, '')
  xml = xml.replace('<Scene>', `<Atmosphere><enabled>${enabled ? 1 : 0}</enabled><model>midlatitude-summer</model><waterVapor>2.8</waterVapor><aerosol>urban</aerosol><visibility>31</visibility><lutFile>${lut}</lutFile></Atmosphere>\n  <Scene>`)
  writeFileSync(input, xml, 'utf8')
  const result = spawnSync(engine, [testCase.mode, input], {
    cwd: caseRoot, windowsHide: true, encoding: 'utf8', timeout: 120000
  })
  if (result.status !== 0) throw new Error(`${testCase.mode} 大气查找表测试失败：\n${result.stdout}\n${result.stderr}`)
  const tiff = readdirSync(output).find((name) => ['.tif', '.tiff'].includes(extname(name).toLowerCase()))
  if (!tiff) throw new Error('VoxelRT 大气查找表测试未生成 TIFF')
  const statistics = readdirSync(output).find((name) => /^result_statistics_.*\.csv$/i.test(name))
  if (!statistics) throw new Error(`${testCase.mode} 没有生成波段统计`)
  const means = new Map(readFileSync(join(output, statistics), 'utf8').trim().split(/\r?\n/).slice(1).map((line) => {
    const cells = line.split(',')
    return [Number(cells[5]), Number(cells[10])]
  }))
  return { path: join(output, tiff), means }
}

const results = cases.map((testCase) => {
  const disabled = run(testCase, false)
  const enabled = run(testCase, true)
  if (!enabled.path.toLowerCase().endsWith('_a.tif')) throw new Error(`${testCase.mode} 有大气结果缺少 _a.tif 后缀`)
  if (disabled.path.toLowerCase().endsWith('_a.tif')) throw new Error(`${testCase.mode} 无大气结果错误使用 _a.tif 后缀`)
  if (readFileSync(disabled.path).equals(readFileSync(enabled.path))) throw new Error(`${testCase.mode} 启用大气查找表后 TIFF 没有发生变化`)
  for (const band of [0, 2]) {
    if (!Number.isFinite(disabled.means.get(band)) || !Number.isFinite(enabled.means.get(band))
        || Math.abs(disabled.means.get(band) - enabled.means.get(band)) < 1e-8) {
      throw new Error(`${testCase.mode} 第 ${band + 1} 波段没有应用大气修正`)
    }
  }
  const thermalDeltaKelvin = enabled.means.get(2) - disabled.means.get(2)
  if (Math.abs(thermalDeltaKelvin) > 10) throw new Error(`${testCase.mode} 热红外亮温修正异常：${thermalDeltaKelvin} K`)
  return {
    mode: testCase.mode,
    model: 'midlatitude-summer',
    waterVapor: 2.8,
    aerosol: 'urban',
    visibility: 31,
    disabled: disabled.path,
    enabled: enabled.path,
    opticalChanged: true,
    thermalChanged: true,
    thermalDeltaKelvin: Number(thermalDeltaKelvin.toFixed(4))
  }
})
console.log(JSON.stringify(results, null, 2))
