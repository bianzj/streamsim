#include "vulkan_radiosity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Vec3 = std::array<float, 3>;

struct Scene {
    std::vector<facetvk::Vertex> vertices;
    uint32_t leafFacetCount{};
};

Vec3 subtract(const Vec3& a, const Vec3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

float dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vec3 normalized(const Vec3& value)
{
    const float length = std::sqrt(dot(value, value));
    if (!(length > 1.0e-8f)) {
        throw std::runtime_error("Degenerate triangle in OBJ scene");
    }
    return {value[0] / length, value[1] / length, value[2] / length};
}

int parsePositionIndex(const std::string& token, size_t vertexCount)
{
    const size_t slash = token.find('/');
    const std::string indexText = token.substr(0, slash);
    if (indexText.empty()) {
        throw std::runtime_error("OBJ face has no position index");
    }
    const int raw = std::stoi(indexText);
    const int index = raw > 0 ? raw - 1 : static_cast<int>(vertexCount) + raw;
    if (index < 0 || static_cast<size_t>(index) >= vertexCount) {
        throw std::runtime_error("OBJ position index is out of range");
    }
    return index;
}

void appendTriangle(Scene& scene, const Vec3& a, const Vec3& b, const Vec3& c)
{
    const Vec3 normal = normalized(cross(subtract(b, a), subtract(c, a)));
    for (const Vec3& position : {a, b, c}) {
        facetvk::Vertex vertex{};
        std::copy(position.begin(), position.end(), vertex.position);
        std::copy(normal.begin(), normal.end(), vertex.normal);
        scene.vertices.push_back(vertex);
    }
}

Scene loadTreeWithSoil(const std::string& fileName)
{
    std::ifstream input(fileName);
    if (!input) {
        throw std::runtime_error("Cannot open tree OBJ: " + fileName);
    }

    std::vector<Vec3> positions;
    std::vector<std::array<int, 3>> triangles;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        std::string record;
        stream >> record;
        if (record == "v") {
            Vec3 position{};
            if (!(stream >> position[0] >> position[1] >> position[2])) {
                throw std::runtime_error("Invalid OBJ vertex record");
            }
            positions.push_back(position);
        } else if (record == "f") {
            std::vector<int> face;
            std::string token;
            while (stream >> token) {
                face.push_back(parsePositionIndex(token, positions.size()));
            }
            if (face.size() < 3U) {
                throw std::runtime_error("OBJ face has fewer than three vertices");
            }
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                triangles.push_back({face[0], face[i], face[i + 1]});
            }
        }
    }
    if (positions.empty() || triangles.empty()) {
        throw std::runtime_error("Tree OBJ contains no usable geometry");
    }

    Vec3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max()};
    Vec3 maximum{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest()};
    for (const Vec3& position : positions) {
        for (size_t axis = 0; axis < 3; ++axis) {
            minimum[axis] = std::min(minimum[axis], position[axis]);
            maximum[axis] = std::max(maximum[axis], position[axis]);
        }
    }

    // The example tree uses Y as its vertical axis. Put its lowest point at Y=0.
    for (Vec3& position : positions) {
        position[1] -= minimum[1];
    }

    Scene scene;
    scene.vertices.reserve((triangles.size() + 2U) * 3U);
    for (const auto& triangle : triangles) {
        appendTriangle(scene, positions[triangle[0]], positions[triangle[1]],
                       positions[triangle[2]]);
    }
    scene.leafFacetCount = static_cast<uint32_t>(triangles.size());

    const float spanX = maximum[0] - minimum[0];
    const float spanZ = maximum[2] - minimum[2];
    const float margin = 0.5f * std::max(spanX, spanZ);
    const float x0 = minimum[0] - margin;
    const float x1 = maximum[0] + margin;
    const float z0 = minimum[2] - margin;
    const float z1 = maximum[2] + margin;
    constexpr float groundY = -0.01f;
    appendTriangle(scene, {x0, groundY, z0}, {x1, groundY, z1}, {x1, groundY, z0});
    appendTriangle(scene, {x0, groundY, z0}, {x0, groundY, z1}, {x1, groundY, z1});
    return scene;
}

std::vector<facetvk::Direction> makeHemisphereDirections(uint32_t count)
{
    constexpr float goldenAngle = 2.39996322972865332f;
    std::vector<facetvk::Direction> directions;
    directions.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const float y = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float azimuth = goldenAngle * static_cast<float>(i);
        directions.push_back({radius * std::cos(azimuth), y,
                              radius * std::sin(azimuth)});
    }
    return directions;
}

struct Statistics {
    float minimum{std::numeric_limits<float>::max()};
    float maximum{std::numeric_limits<float>::lowest()};
    double sum{};
    uint32_t count{};

    void add(float value)
    {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
        sum += value;
        ++count;
    }

    float mean() const { return count == 0U ? 0.0f : static_cast<float>(sum / count); }
};

