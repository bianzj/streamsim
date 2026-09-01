#include "facetrtcore.h"
#include "facetrt_vulkan.h"
#include "../base/utils.h"
#include "../thirdparty/spa.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Vec3 = std::array<float, 3>;
constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kRasterSize = 384;
constexpr uint32_t kRasterLayers = 64;
constexpr uint32_t kSkyDirectionCount = 40;
constexpr uint32_t kIterations = 64;
constexpr uint32_t kSoilGridResolution = 48;
constexpr float kBeamNormalIrradiance = 700.0f;
constexpr float kSkyDiffuseIrradiance = 100.0f;

struct Scene {
    std::vector<facetvk::Vertex> vertices;
    uint32_t leafFacetCount{};
    Vec3 minimum{};
    Vec3 maximum{};
    float sunZenith{30.0f};
    float sunAzimuth{135.0f};

    uint32_t facetCount() const { return static_cast<uint32_t>(vertices.size() / 3U); }
};

struct Edge {
    uint32_t neighbor{};
    uint32_t count{};
};

struct CpuGraph {
    std::vector<uint32_t> rowOffsets;
    std::vector<Edge> edges;
    std::vector<uint32_t> denominators;
    std::vector<uint32_t> skyCounts;
    facetvk::GraphDiagnostics diagnostics;
};

struct RunResult {
    std::string backend;
    facetvk::GraphDiagnostics graph;
    facetvk::SolveResult solution;
    std::vector<float> sunlit;
    std::vector<float> lightEnhancement;
    double visibilityMs{};
    double sunlightMs{};
    double solveMs{};
    double totalMs{};
};

