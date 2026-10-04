// CPU tests compile production functions without transcription. --engine also
// runs two tiny Vulkan scenes serially; run after building the current engine.
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawnSync } from 'node:child_process'
import { createDefaultProject, validateProject } from '../src/renderer/src/project-schema.js'

const root = fileURLToPath(new URL('../', import.meta.url))
const shader = fs.readFileSync(path.join(root, 'models/histream/shader/voxelrt/voxelrad_diffuse_single.comp'), 'utf8')
const functions = fs.readFileSync(path.join(root, 'models/histream/shader/functions.glsl'), 'utf8')
const background = fs.readFileSync(path.join(root, 'models/histream/shader/background_hapke.glsl'), 'utf8')
const water = fs.readFileSync(path.join(root, 'models/histream/shader/water_brdf.glsl'), 'utf8')
const cpu = fs.readFileSync(path.join(root, 'models/histream/src/voxelrt/voxelrt.cpp'), 'utf8')
fs.mkdirSync(path.join(root, 'tmp'), { recursive: true })
const work = fs.mkdtempSync(path.join(root, 'tmp/voxelrt-radiation-'))

function extractFunction(source, signature) {
  const start = source.indexOf(signature)
  assert.ok(start >= 0, signature)
  const body = source.indexOf('{', start)
  let depth = 1, end = body + 1
  while (depth > 0 && end < source.length) {
    if (source[end] === '{') ++depth
    if (source[end] === '}') --depth
    ++end
  }
  assert.equal(depth, 0)
  return source.slice(start, end)
}

function command(executable, args, options = {}) {
  const result = spawnSync(executable, args, {
    cwd: root, encoding: 'utf8', windowsHide: true, timeout: 120000,
    maxBuffer: 8 * 1024 * 1024, ...options
  })
  assert.equal(result.status, 0,
    `${executable}: ${result.error?.message || ''}\n${result.stdout || ''}\n${result.stderr || ''}`)
  return result.stdout
}

