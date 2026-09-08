import { mkdirSync, writeFileSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'

const outputDir = join(dirname(fileURLToPath(import.meta.url)), 'dem_examples')
const width = 60
const height = 60
const noData = -9999

function align(value, boundary) {
  return Math.ceil(value / boundary) * boundary
}

function writeGeoTiff(path, description, elevationAt) {
  const pixels = Buffer.alloc(width * height * 4)
  for (let row = 0; row < height; row += 1) {
    for (let column = 0; column < width; column += 1) {
      pixels.writeFloatLE(elevationAt(column, row), (row * width + column) * 4)
    }
  }

  const descriptionData = Buffer.from(`${description}\0`, 'ascii')
  const noDataData = Buffer.from(`${noData}\0`, 'ascii')
  const geoKeys = [
    1, 1, 0, 4,
    1024, 0, 1, 1,       // GTModelTypeGeoKey: Projected
    1025, 0, 1, 1,       // GTRasterTypeGeoKey: PixelIsArea
    3072, 0, 1, 32650,   // ProjectedCSTypeGeoKey: WGS 84 / UTM zone 50N
    3076, 0, 1, 9001     // ProjLinearUnitsGeoKey: metre
  ]
  const entryCount = 16
  const stripOffset = 8
  const ifdOffset = stripOffset + pixels.length
  const ifdSize = 2 + entryCount * 12 + 4
  let extraOffset = ifdOffset + ifdSize
  const descriptionOffset = extraOffset
  extraOffset += descriptionData.length
  extraOffset = align(extraOffset, 8)
  const pixelScaleOffset = extraOffset
  extraOffset += 24
  const tiePointOffset = extraOffset
  extraOffset += 48
  const geoKeysOffset = extraOffset
  extraOffset += geoKeys.length * 2
  const noDataOffset = extraOffset
  extraOffset += noDataData.length

  const output = Buffer.alloc(extraOffset)
  output.write('II', 0, 2, 'ascii')
  output.writeUInt16LE(42, 2)
  output.writeUInt32LE(ifdOffset, 4)
  pixels.copy(output, stripOffset)
  output.writeUInt16LE(entryCount, ifdOffset)

  let entryIndex = 0
  const entry = (tag, type, count, value, inlineShort = false) => {
    const offset = ifdOffset + 2 + entryIndex++ * 12
    output.writeUInt16LE(tag, offset)
    output.writeUInt16LE(type, offset + 2)
    output.writeUInt32LE(count, offset + 4)
    if (inlineShort) output.writeUInt16LE(value, offset + 8)
    else output.writeUInt32LE(value, offset + 8)
  }
  entry(256, 4, 1, width)
  entry(257, 4, 1, height)
  entry(258, 3, 1, 32, true)
  entry(259, 3, 1, 1, true)
  entry(262, 3, 1, 1, true)
  entry(270, 2, descriptionData.length, descriptionOffset)
  entry(273, 4, 1, stripOffset)
  entry(277, 3, 1, 1, true)
  entry(278, 4, 1, height)
  entry(279, 4, 1, pixels.length)
  entry(284, 3, 1, 1, true)
  entry(339, 3, 1, 3, true)
  entry(33550, 12, 3, pixelScaleOffset)
  entry(33922, 12, 6, tiePointOffset)
  entry(34735, 3, geoKeys.length, geoKeysOffset)
  entry(42113, 2, noDataData.length, noDataOffset)
  output.writeUInt32LE(0, ifdOffset + 2 + entryCount * 12)

  descriptionData.copy(output, descriptionOffset)
  ;[1, 1, 0].forEach((value, index) => output.writeDoubleLE(value, pixelScaleOffset + index * 8))
  ;[0, 0, 0, 500000, 4400000, 0].forEach((value, index) => output.writeDoubleLE(value, tiePointOffset + index * 8))
  geoKeys.forEach((value, index) => output.writeUInt16LE(value, geoKeysOffset + index * 2))
  noDataData.copy(output, noDataOffset)
  writeFileSync(path, output)
}

mkdirSync(outputDir, { recursive: true })
writeGeoTiff(join(outputDir, 'single_slope_60x60m.tif'), 'Single slope DEM; elevation unit: metre', (x) => 2 + 8 * x / (width - 1))
writeGeoTiff(join(outputDir, 'compound_slope_60x60m.tif'), 'Compound slope DEM; elevation unit: metre', (x, y) => {
  const nx = x / (width - 1)
  const ny = y / (height - 1)
  const hill = 5 * Math.exp(-((nx - .68) ** 2 + (ny - .35) ** 2) / .035)
  const valley = 2.5 * Math.exp(-((nx - .28) ** 2 + (ny - .72) ** 2) / .025)
  return 3 + 4 * nx + 2.5 * ny + hill - valley
})

console.log(`Generated DEM examples in ${outputDir}`)
