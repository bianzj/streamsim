# Vulkan facet radiosity

This module uses Vulkan graphics and compute pipelines only. It does not create
an acceleration structure, ray query, or ray-tracing pipeline.

Pipeline:

1. Rasterize every facet for each equal-solid-angle sky direction.
2. Store all fragments per pixel in a bounded A-buffer.
3. Sort fragments by depth in `visibility_resolve.comp`.
4. Convert adjacent oriented facet sides into a reciprocal visibility graph.
5. Compact the graph to CSR once, then reuse it for all bands and time nodes.
6. Solve two-sided reflection/transmission with ping-pong GPU Jacobi iterations.

Side indexing is `2 * facet + side`: side 0 follows `Facet::pnorm`, side 1 is
the opposite side.

```cpp
#include "vulkanradiosity/radiosityeb_vulkan.h"

facetvk::Config config;
config.rasterWidth = 512;
config.rasterHeight = 512;
config.maxFragmentsPerPixel = 32;

RadiosityEBVulkan gpu;
auto graph = gpu.prepare(modelio, config, RADIOSITY_VULKAN_SHADER_DIR);

const float skyBand = atomcoeff.fesky[band] * meteo.Rin * 0.001f;
const float beamBand = atomcoeff.fesun[band] * meteo.Rin * 0.001f /
                       std::max(std::cos(modelio->sza * RD), 1.0e-4f);
auto result = gpu.solveShortwaveBand(modelio, band, skyBand, beamBand);
```

`GraphDiagnostics::fragmentOverflow` and `hashOverflow` must both be zero.
`maxClosureError` checks `F_sky + sum(F_ij) = 1` for every sampled side.

Validation test:

```text
cmake --build cmake-build-vulkan --target vulkan_radiosity_smoke
cmake-build-vulkan/vulkan_radiosity_smoke.exe \
  cmake-build-vulkan/shader/vulkanradiosity
```

The smoke scene contains two parallel triangles. It verifies reciprocal edges,
form-factor closure, multiple reflection, and direct-light occlusion with the
Vulkan validation layer enabled.
