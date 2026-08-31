import * as THREE from 'three'
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js'

function positionsFromText(content) {
  return String(content).split(/\r?\n/).map((line) => line.trim()).filter(Boolean).map((line) => {
    const values = line.split(/[\s,]+/).map(Number)
    return { x: values[0] || 0, y: values[1] || 0, z: values[2] || 0, scale: Number.isFinite(values[3]) ? values[3] : 1, rotation: Number.isFinite(values[4]) ? values[4] : 0 }
  })
}

function dispose(root) {
  root.traverse((child) => {
    child.geometry?.dispose?.()
    if (Array.isArray(child.material)) child.material.forEach((material) => material.dispose?.())
    else child.material?.dispose?.()
  })
}

function colorFor(type) {
  if (type.includes('water')) return 0x3d8eaa
  if (type.includes('build')) return 0x7e8580
  if (type.includes('soil')) return 0x756247
  return 0x3f9568
}

function primitive(item) {
  const [length = 1, width = 1, height = 1] = item.dimensions
  const geometry = item.shape.toLowerCase().includes('ellipsoid')
    ? new THREE.SphereGeometry(.5, 16, 10)
    : new THREE.BoxGeometry(1, 1, 1)
  const mesh = new THREE.Mesh(geometry, new THREE.MeshStandardMaterial({ color: colorFor(item.type), roughness: .82 }))
  mesh.scale.set(length, height, width)
  return mesh
}

export async function loadXmlScene({ api, world, config, log }) {
  for (const child of [...world.children]) {
    if (child.userData.projectObject || child.userData.kind === 'vegetation' || child.userData.kind === 'building') {
      world.remove(child)
      dispose(child)
    }
  }

  const items = config.objects.items || []
  const sceneScale = Math.min(1, 45 / Math.max(config.scene.x, config.scene.y, 1))
  const worldWidth = config.scene.x * sceneScale
  const worldDepth = config.scene.y * sceneScale
  let instanceCount = 0

  for (const item of items) {
    try {
      let placements = [{ x: config.scene.x / 2, y: config.scene.y / 2, z: 0, scale: 1, rotation: 0 }]
      if (item.positionFile) {
        const result = await api.readText(item.positionFile)
        placements = positionsFromText(result.content)
      }
      placements = placements.slice(0, 2000)

      if (item.fileName) {
        const result = await api.readText(item.fileName)
        const source = new OBJLoader().parse(result.content)
        source.traverse((child) => {
          if (!child.isMesh) return
          child.castShadow = child.receiveShadow = true
          child.material = new THREE.MeshStandardMaterial({ color: colorFor(item.type), roughness: .78 })
        })
        for (const placement of placements) {
          const object = source.clone(true)
          object.userData.projectObject = true
          object.scale.setScalar(sceneScale * placement.scale)
          object.position.set(placement.x * sceneScale - worldWidth / 2, placement.z * sceneScale, placement.y * sceneScale - worldDepth / 2)
          object.rotation.y = THREE.MathUtils.degToRad(placement.rotation)
          world.add(object)
          instanceCount += 1
        }
      } else {
        for (const placement of placements) {
          const object = primitive(item)
          object.userData.projectObject = true
          object.userData.kind = item.type.includes('build') ? 'building' : 'vegetation'
          object.scale.multiplyScalar(sceneScale * placement.scale)
          object.position.set(placement.x * sceneScale - worldWidth / 2, placement.z * sceneScale + object.scale.y / 2, placement.y * sceneScale - worldDepth / 2)
          object.rotation.y = THREE.MathUtils.degToRad(placement.rotation)
          object.castShadow = object.receiveShadow = true
          world.add(object)
          instanceCount += 1
        }
      }
    } catch (error) {
      log(`预览对象 ${item.name} 失败：${error.message}`, 'error')
    }
  }
  log(`Three.js 已按工程 XML 加载 ${items.length} 类、${instanceCount} 个场景对象`, 'success')
  return instanceCount
}
