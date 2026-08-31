#include "vulkan_radiosity.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const std::string shaderDirectory = argc > 1 ? argv[1] : RADIOSITY_VULKAN_SHADER_DIR;
    facetvk::Config config{};
    config.rasterWidth = 128;
    config.rasterHeight = 128;
    config.maxFragmentsPerPixel = 8;
    config.enableValidation = true;

    // Two parallel, exactly overlapping triangles. Side 0 of each triangle
    // faces the gap; side 1 faces the sky boundary.
    const std::vector<facetvk::Vertex> vertices{
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    };

    facetvk::VulkanRadiosity model;
    model.initialize(config, shaderDirectory);
    model.setGeometry(vertices);
    const facetvk::GraphDiagnostics graph =
        model.buildVisibilityGraph({facetvk::Direction{0.0f, 0.0f, 1.0f}});
    const std::vector<float> sunlit =
        model.computeSunlitFraction({0.0f, 0.0f, 1.0f});

    std::vector<facetvk::SurfaceOptics> optics(model.surfaceCount());
    for (auto& surface : optics) {
        surface.reflectance = 0.5f;
    }
    optics[0].emission = 1.0f;
    const facetvk::SolveResult result = model.solve(optics, 0.0f, 32, 1.0f);

    std::cout << "facets=" << graph.facetCount
              << " directedEdges=" << graph.directedEdgeCount
              << " closureError=" << graph.maxClosureError
              << " maxDelta=" << result.maxDelta << '\n';
    for (uint32_t side = 0; side < result.radiosity.size(); ++side) {
        std::cout << "B[" << side << "]=" << result.radiosity[side]
                  << " sunlit=" << sunlit[side] << '\n';
    }
    const bool directVisibilityOk = std::abs(sunlit[0]) < 1.0e-6f &&
                                    std::abs(sunlit[3] - 1.0f) < 1.0e-6f;
    return graph.maxClosureError < 1.0e-5f &&
                   graph.directedEdgeCount == 2U && directVisibilityOk
               ? 0
               : 1;
}