Vec3 add(const Vec3& a, const Vec3& b)
{
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

Vec3 subtract(const Vec3& a, const Vec3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Vec3 multiply(const Vec3& value, float scale)
{
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

float dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

Vec3 normalized(const Vec3& value)
{
    const float length = std::sqrt(dot(value, value));
    if (!(length > 1.0e-8f)) {
        throw std::runtime_error("Zero-length direction or degenerate triangle");
    }
    return multiply(value, 1.0f / length);
}

double milliseconds(Clock::time_point begin, Clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

int parsePositionIndex(const std::string& token, size_t vertexCount)
{
    const std::string text = token.substr(0, token.find('/'));
    if (text.empty()) {
        throw std::runtime_error("OBJ face has no position index");
    }
    const int raw = std::stoi(text);
    const int index = raw > 0 ? raw - 1 : static_cast<int>(vertexCount) + raw;
    if (index < 0 || static_cast<size_t>(index) >= vertexCount) {
        throw std::runtime_error("OBJ position index is out of range");
    }
    return index;
}

bool appendTriangle(Scene& scene, const Vec3& a, const Vec3& b, const Vec3& c)
{
    const Vec3 normalVector = cross(subtract(b, a), subtract(c, a));
    if (dot(normalVector, normalVector) <= 1.0e-16f) {
        return false;
    }
    const Vec3 normal = normalized(normalVector);
    for (const Vec3& position : {a, b, c}) {
        facetvk::Vertex vertex{};
        std::copy(position.begin(), position.end(), vertex.position);
        std::copy(normal.begin(), normal.end(), vertex.normal);
        scene.vertices.push_back(vertex);
    }
    return true;
}

Scene loadTreeWithSoil(const std::string& fileName)
{
    std::ifstream input(fileName);
    if (!input) {
        throw std::runtime_error("Cannot open tree OBJ: " + fileName);
    }

    std::vector<Vec3> positions;
    std::vector<std::array<int, 3>> triangles;
    size_t terrainTriangleStart = std::numeric_limits<size_t>::max();
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        std::string record;
        stream >> record;
        if (record == "o") {
            std::string name;
            stream >> name;
            if (name == "STREAMSIM_TERRAIN" && terrainTriangleStart == std::numeric_limits<size_t>::max()) {
                terrainTriangleStart = triangles.size();
            }
        } else if (record == "v") {
            Vec3 position{};
            if (!(stream >> position[0] >> position[1] >> position[2])) {
                throw std::runtime_error("Invalid OBJ vertex");
            }
            positions.push_back(position);
        } else if (record == "f") {
            std::vector<int> face;
            std::string token;
            while (stream >> token) {
                face.push_back(parsePositionIndex(token, positions.size()));
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
    for (Vec3& position : positions) {
        position[1] -= minimum[1];
    }

    const bool hasTerrain = terrainTriangleStart != std::numeric_limits<size_t>::max() &&
                            terrainTriangleStart < triangles.size();
    Scene scene;
    const uint32_t soilFacetCount = hasTerrain ? 0U :
        2U * kSoilGridResolution * kSoilGridResolution;
    scene.vertices.reserve((triangles.size() + soilFacetCount) * 3U);
    uint32_t leafFacetCount = 0;
    for (size_t triangleIndex = 0; triangleIndex < triangles.size(); ++triangleIndex) {
        const auto& triangle = triangles[triangleIndex];
        if (appendTriangle(scene, positions[triangle[0]], positions[triangle[1]],
                           positions[triangle[2]]) &&
            (!hasTerrain || triangleIndex < terrainTriangleStart)) {
            ++leafFacetCount;
        }
    }
    scene.leafFacetCount = hasTerrain ? leafFacetCount : scene.facetCount();

    float x0 = minimum[0];
    float x1 = maximum[0];
    float z0 = minimum[2];
    float z1 = maximum[2];
    if (!hasTerrain) {
        const float margin = 0.5f * std::max(maximum[0] - minimum[0],
                                             maximum[2] - minimum[2]);
        x0 -= margin;
        x1 += margin;
        z0 -= margin;
        z1 += margin;
        for (uint32_t row = 0; row < kSoilGridResolution; ++row) {
            const float za = z0 + (z1 - z0) * static_cast<float>(row) /
                                      static_cast<float>(kSoilGridResolution);
            const float zb = z0 + (z1 - z0) * static_cast<float>(row + 1U) /
                                      static_cast<float>(kSoilGridResolution);
            for (uint32_t column = 0; column < kSoilGridResolution; ++column) {
                const float xa = x0 + (x1 - x0) * static_cast<float>(column) /
                                          static_cast<float>(kSoilGridResolution);
                const float xb = x0 + (x1 - x0) * static_cast<float>(column + 1U) /
                                          static_cast<float>(kSoilGridResolution);
                appendTriangle(scene, {xa, -0.01f, za}, {xb, -0.01f, zb},
                               {xb, -0.01f, za});
                appendTriangle(scene, {xa, -0.01f, za}, {xa, -0.01f, zb},
                               {xb, -0.01f, zb});
            }
        }
    }

    scene.minimum = {x0, hasTerrain ? 0.0f : -0.01f, z0};
    scene.maximum = {x1, maximum[1] - minimum[1], z1};
    return scene;
}

std::vector<facetvk::Direction> makeHemisphereDirections()
{
    constexpr float goldenAngle = 2.39996322972865332f;
    std::vector<facetvk::Direction> directions;
    directions.reserve(kSkyDirectionCount);
    for (uint32_t i = 0; i < kSkyDirectionCount; ++i) {
        const float y = (static_cast<float>(i) + 0.5f) /
                        static_cast<float>(kSkyDirectionCount);
        const float radius = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float azimuth = goldenAngle * static_cast<float>(i);
        directions.push_back({radius * std::cos(azimuth), y,
                              radius * std::sin(azimuth)});
    }
    return directions;
}

facetvk::Direction sunDirection(const Scene& scene)
{
    const float zenith = scene.sunZenith * kPi / 180.0f;
    const float azimuth = scene.sunAzimuth * kPi / 180.0f;
    return {std::sin(zenith) * std::cos(azimuth), std::cos(zenith),
            std::sin(zenith) * std::sin(azimuth)};
}

std::vector<facetvk::SurfaceOptics> makeOptics(const Scene& scene,
                                                const std::vector<float>& sunlit)
{
    const facetvk::Direction direction = sunDirection(scene);
    const Vec3 sun{direction.x, direction.y, direction.z};
    std::vector<facetvk::SurfaceOptics> optics(scene.facetCount() * 2U);
    for (uint32_t facet = 0; facet < scene.facetCount(); ++facet) {
        const bool leaf = facet < scene.leafFacetCount;
        const facetvk::Vertex& vertex = scene.vertices[facet * 3U];
        const Vec3 normal{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        for (uint32_t localSide = 0; localSide < 2U; ++localSide) {
            const uint32_t side = facet * 2U + localSide;
            const float sign = localSide == 0U ? 1.0f : -1.0f;
            const Vec3 sideNormal = multiply(normal, sign);
            optics[side].reflectance = leaf ? 0.10f : 0.20f;
            optics[side].transmittance = leaf ? 0.05f : 0.0f;
            optics[side].directIrradiance =
                sunlit[side] * std::max(0.0f, dot(sideNormal, sun)) *
                kBeamNormalIrradiance;
        }
    }
    return optics;
}

struct ProjectedVertex {
    float x{};
    float y{};
    float depth{};
};

struct Fragment {
    float depth{};
    uint32_t cameraSide{};
};

class CpuRasterizer {
public:
    CpuRasterizer(const Scene& scene, uint32_t width, uint32_t height, uint32_t layers)
        : m_scene(scene),
          m_width(width),
          m_height(height),
          m_layers(layers),
          m_counts(static_cast<size_t>(width) * height),
          m_fragments(static_cast<size_t>(width) * height * layers)
    {
        const Vec3 diagonal = subtract(scene.maximum, scene.minimum);
        m_center = multiply(add(scene.minimum, scene.maximum), 0.5f);
        m_radius = std::max(0.5f * std::sqrt(dot(diagonal, diagonal)) * 1.001f,
                            1.0e-4f);
    }

    uint32_t rasterize(const facetvk::Direction& inputDirection)
    {
        std::fill(m_counts.begin(), m_counts.end(), 0U);
        const Vec3 view = normalized({inputDirection.x, inputDirection.y, inputDirection.z});
        const Vec3 reference = std::abs(view[2]) < 0.9f ? Vec3{0.0f, 0.0f, 1.0f}
                                                        : Vec3{0.0f, 1.0f, 0.0f};
        const Vec3 right = normalized(cross(reference, view));
        const Vec3 up = cross(view, right);
        uint32_t overflow = 0;

        for (uint32_t facet = 0; facet < m_scene.facetCount(); ++facet) {
            ProjectedVertex projected[3];
            for (uint32_t corner = 0; corner < 3U; ++corner) {
                const facetvk::Vertex& vertex = m_scene.vertices[facet * 3U + corner];
                const Vec3 position{vertex.position[0], vertex.position[1],
                                    vertex.position[2]};
                const Vec3 relative = subtract(position, m_center);
                projected[corner].x =
                    (0.5f + 0.5f * dot(relative, right) / m_radius) * m_width;
                projected[corner].y =
                    (0.5f + 0.5f * dot(relative, up) / m_radius) * m_height;
                projected[corner].depth =
                    0.5f - dot(relative, view) / (2.0f * m_radius);
            }
            const facetvk::Vertex& first = m_scene.vertices[facet * 3U];
            const Vec3 normal{first.normal[0], first.normal[1], first.normal[2]};
            const uint32_t cameraSide = facet * 2U + (dot(normal, view) >= 0.0f ? 0U : 1U);
            rasterTriangle(projected, cameraSide, overflow);
        }
        return overflow;
    }

    const std::vector<uint32_t>& counts() const { return m_counts; }
    const std::vector<Fragment>& fragments() const { return m_fragments; }
    uint32_t layers() const { return m_layers; }

private:
    static float edge(float ax, float ay, float bx, float by, float px, float py)
    {
        return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    }

    void rasterTriangle(const ProjectedVertex (&p)[3], uint32_t side, uint32_t& overflow)
    {
        const float area = edge(p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y);
        if (std::abs(area) < 1.0e-8f) {
            return;
        }
        const int x0 = std::max(0, static_cast<int>(std::floor(
                                       std::min({p[0].x, p[1].x, p[2].x}))));
        const int x1 = std::min(static_cast<int>(m_width) - 1,
                                static_cast<int>(std::ceil(
                                    std::max({p[0].x, p[1].x, p[2].x}))));
        const int y0 = std::max(0, static_cast<int>(std::floor(
                                       std::min({p[0].y, p[1].y, p[2].y}))));
        const int y1 = std::min(static_cast<int>(m_height) - 1,
                                static_cast<int>(std::ceil(
                                    std::max({p[0].y, p[1].y, p[2].y}))));
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                const float w0 = edge(p[1].x, p[1].y, p[2].x, p[2].y, px, py);
                const float w1 = edge(p[2].x, p[2].y, p[0].x, p[0].y, px, py);
                const float w2 = edge(p[0].x, p[0].y, p[1].x, p[1].y, px, py);
                const bool inside = area > 0.0f ? (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                                               : (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
                if (!inside) {
                    continue;
                }
                const size_t pixel = static_cast<size_t>(y) * m_width + x;
                const uint32_t layer = m_counts[pixel]++;
                if (layer >= m_layers) {
                    ++overflow;
                    continue;
                }
                const float invArea = 1.0f / area;
                m_fragments[pixel * m_layers + layer] = {
                    (w0 * p[0].depth + w1 * p[1].depth + w2 * p[2].depth) * invArea,
                    side};
            }
        }
    }

    const Scene& m_scene;
    uint32_t m_width{};
    uint32_t m_height{};
    uint32_t m_layers{};
    Vec3 m_center{};
    float m_radius{};
    std::vector<uint32_t> m_counts;
    std::vector<Fragment> m_fragments;
};

using Progress = std::function<void(int, const std::string&)>;

CpuGraph buildCpuGraph(const Scene& scene, const Progress& progress)
{
    CpuRasterizer raster(scene, kRasterSize, kRasterSize, kRasterLayers);
    const uint32_t surfaceCount = scene.facetCount() * 2U;
    CpuGraph graph;
    graph.denominators.assign(surfaceCount, 0U);
    graph.skyCounts.assign(surfaceCount, 0U);
    std::unordered_map<uint64_t, uint32_t> edgeCounts;
    uint32_t overflow = 0;
    const std::vector<facetvk::Direction> directions = makeHemisphereDirections();

    for (uint32_t directionIndex = 0; directionIndex < directions.size(); ++directionIndex) {
        overflow += raster.rasterize(directions[directionIndex]);
        const auto& counts = raster.counts();
        const auto& fragments = raster.fragments();
        std::vector<Fragment> local;
        local.reserve(kRasterLayers);
        for (size_t pixel = 0; pixel < counts.size(); ++pixel) {
            const uint32_t count = std::min(counts[pixel], raster.layers());
            if (count == 0U) {
                continue;
            }
            local.assign(fragments.begin() + pixel * raster.layers(),
                         fragments.begin() + pixel * raster.layers() + count);
            std::sort(local.begin(), local.end(),
                      [](const Fragment& a, const Fragment& b) { return a.depth < b.depth; });
            for (const Fragment& fragment : local) {
                ++graph.denominators[fragment.cameraSide];
                ++graph.denominators[fragment.cameraSide ^ 1U];
            }
            ++graph.skyCounts[local.front().cameraSide];
            ++graph.skyCounts[local.back().cameraSide ^ 1U];
            for (uint32_t i = 0; i + 1U < count; ++i) {
                const uint32_t sideA = local[i].cameraSide ^ 1U;
                const uint32_t sideB = local[i + 1U].cameraSide;
                if ((sideA >> 1U) == (sideB >> 1U)) {
                    continue;
                }
                const uint32_t lo = std::min(sideA, sideB);
                const uint32_t hi = std::max(sideA, sideB);
                ++edgeCounts[(uint64_t(lo) << 32U) | hi];
            }
        }
        progress(5 + static_cast<int>(65U * (directionIndex + 1U) / directions.size()),
                 "CPU visibility raster " + std::to_string(directionIndex + 1U) + "/" +
                     std::to_string(directions.size()));
    }

    graph.rowOffsets.assign(surfaceCount + 1U, 0U);
    for (const auto& entry : edgeCounts) {
        ++graph.rowOffsets[static_cast<uint32_t>(entry.first >> 32U) + 1U];
        ++graph.rowOffsets[static_cast<uint32_t>(entry.first) + 1U];
    }
    std::partial_sum(graph.rowOffsets.begin(), graph.rowOffsets.end(),
                     graph.rowOffsets.begin());
    graph.edges.resize(graph.rowOffsets.back());
    std::vector<uint32_t> cursor = graph.rowOffsets;
    for (const auto& entry : edgeCounts) {
        const uint32_t a = static_cast<uint32_t>(entry.first >> 32U);
        const uint32_t b = static_cast<uint32_t>(entry.first);
        graph.edges[cursor[a]++] = {b, entry.second};
        graph.edges[cursor[b]++] = {a, entry.second};
    }

    float closureError = 0.0f;
    for (uint32_t side = 0; side < surfaceCount; ++side) {
        uint64_t visible = graph.skyCounts[side];
        for (uint32_t edgeIndex = graph.rowOffsets[side];
             edgeIndex < graph.rowOffsets[side + 1U]; ++edgeIndex) {
            visible += graph.edges[edgeIndex].count;
        }
        if (graph.denominators[side] != 0U) {
            const float closure = static_cast<float>(visible) /
                                  static_cast<float>(graph.denominators[side]);
            closureError = std::max(closureError, std::abs(closure - 1.0f));
        }
    }
    graph.diagnostics.facetCount = scene.facetCount();
    graph.diagnostics.surfaceCount = surfaceCount;
    graph.diagnostics.directedEdgeCount = static_cast<uint32_t>(graph.edges.size());
    graph.diagnostics.fragmentOverflow = overflow;
    graph.diagnostics.maxClosureError = closureError;
    if (overflow != 0U) {
        throw std::runtime_error("CPU A-buffer overflow");
    }
    return graph;
}

std::vector<float> computeCpuSunlit(const Scene& scene)
{
    CpuRasterizer raster(scene, kRasterSize, kRasterSize, kRasterLayers);
    if (raster.rasterize(sunDirection(scene)) != 0U) {
        throw std::runtime_error("CPU sunlight A-buffer overflow");
    }
    std::vector<uint32_t> total(scene.facetCount() * 2U, 0U);
    std::vector<uint32_t> visible(scene.facetCount() * 2U, 0U);
    const auto& counts = raster.counts();
    const auto& fragments = raster.fragments();
    for (size_t pixel = 0; pixel < counts.size(); ++pixel) {
        const uint32_t count = std::min(counts[pixel], raster.layers());
        if (count == 0U) {
            continue;
        }
        float nearestDepth = std::numeric_limits<float>::max();
        uint32_t nearestSide = 0U;
        for (uint32_t layer = 0; layer < count; ++layer) {
            const Fragment& fragment = fragments[pixel * raster.layers() + layer];
            ++total[fragment.cameraSide];
            if (fragment.depth < nearestDepth) {
                nearestDepth = fragment.depth;
                nearestSide = fragment.cameraSide;
            }
        }
        ++visible[nearestSide];
    }
    std::vector<float> fraction(total.size(), 0.0f);
    for (size_t side = 0; side < total.size(); ++side) {
        if (total[side] != 0U) {
            fraction[side] = static_cast<float>(visible[side]) / total[side];
        }
    }
    return fraction;
}

facetvk::SolveResult solveCpu(const CpuGraph& graph,
                              const std::vector<facetvk::SurfaceOptics>& optics,
                              const Progress& progress)
{
    const uint32_t surfaceCount = graph.diagnostics.surfaceCount;
    std::vector<float> current(surfaceCount, 0.0f);
    std::vector<float> next(surfaceCount, 0.0f);
    float maxDelta = 0.0f;

    auto incoming = [&](uint32_t side) {
        const uint32_t denominator = graph.denominators[side];
        if (denominator == 0U) {
            return kSkyDiffuseIrradiance;
        }
        double value = static_cast<double>(graph.skyCounts[side]) *
                       kSkyDiffuseIrradiance;
        for (uint32_t edgeIndex = graph.rowOffsets[side];
             edgeIndex < graph.rowOffsets[side + 1U]; ++edgeIndex) {
            const Edge& edge = graph.edges[edgeIndex];
            value += static_cast<double>(edge.count) * current[edge.neighbor];
        }
        return static_cast<float>(value / denominator);
    };

    for (uint32_t iteration = 0; iteration < kIterations; ++iteration) {
        maxDelta = 0.0f;
        for (uint32_t facet = 0; facet < surfaceCount / 2U; ++facet) {
            const uint32_t side0 = facet * 2U;
            const uint32_t side1 = side0 + 1U;
            const float h0 = incoming(side0) + optics[side0].directIrradiance;
            const float h1 = incoming(side1) + optics[side1].directIrradiance;
            next[side0] = optics[side0].emission + optics[side0].reflectance * h0 +
                          optics[side0].transmittance * h1;
            next[side1] = optics[side1].emission + optics[side1].reflectance * h1 +
                          optics[side1].transmittance * h0;
            maxDelta = std::max(maxDelta, std::abs(next[side0] - current[side0]));
            maxDelta = std::max(maxDelta, std::abs(next[side1] - current[side1]));
        }
        current.swap(next);
        if ((iteration & 7U) == 7U) {
            progress(80 + static_cast<int>(20U * (iteration + 1U) / kIterations),
                     "CPU radiosity iteration " + std::to_string(iteration + 1U) + "/" +
                         std::to_string(kIterations));
        }
    }
    return {std::move(current), kIterations, maxDelta};
}

std::vector<facetvk::SurfaceOptics> withoutDirectLight(
    const std::vector<facetvk::SurfaceOptics>& optics)
{
    std::vector<facetvk::SurfaceOptics> diffuse = optics;
    for (facetvk::SurfaceOptics& surface : diffuse) {
        surface.directIrradiance = 0.0f;
    }
    return diffuse;
}

std::vector<float> lightEnhancement(
    const std::vector<float>& illuminated,
    const std::vector<float>& diffuseOnly)
{
    if (illuminated.size() != diffuseOnly.size()) {
        throw std::runtime_error("Radiosity baseline size mismatch");
    }
    std::vector<float> enhancement(illuminated.size());
    for (size_t index = 0; index < illuminated.size(); ++index) {
        enhancement[index] = std::max(0.0f, illuminated[index] - diffuseOnly[index]);
    }
    return enhancement;
}

RunResult runCpu(const Scene& scene, const Progress& progress)
{
    RunResult result;
    result.backend = "CPU raster";
    const auto totalBegin = Clock::now();
    progress(2, "Preparing CPU raster buffers");

    const auto visibilityBegin = Clock::now();
    const CpuGraph graph = buildCpuGraph(scene, progress);
    const auto visibilityEnd = Clock::now();
    result.graph = graph.diagnostics;
    result.visibilityMs = milliseconds(visibilityBegin, visibilityEnd);

    progress(74, "CPU sunlight raster");
    const auto sunlightBegin = Clock::now();
    result.sunlit = computeCpuSunlit(scene);
    const auto sunlightEnd = Clock::now();
    result.sunlightMs = milliseconds(sunlightBegin, sunlightEnd);

    const std::vector<facetvk::SurfaceOptics> optics = makeOptics(scene, result.sunlit);
    progress(80, "CPU radiosity solve");
    const auto solveBegin = Clock::now();
    result.solution = solveCpu(graph, optics, progress);
    progress(98, "CPU diffuse baseline and direct-light enhancement");
    const facetvk::SolveResult diffuseSolution =
        solveCpu(graph, withoutDirectLight(optics),
                 [](int, const std::string&) {});
    result.lightEnhancement =
        lightEnhancement(result.solution.radiosity, diffuseSolution.radiosity);
    const auto solveEnd = Clock::now();
    result.solveMs = milliseconds(solveBegin, solveEnd);
    result.totalMs = milliseconds(totalBegin, solveEnd);
    progress(100, "CPU complete");
    return result;
}

RunResult runGpu(const Scene& scene,
                 const std::string& shaderDirectory,
                 const Progress& progress)
{
    RunResult result;
    result.backend = "Vulkan GPU";
    const auto totalBegin = Clock::now();
    progress(3, "Creating Vulkan device and GPU buffers");
    facetvk::Config config{};
    config.rasterWidth = kRasterSize;
    config.rasterHeight = kRasterSize;
    config.maxFragmentsPerPixel = kRasterLayers;
    config.enableValidation = false;

    facetvk::FacetrtVulkan model;
    model.initialize(config, shaderDirectory);
    model.setGeometry(scene.vertices);

    progress(10, "GPU visibility graph: 40 raster directions");
    const auto visibilityBegin = Clock::now();
    result.graph = model.buildVisibilityGraph(makeHemisphereDirections());
    const auto visibilityEnd = Clock::now();
    result.visibilityMs = milliseconds(visibilityBegin, visibilityEnd);

    progress(72, "GPU sunlight visibility raster");
    const auto sunlightBegin = Clock::now();
    result.sunlit = model.computeSunlitFraction(sunDirection(scene));
    const auto sunlightEnd = Clock::now();
    result.sunlightMs = milliseconds(sunlightBegin, sunlightEnd);

    progress(82, "GPU Jacobi radiosity: 64 iterations");
    const std::vector<facetvk::SurfaceOptics> optics = makeOptics(scene, result.sunlit);
    const auto solveBegin = Clock::now();
    result.solution = model.solve(optics, kSkyDiffuseIrradiance, kIterations, 1.0f);
    progress(98, "GPU diffuse baseline and direct-light enhancement");
    const facetvk::SolveResult diffuseSolution =
        model.solve(withoutDirectLight(optics), kSkyDiffuseIrradiance,
                    kIterations, 1.0f);
    result.lightEnhancement =
        lightEnhancement(result.solution.radiosity, diffuseSolution.radiosity);
    const auto solveEnd = Clock::now();
    result.solveMs = milliseconds(solveBegin, solveEnd);
    result.totalMs = milliseconds(totalBegin, solveEnd);
    progress(100, "Vulkan GPU complete");
    return result;
}

void writeWebResult(const std::string& fileName,
                    const Scene& scene,
                    const RunResult& result)
{
    std::ofstream output(fileName, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Cannot write web result: " + fileName);
    }
    output << std::setprecision(9)
           << "{\n"
           << "  \"backend\": \"" << result.backend << "\",\n"
           << "  \"facetCount\": " << scene.facetCount() << ",\n"
           << "  \"leafFacetCount\": " << scene.leafFacetCount << ",\n"
           << "  \"vertexPositions\": [";
    for (size_t index = 0; index < scene.vertices.size(); ++index) {
        if (index != 0U) {
            output << ',';
        }
        const auto& vertex = scene.vertices[index];
        output << vertex.position[0] << ',' << vertex.position[1] << ',' << vertex.position[2];
    }
    output << "],\n"
           << "  \"graph\": {"
           << "\"directedEdges\":" << result.graph.directedEdgeCount << ","
           << "\"closureError\":" << result.graph.maxClosureError << ","
           << "\"fragmentOverflow\":" << result.graph.fragmentOverflow << ","
           << "\"hashOverflow\":" << result.graph.hashOverflow << "},\n"
           << "  \"timing\": {"
           << "\"totalMs\":" << result.totalMs << ","
           << "\"visibilityMs\":" << result.visibilityMs << ","
           << "\"sunlightMs\":" << result.sunlightMs << ","
           << "\"solveMs\":" << result.solveMs << "},\n"
           << "  \"iterations\": " << result.solution.iterations << ",\n"
           << "  \"maxDelta\": " << result.solution.maxDelta << ",\n";

    const auto writeArray = [&output](const char* name,
                                      const std::vector<float>& values,
                                      bool trailingComma) {
        output << "  \"" << name << "\": [";
        for (size_t i = 0; i < values.size(); ++i) {
            if (i != 0U) {
                output << ',';
            }
            output << values[i];
        }
        output << ']' << (trailingComma ? "," : "") << '\n';
    };
    writeArray("sunlit", result.sunlit, true);
    writeArray("radiosity", result.solution.radiosity, true);
    writeArray("lightEnhancement", result.lightEnhancement, false);
    output << "}\n";
}

} // namespace

namespace {

std::string readTextFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open HiStream input: " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string decodeXmlText(std::string value)
{
    const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&apos;", "'"}
    };
    for (const auto& entity : entities) {
        size_t position = 0;
        while ((position = value.find(entity.first, position)) != std::string::npos) {
            value.replace(position, std::strlen(entity.first), entity.second);
            position += std::strlen(entity.second);
        }
    }
    return value;
}

std::string firstXmlTag(const std::string& xml, const char* tag)
{
    const std::string open = "<" + std::string(tag);
    const std::string close = "</" + std::string(tag) + ">";
    const size_t begin = xml.find(open);
    if (begin == std::string::npos) {
        return {};
    }
    const size_t contentBegin = xml.find('>', begin + open.size());
    if (contentBegin == std::string::npos) {
        return {};
    }
    const size_t contentEnd = xml.find(close, contentBegin + 1U);
    if (contentEnd == std::string::npos) {
        return {};
    }
    return decodeXmlText(xml.substr(contentBegin + 1U, contentEnd - contentBegin - 1U));
}

std::string trim(std::string value)
{
    const auto whitespace = [](unsigned char character) { return std::isspace(character) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [&](char character) { return !whitespace(character); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](char character) { return !whitespace(character); }).base(),
                value.end());
    return value;
}

float xmlFloat(const std::string& xml, const char* tag, float fallback)
{
    const std::string value = trim(firstXmlTag(xml, tag));
    if (value.empty()) {
        return fallback;
    }
    try {
        const float parsed = std::stof(value);
        return std::isfinite(parsed) ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> xmlObjectBlocks(const std::string& xml)
{
    std::vector<std::string> blocks;
    const std::string open = "<object";
    const std::string close = "</object>";
    size_t position = 0;
    while ((position = xml.find(open, position)) != std::string::npos) {
        const size_t suffix = position + open.size();
        if (suffix >= xml.size() ||
            !(xml[suffix] == '>' || std::isspace(static_cast<unsigned char>(xml[suffix])))) {
            position = suffix;
            continue;
        }
        const size_t end = xml.find(close, suffix);
        if (end == std::string::npos) {
            break;
        }
        blocks.push_back(xml.substr(position, end + close.size() - position));
        position = end + close.size();
    }
    return blocks;
}

std::filesystem::path resolveProjectAsset(const std::filesystem::path& inputPath,
                                          const std::string& value)
{
    const std::filesystem::path path(trim(value));
    return path.is_absolute() ? path : (inputPath.parent_path() / path).lexically_normal();
}

struct ObjGeometry {
    std::vector<Vec3> positions;
    std::vector<std::array<int, 3>> triangles;
};

ObjGeometry readObjGeometry(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open scene OBJ: " + path.string());
    }
    ObjGeometry geometry;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        std::string record;
        stream >> record;
        if (record == "v") {
            Vec3 position{};
            if (!(stream >> position[0] >> position[1] >> position[2])) {
                throw std::runtime_error("Invalid OBJ vertex in " + path.string());
            }
            geometry.positions.push_back(position);
        } else if (record == "f") {
            std::vector<int> face;
            std::string token;
            while (stream >> token) {
                face.push_back(parsePositionIndex(token, geometry.positions.size()));
            }
            for (size_t index = 1; index + 1 < face.size(); ++index) {
                geometry.triangles.push_back({face[0], face[index], face[index + 1]});
            }
        }
    }
    if (geometry.positions.empty() || geometry.triangles.empty()) {
        throw std::runtime_error("Scene OBJ contains no usable geometry: " + path.string());
    }
    float minimumY = std::numeric_limits<float>::max();
    for (const Vec3& position : geometry.positions) {
        minimumY = std::min(minimumY, position[1]);
    }
    for (Vec3& position : geometry.positions) {
        position[1] -= minimumY;
    }
    return geometry;
}

struct Placement {
    float x{};
    float y{};
    float z{};
    float scale{1.0f};
    float rotation{};
};

std::vector<Placement> readPlacements(const std::filesystem::path& inputPath,
                                      const std::string& objectXml,
                                      float sceneWidth,
                                      float sceneDepth)
{
    std::vector<Placement> placements;
    const std::string positionFile = trim(firstXmlTag(objectXml, "objectPosition"));
    if (!positionFile.empty()) {
        const std::filesystem::path path = resolveProjectAsset(inputPath, positionFile);
        std::ifstream input(path);
        if (!input) {
            throw std::runtime_error("Cannot open object positions: " + path.string());
        }
        std::string line;
        while (std::getline(input, line)) {
            std::istringstream stream(line);
            Placement placement;
            if (!(stream >> placement.x >> placement.y >> placement.z)) {
                continue;
            }
            if (!(stream >> placement.scale)) {
                placement.scale = 1.0f;
                stream.clear();
            }
            if (!(stream >> placement.rotation)) {
                placement.rotation = 0.0f;
            }
            if (std::isfinite(placement.x) && std::isfinite(placement.y) &&
                std::isfinite(placement.z) && std::isfinite(placement.scale) &&
                std::isfinite(placement.rotation) && placement.scale > 0.0f) {
                placements.push_back(placement);
            }
        }
    }
    if (placements.empty()) {
        placements.push_back({sceneWidth * 0.5f, sceneDepth * 0.5f, 0.0f, 1.0f, 0.0f});
    }
    return placements;
}

Vec3 transformPosition(const Vec3& position,
                       const Placement& placement,
                       float sceneWidth,
                       float sceneDepth)
{
    const float radians = placement.rotation * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float x = position[0] * placement.scale;
    const float y = position[1] * placement.scale;
    const float z = position[2] * placement.scale;
    return {
        cosine * x + sine * z + placement.x - sceneWidth * 0.5f,
        y + placement.z,
        -sine * x + cosine * z + placement.y - sceneDepth * 0.5f
    };
}

void appendGeometry(Scene& scene,
                    const ObjGeometry& geometry,
                    const Placement& placement,
                    float sceneWidth,
                    float sceneDepth)
{
    for (const auto& triangle : geometry.triangles) {
        appendTriangle(scene,
                       transformPosition(geometry.positions[triangle[0]], placement,
                                         sceneWidth, sceneDepth),
                       transformPosition(geometry.positions[triangle[1]], placement,
                                         sceneWidth, sceneDepth),
                       transformPosition(geometry.positions[triangle[2]], placement,
                                         sceneWidth, sceneDepth));
    }
}

ObjGeometry cubeGeometry(const std::string& objectXml)
{
    float length = 1.0f;
    float width = 1.0f;
    float height = 1.0f;
    std::istringstream values(firstXmlTag(objectXml, "shapes"));
    char separator = 0;
    values >> length >> separator >> width >> separator >> height;
    if (!(length > 0.0f) || !(width > 0.0f) || !(height > 0.0f)) {
        length = width = height = 1.0f;
    }
    const float halfX = length * 0.5f;
    const float halfZ = width * 0.5f;
    ObjGeometry geometry;
    geometry.positions = {
        {-halfX, 0.0f, -halfZ}, {halfX, 0.0f, -halfZ},
        {halfX, 0.0f, halfZ}, {-halfX, 0.0f, halfZ},
        {-halfX, height, -halfZ}, {halfX, height, -halfZ},
        {halfX, height, halfZ}, {-halfX, height, halfZ}
    };
    geometry.triangles = {
        {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
        {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}
    };
    return geometry;
}

void appendProjectGround(Scene& scene, float sceneWidth, float sceneDepth)
{
    const float x0 = -sceneWidth * 0.5f;
    const float z0 = -sceneDepth * 0.5f;
    for (uint32_t row = 0; row < kSoilGridResolution; ++row) {
        const float za = z0 + sceneDepth * static_cast<float>(row) /
                                 static_cast<float>(kSoilGridResolution);
        const float zb = z0 + sceneDepth * static_cast<float>(row + 1U) /
                                 static_cast<float>(kSoilGridResolution);
        for (uint32_t column = 0; column < kSoilGridResolution; ++column) {
            const float xa = x0 + sceneWidth * static_cast<float>(column) /
                                     static_cast<float>(kSoilGridResolution);
            const float xb = x0 + sceneWidth * static_cast<float>(column + 1U) /
                                     static_cast<float>(kSoilGridResolution);
            appendTriangle(scene, {xa, -0.01f, za}, {xb, -0.01f, zb}, {xb, -0.01f, za});
            appendTriangle(scene, {xa, -0.01f, za}, {xa, -0.01f, zb}, {xb, -0.01f, zb});
        }
    }
}

Scene loadProjectScene(const std::string& inputFile)
{
    const std::filesystem::path inputPath(inputFile);
    const std::string xml = readTextFile(inputFile);
    const float sceneWidth = std::max(0.01f, xmlFloat(xml, "sceneSizeX", 8.0f));
    const float sceneDepth = std::max(0.01f, xmlFloat(xml, "sceneSizeY", 8.0f));
    Scene scene;

    std::string lightAngles = firstXmlTag(xml, "lightAngle");
    const size_t nestedTagEnd = lightAngles.find('>');
    if (nestedTagEnd != std::string::npos) {
        lightAngles = lightAngles.substr(nestedTagEnd + 1U);
    }
    std::istringstream angleValues(lightAngles);
    char separator = 0;
    if (!(angleValues >> scene.sunZenith >> separator >> scene.sunAzimuth)) {
        scene.sunZenith = 30.0f;
        scene.sunAzimuth = 135.0f;
    }

    for (const std::string& objectXml : xmlObjectBlocks(xml)) {
        std::string modelFile = trim(firstXmlTag(objectXml, "fileName"));
        if (modelFile.empty()) {
            modelFile = trim(firstXmlTag(objectXml, "objectfile"));
        }
        const ObjGeometry geometry = modelFile.empty()
            ? cubeGeometry(objectXml)
            : readObjGeometry(resolveProjectAsset(inputPath, modelFile));
        for (const Placement& placement :
             readPlacements(inputPath, objectXml, sceneWidth, sceneDepth)) {
            appendGeometry(scene, geometry, placement, sceneWidth, sceneDepth);
        }
    }

    scene.leafFacetCount = scene.facetCount();
    appendProjectGround(scene, sceneWidth, sceneDepth);
    scene.minimum = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()};
    scene.maximum = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest()};
    for (const facetvk::Vertex& vertex : scene.vertices) {
        for (size_t axis = 0; axis < 3; ++axis) {
            scene.minimum[axis] = std::min(scene.minimum[axis], vertex.position[axis]);
            scene.maximum[axis] = std::max(scene.maximum[axis], vertex.position[axis]);
        }
    }
    return scene;
}

std::string generatedCubeFromInput(const std::string& inputPath, const std::string& xml)
{
    float length = 1.0f;
    float width = 1.0f;
    float height = 1.0f;
    const std::string dimensions = firstXmlTag(xml, "shapes");
    std::istringstream values(dimensions);
    char separator = 0;
    values >> length >> separator >> width >> separator >> height;
    if (!(length > 0.0f) || !(width > 0.0f) || !(height > 0.0f)) {
        length = width = height = 1.0f;
    }

    const size_t hash = std::hash<std::string>{}(inputPath);
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("histream_facetrt_default_" + std::to_string(hash) + ".obj");
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Input.xml contains no OBJ fileName and fallback OBJ could not be created");
    }
    const float hx = length * 0.5f;
    const float hz = width * 0.5f;
    output << "# Generated from Input.xml cube dimensions\n"
           << "v " << -hx << " 0 " << -hz << "\n"
           << "v " << hx << " 0 " << -hz << "\n"
           << "v " << hx << " 0 " << hz << "\n"
           << "v " << -hx << " 0 " << hz << "\n"
           << "v " << -hx << " " << height << " " << -hz << "\n"
           << "v " << hx << " " << height << " " << -hz << "\n"
           << "v " << hx << " " << height << " " << hz << "\n"
           << "v " << -hx << " " << height << " " << hz << "\n"
           << "f 1 3 2\n f 1 4 3\n"
           << "f 5 6 7\n f 5 7 8\n"
           << "f 1 2 6\n f 1 6 5\n"
           << "f 2 3 7\n f 2 7 6\n"
           << "f 3 4 8\n f 3 8 7\n"
           << "f 4 1 5\n f 4 5 8\n";
    return path.string();
}

std::string sceneFileFromInput(const std::string& inputPath)
{
    const std::filesystem::path input(inputPath);
    if (input.extension() == ".obj" || input.extension() == ".OBJ") {
        return input.string();
    }
    const std::string xml = readTextFile(inputPath);
    std::string scene = firstXmlTag(xml, "fileName");
    if (scene.empty()) {
        scene = firstXmlTag(xml, "objectfile");
    }
    if (scene.empty()) {
        std::cerr << "facetrt: Input.xml has no OBJ; using the first cube shape as a generated scene\n";
        return generatedCubeFromInput(inputPath, xml);
    }
    const std::filesystem::path scenePath(scene);
    return scenePath.is_absolute() ? scenePath.string() :
        (input.parent_path() / scenePath).lexically_normal().string();
}

std::string outputFileFromInput(const std::string& inputPath,
                                const std::string& requestedOutput,
                                const char* defaultName)
{
    if (!requestedOutput.empty()) {
        return requestedOutput;
    }
    const std::filesystem::path input(inputPath);
    const std::string xml = readTextFile(inputPath);
    std::string output = firstXmlTag(xml, "outDir");
    if (output.empty()) {
        output = input.parent_path().string();
    }
    return (std::filesystem::path(output) / defaultName).lexically_normal().string();
}

} // namespace

int runFacetRTCore(const std::string& inputPath,
               const std::string& shaderDirectory,
               const std::string& outputFile)
try {
    if (inputPath.empty()) {
        throw std::invalid_argument("FacetRT requires Input.xml");
    }
    const std::string resultFile = outputFileFromInput(inputPath, outputFile, "radiosity_gpu.json");
    const std::filesystem::path input(inputPath);
    const Scene scene = input.extension() == ".obj" || input.extension() == ".OBJ"
        ? loadTreeWithSoil(inputPath)
        : loadProjectScene(inputPath);
    const Progress progress = [](int value, const std::string& stage) {
        std::cout << "PROGRESS\t" << value << '\t' << stage << std::endl;
    };
    const RunResult result = runGpu(scene, shaderDirectory, progress);
    std::filesystem::create_directories(std::filesystem::path(resultFile).parent_path());
    writeWebResult(resultFile, scene, result);
    std::cout << "RESULT\t" << resultFile << std::endl;
    return 0;
} catch (const std::exception& error) {
    std::cerr << "facetrt: " << error.what() << '\n';
    return 1;
}


int runFacetEBCore(const std::string& inputPath,
                   const std::string& shaderDirectory,
                   const std::string& outputFile)
try {
    if (inputPath.empty()) {
        throw std::invalid_argument("FacetEB requires Input.xml");
    }

    struct MeteoStep {
        int node{};
        float julianTime{};
        float airTemperature{25.0f};
        float vaporPressure{15.0f};
        float pressure{839.0f};
        float windSpeed{1.0f};
        float shortwave{};
        float longwave{320.0f};
    };

    const std::filesystem::path input(inputPath);
    const std::string xml = readTextFile(inputPath);
    Scene scene = input.extension() == ".obj" || input.extension() == ".OBJ"
        ? loadTreeWithSoil(inputPath)
        : loadProjectScene(inputPath);
    const std::string resultFile =
        outputFileFromInput(inputPath, outputFile, "faceteb.json");
    const std::filesystem::path outputDirectory =
        std::filesystem::path(resultFile).parent_path();
    std::filesystem::create_directories(outputDirectory);

    const int startNode = std::max(0, static_cast<int>(
        std::lround(xmlFloat(xml, "startTimeNode", 0.0f))));
    const int endNode = std::max(startNode + 1, static_cast<int>(
        std::lround(xmlFloat(xml, "endTimeNode", static_cast<float>(startNode + 1)))));
    const float dTime = std::max(1.0f, xmlFloat(xml, "dTime", 1800.0f));
    const bool saveProcess = trim(firstXmlTag(xml, "isProcess")) == "1";
    const uint32_t maximumCouplingIterations = static_cast<uint32_t>(std::clamp(
        static_cast<int>(std::lround(xmlFloat(xml, "couplingIterations", 20.0f))),
        1, 100));
    const float temperatureTolerance = std::clamp(
        xmlFloat(xml, "temperatureTolerance", 0.05f), 0.001f, 10.0f);
    const float temperatureRelaxation = std::clamp(
        xmlFloat(xml, "temperatureRelaxation", 0.5f), 0.05f, 1.0f);
    const float latitude = xmlFloat(xml, "Latitude", 40.0f);
    const float longitude = xmlFloat(xml, "Longitude", 116.0f);
    const float skyTemperature = std::clamp(
        xmlFloat(xml, "skyTemperature", 250.0f), 150.0f, 350.0f);

    std::vector<MeteoStep> meteorology;
    const std::string meteoName = trim(firstXmlTag(xml, "filePath"));
    if (!meteoName.empty()) {
        std::ifstream meteo(resolveProjectAsset(input, meteoName));
        if (!meteo) {
            throw std::runtime_error("FacetEB cannot open meteorology: " + meteoName);
        }
        std::string line;
        std::getline(meteo, line);
        int row = 0;
        while (std::getline(meteo, line)) {
            std::istringstream values(line);
            MeteoStep step;
            step.node = row++;
            if (!(values >> step.julianTime >> step.airTemperature >>
                  step.vaporPressure >> step.pressure >> step.windSpeed >>
                  step.shortwave >> step.longwave)) {
                continue;
            }
            meteorology.push_back(step);
        }
    }
    if (meteorology.empty()) {
        meteorology.resize(static_cast<size_t>(endNode));
        for (int node = 0; node < endNode; ++node) {
            meteorology[static_cast<size_t>(node)].node = node;
            meteorology[static_cast<size_t>(node)].julianTime =
                1.0f + static_cast<float>(node) * dTime / 86400.0f;
        }
    }
    if (endNode > static_cast<int>(meteorology.size())) {
        throw std::runtime_error("FacetEB time range exceeds meteorology rows");
    }

    const auto timeToken = [](float julianTime) {
        int day = static_cast<int>(std::floor(julianTime));
        int minutes = static_cast<int>(std::lround(
            (static_cast<double>(julianTime) - day) * 1440.0));
        if (minutes >= 1440) {
            day += minutes / 1440;
            minutes %= 1440;
        }
        std::ostringstream token;
        token << "DOY" << day << '_' << std::setw(2) << std::setfill('0')
              << minutes / 60 << '-' << std::setw(2) << std::setfill('0')
              << minutes % 60;
        return token.str();
    };

    const auto updateSolarPosition = [&](const MeteoStep& step) {
        int month = 1;
        int day = 1;
        const int dayOfYear = static_cast<int>(std::floor(step.julianTime));
        Utils::calculateMonthAndDay(2019, dayOfYear, &month, &day);
        const double fractionalDay =
            static_cast<double>(step.julianTime) - dayOfYear;
        const int totalSeconds =
            static_cast<int>(std::lround(fractionalDay * 86400.0));
        spa_data data{};
        data.year = 2019;
        data.month = month;
        data.day = day;
        data.hour = std::clamp(totalSeconds / 3600, 0, 23);
        data.minute = std::clamp((totalSeconds % 3600) / 60, 0, 59);
        data.second = std::clamp(totalSeconds % 60, 0, 59);
        data.timezone = 8.0;
        data.delta_t = Utils::calculateDeltaT(data.year, data.month);
        data.longitude = longitude;
        data.latitude = latitude;
        data.elevation = 100.0;
        data.pressure = std::max(100.0f, step.pressure);
        data.temperature = step.airTemperature;
        data.slope = 0.0;
        data.azm_rotation = 0.0;
        data.atmos_refract = 0.5667;
        data.function = SPA_ZA;
        SPACalc calculator;
        const int error = calculator.spa_calculate(&data);
        if (error != 0) {
            throw std::runtime_error("FacetEB solar-position calculation failed");
        }
        scene.sunZenith = static_cast<float>(data.zenith);
        scene.sunAzimuth = static_cast<float>(data.azimuth);
    };

    facetvk::Config config{};
    config.rasterWidth = kRasterSize;
    config.rasterHeight = kRasterSize;
    config.maxFragmentsPerPixel = kRasterLayers;
    config.gpuIndex = static_cast<uint32_t>(std::max(
        0, static_cast<int>(std::lround(xmlFloat(xml, "GPU", 0.0f)))));

    std::cout << "PROGRESS\t3\tFacetEB initializes geometry and Vulkan once" << std::endl;
    facetvk::FacetrtVulkan model;
    model.initialize(config, shaderDirectory);
    model.setGeometry(scene.vertices);
    std::cout << "PROGRESS\t10\tFacetEB builds the shared visibility graph once" << std::endl;
    const facetvk::GraphDiagnostics graph =
        model.buildVisibilityGraph(makeHemisphereDirections());

    const size_t surfaceCount = static_cast<size_t>(scene.facetCount()) * 2U;
    const float initialSunlitTemperature = std::clamp(
        xmlFloat(xml, "sunlitTemperature", 300.0f), 220.0f, 360.0f);
    const float initialShadedTemperature = std::clamp(
        xmlFloat(xml, "shadedTemperature", 296.0f), 220.0f, 360.0f);
    std::vector<float> temperature(surfaceCount, initialShadedTemperature);
    for (size_t side = 0; side < surfaceCount; side += 2U) {
        temperature[side] = initialSunlitTemperature;
    }

    RunResult latest;
    latest.backend = "Vulkan GPU Facet RT-EB coupled";
    latest.graph = graph;
    std::vector<float> latestNetRadiation(surfaceCount);
    std::vector<float> latestSensibleHeat(surfaceCount);
    std::vector<float> latestLatentHeat(surfaceCount);
    std::vector<float> latestStorageHeat(surfaceCount);
    uint32_t latestCouplingIterations = 0;
    float latestTemperatureDelta = 0.0f;
    std::string latestTime;

    constexpr float stefanBoltzmann = 5.670374419e-8f;
    const std::filesystem::path stepDirectory = outputDirectory / ".facet_steps";
    std::filesystem::create_directories(stepDirectory);
    if (saveProcess) {
        std::filesystem::create_directories(outputDirectory / "process");
    }

    const auto totalBegin = Clock::now();
    for (int node = startNode; node < endNode; ++node) {
        const MeteoStep& meteo = meteorology[static_cast<size_t>(node)];
        updateSolarPosition(meteo);
        const std::string token = timeToken(meteo.julianTime);
        const std::vector<float> previousTemperature = temperature;
        const std::vector<float> sunlit =
            model.computeSunlitFraction(sunDirection(scene));
        std::vector<float> netRadiation(surfaceCount);
        std::vector<float> sensibleHeat(surfaceCount);
        std::vector<float> latentHeat(surfaceCount);
        std::vector<float> storageHeat(surfaceCount);
        facetvk::SolveResult radiativeSolution;
        uint32_t couplingIterations = 0;
        float maximumTemperatureDelta = 0.0f;

        for (uint32_t coupling = 0;
             coupling < maximumCouplingIterations; ++coupling) {
            std::vector<facetvk::SurfaceOptics> optics =
                makeOptics(scene, sunlit);
            const float shortwaveScale =
                std::max(0.0f, meteo.shortwave) / kBeamNormalIrradiance;
            for (size_t side = 0; side < surfaceCount; ++side) {
                const float emissivity = std::clamp(
                    1.0f - optics[side].reflectance -
                    optics[side].transmittance, 0.01f, 1.0f);
                optics[side].directIrradiance *= shortwaveScale;
                optics[side].emission = emissivity * stefanBoltzmann *
                    std::pow(temperature[side], 4.0f);
            }

            const float skyRadiosity =
                std::max(0.0f, meteo.longwave) +
                0.15f * std::max(0.0f, meteo.shortwave);
            radiativeSolution =
                model.solve(optics, skyRadiosity, kIterations, 1.0f);

            maximumTemperatureDelta = 0.0f;
            const float airTemperature =
                std::clamp(meteo.airTemperature + 273.15f, 220.0f, 340.0f);
            const float aerodynamicConductance =
                5.8f + 4.1f * std::sqrt(std::max(0.1f, meteo.windSpeed));
            for (size_t side = 0; side < surfaceCount; ++side) {
                const bool leaf = side / 2U < scene.leafFacetCount;
                const float emissivity = std::clamp(
                    1.0f - optics[side].reflectance -
                    optics[side].transmittance, 0.01f, 1.0f);
                const float emittedLongwave = emissivity * stefanBoltzmann *
                    std::pow(temperature[side], 4.0f);
                const float absorbedRadiation = std::max(
                    0.0f, radiativeSolution.radiosity[side] -
                    emittedLongwave + emissivity * std::max(0.0f, meteo.longwave));
                netRadiation[side] = absorbedRadiation - emittedLongwave;
                sensibleHeat[side] = aerodynamicConductance *
                    (temperature[side] - airTemperature);
                const float available = std::max(
                    0.0f, netRadiation[side] - sensibleHeat[side]);
                latentHeat[side] = (leaf ? 0.42f : 0.08f) * available;
                const float heatCapacity = leaf ? 60000.0f : 1800000.0f;
                storageHeat[side] = heatCapacity *
                    (temperature[side] - previousTemperature[side]) / dTime;
                const float residual = netRadiation[side] -
                    sensibleHeat[side] - latentHeat[side] -
                    storageHeat[side];
                const float derivative =
                    4.0f * emissivity * stefanBoltzmann *
                    std::pow(temperature[side], 3.0f) +
                    aerodynamicConductance + heatCapacity / dTime;
                const float candidate = std::clamp(
                    temperature[side] + residual / std::max(1.0f, derivative),
                    220.0f, 360.0f);
                const float updated = temperature[side] +
                    temperatureRelaxation * (candidate - temperature[side]);
                maximumTemperatureDelta = std::max(
                    maximumTemperatureDelta,
                    std::abs(updated - temperature[side]));
                temperature[side] = updated;
            }
            couplingIterations = coupling + 1U;
            if (maximumTemperatureDelta <= temperatureTolerance) {
                break;
            }
        }

        std::vector<facetvk::SurfaceOptics> finalOptics =
            makeOptics(scene, sunlit);
        const float shortwaveScale =
            std::max(0.0f, meteo.shortwave) / kBeamNormalIrradiance;
        for (size_t side = 0; side < surfaceCount; ++side) {
            const float emissivity = std::clamp(
                1.0f - finalOptics[side].reflectance -
                finalOptics[side].transmittance, 0.01f, 1.0f);
            finalOptics[side].directIrradiance *= shortwaveScale;
            finalOptics[side].emission = emissivity * stefanBoltzmann *
                std::pow(temperature[side], 4.0f);
        }
        const float skyRadiosity =
            std::max(0.0f, meteo.longwave) +
            0.15f * std::max(0.0f, meteo.shortwave);
        radiativeSolution =
            model.solve(finalOptics, skyRadiosity, kIterations, 1.0f);
        const facetvk::SolveResult diffuseSolution =
            model.solve(withoutDirectLight(finalOptics), skyRadiosity,
                        kIterations, 1.0f);

        const std::filesystem::path binaryPath =
            stepDirectory / ("energy_T=" + token + ".bin");
        std::ofstream binary(binaryPath, std::ios::binary | std::ios::trunc);
        if (!binary) {
            throw std::runtime_error(
                "FacetEB cannot write time-step energy file");
        }
        for (size_t side = 0; side < surfaceCount; ++side) {
            const float values[7] = {
                temperature[side],
                radiativeSolution.radiosity[side],
                netRadiation[side],
                sensibleHeat[side],
                latentHeat[side],
                storageHeat[side],
                sunlit[side]
            };
            binary.write(reinterpret_cast<const char*>(values), sizeof(values));
        }

        if (saveProcess) {
            const std::filesystem::path metadataPath =
                outputDirectory / "process" / ("energy_T=" + token + ".json");
            std::ofstream metadata(metadataPath, std::ios::trunc);
            metadata << std::setprecision(9)
                     << "{\n  \"kind\": \"facet-energy-process\",\n"
                     << "  \"node\": " << node << ",\n"
                     << "  \"julianTime\": " << meteo.julianTime << ",\n"
                     << "  \"time\": \"" << token << "\",\n"
                     << "  \"facetCount\": " << scene.facetCount() << ",\n"
                     << "  \"surfaceCount\": " << surfaceCount << ",\n"
                     << "  \"dataFile\": \"../.facet_steps/"
                     << binaryPath.filename().string() << "\",\n"
                     << "  \"dataType\": \"float32-little-endian\",\n"
                     << "  \"layout\": \"surface-interleaved\",\n"
                     << "  \"couplingIterations\": " << couplingIterations << ",\n"
                     << "  \"temperatureDelta\": "
                     << maximumTemperatureDelta << ",\n"
                     << "  \"fields\": [\"temperature\", \"radiosity\", "
                        "\"netRadiation\", \"sensibleHeat\", \"latentHeat\", "
                        "\"storageHeat\", \"sunlit\"]\n}\n";
        }

        latest.sunlit = sunlit;
        latest.solution = radiativeSolution;
        latest.lightEnhancement = lightEnhancement(
            radiativeSolution.radiosity, diffuseSolution.radiosity);
        latestNetRadiation = netRadiation;
        latestSensibleHeat = sensibleHeat;
        latestLatentHeat = latentHeat;
        latestStorageHeat = storageHeat;
        latestCouplingIterations = couplingIterations;
        latestTemperatureDelta = maximumTemperatureDelta;
        latestTime = token;

        const int progress = 10 + static_cast<int>(
            86.0 * (node - startNode + 1) / (endNode - startNode));
        std::cout << "PROGRESS\t" << progress
                  << "\tFacetEB coupled node " << node << " " << token
                  << " iterations=" << couplingIterations
                  << " deltaT=" << maximumTemperatureDelta << "K"
                  << std::endl;
    }

    latest.totalMs = milliseconds(totalBegin, Clock::now());
    latest.solveMs = latest.totalMs;

    std::ofstream output(resultFile, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("FacetEB cannot write result: " + resultFile);
    }
    output << std::setprecision(9)
           << "{\n  \"backend\": \"" << latest.backend << "\",\n"
           << "  \"facetCount\": " << scene.facetCount() << ",\n"
           << "  \"leafFacetCount\": " << scene.leafFacetCount << ",\n"
           << "  \"time\": \"" << latestTime << "\",\n"
           << "  \"vertexPositions\": [";
    for (size_t index = 0; index < scene.vertices.size(); ++index) {
        if (index != 0U) output << ',';
        const auto& vertex = scene.vertices[index];
        output << vertex.position[0] << ',' << vertex.position[1] << ','
               << vertex.position[2];
    }
    output << "],\n"
           << "  \"graph\": {\"directedEdges\":"
           << latest.graph.directedEdgeCount
           << ",\"closureError\":" << latest.graph.maxClosureError
           << ",\"fragmentOverflow\":" << latest.graph.fragmentOverflow
           << ",\"hashOverflow\":" << latest.graph.hashOverflow << "},\n"
           << "  \"iterations\": " << latest.solution.iterations << ",\n"
           << "  \"maxDelta\": " << latest.solution.maxDelta << ",\n"
           << "  \"couplingIterations\": " << latestCouplingIterations << ",\n"
           << "  \"temperatureDelta\": " << latestTemperatureDelta << ",\n";

    const auto writeArray = [&output](
        const char* name, const std::vector<float>& values, bool comma) {
        output << "  \"" << name << "\": [";
        for (size_t index = 0; index < values.size(); ++index) {
            if (index != 0U) output << ',';
            output << values[index];
        }
        output << ']' << (comma ? "," : "") << '\n';
    };
    writeArray("sunlit", latest.sunlit, true);
    writeArray("radiosity", latest.solution.radiosity, true);
    writeArray("lightEnhancement", latest.lightEnhancement, true);
    writeArray("temperature", temperature, true);
    writeArray("netRadiation", latestNetRadiation, true);
    writeArray("sensibleHeat", latestSensibleHeat, true);
    writeArray("latentHeat", latestLatentHeat, true);
    writeArray("storageHeat", latestStorageHeat, false);
    output << "}\n";

    std::cout << "PROGRESS\t100\tFacetEB RT-EB coupling complete" << std::endl;
    std::cout << "RESULT\t" << resultFile << std::endl;
    return 0;
} catch (const std::exception& error) {
    std::cerr << "faceteb: " << error.what() << '\n';
    return 1;
}
