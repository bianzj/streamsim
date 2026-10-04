# Opaque voxel boundary regression

The OBJ voxel path previously emitted three orthogonal centre planes for every
cell. Those are transport integration proxies for participating media, not the
boundary of an opaque solid. In building images they expose interior planes and
create repeated bright/dark facade strips.

Opaque Building/Soil/Water meshes now emit outward-facing boundary quads and cull
shared interior faces. Participating media keep their existing integration
planes. Voxel IDs and physical state row ordering remain unchanged.

The CPU test uses the production boundary emitter and checks both triangles of
every face, oriented edge closure and enclosed volume for isolated, adjacent,
concave, negative-coordinate and multi-cell cases:

```powershell
cmake -S tests/voxel_boundary -B build/voxel_boundary
cmake --build build/voxel_boundary --config Release
ctest --test-dir build/voxel_boundary -C Release --output-on-failure
```

Related shader corrections:

- World normals use the inverse transpose of the instance transform.
- Single-state OBJ cells (`faceId=0`) never use legacy face offsets.
- Five-state procedural building cells resolve the hit face from its normal and
  validate that the resolved state still belongs to the same voxel.
- Material lookup occurs after resolving the state, including roof materials.

The Beijing observer smoke capture uses exactly the same poses as the previous
dataset. Temperature fields generated with the old geometry must be recalculated;
reusing them is only valid for a geometry comparison, not corrected thermal truth.
