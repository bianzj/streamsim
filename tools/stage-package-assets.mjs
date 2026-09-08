import { copyFile, mkdir, readFile, readdir, rm, stat, writeFile } from 'node:fs/promises'
import { dirname, extname, join, relative, resolve, sep } from 'node:path'
import { fileURLToPath } from 'node:url'

const MAX_OBJ_BYTES = 15 * 1024 * 1024
const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const sourceRoot = join(projectRoot, 'assets')
const targetRoot = join(projectRoot, '.packaging', 'assets')

const sourceDirectories = new Set()
const oversizedObjects = new Map()
const excludedSceneDirectories = new Set()
let copiedFiles = 0
let copiedBytes = 0

function isInside(path, directory) {
  const local = relative(directory, path)
  return local === '' || (!local.startsWith(`..${sep}`) && local !== '..')
}

async function scan(path) {
  for (const entry of await readdir(path, { withFileTypes: true })) {
    const fullPath = join(path, entry.name)
    if (entry.isDirectory()) {
      if (/^_.*_source$/i.test(entry.name)) {
        sourceDirectories.add(fullPath)
        continue
      }
      await scan(fullPath)
      continue
    }
    if (!entry.isFile() || extname(entry.name).toLowerCase() !== '.obj') continue
    const info = await stat(fullPath)
    if (info.size > MAX_OBJ_BYTES) oversizedObjects.set(fullPath, info.size)
  }
}

async function findIncompleteRamiScenes() {
  const catalogPath = join(sourceRoot, 'rami-library', 'catalog.json')
  const catalog = JSON.parse(await readFile(catalogPath, 'utf8'))
  for (const scene of catalog.scenes || []) {
    const manifestPath = resolve(dirname(catalogPath), scene.manifest)
    const manifest = JSON.parse(await readFile(manifestPath, 'utf8'))
    const hasOversizedObject = (manifest.objects || []).some((object) =>
      oversizedObjects.has(resolve(dirname(manifestPath), object.file)))
    if (hasOversizedObject) excludedSceneDirectories.add(dirname(manifestPath))
  }
}

function excluded(path) {
  if (oversizedObjects.has(path)) return true
  for (const directory of sourceDirectories) if (isInside(path, directory)) return true
  for (const directory of excludedSceneDirectories) if (isInside(path, directory)) return true
  return false
}

async function copyTree(source, target) {
  await mkdir(target, { recursive: true })
  for (const entry of await readdir(source, { withFileTypes: true })) {
    const sourcePath = join(source, entry.name)
    if (excluded(sourcePath)) continue
    const targetPath = join(target, entry.name)
    if (entry.isDirectory()) {
      await copyTree(sourcePath, targetPath)
    } else if (entry.isFile()) {
      await mkdir(dirname(targetPath), { recursive: true })
      await copyFile(sourcePath, targetPath)
      const info = await stat(sourcePath)
      copiedFiles += 1
      copiedBytes += info.size
    }
  }
}

async function rewriteObjCatalog() {
  const sourceCatalog = join(sourceRoot, 'obj-library', 'catalog.json')
  const targetCatalog = join(targetRoot, 'obj-library', 'catalog.json')
  const catalog = JSON.parse(await readFile(sourceCatalog, 'utf8'))
  catalog.models = (catalog.models || []).filter((model) =>
    !oversizedObjects.has(resolve(dirname(sourceCatalog), model.file)))
  const includedIds = new Set(catalog.models.map((model) => model.id))
  for (const defaults of Object.values(catalog.defaultSets || {})) {
    if (Array.isArray(defaults.typical)) {
      defaults.typical = defaults.typical.filter((id) => includedIds.has(id))
    }
    if (defaults.simple && !includedIds.has(defaults.simple)) delete defaults.simple
  }
  await writeFile(targetCatalog, `${JSON.stringify(catalog, null, 2)}\n`, 'utf8')
}

async function rewriteRamiCatalog() {
  const sourceCatalog = join(sourceRoot, 'rami-library', 'catalog.json')
  const targetCatalog = join(targetRoot, 'rami-library', 'catalog.json')
  const catalog = JSON.parse(await readFile(sourceCatalog, 'utf8'))
  catalog.scenes = (catalog.scenes || []).filter((scene) =>
    !excludedSceneDirectories.has(dirname(resolve(dirname(sourceCatalog), scene.manifest))))
  await writeFile(targetCatalog, `${JSON.stringify(catalog, null, 2)}\n`, 'utf8')
}

await rm(targetRoot, { recursive: true, force: true })
await scan(sourceRoot)
await findIncompleteRamiScenes()
await copyTree(sourceRoot, targetRoot)
await rewriteObjCatalog()
await rewriteRamiCatalog()

const excludedModels = [...oversizedObjects.entries()]
  .map(([path, size]) => `${relative(sourceRoot, path)} (${(size / 1024 / 1024).toFixed(2)} MiB)`)
console.log(`Packaged assets: ${copiedFiles} files, ${(copiedBytes / 1024 / 1024).toFixed(2)} MiB`)
console.log(`Excluded source directories: ${sourceDirectories.size}`)
console.log(`Excluded OBJ files over 15 MiB: ${excludedModels.length}`)
for (const model of excludedModels) console.log(`  - ${model}`)
console.log(`Excluded incomplete RAMI scenes: ${excludedSceneDirectories.size}`)
