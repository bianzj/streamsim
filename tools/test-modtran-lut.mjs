import { readFileSync } from 'node:fs'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const path = join(repoRoot, 'assets', 'atmosphere', 'simple_modtran_lut.csv')
const rows = readFileSync(path, 'utf8').split(/\r?\n/)
  .filter((line) => line && !line.startsWith('#') && !line.startsWith('atmosphere_model'))
  .map((line) => {
    const cells = line.split(',')
    return [cells[0], cells[1], ...cells.slice(2).map(Number)]
  })

const assert = (condition, message) => {
  if (!condition) throw new Error(message)
}
const unique = (index) => [...new Set(rows.map((row) => row[index]))].sort((a, b) => typeof a === 'number' ? a - b : String(a).localeCompare(String(b)))
const atmosphere = unique(0)
const aerosol = unique(1)
const waterVapor = unique(2)
const visibility = unique(3)
const altitude = unique(4)
const zenith = unique(5)
const wavelength = unique(6)

assert(JSON.stringify(atmosphere) === JSON.stringify(['midlatitude-summer', 'midlatitude-winter', 'tropical']), '大气类型轴错误')
assert(JSON.stringify(aerosol) === JSON.stringify(['rural', 'urban']), '气溶胶类型轴错误')
assert(JSON.stringify(waterVapor) === JSON.stringify([0.5, 2, 5]), '柱状水汽量轴错误')
assert(JSON.stringify(visibility) === JSON.stringify([10, 23, 50]), '能见度轴错误')
assert(JSON.stringify(altitude) === JSON.stringify([1, 3, 10]), '传感器高度轴错误')
assert(JSON.stringify(zenith) === JSON.stringify([0, 30, 60]), '观测角度轴错误')
assert(wavelength[0] === 350 && wavelength.at(-1) === 14000, '光谱范围错误')
assert(rows.length === atmosphere.length * aerosol.length * waterVapor.length * visibility.length * altitude.length * zenith.length * wavelength.length, 'LUT 网格不完整')

const find = (model, aerosolModel, water, visibilityKm, altitudeKm, zenithDeg, wavelengthNm) => rows.find((row) =>
  row[0] === model && row[1] === aerosolModel && row[2] === water && row[3] === visibilityKm
  && row[4] === altitudeKm && row[5] === zenithDeg && row[6] === wavelengthNm)
const optical = find('midlatitude-summer', 'rural', 2, 23, 3, 0, 550)
const thermal = find('midlatitude-summer', 'rural', 2, 23, 3, 0, 10500)
assert(optical && optical[7] > 0 && optical[7] < 1, '可见光透过率无效')
assert(thermal && thermal[7] > 0 && thermal[7] < 1, '热红外透过率无效')
assert(thermal[8] > 0.1 && Number.isFinite(thermal[8]), '热红外路径辐亮度无效')
const dryWaterBand = find('midlatitude-summer', 'rural', 0.5, 23, 3, 0, 940)
const wetWaterBand = find('midlatitude-summer', 'rural', 5, 23, 3, 0, 940)
assert(dryWaterBand && wetWaterBand && Math.abs(dryWaterBand[7] - wetWaterBand[7]) > 1e-4, '柱状水汽量没有改变水汽吸收带')
// Visibility is defined near 550 nm, so aerosol models are intentionally
// almost equal there; compare at 1000 nm where their spectral shapes differ.
const rural = find('midlatitude-summer', 'rural', 2, 10, 3, 0, 1000)
const urban = find('midlatitude-summer', 'urban', 2, 10, 3, 0, 1000)
assert(rural && urban && Math.abs(rural[7] - urban[7]) > 1e-4, '气溶胶类型没有改变近红外结果')
const tropical = find('tropical', 'rural', 2, 23, 3, 0, 10500)
const winter = find('midlatitude-winter', 'rural', 2, 23, 3, 0, 10500)
assert(tropical && winter && Math.abs(tropical[8] - winter[8]) > 1e-4, '大气类型没有改变热红外路径辐射')

console.log(JSON.stringify({
  nodes: atmosphere.length * aerosol.length * waterVapor.length * visibility.length * altitude.length * zenith.length,
  wavelengths: wavelength.length,
  rangeNm: [wavelength[0], wavelength.at(-1)],
  sample550nm: { transmittance: optical[7], pathRadiance: optical[8] },
  sample10500nm: { transmittance: thermal[7], pathRadiance: thermal[8] }
}, null, 2))
