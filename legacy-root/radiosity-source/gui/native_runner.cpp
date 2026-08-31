#include "../vulkanradiosity/vulkan_radiosity.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
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
    for (const auto& triangle : triangles) {
        appendTriangle(scene, positions[triangle[0]], positions[triangle[1]],
                       positions[triangle[2]]);
    }
    scene.leafFacetCount = static_cast<uint32_t>(hasTerrain ? terrainTriangleStart : triangles.size());

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

facetvk::Direction sunDirection()
{
    constexpr float zenith = 30.0f * kPi / 180.0f;
    constexpr float azimuth = 135.0f * kPi / 180.0f;
    return {std::sin(zenith) * std::cos(azimuth), std::cos(zenith),
            std::sin(zenith) * std::sin(azimuth)};
}

std::vector<facetvk::SurfaceOptics> makeOptics(const Scene& scene,
                                                const std::vector<float>& sunlit)
{
    const facetvk::Direction direction = sunDirection();
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
    if (raster.rasterize(sunDirection()) != 0U) {
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
    config.enableValidation = true;

    facetvk::VulkanRadiosity model;
    model.initialize(config, shaderDirectory);
    model.setGeometry(scene.vertices);

    progress(10, "GPU visibility graph: 40 raster directions");
    const auto visibilityBegin = Clock::now();
    result.graph = model.buildVisibilityGraph(makeHemisphereDirections());
    const auto visibilityEnd = Clock::now();
    result.visibilityMs = milliseconds(visibilityBegin, visibilityEnd);

    progress(72, "GPU sunlight visibility raster");
    const auto sunlightBegin = Clock::now();
    result.sunlit = model.computeSunlitFraction(sunDirection());
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

int main(int argc, char** argv)
try {
    const std::string shaderDirectory = argc > 1 ? argv[1] : RADIOSITY_VULKAN_SHADER_DIR;
    const std::string treeFile = argc > 2 ? argv[2] : "data/example/single_tree_LAI_4.obj";
    const Scene scene = loadTreeWithSoil(treeFile);
    if (argc > 5 && std::string(argv[3]) == "--web") {
        const std::string backend = argv[4];
        const std::string outputFile = argv[5];
        const Progress progress = [](int value, const std::string& stage) {
            std::cout << "PROGRESS\t" << value << '\t' << stage << std::endl;
        };
        RunResult result;
        if (backend == "cpu") {
            result = runCpu(scene, progress);
        } else if (backend == "gpu") {
            result = runGpu(scene, shaderDirectory, progress);
        } else {
            throw std::invalid_argument("Web backend must be cpu or gpu");
        }
        writeWebResult(outputFile, scene, result);
        std::cout << "RESULT\t" << outputFile << std::endl;
        return 0;
    }
    std::cerr << "This executable is the Three.js compute backend.\n"
              << "Start gui/start.cmd and use the web page.\n";
    return 2;
} catch (const std::exception& error) {
    std::cerr << "radiosity_web_runner: " << error.what() << '\n';
    return 1;
}