function findCmake() {
  if (process.env.CMAKE) return process.env.CMAKE
  const vs = path.join(process.env.ProgramFiles || 'C:/Program Files', 'Microsoft Visual Studio')
  if (fs.existsSync(vs)) {
    for (const version of fs.readdirSync(vs).sort().reverse()) {
      const directory = path.join(vs, version)
      if (!fs.statSync(directory).isDirectory()) continue
      for (const edition of fs.readdirSync(directory)) {
        const exe = path.join(directory, edition, 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
        if (fs.existsSync(exe)) return exe
      }
    }
  }
  return 'cmake'
}

// GDAL's uncompressed Float32 TIFF output; this deliberately rejects other
// encodings so a changed writer cannot silently bypass rendered-value checks.
function readFloat32Tiff(file) {
  const data = fs.readFileSync(file)
  const le = data.subarray(0, 2).toString() === 'II'
  assert.ok(le || data.subarray(0, 2).toString() === 'MM')
  const u16 = offset => le ? data.readUInt16LE(offset) : data.readUInt16BE(offset)
  const u32 = offset => le ? data.readUInt32LE(offset) : data.readUInt32BE(offset)
  const f32 = offset => le ? data.readFloatLE(offset) : data.readFloatBE(offset)
  assert.equal(u16(2), 42, 'Expected classic TIFF')
  const ifd = u32(4), tags = new Map()
  for (let i = 0; i < u16(ifd); ++i) {
    const entry = ifd + 2 + i * 12, tag = u16(entry), type = u16(entry + 2), count = u32(entry + 4)
    if (![256, 257, 258, 259, 273, 277, 279, 284, 339].includes(tag)) continue
    const size = type === 3 ? 2 : type === 4 ? 4 : 1
    const start = size * count <= 4 ? entry + 8 : u32(entry + 8)
    tags.set(tag, Array.from({ length: count }, (_, j) =>
      type === 3 ? u16(start + 2 * j) : type === 4 ? u32(start + 4 * j) : data[start + j]))
  }
  const width = tags.get(256)[0], height = tags.get(257)[0], bands = tags.get(277)?.[0] || 1
  assert.equal(tags.get(259)?.[0] || 1, 1, 'TIFF compression unsupported by test')
  assert.ok(tags.get(258).every(bits => bits === 32), 'Expected Float32 output')
  assert.ok(tags.get(339).every(format => format === 3), 'Expected IEEE floats')
  const flattened = []
  tags.get(273).forEach((offset, i) => {
    for (let j = 0; j < tags.get(279)[i]; j += 4) flattened.push(f32(offset + j))
  })
  assert.equal(flattened.length, width * height * bands)
  const planar = tags.get(284)?.[0] || 1
  return { width, height, bands, pixels: Array.from({ length: bands }, (_, band) =>
    Array.from({ length: width * height }, (_, pixel) =>
      flattened[planar === 2 ? band * width * height + pixel : pixel * bands + band])) }
}

fs.writeFileSync(path.join(work, 'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.15)
project(voxelrt_radiation_limits LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(voxelrt_radiation_limits main.cpp)
if(MSVC)
 target_compile_options(voxelrt_radiation_limits PRIVATE /utf-8)
endif()
`)
fs.writeFileSync(path.join(work, 'main.cpp'), `#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
float clamp(float x, float low, float high) {return std::clamp(x, low, high);}
float max(float x, float y) {return std::max(x,y);}
float mix(float x, float y, float a) {return (1-a)*x+a*y;}
struct vec3 {float x,y,z; vec3 operator-() const {return {-x,-y,-z};}};
float dot(vec3 a,vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
vec3 normalize(vec3 a) {float l=std::sqrt(dot(a,a)); return {a.x/l,a.y/l,a.z/l};}
using uint=unsigned;
constexpr uint TYPE_WATER=3;
struct MeshLink {uint type,spectralId,bioId; float angularEffectStrength;};
struct SpectralMaterial {float reflectance,transmittance;};
struct WaterSet {int brdfModel; float refractiveIndex,diffuseFraction;};
WaterSet waterSets[2]={{0,1.333f,0},{1,1.333f,0}};
SpectralMaterial spectralMaterials[8]{};
struct Setting {int n_wave;} setting{4};
${extractFunction(functions, 'float Planck(')}
${extractFunction(functions, 'float Emissivity_Hapke(')}
${extractFunction(background, 'vec3 surfaceHemisphereDirection(')}
${extractFunction(background, 'float surfaceThermalEmissivity(')}
${extractFunction(water, 'float waterDielectricFresnel(')}
${extractFunction(water, 'float waterDirectionalReflectance(')}
${extractFunction(shader, 'float voxelSurfaceEmissivity(')}
${extractFunction(shader, 'float voxelSurfaceEmission(')}
${extractFunction(cpu, 'void writeVoxelrtProcessField(')}
void close(float actual, float expected) {
 if(!std::isfinite(actual) || std::fabs(actual-expected)>2e-6f*std::max(1.0f,std::fabs(expected)))
  throw std::runtime_error("thermal emission limit failed");
}
int main() {
 spectralMaterials[6]={0.2f,0.1f};
 MeshLink material{TYPE_WATER,1,0,0};
 close(voxelSurfaceEmissivity(material,2,{0,1,0},{0,1,0}),0.7f);
 material.bioId=1;
 const float fresnel=std::pow((1.333f-1)/(1.333f+1),2);
 close(voxelSurfaceEmissivity(material,2,{0,1,0},{0,1,0}),0.9f-fresnel);
 material.type=2;
 close(voxelSurfaceEmissivity(material,2,{0,1,0},{0,1,0}),0.7f);
 const float b=Planck(10500,300), hot=Planck(10500,320), cold=Planck(10500,280);
 close(voxelSurfaceEmission(10500,300,300,0,1,1),b);
 close(voxelSurfaceEmission(10500,300,300,1,1,1),b);
 close(voxelSurfaceEmission(10500,300,300,0.2f,0.95f,1),0.95f*b);
 close(voxelSurfaceEmission(10500,320,280,0.25f,0.8f,0.3f),
       (0.25f*hot+0.75f*cold)*0.8f*0.3f);
 close(voxelSurfaceEmission(10500,300,300,0.5f,0.95f,0),0);
 close(voxelSurfaceEmission(10500,300,300,0.5f,0,1),0);
 close(voxelSurfaceEmission(660,300,300,0.5f,1,1),0);
 close(voxelSurfaceEmission(10500,320,280,2,2,2),hot);
 // Kirchhoff closure when the incoming enclosure is the same blackbody.
 close(voxelSurfaceEmission(10500,300,300,0.5f,0.95f,1)+0.05f*b,b);
 std::cout << "{\\"passed\\":true,\\"blackbody_10500_W_m2_sr_um\\":" << b << ",\\"fields\\":[";
 writeVoxelrtProcessField(std::cout,660,3); std::cout << ',';
 writeVoxelrtProcessField(std::cout,10500,5); std::cout << "]}\\n";
}
`)
const cmake = findCmake()
command(cmake, ['-S', work, '-B', path.join(work, 'build')])
command(cmake, ['--build', path.join(work, 'build'), '--config', 'Release', '--parallel', '2'])
const native = path.join(work, 'build', process.platform === 'win32' ? 'Release/voxelrt_radiation_limits.exe' : 'voxelrt_radiation_limits')
const limits = JSON.parse(command(native, []))
assert.equal(limits.fields[0].id, 'illumination_660')
assert.equal(limits.fields[0].quantity, 'incoming_normalized_illumination')
assert.equal(limits.fields[0].unit, '1')
assert.doesNotMatch(limits.fields[0].label, /W|sr|μm/)
assert.deepEqual(limits.fields[0].aliases, ['radiosity_660'])
assert.equal(limits.fields[1].quantity, 'outgoing_spectral_radiance')
assert.equal(limits.fields[1].unit, 'W m-2 sr-1 um-1')
assert.equal(limits.fields[1].offset, 5)
console.log('Production thermal emission limits and process field metadata: passed')

// A closed, isothermal blackbody ceiling/walls illuminate a grey ground plane.
// Camera is inside the enclosure. This detects missing remote solid emission:
// a grey floor must reflect the enclosure, not show only its own emissivity.
if (process.argv.includes('--engine')) {
  const executable = process.env.STREAMSIM_ENGINE || path.join(root, 'models/bin_x64/Release/histream.exe')
  const obj = `o blackbody_enclosure
v -3 0 -2
v 3 0 -2
v 3 0 2
v -3 0 2
v -3 3 -2
v 3 3 -2
v 3 3 2
v -3 3 2
f 5 7 6
f 5 8 7
f 1 6 2
f 1 5 6
f 2 7 3
f 2 6 7
f 3 8 4
f 3 7 8
f 4 5 1
f 4 8 5
`
  const objPath = path.join(work, 'enclosure.obj')
  const positions = path.join(work, 'position.txt')
  fs.writeFileSync(objPath, obj)
  fs.writeFileSync(positions, '3.5 2.5 0 1 0\n')
  const results = []
  for (const solver of ['traditional', 'accelerated']) {
    const project = createDefaultProject({ mode: 'eVoxelRT' })
    const c = project.configuration
    c.outDir = path.join(work, solver, 'output')
    fs.mkdirSync(c.outDir, { recursive: true })
    c.scene = { ...c.scene, x: 7, y: 5, height: 4, voxel: 1, terrain: false,
      background: { spectralName: 'grey', thermalName: 'equal300', materialName: 'soilset', materialType: 'Soil' } }
    c.light = { ...c.light, direct: 0.8, diffuse: 0.2, skyTemperature: 300, zenith: 30, azimuth: 135 }
    c.sensor = { ...c.sensor, x: 16, y: 16, bands: '660,860,10500,12000',
      continuousBands: false, temperature: false, projection: 'perspective',
      positionX: 3.5, positionY: 2.5, height: 2, fov: 30, vza: 0, vaa: 0,
      image: true, radiationProcess: true, process: true, principalPlane: false, hemisphere: false }
    c.control = { ...c.control, radiationSolver: solver, depth: 4, periodicTraversalCount: 0, gpu: 0 }
    c.spectra = [
      { name: 'grey', model: 'custom', reflectance: '0.25', transmittance: '0', refTir: 0.5, tauTir: 0, params: {} },
      { name: 'black', model: 'custom', reflectance: '0', transmittance: '0', refTir: 0, tauTir: 0, params: {} }
    ]
    c.thermals = [{ name: 'equal300', sunlitTemperature: 300, shadedTemperature: 300 }]
    c.objects = { count: 1, names: ['enclosure'], items: [{ name: 'enclosure', type: 'Building',
      fileName: objPath, positionFile: positions, instanceCount: 1, materialName: 'soilset',
      canopyName: 'rigid_body', meshes: [{ name: 'blackbody_enclosure', spectralName: 'black',
        thermalName: 'equal300', materialName: 'soilset', canopyName: 'rigid_body' }] }] }
    const validation = validateProject(project)
    assert.equal(validation.valid, true, validation.errors.join('; '))
    const input = path.join(work, solver, 'project.json')
    fs.writeFileSync(input, JSON.stringify(project, null, 2))
    const log = command(executable, ['eVoxelRT', input], { cwd: path.dirname(executable) })
    fs.writeFileSync(path.join(work, `${solver}.log`), log)
    const metadata = JSON.parse(fs.readFileSync(path.join(c.outDir, 'voxelrt.json'), 'utf8'))
    assert.ok(metadata.voxelCount > 0 && metadata.voxelCount % 64 !== 0,
      `Regression requires partial workgroup; actual count=${metadata.voxelCount}`)
    assert.equal(metadata.fields[0].unit, '1')
    assert.equal(metadata.fields[2].unit, 'W m-2 sr-1 um-1')
    const data = fs.readFileSync(path.join(c.outDir, metadata.dataFile))
    const floor = []
    for (let i = 0; i < metadata.voxelCount; ++i) {
      const base = i * metadata.recordFloats * 4
      const x = data.readFloatLE(base), y = data.readFloatLE(base + 4), z = data.readFloatLE(base + 8)
      if (x > 1 && x < 6 && z > 1 && z < 4 && y < 0)
        floor.push(data.readFloatLE(base + metadata.fields[2].offset * 4))
    }
    assert.ok(floor.length > 0, 'No interior floor voxels')
    const planck = 1.1910439340652e8 / (10.5 ** 5 * Math.expm1(14388.291040407 / (300 * 10.5)))
    const average = floor.reduce((a, b) => a + b, 0) / floor.length
    const imageFile = fs.readdirSync(c.outDir).find(file => file.endsWith('.tif'))
    assert.ok(imageFile, 'Engine did not produce a rendered TIFF')
    const image = readFloat32Tiff(path.join(c.outDir, imageFile))
    assert.equal(image.bands, 4)
    const thermalPixels = image.pixels[2].filter(value => Number.isFinite(value) && value > 0)
    assert.equal(thermalPixels.length, image.width * image.height, 'Enclosed camera must see complete finite floor')
    const renderedRadiance = thermalPixels.reduce((a, b) => a + b, 0) / thermalPixels.length
    const record = { solver, voxelCount: metadata.voxelCount, floorSamples: floor.length,
      meanRadiance: average, blackbodyRadiance: planck, relativeError: (average - planck) / planck }
    record.renderedMeanRadiance = renderedRadiance
    record.renderedRelativeError = (renderedRadiance - planck) / planck
    results.push(record)
    fs.writeFileSync(path.join(work, 'engine-results.json'), JSON.stringify(results, null, 2))
    console.log(JSON.stringify(record))
    assert.ok(Math.abs(record.relativeError) < 0.05, 'Grey floor in isothermal enclosure must approach B(300 K)')
    assert.ok(Math.abs(record.renderedRelativeError) < 0.05, 'Rendered grey floor must approach B(300 K)')
  }
  assert.ok(Math.abs(results[0].meanRadiance - results[1].meanRadiance) / results[0].blackbodyRadiance < 0.005,
    'Traditional and accelerated enclosure radiance differ')
  console.log(`Partial workgroups, enclosed emitter, solver equivalence and units: passed; ${work}`)
}
fs.writeFileSync(path.join(work, 'native-limits.json'), JSON.stringify(limits, null, 2))
console.log(`Evidence: ${work}`)