void printStatistics(const char* label, const Statistics& values)
{
    std::cout << label << ": mean=" << values.mean() << " min=" << values.minimum
              << " max=" << values.maximum << '\n';
}

} // namespace

int main(int argc, char** argv)
try {
    const std::string shaderDirectory = argc > 1 ? argv[1] : RADIOSITY_VULKAN_SHADER_DIR;
    const std::string treeFile = argc > 2 ? argv[2] : "data/example/single_tree_LAI_4.obj";
    const Scene scene = loadTreeWithSoil(treeFile);

    facetvk::Config config{};
    config.rasterWidth = 256;
    config.rasterHeight = 256;
    config.maxFragmentsPerPixel = 64;
    config.enableValidation = true;

    facetvk::VulkanRadiosity model;
    model.initialize(config, shaderDirectory);
    model.setGeometry(scene.vertices);
    const std::vector<facetvk::Direction> skyDirections = makeHemisphereDirections(40);
    const facetvk::GraphDiagnostics graph = model.buildVisibilityGraph(skyDirections);

    // Solar zenith 30 degrees, azimuth 135 degrees; direction points to the sun.
    constexpr float pi = 3.14159265358979323846f;
    constexpr float zenith = 30.0f * pi / 180.0f;
    constexpr float azimuth = 135.0f * pi / 180.0f;
    const facetvk::Direction sunDirection{std::sin(zenith) * std::cos(azimuth),
                                          std::cos(zenith),
                                          std::sin(zenith) * std::sin(azimuth)};
    const std::vector<float> sunlit = model.computeSunlitFraction(sunDirection);

    constexpr float beamNormalIrradiance = 700.0f;
    constexpr float skyDiffuseIrradiance = 100.0f;
    std::vector<facetvk::SurfaceOptics> optics(model.surfaceCount());
    for (uint32_t facet = 0; facet < model.facetCount(); ++facet) {
        const bool leaf = facet < scene.leafFacetCount;
        const facetvk::Vertex& vertex = scene.vertices[facet * 3U];
        const Vec3 geometricNormal{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        for (uint32_t localSide = 0; localSide < 2U; ++localSide) {
            const uint32_t side = facet * 2U + localSide;
            const float sign = localSide == 0U ? 1.0f : -1.0f;
            const Vec3 sideNormal{sign * geometricNormal[0], sign * geometricNormal[1],
                                  sign * geometricNormal[2]};
            optics[side].reflectance = leaf ? 0.10f : 0.20f;
            optics[side].transmittance = leaf ? 0.05f : 0.0f;
            const Vec3 sun{sunDirection.x, sunDirection.y, sunDirection.z};
            optics[side].directIrradiance = sunlit[side] *
                                            std::max(0.0f, dot(sideNormal, sun)) *
                                            beamNormalIrradiance;
        }
    }

    const facetvk::SolveResult result = model.solve(optics, skyDiffuseIrradiance, 64, 1.0f);
    Statistics leafSunlit;
    Statistics soilSunlit;
    Statistics leafRadiosity;
    Statistics soilRadiosity;
    for (uint32_t facet = 0; facet < model.facetCount(); ++facet) {
        for (uint32_t localSide = 0; localSide < 2U; ++localSide) {
            const uint32_t side = facet * 2U + localSide;
            Statistics& sunStats = facet < scene.leafFacetCount ? leafSunlit : soilSunlit;
            Statistics& radiosityStats = facet < scene.leafFacetCount
                                              ? leafRadiosity
                                              : soilRadiosity;
            sunStats.add(sunlit[side]);
            radiosityStats.add(result.radiosity[side]);
        }
    }

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "tree=" << treeFile << '\n'
              << "leafFacets=" << scene.leafFacetCount
              << " soilFacets=" << model.facetCount() - scene.leafFacetCount
              << " surfaces=" << graph.surfaceCount << '\n'
              << "skyDirections=" << skyDirections.size()
              << " raster=" << config.rasterWidth << 'x' << config.rasterHeight
              << " layers=" << config.maxFragmentsPerPixel << '\n'
              << "directedEdges=" << graph.directedEdgeCount
              << " fragmentOverflow=" << graph.fragmentOverflow
              << " hashOverflow=" << graph.hashOverflow << '\n'
              << "maxClosureError=" << graph.maxClosureError
              << " iterations=" << result.iterations
              << " maxDelta=" << result.maxDelta << '\n';
    printStatistics("leafSunlit", leafSunlit);
    printStatistics("soilSunlit", soilSunlit);
    printStatistics("leafRadiosity", leafRadiosity);
    printStatistics("soilRadiosity", soilRadiosity);

    return graph.fragmentOverflow == 0U && graph.hashOverflow == 0U &&
                   graph.maxClosureError < 1.0e-5f
               ? 0
               : 1;
} catch (const std::exception& error) {
    std::cerr << "vulkan_radiosity_tree: " << error.what() << '\n';
    return 1;
}
