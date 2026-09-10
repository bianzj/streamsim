#include "facetrtcore.h"
#include "../base/projectjson.h"
#include "facetrt_vulkan.h"
#include "../base/compo.h"
#include "../base/utils.h"
#include "../thirdparty/spa.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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

#include <gdal_priv.h>

namespace {

using Clock = std::chrono::steady_clock;
using Vec3 = std::array<float, 3>;
constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kRasterSize = 1024;
constexpr uint32_t kRasterLayers = 64;
constexpr uint32_t kLargeSceneRasterSize = 2048;
constexpr uint32_t kLargeSceneRasterLayers = 16;
constexpr uint32_t kMaximumRasterLayers = 64;
constexpr float kLargeSceneExtent = 1000.0f;
constexpr uint32_t kSkyDirectionCount = 40;
constexpr uint32_t kIterations = 64;
constexpr uint32_t kStandaloneSoilGridResolution = 48;
constexpr float kBeamNormalIrradiance = 700.0f;
constexpr float kSkyDiffuseIrradiance = 100.0f;
constexpr float kStefanBoltzmann = 5.670374419e-8f;

struct ReflectanceVarianceTexture {
    int width{};
    int height{};
    std::vector<float> values;

    float sample(float u, float v) const
    {
        if (width <= 0 || height <= 0 || values.empty()) return 0.0f;
        const float x = (u - std::floor(u)) * static_cast<float>(width);
        const float y = (v - std::floor(v)) * static_cast<float>(height);
        const int x0 = static_cast<int>(std::floor(x)) % width;
        const int y0 = static_cast<int>(std::floor(y)) % height;
        const int x1 = (x0 + 1) % width;
        const int y1 = (y0 + 1) % height;
        const float tx = x - std::floor(x);
        const float ty = y - std::floor(y);
        const auto at = [this](int column, int row) {
            return values[static_cast<size_t>(row) * width + column];
        };
        const float top = at(x0, y0) * (1.0f - tx) + at(x1, y0) * tx;
        const float bottom = at(x0, y1) * (1.0f - tx) + at(x1, y1) * tx;
        return top * (1.0f - ty) + bottom * ty;
    }
};

struct SurfaceParameters {
    std::vector<float> reflectance{0.10f};
    std::vector<float> transmittance{0.05f};
    float tirReflectance{0.03f};
    float tirTransmittance{};
    float sunlitTemperature{300.0f};
    float shadedTemperature{296.0f};
    float surfaceResistance{120.0f};
    float specificHeat{1180.0f};
    float density{1800.0f};
    float thermalConductivity{1.55f};
    float soilMoisture{0.25f};
    float heatCapacityPerArea{60000.0f};
    int soilTemperatureMethod{1};
    std::vector<float> energyReflectance;
    std::vector<float> energyTransmittance;
    std::shared_ptr<const ReflectanceVarianceTexture> reflectanceTexture;
    float reflectanceTextureStrength{};
    float reflectanceTextureRepeatSize{1.0f};
    bool photovoltaic{false};
    bool vegetation{true};
    float pvEta25{0.22f}, pvGamma{-0.0035f}, pvBifaciality{0.0f};
    float convectiveScale{1.0f};

};

struct FacetRTParameters {
    std::vector<float> wavelengths{550.0f};
    SurfaceParameters object;
    SurfaceParameters ground{{0.20f}, {0.0f}, 0.05f, 0.0f, 305.0f, 295.0f};
    float skyTemperature{250.0f};
    bool acceleratedSolver{};
};

struct Scene {
    std::vector<facetvk::Vertex> vertices;
    std::vector<uint32_t> materialIndices;
    std::vector<SurfaceParameters> materials;
    std::vector<float> reflectanceTextureFactors;
    uint32_t leafFacetCount{};
    Vec3 minimum{};
    Vec3 maximum{};
    float sunZenith{30.0f};
    float sunAzimuth{135.0f};
    float backgroundAngularStrength{};
    uint32_t visibilityRasterSize{kRasterSize};
    uint32_t visibilityRasterLayers{kRasterLayers};
    uint32_t periodicNeighborCount{};
    float periodicSizeX{1.0f};
    float periodicSizeZ{1.0f};

    uint32_t facetCount() const { return static_cast<uint32_t>(vertices.size() / 3U); }
};

uint32_t adaptiveVisibilityRasterLayers(const Scene& scene)
{
    const double pixelCount = static_cast<double>(scene.visibilityRasterSize) *
                              scene.visibilityRasterSize;
    if (pixelCount <= 0.0) return scene.visibilityRasterLayers;

    // Triangle projections are highly non-uniform in vegetation and urban
    // scenes. Reserve sixteen times the scene-wide average depth and round to
    // a power of two. This keeps sparse large scenes at 16 layers while dense
    // canopies automatically expand to 32 or 64 layers.
    const double averageFragments =
        static_cast<double>(scene.facetCount()) *
        static_cast<double>(scene.periodicNeighborCount + 1U) / pixelCount;
    const uint32_t estimatedLayers = static_cast<uint32_t>(std::ceil(
        std::max(1.0, averageFragments * 16.0)));
    uint32_t layers = scene.visibilityRasterLayers;
    while (layers < estimatedLayers && layers < kMaximumRasterLayers) {
        layers = std::min(kMaximumRasterLayers, layers * 2U);
    }
    return layers;
}

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
    std::vector<float> wavelengths;
    std::vector<float> bandRadiosity;
    std::string quantity{"hemispherical radiosity"};
    std::string units{"W m-2"};
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

std::array<int, 2> periodicTileOffset(uint32_t index)
{
    static constexpr std::array<std::array<int, 2>, 20> offsets{{
        {{-1, 0}}, {{1, 0}}, {{0, -1}}, {{0, 1}},
        {{-1, -1}}, {{-1, 1}}, {{1, -1}}, {{1, 1}},
        {{-2, 0}}, {{2, 0}}, {{0, -2}}, {{0, 2}},
        {{-2, -1}}, {{-2, 1}}, {{2, -1}}, {{2, 1}},
        {{-1, -2}}, {{1, -2}}, {{-1, 2}}, {{1, 2}}
    }};
    return index == 0U ? std::array<int, 2>{0, 0}
                       : offsets[std::min<uint32_t>(index - 1U, 19U)];
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

bool appendTriangle(Scene& scene,
                    const Vec3& a,
                    const Vec3& b,
                    const Vec3& c,
                    uint32_t materialIndex = 0U)
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
    scene.materialIndices.push_back(materialIndex);
    return true;
}

std::vector<Vec3> clipPolygonToPlane(const std::vector<Vec3>& source,
                                     size_t axis,
                                     float plane,
                                     bool keepGreater)
{
    std::vector<Vec3> result;
    if (source.empty()) return result;
    const auto inside = [axis, plane, keepGreater](const Vec3& point) {
        return keepGreater ? point[axis] >= plane - 1.0e-6f
                           : point[axis] <= plane + 1.0e-6f;
    };
    Vec3 previous = source.back();
    bool previousInside = inside(previous);
    for (const Vec3& current : source) {
        const bool currentInside = inside(current);
        if (currentInside != previousInside) {
            const float denominator = current[axis] - previous[axis];
            if (std::abs(denominator) > 1.0e-8f) {
                const float amount = std::clamp(
                    (plane - previous[axis]) / denominator, 0.0f, 1.0f);
                Vec3 intersection{};
                for (size_t component = 0; component < 3; ++component) {
                    intersection[component] = previous[component] + amount *
                        (current[component] - previous[component]);
                }
                result.push_back(intersection);
            }
        }
        if (currentInside) result.push_back(current);
        previous = current;
        previousInside = currentInside;
    }
    return result;
}

void appendTriangleInsideDomain(Scene& scene,
                                const Vec3& a,
                                const Vec3& b,
                                const Vec3& c,
                                const Vec3& minimum,
                                const Vec3& maximum,
                                uint32_t materialIndex)
{
    std::vector<Vec3> polygon{a, b, c};
    for (size_t axis = 0; axis < 3 && polygon.size() >= 3U; ++axis) {
        polygon = clipPolygonToPlane(polygon, axis, minimum[axis], true);
        polygon = clipPolygonToPlane(polygon, axis, maximum[axis], false);
    }
    for (size_t index = 1; index + 1U < polygon.size(); ++index) {
        appendTriangle(scene, polygon[0], polygon[index], polygon[index + 1U],
                       materialIndex);
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
    scene.materials = {
        SurfaceParameters{},
        SurfaceParameters{{0.20f}, {0.0f}, 0.05f, 0.0f, 305.0f, 295.0f}
    };
    const uint32_t soilFacetCount = hasTerrain ? 0U :
        2U * kStandaloneSoilGridResolution * kStandaloneSoilGridResolution;
    scene.vertices.reserve((triangles.size() + soilFacetCount) * 3U);
    uint32_t leafFacetCount = 0;
    for (size_t triangleIndex = 0; triangleIndex < triangles.size(); ++triangleIndex) {
        const auto& triangle = triangles[triangleIndex];
        const uint32_t materialIndex =
            hasTerrain && triangleIndex >= terrainTriangleStart ? 1U : 0U;
        if (appendTriangle(scene, positions[triangle[0]], positions[triangle[1]],
                           positions[triangle[2]], materialIndex) &&
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
        for (uint32_t row = 0; row < kStandaloneSoilGridResolution; ++row) {
            const float za = z0 + (z1 - z0) * static_cast<float>(row) /
                                      static_cast<float>(kStandaloneSoilGridResolution);
            const float zb = z0 + (z1 - z0) * static_cast<float>(row + 1U) /
                                      static_cast<float>(kStandaloneSoilGridResolution);
            for (uint32_t column = 0; column < kStandaloneSoilGridResolution; ++column) {
                const float xa = x0 + (x1 - x0) * static_cast<float>(column) /
                                          static_cast<float>(kStandaloneSoilGridResolution);
                const float xb = x0 + (x1 - x0) * static_cast<float>(column + 1U) /
                                          static_cast<float>(kStandaloneSoilGridResolution);
                appendTriangle(scene, {xa, -0.01f, za}, {xb, -0.01f, zb},
                               {xb, -0.01f, za}, 1U);
                appendTriangle(scene, {xa, -0.01f, za}, {xa, -0.01f, zb},
                               {xb, -0.01f, zb}, 1U);
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
    // SPA azimuth is clockwise from north: +X is north and +Z is east.
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

float planckRadiance(float wavelengthNanometers, float temperature)
{
    constexpr double c1 = 1.1910439340652e8;
    constexpr double c2 = 14388.291040407;
    const double wavelengthMicrometers = wavelengthNanometers > 50.0f
        ? static_cast<double>(wavelengthNanometers) / 1000.0
        : static_cast<double>(wavelengthNanometers);
    const double exponent = c2 /
        (std::max(1.0, static_cast<double>(temperature)) *
         std::max(1.0e-6, wavelengthMicrometers));
    return static_cast<float>(c1 /
        (std::pow(wavelengthMicrometers, 5.0) * std::expm1(exponent)));
}

float bandValue(const std::vector<float>& values, size_t band, float fallback)
{
    if (values.empty()) {
        return fallback;
    }
    return values[std::min(band, values.size() - 1U)];
}

float facetHapkeH(float mu, float singleScatteringAlbedo)
{
    const float gamma = std::sqrt(std::max(1.0f - singleScatteringAlbedo, 1.0e-6f));
    return (1.0f + 2.0f * mu) / (1.0f + 2.0f * mu * gamma);
}

float facetHapkeKernel(float mu0, float mu, float phaseAngle,
                       float singleScatteringAlbedo)
{
    constexpr float oppositionAmplitude = 1.0f;
    constexpr float oppositionWidth = 0.1f;
    const float opposition = oppositionAmplitude /
        (1.0f + std::tan(0.5f * phaseAngle) / oppositionWidth);
    const float multiple = facetHapkeH(mu0, singleScatteringAlbedo) *
        facetHapkeH(mu, singleScatteringAlbedo) - 1.0f;
    return singleScatteringAlbedo / (4.0f * kPi) *
        mu0 / std::max(mu0 + mu, 1.0e-5f) * (1.0f + opposition + multiple);
}

float backgroundHapkeReflectance(float reflectance, Vec3 incoming, Vec3 outgoing,
                                 const Vec3& surfaceNormal, float strength)
{
    const Vec3 normal = normalized(surfaceNormal);
    incoming = normalized(incoming);
    outgoing = normalized(outgoing);
    if (dot(incoming, normal) < 0.0f) incoming = multiply(incoming, -1.0f);
    if (dot(outgoing, normal) < 0.0f) outgoing = multiply(outgoing, -1.0f);
    const float mu0 = std::max(dot(normal, incoming), 1.0e-4f);
    const float mu = std::max(dot(normal, outgoing), 1.0e-4f);
    const float phaseAngle = std::acos(std::clamp(dot(incoming, outgoing), -1.0f, 1.0f));
    const float base = std::clamp(reflectance, 0.0f, 0.999f);
    const float singleScatteringAlbedo = std::clamp(
        4.0f * base / std::pow(1.0f + base, 2.0f), 1.0e-5f, 0.999f);
    const float directional = facetHapkeKernel(
        mu0, mu, phaseAngle, singleScatteringAlbedo);
    const float referenceH = facetHapkeH(1.0f, singleScatteringAlbedo);
    const float reference = singleScatteringAlbedo / (8.0f * kPi) *
        referenceH * referenceH;
    const float hapke = std::clamp(
        base * directional / std::max(reference, 1.0e-6f), 0.0f, 1.0f);
    return base + (hapke - base) * std::clamp(strength, 0.0f, 1.0f);
}

float infraredHapkeEmissivity(float baseEmissivity, float cosine,
                              float strength)
{
    const float base = std::clamp(baseEmissivity, 0.0f, 1.0f);
    const float singleScatteringAlbedo = std::clamp(
        1.0f - base * base, 1.0e-5f, 0.999f);
    const auto directional = [singleScatteringAlbedo](float mu) {
        const float root = std::sqrt(1.0f - singleScatteringAlbedo);
        return root * (1.0f + 2.0f * mu / 1.5f) /
            (1.0f + 2.0f * mu / 1.5f * root);
    };
    const float hapke = std::clamp(
        base * directional(std::clamp(cosine, 1.0e-4f, 1.0f)) /
            std::max(directional(1.0f), 1.0e-6f),
        0.0f, 1.0f);
    return base + (hapke - base) * std::clamp(strength, 0.0f, 1.0f);
}

const SurfaceParameters& surfaceForFacet(const Scene& scene, uint32_t facet)
{
    const uint32_t materialIndex =
        facet < scene.materialIndices.size()
        ? scene.materialIndices[facet]
        : std::numeric_limits<uint32_t>::max();
    if (materialIndex < scene.materials.size()) {
        return scene.materials[materialIndex];
    }
    static const SurfaceParameters objectFallback{};
    static const SurfaceParameters groundFallback{
        {0.20f}, {0.0f}, 0.05f, 0.0f, 305.0f, 295.0f};
    return facet < scene.leafFacetCount
        ? objectFallback : groundFallback;
}

float srgbToLinear(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value <= 0.04045f
        ? value / 12.92f
        : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

std::shared_ptr<const ReflectanceVarianceTexture> loadReflectanceVarianceTexture(
    const std::filesystem::path& path)
{
    const std::string key = std::filesystem::absolute(path).lexically_normal().string();
    static std::unordered_map<
        std::string, std::weak_ptr<const ReflectanceVarianceTexture>> cache;
    const auto cached = cache.find(key);
    if (cached != cache.end()) {
        if (const auto texture = cached->second.lock()) return texture;
    }

    GDALAllRegister();
    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpen(key.c_str(), GA_ReadOnly));
    if (!dataset || dataset->GetRasterCount() < 1) {
        if (dataset) GDALClose(dataset);
        throw std::runtime_error("Cannot open reflectance variance texture: " + key);
    }
    const int width = dataset->GetRasterXSize();
    const int height = dataset->GetRasterYSize();
    const int channelCount = std::min(3, dataset->GetRasterCount());
    if (width < 2 || height < 2) {
        GDALClose(dataset);
        throw std::runtime_error(
            "Reflectance variance texture must contain at least 2 x 2 pixels: " + key);
    }

    const size_t pixelCount = static_cast<size_t>(width) * height;
    std::array<std::vector<uint8_t>, 3> channels;
    for (int channel = 0; channel < channelCount; ++channel) {
        channels[channel].resize(pixelCount);
        const CPLErr result = dataset->GetRasterBand(channel + 1)->RasterIO(
            GF_Read, 0, 0, width, height, channels[channel].data(),
            width, height, GDT_Byte, 0, 0);
        if (result != CE_None) {
            GDALClose(dataset);
            throw std::runtime_error(
                "Cannot read reflectance variance texture: " + key);
        }
    }
    GDALClose(dataset);

    auto texture = std::make_shared<ReflectanceVarianceTexture>();
    texture->width = width;
    texture->height = height;
    texture->values.resize(pixelCount);
    double mean = 0.0;
    for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const float red = srgbToLinear(channels[0][pixel] / 255.0f);
        const float green = channelCount > 1
            ? srgbToLinear(channels[1][pixel] / 255.0f) : red;
        const float blue = channelCount > 2
            ? srgbToLinear(channels[2][pixel] / 255.0f) : red;
        const float luminance =
            0.2126f * red + 0.7152f * green + 0.0722f * blue;
        texture->values[pixel] = luminance;
        mean += luminance;
    }
    mean /= static_cast<double>(pixelCount);
    float maximumAbsolute = 0.0f;
    for (float& value : texture->values) {
        value -= static_cast<float>(mean);
        maximumAbsolute = std::max(maximumAbsolute, std::abs(value));
    }
    if (!(maximumAbsolute > 1.0e-6f)) {
        throw std::runtime_error(
            "Reflectance variance texture is effectively constant: " + key);
    }
    for (float& value : texture->values) value /= maximumAbsolute;
    cache[key] = texture;
    return texture;
}

float facetArea(const Scene& scene, uint32_t facet)
{
    const facetvk::Vertex& a = scene.vertices[facet * 3U];
    const facetvk::Vertex& b = scene.vertices[facet * 3U + 1U];
    const facetvk::Vertex& c = scene.vertices[facet * 3U + 2U];
    const Vec3 edgeA{b.position[0] - a.position[0],
                     b.position[1] - a.position[1],
                     b.position[2] - a.position[2]};
    const Vec3 edgeB{c.position[0] - a.position[0],
                     c.position[1] - a.position[1],
                     c.position[2] - a.position[2]};
    return 0.5f * std::sqrt(dot(cross(edgeA, edgeB), cross(edgeA, edgeB)));
}

float sampledReflectanceVariance(const Scene& scene, uint32_t facet,
                                 const SurfaceParameters& surface)
{
    if (!surface.reflectanceTexture) return 0.0f;
    const facetvk::Vertex& a = scene.vertices[facet * 3U];
    const facetvk::Vertex& b = scene.vertices[facet * 3U + 1U];
    const facetvk::Vertex& c = scene.vertices[facet * 3U + 2U];
    const Vec3 center{
        (a.position[0] + b.position[0] + c.position[0]) / 3.0f,
        (a.position[1] + b.position[1] + c.position[1]) / 3.0f,
        (a.position[2] + b.position[2] + c.position[2]) / 3.0f};
    const Vec3 normal{std::abs(a.normal[0]), std::abs(a.normal[1]),
                      std::abs(a.normal[2])};
    const float weightSum = std::max(1.0e-6f, normal[0] + normal[1] + normal[2]);
    const float inverseRepeat =
        1.0f / std::max(0.01f, surface.reflectanceTextureRepeatSize);
    const auto& texture = *surface.reflectanceTexture;
    return (
        normal[0] * texture.sample(center[2] * inverseRepeat,
                                   center[1] * inverseRepeat) +
        normal[1] * texture.sample(center[0] * inverseRepeat,
                                   center[2] * inverseRepeat) +
        normal[2] * texture.sample(center[0] * inverseRepeat,
                                   center[1] * inverseRepeat)) / weightSum;
}

float maximumMeanPreservingFactor(const SurfaceParameters& surface)
{
    float maximumFactor = std::numeric_limits<float>::infinity();
    const auto includeBands = [&maximumFactor](
        const std::vector<float>& reflectance,
        const std::vector<float>& transmittance) {
        for (size_t band = 0; band < reflectance.size(); ++band) {
            const float base = std::clamp(reflectance[band], 0.0f, 1.0f);
            if (!(base > 1.0e-6f)) continue;
            const float tau = transmittance.empty() ? 0.0f : std::clamp(
                transmittance[std::min(band, transmittance.size() - 1U)],
                0.0f, 1.0f - base);
            maximumFactor = std::min(maximumFactor, (1.0f - tau) / base);
        }
    };
    includeBands(surface.reflectance, surface.transmittance);
    includeBands(surface.energyReflectance, surface.energyTransmittance);
    return maximumFactor;
}

void finalizeReflectanceTextureFactors(Scene& scene)
{
    const uint32_t facetCount = scene.facetCount();
    scene.reflectanceTextureFactors.assign(facetCount, 1.0f);
    if (facetCount == 0U || scene.materials.empty()) return;

    struct MaterialStatistics {
        double area{};
        double areaWeightedSample{};
        float minimumDeviation{};
        float maximumDeviation{};
        float scale{};
        double areaWeightedFactor{};
        float minimumFactor{std::numeric_limits<float>::infinity()};
        float maximumFactor{std::numeric_limits<float>::lowest()};
    };
    std::vector<MaterialStatistics> statistics(scene.materials.size());
    std::vector<float> samples(facetCount, 0.0f);
    std::vector<float> areas(facetCount, 0.0f);
    for (uint32_t facet = 0; facet < facetCount; ++facet) {
        if (facet >= scene.materialIndices.size()) continue;
        const uint32_t materialIndex = scene.materialIndices[facet];
        if (materialIndex >= scene.materials.size()) continue;
        const SurfaceParameters& surface = scene.materials[materialIndex];
        if (!surface.reflectanceTexture ||
            !(surface.reflectanceTextureStrength > 0.0f)) continue;
        areas[facet] = facetArea(scene, facet);
        samples[facet] = sampledReflectanceVariance(scene, facet, surface);
        statistics[materialIndex].area += areas[facet];
        statistics[materialIndex].areaWeightedSample +=
            static_cast<double>(areas[facet]) * samples[facet];
    }
    for (uint32_t facet = 0; facet < facetCount; ++facet) {
        if (facet >= scene.materialIndices.size() || !(areas[facet] > 0.0f)) continue;
        const uint32_t materialIndex = scene.materialIndices[facet];
        MaterialStatistics& stats = statistics[materialIndex];
        const float mean = stats.area > 0.0
            ? static_cast<float>(stats.areaWeightedSample / stats.area) : 0.0f;
        const float deviation = samples[facet] - mean;
        samples[facet] = deviation;
        stats.minimumDeviation = std::min(stats.minimumDeviation, deviation);
        stats.maximumDeviation = std::max(stats.maximumDeviation, deviation);
    }
    for (size_t materialIndex = 0;
         materialIndex < scene.materials.size(); ++materialIndex) {
        MaterialStatistics& stats = statistics[materialIndex];
        if (!(stats.area > 0.0)) continue;
        const SurfaceParameters& surface = scene.materials[materialIndex];
        float scale = std::clamp(
            surface.reflectanceTextureStrength, 0.0f, 1.0f);
        if (stats.minimumDeviation < 0.0f) {
            scale = std::min(scale, 0.999f / -stats.minimumDeviation);
        }
        const float maximumFactor = maximumMeanPreservingFactor(surface);
        if (stats.maximumDeviation > 0.0f && std::isfinite(maximumFactor)) {
            scale = std::min(
                scale, std::max(0.0f, maximumFactor - 1.0f) /
                           stats.maximumDeviation);
        }
        stats.scale = scale;
    }
    for (uint32_t facet = 0; facet < facetCount; ++facet) {
        if (facet >= scene.materialIndices.size() || !(areas[facet] > 0.0f)) continue;
        const uint32_t materialIndex = scene.materialIndices[facet];
        const float factor =
            1.0f + statistics[materialIndex].scale * samples[facet];
        scene.reflectanceTextureFactors[facet] = factor;
        MaterialStatistics& stats = statistics[materialIndex];
        stats.areaWeightedFactor += static_cast<double>(areas[facet]) * factor;
        stats.minimumFactor = std::min(stats.minimumFactor, factor);
        stats.maximumFactor = std::max(stats.maximumFactor, factor);
    }
    for (size_t materialIndex = 0;
         materialIndex < statistics.size(); ++materialIndex) {
        const MaterialStatistics& stats = statistics[materialIndex];
        if (!(stats.area > 0.0)) continue;
        std::cout << "facetrt: physical texture material " << materialIndex
                  << ", area-weighted factor mean="
                  << stats.areaWeightedFactor / stats.area
                  << ", range=[" << stats.minimumFactor << ','
                  << stats.maximumFactor << "]" << std::endl;
    }
}

float reflectanceTextureFactor(const Scene& scene, uint32_t facet)
{
    return facet < scene.reflectanceTextureFactors.size()
        ? scene.reflectanceTextureFactors[facet] : 1.0f;
}

float saturationVaporPressure(float temperature)
{
    const float celsius = std::clamp(temperature - 273.15f, -80.0f, 80.0f);
    return 6.107f * std::pow(10.0f, 7.5f * celsius / (237.3f + celsius));
}

float saturationVaporPressureSlope(float temperature)
{
    const float celsius = std::clamp(temperature - 273.15f, -80.0f, 80.0f);
    const float saturation = saturationVaporPressure(temperature);
    return saturation * 2.3026f * 7.5f * 237.3f /
        std::pow(237.3f + celsius, 2.0f);
}

float latentHeatFlux(float temperature,
                     float airDensity,
                     float airPressure,
                     float vaporPressure,
                     float aerodynamicResistance,
                     float surfaceResistance)
{
    const float celsius = temperature - 273.15f;
    const float vaporizationHeat =
        (2.501f - 0.002361f * celsius) * 1.0e6f;
    const float humidityDeficit = 0.622f *
        std::max(0.0f, saturationVaporPressure(temperature) - vaporPressure) /
        std::max(100.0f, airPressure);
    return airDensity * vaporizationHeat * humidityDeficit /
        std::max(1.0f, aerodynamicResistance + surfaceResistance);
}

float latentHeatFluxDerivative(float temperature,
                               float airDensity,
                               float airPressure,
                               float vaporPressure,
                               float aerodynamicResistance,
                               float surfaceResistance)
{
    const float celsius = temperature - 273.15f;
    const float saturation = saturationVaporPressure(temperature);
    const float slope = saturationVaporPressureSlope(temperature);
    const float vaporizationHeat =
        (2.501f - 0.002361f * celsius) * 1.0e6f;
    const float humidityDeficit = 0.622f *
        std::max(0.0f, saturation - vaporPressure) /
        std::max(100.0f, airPressure);
    const float derivative = airDensity /
        std::max(1.0f, aerodynamicResistance + surfaceResistance) *
        (-2361.0f * humidityDeficit +
         vaporizationHeat * 0.622f * slope /
             std::max(100.0f, airPressure));
    return std::max(0.0f, derivative);
}

float soilStorageCoefficient(const SurfaceParameters& surface, float dTime)
{
    const float thermalInertia = std::sqrt(std::max(
        0.0f, surface.specificHeat * surface.density *
                  surface.thermalConductivity));
    return 2.0f * thermalInertia /
        std::sqrt(kPi * std::max(1.0f, dTime));
}

float shortwaveWeightedBandValue(
    const std::vector<float>& values, float fallback)
{
    if (values.empty()) {
        return fallback;
    }
    if (values.size() == 1U) {
        return std::isfinite(values.front()) ? values.front() : fallback;
    }
    // Two-value materials represent visible and NIR/SWIR broad bands.
    if (values.size() == 2U) {
        const float visible = std::isfinite(values[0]) ? values[0] : fallback;
        const float infrared = std::isfinite(values[1]) ? values[1] : visible;
        return 0.43f * visible + 0.57f * infrared;
    }

    double weightedValue = 0.0;
    double weightSum = 0.0;
    for (size_t band = 0; band < values.size(); ++band) {
        if (!std::isfinite(values[band])) {
            continue;
        }
        const float wavelength = 400.0f + 2000.0f *
            static_cast<float>(band) /
            static_cast<float>(values.size() - 1U);
        const double weight = std::max(
            0.0f, planckRadiance(wavelength, 5778.0f));
        weightedValue += weight * static_cast<double>(values[band]);
        weightSum += weight;
    }
    return weightSum > 0.0
        ? static_cast<float>(weightedValue / weightSum)
        : fallback;
}

std::vector<facetvk::SurfaceOptics> makeEnergyShortwaveOptics(
    const Scene& scene,
    const std::vector<float>& sunlit,
    float directNormalIrradiance,
    size_t energyBand)
{
    const facetvk::Direction direction = sunDirection(scene);
    const Vec3 sun{direction.x, direction.y, direction.z};
    std::vector<facetvk::SurfaceOptics> optics(scene.facetCount() * 2U);
    for (uint32_t facet = 0; facet < scene.facetCount(); ++facet) {
        const bool leaf = facet < scene.leafFacetCount;
        const SurfaceParameters& surface = surfaceForFacet(scene, facet);
        const float meanReflectance =
            surface.energyReflectance.empty()
                ? shortwaveWeightedBandValue(
                    surface.reflectance, leaf ? 0.10f : 0.20f)
                : bandValue(
                    surface.energyReflectance, energyBand,
                    leaf ? 0.10f : 0.20f);
        const float reflectance = std::clamp(
            meanReflectance * reflectanceTextureFactor(scene, facet),
            0.0f, 1.0f);
        const float transmittance = std::clamp(
            surface.energyTransmittance.empty()
                ? shortwaveWeightedBandValue(
                    surface.transmittance, leaf ? 0.05f : 0.0f)
                : bandValue(
                    surface.energyTransmittance, energyBand,
                    leaf ? 0.05f : 0.0f),
            0.0f, 1.0f - reflectance);
        const facetvk::Vertex& vertex = scene.vertices[facet * 3U];
        const Vec3 normal{
            vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        for (uint32_t localSide = 0; localSide < 2U; ++localSide) {
            const uint32_t side = facet * 2U + localSide;
            const float sign = localSide == 0U ? 1.0f : -1.0f;
            const Vec3 sideNormal = multiply(normal, sign);
            const float sideReflectance = leaf ? reflectance :
                backgroundHapkeReflectance(
                    reflectance, sun, sideNormal, sideNormal,
                    scene.backgroundAngularStrength);
            optics[side].reflectance = sideReflectance;
            optics[side].transmittance = std::min(
                transmittance, 1.0f - sideReflectance);
            optics[side].directIrradiance =
                std::clamp(sunlit[side], 0.0f, 1.0f) *
                std::max(0.0f, dot(sideNormal, sun)) *
                directNormalIrradiance;
        }
    }
    return optics;
}

std::vector<facetvk::SurfaceOptics> makeEnergyLongwaveOptics(
    const Scene& scene,
    const std::vector<float>& temperature)
{
    std::vector<facetvk::SurfaceOptics> optics(scene.facetCount() * 2U);
    for (uint32_t facet = 0; facet < scene.facetCount(); ++facet) {
        const bool ground = facet >= scene.leafFacetCount;
        const SurfaceParameters& surface = surfaceForFacet(scene, facet);
        const float reflectance =
            std::clamp(surface.tirReflectance, 0.0f, 1.0f);
        const float transmittance = std::clamp(
            surface.tirTransmittance, 0.0f, 1.0f - reflectance);
        const float emissivity =
            std::max(0.0f, 1.0f - reflectance - transmittance);
        const facetvk::Vertex& vertex = scene.vertices[facet * 3U];
        const Vec3 normal{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        for (uint32_t localSide = 0; localSide < 2U; ++localSide) {
            const uint32_t side = facet * 2U + localSide;
            const float sign = localSide == 0U ? 1.0f : -1.0f;
            const Vec3 sideNormal = multiply(normal, sign);
            const float sideEmissivity = ground ? infraredHapkeEmissivity(
                emissivity, std::abs(sideNormal[1]),
                scene.backgroundAngularStrength) : emissivity;
            optics[side].reflectance = std::clamp(
                1.0f - transmittance - sideEmissivity, 0.0f, 1.0f);
            optics[side].transmittance = transmittance;
            optics[side].emission = sideEmissivity * kStefanBoltzmann *
                std::pow(std::clamp(temperature[side], 1.0f, 1000.0f), 4.0f);
        }
    }
    return optics;
}

facetvk::SolveResult combineRadiativeSolutions(
    const facetvk::SolveResult& shortwave,
    const facetvk::SolveResult& longwave)
{
    if (shortwave.radiosity.size() != longwave.radiosity.size()) {
        throw std::runtime_error(
            "Shortwave and longwave radiosity size mismatch");
    }
    facetvk::SolveResult combined = longwave;
    for (size_t side = 0; side < combined.radiosity.size(); ++side) {
        combined.radiosity[side] += shortwave.radiosity[side];
    }
    combined.iterations = std::max(
        shortwave.iterations, longwave.iterations);
    combined.maxDelta = std::max(
        shortwave.maxDelta, longwave.maxDelta);
    return combined;
}

std::vector<facetvk::SurfaceOptics> makeSpectralOptics(
    const Scene& scene,
    const std::vector<float>& sunlit,
    const FacetRTParameters& parameters,
    size_t band)
{
    const float wavelength = parameters.wavelengths[band];
    const bool thermal = wavelength > 2500.0f;
    const facetvk::Direction direction = sunDirection(scene);
    const Vec3 sun{direction.x, direction.y, direction.z};
    std::vector<facetvk::SurfaceOptics> optics(scene.facetCount() * 2U);
    for (uint32_t facet = 0; facet < scene.facetCount(); ++facet) {
        const bool object = facet < scene.leafFacetCount;
        const uint32_t materialIndex =
            facet < scene.materialIndices.size()
            ? scene.materialIndices[facet] : std::numeric_limits<uint32_t>::max();
        const SurfaceParameters& surface =
            materialIndex < scene.materials.size()
            ? scene.materials[materialIndex]
            : (object ? parameters.object : parameters.ground);
        const facetvk::Vertex& vertex = scene.vertices[facet * 3U];
        const Vec3 normal{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        for (uint32_t localSide = 0; localSide < 2U; ++localSide) {
            const uint32_t side = facet * 2U + localSide;
            const float meanReflectance = thermal
                ? surface.tirReflectance
                : bandValue(surface.reflectance, band,
                            object ? 0.10f : 0.20f);
            float reflectance = std::clamp(
                meanReflectance * (thermal
                    ? 1.0f : reflectanceTextureFactor(scene, facet)),
                0.0f, 1.0f);
            const float transmittance = std::clamp(
                thermal ? surface.tirTransmittance
                        : bandValue(surface.transmittance, band, object ? 0.05f : 0.0f),
                0.0f, 1.0f - reflectance);
            const float sign = localSide == 0U ? 1.0f : -1.0f;
            const Vec3 sideNormal = multiply(normal, sign);
            float emissivity = std::max(0.0f, 1.0f - reflectance - transmittance);
            if (!object && thermal) {
                emissivity = infraredHapkeEmissivity(
                    emissivity, std::abs(sideNormal[1]),
                    scene.backgroundAngularStrength);
                reflectance = std::clamp(
                    1.0f - transmittance - emissivity, 0.0f, 1.0f);
            } else if (!object) {
                reflectance = backgroundHapkeReflectance(
                    reflectance, sun, sideNormal, sideNormal,
                    scene.backgroundAngularStrength);
            }
            optics[side].reflectance = reflectance;
            optics[side].transmittance = std::min(
                transmittance, 1.0f - reflectance);
            if (thermal) {
                const float illuminated = std::clamp(sunlit[side], 0.0f, 1.0f);
                const float blackbodyRadiance =
                    illuminated * planckRadiance(wavelength, surface.sunlitTemperature) +
                    (1.0f - illuminated) *
                        planckRadiance(wavelength, surface.shadedTemperature);
                optics[side].emission = emissivity * blackbodyRadiance;
                optics[side].directIrradiance = 0.0f;
            } else {
                optics[side].directIrradiance =
                    sunlit[side] * std::max(0.0f, dot(sideNormal, sun)) *
                    kBeamNormalIrradiance;
            }
        }
    }
    return optics;
}

bool isOpticalWavelength(float wavelength)
{
    const float wavelengthNanometers = wavelength <= 2.5f
        ? wavelength * 1000.0f : wavelength;
    return wavelengthNanometers <= 2500.0f;
}

std::vector<facetvk::SurfaceOptics> makeStateSpectralOptics(
    const Scene& scene,
    const std::vector<float>& sunlit,
    const FacetRTParameters& parameters,
    size_t band,
    const std::vector<float>& temperature,
    float directNormalIrradiance)
{
    const float wavelength = parameters.wavelengths[band];
    const bool optical = isOpticalWavelength(wavelength);
    const facetvk::Direction direction = sunDirection(scene);
    const Vec3 sun{direction.x, direction.y, direction.z};
    std::vector<facetvk::SurfaceOptics> optics(scene.facetCount() * 2U);
    for (uint32_t facet = 0; facet < scene.facetCount(); ++facet) {
        const bool object = facet < scene.leafFacetCount;
        const SurfaceParameters& surface = surfaceForFacet(scene, facet);
        const facetvk::Vertex& vertex = scene.vertices[facet * 3U];
        const Vec3 normal{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        for (uint32_t localSide = 0; localSide < 2U; ++localSide) {
            const uint32_t side = facet * 2U + localSide;
            const float meanReflectance = optical
                ? bandValue(surface.reflectance, band,
                            object ? 0.10f : 0.20f)
                : surface.tirReflectance;
            float reflectance = std::clamp(
                meanReflectance * (optical
                    ? reflectanceTextureFactor(scene, facet) : 1.0f),
                0.0f, 1.0f);
            const float transmittance = std::clamp(
                optical
                    ? bandValue(surface.transmittance, band,
                                object ? 0.05f : 0.0f)
                    : surface.tirTransmittance,
                0.0f, 1.0f - reflectance);
            const float sign = localSide == 0U ? 1.0f : -1.0f;
            const Vec3 sideNormal = multiply(normal, sign);
            float emissivity = std::max(0.0f, 1.0f - reflectance - transmittance);
            if (!object && optical) {
                reflectance = backgroundHapkeReflectance(
                    reflectance, sun, sideNormal, sideNormal,
                    scene.backgroundAngularStrength);
            } else if (!object) {
                emissivity = infraredHapkeEmissivity(
                    emissivity, std::abs(sideNormal[1]),
                    scene.backgroundAngularStrength);
                reflectance = std::clamp(
                    1.0f - transmittance - emissivity, 0.0f, 1.0f);
            }
            optics[side].reflectance = reflectance;
            optics[side].transmittance = std::min(
                transmittance, 1.0f - reflectance);
            if (optical) {
                optics[side].directIrradiance =
                    std::clamp(sunlit[side], 0.0f, 1.0f) *
                    std::max(0.0f, dot(sideNormal, sun)) *
                    directNormalIrradiance;
            } else {
                optics[side].emission = emissivity * planckRadiance(
                    wavelength,
                    std::clamp(temperature[side], 1.0f, 2000.0f));
            }
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
        Vec3 extendedMinimum = scene.minimum;
        Vec3 extendedMaximum = scene.maximum;
        for (uint32_t tileIndex = 1U;
             tileIndex <= scene.periodicNeighborCount; ++tileIndex) {
            const auto tile = periodicTileOffset(tileIndex);
            const float offsetX = tile[0] * scene.periodicSizeX;
            const float offsetZ = tile[1] * scene.periodicSizeZ;
            extendedMinimum[0] = std::min(
                extendedMinimum[0], scene.minimum[0] + offsetX);
            extendedMaximum[0] = std::max(
                extendedMaximum[0], scene.maximum[0] + offsetX);
            extendedMinimum[2] = std::min(
                extendedMinimum[2], scene.minimum[2] + offsetZ);
            extendedMaximum[2] = std::max(
                extendedMaximum[2], scene.maximum[2] + offsetZ);
        }
        const Vec3 diagonal = subtract(extendedMaximum, extendedMinimum);
        m_center = multiply(add(extendedMinimum, extendedMaximum), 0.5f);
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

        for (uint32_t tileIndex = 0;
             tileIndex <= m_scene.periodicNeighborCount; ++tileIndex) {
          const auto tile = periodicTileOffset(tileIndex);
          const Vec3 tileTranslation{
              tile[0] * m_scene.periodicSizeX, 0.0f,
              tile[1] * m_scene.periodicSizeZ};
          for (uint32_t facet = 0; facet < m_scene.facetCount(); ++facet) {
            ProjectedVertex projected[3];
            for (uint32_t corner = 0; corner < 3U; ++corner) {
                const facetvk::Vertex& vertex = m_scene.vertices[facet * 3U + corner];
                const Vec3 position{
                    vertex.position[0] + tileTranslation[0], vertex.position[1],
                    vertex.position[2] + tileTranslation[2]};
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
    CpuRasterizer raster(scene, scene.visibilityRasterSize,
                         scene.visibilityRasterSize,
                         scene.visibilityRasterLayers);
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
    CpuRasterizer raster(scene, scene.visibilityRasterSize,
                         scene.visibilityRasterSize,
                         scene.visibilityRasterLayers);
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
                 const Progress& progress,
                 const FacetRTParameters* spectralParameters = nullptr)
{
    RunResult result;
    result.backend = "Vulkan GPU";
    const auto totalBegin = Clock::now();
    progress(3, "Creating Vulkan device and GPU buffers");
    facetvk::Config config{};
    config.rasterWidth = scene.visibilityRasterSize;
    config.rasterHeight = scene.visibilityRasterSize;
    config.maxFragmentsPerPixel = scene.visibilityRasterLayers;
    config.enableValidation = false;
    config.periodicNeighborCount = scene.periodicNeighborCount;
    config.periodicSizeX = scene.periodicSizeX;
    config.periodicSizeZ = scene.periodicSizeZ;

    facetvk::FacetrtVulkan model;
    model.initialize(config, shaderDirectory);
    model.setGeometry(scene.vertices);

    progress(10, "GPU visibility graph: 40 directions at " +
                     std::to_string(scene.visibilityRasterSize) + " x " +
                     std::to_string(scene.visibilityRasterSize));
    const auto visibilityBegin = Clock::now();
    result.graph = model.buildVisibilityGraph(makeHemisphereDirections());
    const auto visibilityEnd = Clock::now();
    result.visibilityMs = milliseconds(visibilityBegin, visibilityEnd);

    progress(72, "GPU sunlight visibility raster");
    const auto sunlightBegin = Clock::now();
    result.sunlit = model.computeSunlitFraction(sunDirection(scene));
    const auto sunlightEnd = Clock::now();
    result.sunlightMs = milliseconds(sunlightBegin, sunlightEnd);

    const auto solveBegin = Clock::now();
    if (spectralParameters == nullptr) {
        progress(82, "GPU Jacobi radiosity: 64 iterations");
        const std::vector<facetvk::SurfaceOptics> optics =
            makeOptics(scene, result.sunlit);
        result.solution =
            model.solve(optics, kSkyDiffuseIrradiance, kIterations, 1.0f);
        progress(98, "GPU diffuse baseline and direct-light enhancement");
        const facetvk::SolveResult diffuseSolution =
            model.solve(withoutDirectLight(optics), kSkyDiffuseIrradiance,
                        kIterations, 1.0f);
        result.lightEnhancement =
            lightEnhancement(result.solution.radiosity,
                             diffuseSolution.radiosity);
    } else {
        result.wavelengths = spectralParameters->wavelengths;
        const size_t surfaceCount = static_cast<size_t>(scene.facetCount()) * 2U;
        const bool multiBand = spectralParameters->wavelengths.size() > 1U;
        if (multiBand) {
            result.bandRadiosity.reserve(
                surfaceCount * spectralParameters->wavelengths.size());
        }
        result.lightEnhancement.assign(surfaceCount, 0.0f);
        bool allThermal = true;
        bool anyThermal = false;
        bool previousBandWasThermal = false;
        bool hasPreviousBand = false;
        std::vector<float> previousRawRadiosity;
        const float cosineSolarZenith =
            std::max(0.0f, sunDirection(scene).y);
        const float opticalReferenceIrradiance =
            kBeamNormalIrradiance * cosineSolarZenith +
            kSkyDiffuseIrradiance * 0.5f;
        for (size_t band = 0; band < spectralParameters->wavelengths.size(); ++band) {
            const float wavelength = spectralParameters->wavelengths[band];
            const bool thermal = !isOpticalWavelength(wavelength);
            allThermal = allThermal && thermal;
            anyThermal = anyThermal || thermal;
            progress(82 + static_cast<int>(
                15U * band / spectralParameters->wavelengths.size()),
                std::string(spectralParameters->acceleratedSolver
                    ? "GPU accelerated spectral solve "
                    : "GPU traditional spectral solve ") +
                std::to_string(band + 1U) + "/" +
                std::to_string(spectralParameters->wavelengths.size()));
            const std::vector<facetvk::SurfaceOptics> optics =
                makeSpectralOptics(scene, result.sunlit,
                                   *spectralParameters, band);
            const float skyRadiance = thermal
                ? planckRadiance(wavelength,
                                 spectralParameters->skyTemperature)
                : kSkyDiffuseIrradiance * 0.5f;
            const std::vector<float>* initialRadiosity =
                spectralParameters->acceleratedSolver && hasPreviousBand &&
                previousBandWasThermal == thermal
                    ? &previousRawRadiosity : nullptr;
            facetvk::SolveResult bandSolution =
                spectralParameters->acceleratedSolver
                    ? model.solveAccelerated(
                          optics, skyRadiance, initialRadiosity,
                          kIterations, 8U, 1.0e-4f, 1.0f)
                    : model.solve(
                          optics, skyRadiance, kIterations, 1.0f);
            if (spectralParameters->acceleratedSolver) {
                // Preserve physical radiosity as the next-band initial state.
                // Apparent-reflectance conversion below is output-only.
                previousRawRadiosity = bandSolution.radiosity;
                previousBandWasThermal = thermal;
                hasPreviousBand = true;
            }
            // The optical solver returns outgoing radiosity.  Image products
            // require dimensionless apparent reflectance, referenced to the
            // same direct and diffuse illumination used by this solve.
            if (!thermal && opticalReferenceIrradiance > 1.0e-8f) {
                for (float& value : bandSolution.radiosity) {
                    value = std::clamp(
                        value / opticalReferenceIrradiance, 0.0f, 1.0f);
                }
            }
            if (multiBand) {
                result.bandRadiosity.insert(
                    result.bandRadiosity.end(),
                    bandSolution.radiosity.begin(), bandSolution.radiosity.end());
            }
            if (band == 0U) {
                result.solution = bandSolution;
                if (!thermal) {
                    facetvk::SolveResult diffuseSolution =
                        spectralParameters->acceleratedSolver
                            ? model.solveAccelerated(
                                  withoutDirectLight(optics), skyRadiance,
                                  nullptr, kIterations, 8U, 1.0e-4f, 1.0f)
                            : model.solve(
                                  withoutDirectLight(optics), skyRadiance,
                                  kIterations, 1.0f);
                    if (opticalReferenceIrradiance > 1.0e-8f) {
                        for (float& value : diffuseSolution.radiosity) {
                            value = std::clamp(
                                value / opticalReferenceIrradiance,
                                0.0f, 1.0f);
                        }
                    }
                    result.lightEnhancement = lightEnhancement(
                        bandSolution.radiosity, diffuseSolution.radiosity);
                }
            }
        }
        if (allThermal) {
            result.quantity = "spectral radiance";
            result.units = "W m-2 sr-1 um-1";
        } else if (!anyThermal) {
            result.quantity = "reflectance";
            result.units = "1";
        } else {
            result.quantity = "reflectance and spectral radiance";
            result.units = "band-dependent";
        }
    }
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
           << "  \"quantity\": \"" << result.quantity << "\",\n"
           << "  \"units\": \"" << result.units << "\",\n"
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
    writeArray("wavelengths", result.wavelengths, true);
    writeArray("sunlit", result.sunlit, true);
    writeArray("radiosity", result.solution.radiosity, true);
    writeArray("lightEnhancement", result.lightEnhancement,
               !result.bandRadiosity.empty());
    if (!result.bandRadiosity.empty()) {
        writeArray("bandRadiosity", result.bandRadiosity, false);
    }
    output << "}\n";
}

} // namespace

namespace {

std::string xmlEscaped(std::string value)
{
    const std::pair<const char*, const char*> entities[] = {
        {"&", "&amp;"}, {"<", "&lt;"}, {">", "&gt;"},
        {"\"", "&quot;"}, {"'", "&apos;"}
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

std::string joined(const std::vector<float>& values)
{
    std::ostringstream output;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << values[index];
    }
    return output.str();
}

std::string joinedNames(const ProjectJson::Json& object, const char* key,
                        const std::string& fallback)
{
    std::ostringstream output;
    if (object.contains("meshes") && object["meshes"].is_array()) {
        size_t index = 0;
        for (const auto& mesh : object["meshes"]) {
            if (index++) output << ',';
            output << ProjectJson::string(mesh, key, fallback);
        }
    }
    return output.str().empty() ? fallback : output.str();
}

// The facet solver still uses its proven tag-oriented parsing routines. This
// adapter is memory-only: project.json remains the sole file and CLI contract.
std::string facetInputView(const std::string& path)
{
    const ProjectJson project = ProjectJson::load(path);
    const auto& scene = project.scene();
    const auto& light = project.light();
    const auto& sensor = project.sensor();
    const auto& control = project.control();
    const auto& meteo = project.meteorology();
    const auto background = scene.contains("background")
        ? scene["background"] : ProjectJson::Json::object();
    for(const auto& m:project.materials())
        if(ProjectJson::string(m,"energyModel")=="photovoltaic" && ProjectJson::string(m,"name")==ProjectJson::string(background,"materialName"))
            throw std::runtime_error("Photovoltaic panels must be scene objects, not background ground");
    const std::filesystem::path defined = project.runtimeDefinedDirectory();
    std::ostringstream xml;
    xml << "<HiStreamProject><Control>"
        << "<outDir>" << xmlEscaped(project.outputDirectory().string()) << "</outDir>"
        << "<GPU>" << ProjectJson::integer(control, "gpu", 0) << "</GPU>"
        << "<rayTracingDepth>" << ProjectJson::integer(control, "depth", 4) << "</rayTracingDepth>"
        << "<periodicNeighborCount>"
        << std::clamp(ProjectJson::integer(
               control, "periodicTraversalCount",
               ProjectJson::boolean(control, "periodicBoundary", false)
                   ? ProjectJson::integer(control, "periodicNeighborCount", 8) : 0),
               0, 20)
        << "</periodicNeighborCount>"
        << "<couplingIterations>" << ProjectJson::integer(control, "couplingIterations", 20) << "</couplingIterations>"
        << "<temperatureTolerance>" << ProjectJson::number(control, "temperatureTolerance", 0.05f) << "</temperatureTolerance>"
        << "<temperatureRelaxation>" << ProjectJson::number(control, "temperatureRelaxation", 0.5f) << "</temperatureRelaxation>"
        << "<spectralAccelerationWidth>" << ProjectJson::number(control, "spectralAccelerationWidth", 100.0f) << "</spectralAccelerationWidth>"
        << "<radiationSolver>" << xmlEscaped(ProjectJson::string(control, "radiationSolver", "traditional")) << "</radiationSolver>"
        << "<vegetationTemperatureMethod>" << ProjectJson::integer(control, "vegetationTemperatureMethod", 0) << "</vegetationTemperatureMethod>"
        << "<isDEM>" << (ProjectJson::boolean(scene, "terrain", false) ? 1 : 0) << "</isDEM>"
        << "<isProcess>" << (ProjectJson::boolean(sensor, "process", false) ? 1 : 0) << "</isProcess>"
        << "<isRadiationProcess>" << (ProjectJson::boolean(sensor, "radiationProcess", false) ? 1 : 0) << "</isRadiationProcess>"
        << "<isEnergyProcess>" << (ProjectJson::boolean(sensor, "energyProcess", false) ? 1 : 0) << "</isEnergyProcess>"
        << "</Control><Geometry><Light><light name=\"Solar\">"
        << "<lightAngle><viewAngles>" << ProjectJson::number(light, "zenith", 30.0f)
        << ',' << ProjectJson::number(light, "azimuth", 135.0f) << "</viewAngles></lightAngle>"
        << "<skyTemperature>" << ProjectJson::number(light, "skyTemperature", 250.0f) << "</skyTemperature>"
        << "<directScatteringRatio>" << ProjectJson::number(light, "direct", 0.8f) << "</directScatteringRatio>"
        << "<esunFileName>" << xmlEscaped((defined / "Esun_.dat").string()) << "</esunFileName>"
        << "<eskyFileName>" << xmlEscaped((defined / "Esky_.dat").string()) << "</eskyFileName>"
        << "</light></Light><Sensor><sensor name=\"MainSensor\">"
        << "<controlBand>" << joined(project.sensorBands()) << "</controlBand>"
        << "</sensor></Sensor></Geometry><Scene>"
        << "<sceneSizeX>" << ProjectJson::number(scene, "x", 60.0f) << "</sceneSizeX>"
        << "<sceneSizeY>" << ProjectJson::number(scene, "y", 60.0f) << "</sceneSizeY>"
        << "<Height>" << ProjectJson::number(scene, "height", 15.0f) << "</Height>"
        << "<voxelSize>" << ProjectJson::number(scene, "voxel", 1.0f) << "</voxelSize>"
        << "<bgSpectral>" << xmlEscaped(ProjectJson::string(background, "spectralName", "soil")) << "</bgSpectral>"
        << "<bgThermal>" << xmlEscaped(ProjectJson::string(background, "thermalName", "soil_temperature")) << "</bgThermal>"
        << "<bgBioName>" << xmlEscaped(ProjectJson::string(background, "materialName", "soilset")) << "</bgBioName>"
        << "<backgroundHeterogeneity>" << (ProjectJson::boolean(background, "heterogeneityEnabled", false) ? "Hapke" : "Lambert") << "</backgroundHeterogeneity>"
        << "<backgroundAngularStrength>" << ProjectJson::number(background, "angularEffectStrength", 0.5f) << "</backgroundAngularStrength>";
    if (ProjectJson::boolean(scene, "terrain", false) && !ProjectJson::string(scene, "demFile").empty())
        xml << "<DEM>" << xmlEscaped(project.resolve(ProjectJson::string(scene, "demFile")).string()) << "</DEM>";
    xml << "<Object>";
    for (const auto& object : project.objects()) {
        const std::string type = ProjectJson::string(object, "type", "Vegetation");
        if (type == "Fire" || type == "Fog" || object.contains("medium")) continue;
        const std::string name = ProjectJson::string(object, "name", "object");
        const std::string spectral = ProjectJson::string(object, "spectralName", "soil");
        const std::string thermal = ProjectJson::string(object, "thermalName", "soil_temperature");
        const std::string canopy = ProjectJson::string(object, "canopyName",
            type == "Vegetation" ? "canopy_default" : "rigid_body");
        const std::string material = ProjectJson::string(object, "materialName",
            type == "Vegetation" ? "leaf_c3" : type == "Water" ? "water_set" : "soilset");
        auto dimensions = ProjectJson::numbers(object.contains("dimensions") ? object["dimensions"] : ProjectJson::Json::array());
        while (dimensions.size() < 3) dimensions.push_back(1.0f);
        xml << "<object objName=\"" << xmlEscaped(name) << "\">"
            << "<meshNames>" << xmlEscaped(joinedNames(object, "name", name)) << "</meshNames>"
            << "<spectralNames>" << xmlEscaped(joinedNames(object, "spectralName", spectral)) << "</spectralNames>"
            << "<thermalNames>" << xmlEscaped(joinedNames(object, "thermalName", thermal)) << "</thermalNames>"
            << "<canopyNames>" << xmlEscaped(joinedNames(object, "canopyName", canopy)) << "</canopyNames>"
            << "<bioNames>" << xmlEscaped(joinedNames(object, "materialName", material)) << "</bioNames>"
            << "<propNames>" << xmlEscaped(joinedNames(object, "materialName", material)) << "</propNames>"
            << "<shapes>" << dimensions[0] << ',' << dimensions[1] << ',' << dimensions[2] << "</shapes>";
        const std::string model = ProjectJson::string(object, "fileName");
        const std::string positions = ProjectJson::string(object, "positionFile");
        if (!model.empty()) xml << "<fileName>" << xmlEscaped(project.resolve(model).string()) << "</fileName>";
        if (!positions.empty()) xml << "<objectPosition>" << xmlEscaped(project.resolve(positions).string()) << "</objectPosition>";
        xml << "</object>";
    }
    xml << "</Object></Scene><Attribute><Optical>";
    for (const auto& item : project.spectra()) {
        const auto params = item.contains("params") ? item["params"] : ProjectJson::Json::object();
        const auto physicalTexture = item.contains("physicalTexture")
            ? item["physicalTexture"] : ProjectJson::Json::object();
        xml << "<spectral name=\"" << xmlEscaped(ProjectJson::string(item, "name", "spectral"))
            << "\" type=\"" << xmlEscaped(ProjectJson::string(item, "model", "custom")) << "\">"
            << "<reflectance>" << xmlEscaped(ProjectJson::string(item, "reflectance", "0.2")) << "</reflectance>"
            << "<transmittance>" << xmlEscaped(ProjectJson::string(item, "transmittance", "0")) << "</transmittance>"
            << "<ref_TIR>" << ProjectJson::number(item, "refTir", 0.05f) << "</ref_TIR>"
            << "<tau_TIR>" << ProjectJson::number(item, "tauTir", 0.0f) << "</tau_TIR>"
            << "<Cab>" << ProjectJson::number(params, "Cab", 40) << "</Cab><Cw>" << ProjectJson::number(params, "Cw", .01f)
            << "</Cw><Cdm>" << ProjectJson::number(params, "Cdm", .01f) << "</Cdm><Cs>" << ProjectJson::number(params, "Cs", 0)
            << "</Cs><N>" << ProjectJson::number(params, "N", 1.5f) << "</N><SMC>" << ProjectJson::number(params, "SMC", 25)
            << "</SMC><BSMBrightness>" << ProjectJson::number(params, "BSMBrightness", .5f) << "</BSMBrightness><BSMlat>"
            << ProjectJson::number(params, "BSMlat", 25) << "</BSMlat><BSMlon>" << ProjectJson::number(params, "BSMlon", 45) << "</BSMlon>"
            << "<physicalTextureEnabled>" << (ProjectJson::boolean(physicalTexture, "enabled", false) ? 1 : 0) << "</physicalTextureEnabled>"
            << "<physicalTextureStrength>" << ProjectJson::number(physicalTexture, "strength", 0.2f) << "</physicalTextureStrength>"
            << "<physicalTextureRepeatSize>" << ProjectJson::number(physicalTexture, "repeatSize", 2.0f) << "</physicalTextureRepeatSize>";
        if (!ProjectJson::string(physicalTexture, "fileName").empty())
            xml << "<physicalTextureFile>" << xmlEscaped(project.resolve(ProjectJson::string(physicalTexture, "fileName")).string()) << "</physicalTextureFile>";
        if (!ProjectJson::string(item, "fileName").empty())
            xml << "<spectral_file>" << xmlEscaped(project.resolve(ProjectJson::string(item, "fileName")).string()) << "</spectral_file>";
        xml << "</spectral>";
    }
    xml << "</Optical><Thermal>";
    for (const auto& item : project.thermals())
        xml << "<thermal name=\"" << xmlEscaped(ProjectJson::string(item, "name", "temperature")) << "\"><sunlitTemperature>"
            << ProjectJson::number(item, "sunlitTemperature", 305) << "</sunlitTemperature><shadedTemperature>"
            << ProjectJson::number(item, "shadedTemperature", 295) << "</shadedTemperature></thermal>";
    xml << "</Thermal><Biochemistry>";
    for (const auto& item : project.materials()) {
        if (ProjectJson::string(item, "type", "Soil") != "Soil") continue;
        const auto params = item.contains("params") ? item["params"] : ProjectJson::Json::object();
        xml << "<property><soilSet name=\"" << xmlEscaped(ProjectJson::string(item, "name", "soilset")) << "\">"
            << "<method>" << ProjectJson::integer(control, "soilTemperatureMethod", ProjectJson::integer(params, "method", 1)) << "</method>"
            << "<rss>" << ProjectJson::number(params, "rss", 2000) << "</rss><cs>" << ProjectJson::number(params, "cs", 1180)
            << "</cs><rhos>" << ProjectJson::number(params, "rhos", 1800) << "</rhos><lambdas>"
            << ProjectJson::number(params, "lambdas", 1.55f) << "</lambdas><SMC>" << ProjectJson::number(params, "SMC", 25)
            << "</SMC></soilSet></property>";
    }
    for (const auto& item : project.materials()) {
        const auto p=item.contains("params")?item["params"]:ProjectJson::Json::object();
        const bool pv=ProjectJson::string(item,"energyModel")=="photovoltaic";
        xml << "<surfaceEnergy name=\"" << xmlEscaped(ProjectJson::string(item,"name")) << "\">"
            << "<photovoltaic>" << (pv?1:0) << "</photovoltaic>"
            << "<vegetation>" << (ProjectJson::string(item,"type")=="Vegetation" && ProjectJson::string(item,"energyModel")!="wood"?1:0) << "</vegetation>"
            << "<eta25>" << ProjectJson::number(p,"eta25",.22f) << "</eta25>"
            << "<gamma>" << ProjectJson::number(p,"gamma",-.0035f) << "</gamma>"
            << "<bifaciality>" << ProjectJson::number(p,"bifaciality",0) << "</bifaciality>"
            << "<heatCapacityPerArea>" << ProjectJson::number(p,"heatCapacityPerArea",pv?12000:60000) << "</heatCapacityPerArea>"
            << "<convectiveScale>" << ProjectJson::number(p,"convectiveScale",1) << "</convectiveScale></surfaceEnergy>";
    }
    xml << "</Biochemistry></Attribute><Meteorology>"
        << "<filePath>" << xmlEscaped(project.meteorologyPath().string()) << "</filePath>"
        << "<startTimeNode>" << ProjectJson::integer(meteo, "start", 0) << "</startTimeNode>"
        << "<endTimeNode>" << ProjectJson::integer(meteo, "end", 1) << "</endTimeNode>"
        << "<dTime>" << ProjectJson::number(meteo, "dTime", 1800) << "</dTime>"
        << "<Latitude>" << ProjectJson::number(meteo, "latitude", 40) << "</Latitude>"
        << "<Longitude>" << ProjectJson::number(meteo, "longitude", 116) << "</Longitude>"
        << "</Meteorology></HiStreamProject>";
    return xml.str();
}

std::string readTextFile(const std::string& path)
{
    std::filesystem::path inputPath(path);
    std::string extension = inputPath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if (extension == ".json") return facetInputView(path);
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

std::vector<std::string> xmlObjectBlocks(const std::string& xml);

std::vector<float> numberList(const std::string& text)
{
    std::vector<float> values;
    std::string normalizedText = text;
    std::replace(normalizedText.begin(), normalizedText.end(), ';', ',');
    std::istringstream input(normalizedText);
    std::string token;
    while (std::getline(input, token, ',')) {
        std::istringstream words(token);
        float value = 0.0f;
        while (words >> value) {
            if (std::isfinite(value)) {
                values.push_back(value);
            }
        }
    }
    return values;
}

std::string firstListName(const std::string& text)
{
    const size_t separator = text.find_first_of(",;");
    return trim(text.substr(0, separator));
}

std::vector<std::string> nameList(const std::string& text)
{
    std::vector<std::string> values;
    std::string normalizedText = text;
    std::replace(normalizedText.begin(), normalizedText.end(), ';', ',');
    std::istringstream input(normalizedText);
    std::string value;
    while (std::getline(input, value, ',')) {
        value = trim(value);
        if (!value.empty()) {
            values.push_back(value);
        }
    }
    return values;
}

bool equalIgnoringCase(const std::string& left, const std::string& right)
{
    return left.size() == right.size() &&
        std::equal(left.begin(), left.end(), right.begin(),
                   [](unsigned char a, unsigned char b) {
                       return std::tolower(a) == std::tolower(b);
                   });
}

std::string xmlAttribute(const std::string& openingTag, const char* attribute)
{
    const std::string prefix = std::string(attribute) + '=';
    size_t position = openingTag.find(prefix);
    while (position != std::string::npos) {
        if (position == 0U ||
            std::isspace(static_cast<unsigned char>(openingTag[position - 1U]))) {
            size_t valueStart = position + prefix.size();
            if (valueStart < openingTag.size() &&
                (openingTag[valueStart] == '"' || openingTag[valueStart] == '\'')) {
                const char quote = openingTag[valueStart++];
                const size_t valueEnd = openingTag.find(quote, valueStart);
                if (valueEnd != std::string::npos) {
                    return decodeXmlText(
                        openingTag.substr(valueStart, valueEnd - valueStart));
                }
            }
        }
        position = openingTag.find(prefix, position + prefix.size());
    }
    return {};
}

std::string namedXmlBlock(const std::string& xml,
                          const char* element,
                          const std::string& name)
{
    const std::string open = "<" + std::string(element);
    const std::string close = "</" + std::string(element) + ">";
    size_t position = 0;
    while ((position = xml.find(open, position)) != std::string::npos) {
        const size_t suffix = position + open.size();
        if (suffix < xml.size() &&
            !(xml[suffix] == '>' ||
              std::isspace(static_cast<unsigned char>(xml[suffix])))) {
            position = suffix;
            continue;
        }
        const size_t openingEnd = xml.find('>', suffix);
        if (openingEnd == std::string::npos) {
            return {};
        }
        const std::string openingTag =
            xml.substr(position, openingEnd - position + 1U);
        const size_t end = xml.find(close, openingEnd + 1U);
        if (end == std::string::npos) {
            return {};
        }
        if (name.empty() || xmlAttribute(openingTag, "name") == name) {
            return xml.substr(position, end + close.size() - position);
        }
        position = end + close.size();
    }
    return {};
}

std::filesystem::path resolveProjectAsset(const std::filesystem::path& inputPath,
                                          const std::string& value);

OptCoeff loadOpticalCoefficients(const std::string& xml,
                                 const std::filesystem::path& inputPath)
{
    OptCoeff coefficients;
    const std::string esunFile = trim(firstXmlTag(xml, "esunFileName"));
    if (esunFile.empty()) return coefficients;
    const std::filesystem::path coefficientFile =
        resolveProjectAsset(inputPath, esunFile).parent_path() / "optipar.txt";
    std::ifstream input(coefficientFile);
    if (!input) return coefficients;
    std::array<std::vector<float>*, 18> columns{
        &coefficients.wl_, &coefficients.nr_, &coefficients.kab_,
        &coefficients.kca_, &coefficients.ks_, &coefficients.kw_,
        &coefficients.kdm_, &coefficients.phiI_, &coefficients.phiII_,
        &coefficients.kcaV_, &coefficients.kcaZ_, &coefficients.kcant_,
        &coefficients.kcaV2_, &coefficients.phi_, &coefficients.gsv1_,
        &coefficients.gsv2_, &coefficients.gsv3_, &coefficients.nw_
    };
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        std::array<float, 18> values{};
        bool valid = true;
        for (float& value : values) valid = valid && static_cast<bool>(stream >> value);
        if (!valid) continue;
        for (std::size_t index = 0; index < columns.size(); ++index)
            columns[index]->push_back(values[index]);
    }
    return coefficients;
}

void loadSurfaceParameters(const std::string& xml,
                           const std::string& spectralName,
                           const std::string& thermalName,
                           const std::vector<float>& wavelengths,
                           const OptCoeff& coefficients,
                           const std::filesystem::path& inputPath,
                           SurfaceParameters& surface)
{
    const std::string spectral =
        namedXmlBlock(xml, "spectral", spectralName);
    if (!spectral.empty()) {
        const size_t openingEnd = spectral.find('>');
        const std::string openingTag = openingEnd == std::string::npos
            ? std::string{} : spectral.substr(0, openingEnd + 1U);
        const std::string type = xmlAttribute(openingTag, "type");
        SpectralXml material{};
        material.spectralName = spectralName;
        material.type = equalIgnoringCase(type, "Prospect") ? spectralType::PROSPECT
            : equalIgnoringCase(type, "BSM") ? spectralType::BSM
            : equalIgnoringCase(type, "file") ? spectralType::OTHER
            : spectralType::CUSTOM;
        material.reflectances = numberList(firstXmlTag(spectral, "reflectance"));
        material.transmittance = numberList(firstXmlTag(spectral, "transmittance"));
        material.refl_tir = xmlFloat(spectral, "ref_TIR", surface.tirReflectance);
        material.tau_tir = xmlFloat(spectral, "tau_TIR", surface.tirTransmittance);
        material.fp = {
            xmlFloat(spectral, "Cab", 40.0f), xmlFloat(spectral, "Cw", 0.01f),
            xmlFloat(spectral, "Cdm", 0.01f), xmlFloat(spectral, "Cs", 0.0f),
            xmlFloat(spectral, "N", 1.5f)
        };
        material.bsm = {
            xmlFloat(spectral, "SMC", 25.0f),
            xmlFloat(spectral, "BSMBrightness", 0.5f),
            xmlFloat(spectral, "BSMlat", 25.0f),
            xmlFloat(spectral, "BSMlon", 45.0f)
        };
        if (material.type == spectralType::OTHER) {
            const std::string fileName = trim(firstXmlTag(spectral, "spectral_file"));
            material.path = resolveProjectAsset(inputPath, fileName).string();
        }
        Compo compo;
        const std::vector<Spectral> resolved = compo.resolveSpectrals(
            material, wavelengths, wavelengths, coefficients);
        surface.reflectance.clear();
        surface.transmittance.clear();
        for (const Spectral& value : resolved) {
            surface.reflectance.push_back(value.reflectance);
            surface.transmittance.push_back(value.transmittance);
        }
        std::vector<float> energyWavelengths;
        energyWavelengths.reserve(N1);
        for (int index = 0; index < N1; ++index)
            energyWavelengths.push_back(400.0f + static_cast<float>(index));
        const std::vector<Spectral> energyResolved = compo.resolveSpectrals(
            material, energyWavelengths, wavelengths, coefficients);
        surface.energyReflectance.clear();
        surface.energyTransmittance.clear();
        for (const Spectral& value : energyResolved) {
            surface.energyReflectance.push_back(value.reflectance);
            surface.energyTransmittance.push_back(value.transmittance);
        }
        surface.tirReflectance =
            xmlFloat(spectral, "ref_TIR", surface.tirReflectance);
        surface.tirTransmittance =
            xmlFloat(spectral, "tau_TIR", surface.tirTransmittance);

        surface.reflectanceTexture.reset();
        surface.reflectanceTextureStrength = 0.0f;
        surface.reflectanceTextureRepeatSize = 1.0f;
        const std::string textureEnabled = trim(
            firstXmlTag(spectral, "physicalTextureEnabled"));
        const bool useTexture = textureEnabled == "1" ||
            equalIgnoringCase(textureEnabled, "true") ||
            equalIgnoringCase(textureEnabled, "yes") ||
            equalIgnoringCase(textureEnabled, "on");
        if (useTexture) {
            const std::string textureFile = trim(
                firstXmlTag(spectral, "physicalTextureFile"));
            if (textureFile.empty()) {
                throw std::runtime_error(
                    "Physical texture is enabled but has no file: " + spectralName);
            }
            surface.reflectanceTextureStrength = std::clamp(
                xmlFloat(spectral, "physicalTextureStrength", 0.2f),
                0.0f, 1.0f);
            surface.reflectanceTextureRepeatSize = std::max(
                0.01f, xmlFloat(
                    spectral, "physicalTextureRepeatSize", 2.0f));
            surface.reflectanceTexture = loadReflectanceVarianceTexture(
                resolveProjectAsset(inputPath, textureFile));
        }
    }

    const std::string thermal =
        namedXmlBlock(xml, "thermal", thermalName);
    if (!thermal.empty()) {
        surface.sunlitTemperature =
            xmlFloat(thermal, "sunlitTemperature",
                     surface.sunlitTemperature);
        surface.shadedTemperature =
            xmlFloat(thermal, "shadedTemperature",
                     surface.shadedTemperature);
    }
}


void loadSoilParameters(const std::string& xml,
                        const std::string& soilName,
                        SurfaceParameters& surface)
{
    const std::string soil = namedXmlBlock(xml, "soilSet", soilName);
    if (soil.empty()) {
        return;
    }
    surface.soilTemperatureMethod = std::clamp(
        static_cast<int>(std::lround(xmlFloat(soil, "method", 1.0f))), 0, 2);
    surface.surfaceResistance = std::max(
        0.0f, xmlFloat(soil, "rss", surface.surfaceResistance));
    surface.specificHeat = std::max(
        1.0f, xmlFloat(soil, "cs", surface.specificHeat));
    surface.density = std::max(
        1.0f, xmlFloat(soil, "rhos", surface.density));
    surface.thermalConductivity = std::max(
        0.001f, xmlFloat(
            soil, "lambdas", surface.thermalConductivity));
    const float moisture =
        xmlFloat(soil, "SMC", surface.soilMoisture);
    surface.soilMoisture = std::clamp(
        moisture > 1.0f ? moisture * 0.01f : moisture, 0.0f, 1.0f);
}

FacetRTParameters facetRTParametersFromInput(const std::string& xml,
                                             const std::filesystem::path& inputPath)
{
    FacetRTParameters parameters;
    parameters.wavelengths = numberList(firstXmlTag(xml, "controlBand"));
    parameters.wavelengths.erase(
        std::remove_if(parameters.wavelengths.begin(),
                       parameters.wavelengths.end(),
                       [](float wavelength) {
                           return !std::isfinite(wavelength) ||
                                  wavelength <= 0.0f;
                       }),
        parameters.wavelengths.end());
    if (parameters.wavelengths.empty()) {
        parameters.wavelengths.push_back(550.0f);
    }
    parameters.skyTemperature =
        std::clamp(xmlFloat(xml, "skyTemperature", 250.0f),
                   100.0f, 400.0f);
    parameters.acceleratedSolver =
        trim(firstXmlTag(xml, "radiationSolver")) == "accelerated";
    const OptCoeff coefficients = loadOpticalCoefficients(xml, inputPath);

    const std::vector<std::string> objects = xmlObjectBlocks(xml);
    if (!objects.empty()) {
        loadSurfaceParameters(
            xml,
            firstListName(firstXmlTag(objects.front(), "spectralNames")),
            firstListName(firstXmlTag(objects.front(), "thermalNames")),
            parameters.wavelengths, coefficients, inputPath,
            parameters.object);
    }
    loadSurfaceParameters(
        xml,
        trim(firstXmlTag(xml, "bgSpectral")),
        trim(firstXmlTag(xml, "bgThermal")),
        parameters.wavelengths, coefficients, inputPath,
        parameters.ground);
    loadSoilParameters(
        xml, trim(firstXmlTag(xml, "bgBioName")), parameters.ground);
    return parameters;
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

struct ShortwaveSpectrum {
    std::vector<float> direct;
    std::vector<float> diffuse;

    float sample(const std::vector<float>& values, float wavelength) const
    {
        if (values.empty()) return 0.0f;
        const float wavelengthNanometers = wavelength <= 2.5f
            ? wavelength * 1000.0f : wavelength;
        const int index = std::clamp(
            static_cast<int>(std::lround(wavelengthNanometers - 400.0f)),
            0, static_cast<int>(values.size()) - 1);
        return values[static_cast<size_t>(index)];
    }

    float directAt(float wavelength) const { return sample(direct, wavelength); }
    float diffuseAt(float wavelength) const { return sample(diffuse, wavelength); }

    size_t sampleCount() const
    {
        return std::min(direct.size(), diffuse.size());
    }

    float integrate(const std::vector<float>& values,
                    size_t firstSample,
                    size_t lastSample) const
    {
        if (values.size() < 2U || firstSample >= values.size() - 1U) {
            return 0.0f;
        }
        lastSample = std::min(lastSample, values.size() - 1U);
        double integral = 0.0;
        for (size_t sample = firstSample; sample < lastSample; ++sample) {
            integral += 0.5 * static_cast<double>(
                values[sample] + values[sample + 1U]);
        }
        // Source spectra use 1 nm samples and are normalized per micrometre.
        return static_cast<float>(integral * 0.001);
    }
};

std::vector<float> readScalarSpectrum(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::vector<float> values;
    float value = 0.0f;
    while (input >> value) {
        values.push_back(std::isfinite(value) ? std::max(0.0f, value) : 0.0f);
    }
    if (values.size() > static_cast<size_t>(N1)) values.resize(N1);
    return values;
}

ShortwaveSpectrum loadShortwaveSpectrum(
    const std::string& xml,
    const std::filesystem::path& inputPath,
    float directFraction)
{
    ShortwaveSpectrum spectrum;
    const std::string directName = trim(firstXmlTag(xml, "esunFileName"));
    const std::string diffuseName = trim(firstXmlTag(xml, "eskyFileName"));
    if (!directName.empty() && !diffuseName.empty()) {
        spectrum.direct = readScalarSpectrum(
            resolveProjectAsset(inputPath, directName));
        spectrum.diffuse = readScalarSpectrum(
            resolveProjectAsset(inputPath, diffuseName));
    }
    const size_t count = std::min(spectrum.direct.size(), spectrum.diffuse.size());
    if (count < 2U) {
        // Flat per-micrometre fallback whose 400-2400 nm integral is one.
        spectrum.direct.assign(N1, 0.5f * directFraction);
        spectrum.diffuse.assign(N1, 0.5f * (1.0f - directFraction));
        return spectrum;
    }
    spectrum.direct.resize(count);
    spectrum.diffuse.resize(count);
    double integral = 0.0;
    for (size_t index = 0; index + 1U < count; ++index) {
        integral += 0.5 * (
            spectrum.direct[index] + spectrum.direct[index + 1U] +
            spectrum.diffuse[index] + spectrum.diffuse[index + 1U]);
    }
    // Samples are spaced by 1 nm; convert the integral to micrometres.
    integral *= 0.001;
    if (!(integral > 0.0) || !std::isfinite(integral)) {
        spectrum.direct.assign(N1, 0.5f * directFraction);
        spectrum.diffuse.assign(N1, 0.5f * (1.0f - directFraction));
        return spectrum;
    }
    const float inverseIntegral = static_cast<float>(1.0 / integral);
    for (float& value : spectrum.direct) value *= inverseIntegral;
    for (float& value : spectrum.diffuse) value *= inverseIntegral;
    return spectrum;
}

struct ObjGeometry {
    std::vector<Vec3> positions;
    std::vector<std::array<int, 3>> triangles;
    std::vector<std::string> triangleGroups;
};

ObjGeometry readObjGeometry(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open scene OBJ: " + path.string());
    }
    ObjGeometry geometry;
    std::string activeGroup;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        std::string record;
        stream >> record;
        if (record == "g" || record == "o") {
            std::string group;
            if (stream >> group) {
                activeGroup = group;
            }
        } else if (record == "usemtl" && activeGroup.empty()) {
            stream >> activeGroup;
        } else if (record == "v") {
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
                geometry.triangleGroups.push_back(activeGroup);
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

struct DemHeightField {
    int width{};
    int height{};
    float minimum{};
    std::vector<float> values;

    bool valid() const
    {
        return width >= 2 && height >= 2 &&
               values.size() == static_cast<size_t>(width) * height;
    }

    float heightAboveMinimum(float northX, float eastZ,
                             float sceneWidth, float sceneDepth) const
    {
        if (!valid()) return 0.0f;
        // GeoTIFF columns grow eastward; rows grow southward.
        const float u = std::clamp(eastZ / sceneDepth, 0.0f, 1.0f) * (width - 1);
        const float v = (1.0f - std::clamp(northX / sceneWidth, 0.0f, 1.0f)) * (height - 1);
        const int x0 = static_cast<int>(std::floor(u));
        const int y0 = static_cast<int>(std::floor(v));
        const int x1 = std::min(width - 1, x0 + 1);
        const int y1 = std::min(height - 1, y0 + 1);
        const auto at = [this](int column, int row) {
            return values[static_cast<size_t>(row) * width + column];
        };
        const float dx = u - x0;
        const float dy = v - y0;
        const float top = at(x0, y0) * (1.0f - dx) + at(x1, y0) * dx;
        const float bottom = at(x0, y1) * (1.0f - dx) + at(x1, y1) * dx;
        return std::max(0.0f, top * (1.0f - dy) + bottom * dy - minimum);
    }
};

DemHeightField readDemHeightField(const std::string& xml,
                                  const std::filesystem::path& inputPath)
{
    DemHeightField dem;
    const std::string enabled = trim(firstXmlTag(xml, "isDEM"));
    if (!enabled.empty() && enabled != "1") return dem;
    const std::string fileName = trim(firstXmlTag(xml, "DEM"));
    if (fileName.empty()) return dem;
    const std::filesystem::path path = resolveProjectAsset(inputPath, fileName);
    GDALAllRegister();
    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpen(path.string().c_str(), GA_ReadOnly));
    if (!dataset || dataset->GetRasterCount() < 1) {
        if (dataset) GDALClose(dataset);
        throw std::runtime_error("Cannot open DEM: " + path.string());
    }
    dem.width = dataset->GetRasterXSize();
    dem.height = dataset->GetRasterYSize();
    if (dem.width < 2 || dem.height < 2) {
        GDALClose(dataset);
        throw std::runtime_error("DEM must contain at least 2 x 2 cells: " + path.string());
    }
    dem.values.resize(static_cast<size_t>(dem.width) * dem.height);
    GDALRasterBand* band = dataset->GetRasterBand(1);
    int hasNoData = 0;
    const float noData = static_cast<float>(band->GetNoDataValue(&hasNoData));
    const CPLErr result = band->RasterIO(
        GF_Read, 0, 0, dem.width, dem.height, dem.values.data(),
        dem.width, dem.height, GDT_Float32, 0, 0);
    GDALClose(dataset);
    if (result != CE_None) {
        throw std::runtime_error("Cannot read DEM elevations: " + path.string());
    }
    dem.minimum = std::numeric_limits<float>::infinity();
    for (const float value : dem.values) {
        if (!std::isfinite(value) || (hasNoData && value == noData)) continue;
        dem.minimum = std::min(dem.minimum, value);
    }
    if (!std::isfinite(dem.minimum)) {
        throw std::runtime_error("DEM contains no valid elevations: " + path.string());
    }
    for (float& value : dem.values) {
        if (!std::isfinite(value) || (hasNoData && value == noData)) value = dem.minimum;
    }
    return dem;
}

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
    const float radians = -placement.rotation * kPi / 180.0f;
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

struct ObjectMaterialBinding {
    std::string meshName;
    uint32_t materialIndex{};
};

void appendGeometry(Scene& scene,
                    const ObjGeometry& geometry,
                    const Placement& placement,
                    float sceneWidth,
                    float sceneDepth,
                    float sceneHeight,
                    const std::vector<ObjectMaterialBinding>& bindings)
{
    const Vec3 domainMinimum{-sceneWidth * 0.5f, 0.0f,
                             -sceneDepth * 0.5f};
    const Vec3 domainMaximum{sceneWidth * 0.5f, sceneHeight,
                             sceneDepth * 0.5f};
    for (size_t triangleIndex = 0;
         triangleIndex < geometry.triangles.size(); ++triangleIndex) {
        const auto& triangle = geometry.triangles[triangleIndex];
        uint32_t materialIndex =
            bindings.empty() ? 0U : bindings.front().materialIndex;
        const std::string group =
            triangleIndex < geometry.triangleGroups.size()
            ? geometry.triangleGroups[triangleIndex] : std::string{};
        for (const ObjectMaterialBinding& binding : bindings) {
            if (!binding.meshName.empty() &&
                equalIgnoringCase(binding.meshName, group)) {
                materialIndex = binding.materialIndex;
                break;
            }
        }
        appendTriangleInsideDomain(
            scene,
            transformPosition(geometry.positions[triangle[0]], placement,
                              sceneWidth, sceneDepth),
            transformPosition(geometry.positions[triangle[1]], placement,
                              sceneWidth, sceneDepth),
            transformPosition(geometry.positions[triangle[2]], placement,
                              sceneWidth, sceneDepth),
            domainMinimum, domainMaximum, materialIndex);
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
    geometry.triangleGroups.assign(geometry.triangles.size(), std::string{});
    return geometry;
}

void appendProjectGround(Scene& scene,
                         float sceneWidth,
                         float sceneDepth,
                         float voxelSize,
                         uint32_t maximumGridResolution,
                         uint32_t materialIndex,
                         const DemHeightField& dem)
{
    const float x0 = -sceneWidth * 0.5f;
    const float z0 = -sceneDepth * 0.5f;
    const uint32_t requestedColumns = std::max(
        1U, static_cast<uint32_t>(std::ceil(sceneWidth / voxelSize)));
    const uint32_t requestedRows = std::max(
        1U, static_cast<uint32_t>(std::ceil(sceneDepth / voxelSize)));
    // The complete DEM is bilinearly resampled to the scene grid.  Its source
    // pixel dimensions therefore do not constrain the configured scene size.
    const uint32_t columns = std::clamp(
        requestedColumns, 1U, maximumGridResolution);
    const uint32_t rows = std::clamp(
        requestedRows, 1U, maximumGridResolution);
    const float cellWidth = sceneWidth / static_cast<float>(columns);
    const float cellDepth = sceneDepth / static_cast<float>(rows);
    scene.vertices.reserve(
        scene.vertices.size() + static_cast<size_t>(columns) * rows * 6U);
    for (uint32_t row = 0; row < rows; ++row) {
        const float za = z0 + cellDepth * static_cast<float>(row);
        const float zb = row + 1U == rows
            ? z0 + sceneDepth
            : z0 + cellDepth * static_cast<float>(row + 1U);
        for (uint32_t column = 0; column < columns; ++column) {
            const float xa = x0 + cellWidth * static_cast<float>(column);
            const float xb = column + 1U == columns
                ? x0 + sceneWidth
                : x0 + cellWidth * static_cast<float>(column + 1U);
            const float sceneXa = xa + sceneWidth * 0.5f;
            const float sceneXb = xb + sceneWidth * 0.5f;
            const float sceneYa = za + sceneDepth * 0.5f;
            const float sceneYb = zb + sceneDepth * 0.5f;
            const float haa = dem.heightAboveMinimum(sceneXa, sceneYa, sceneWidth, sceneDepth) - 0.01f;
            const float hab = dem.heightAboveMinimum(sceneXa, sceneYb, sceneWidth, sceneDepth) - 0.01f;
            const float hba = dem.heightAboveMinimum(sceneXb, sceneYa, sceneWidth, sceneDepth) - 0.01f;
            const float hbb = dem.heightAboveMinimum(sceneXb, sceneYb, sceneWidth, sceneDepth) - 0.01f;
            appendTriangle(scene, {xa, haa, za}, {xb, hbb, zb},
                           {xb, hba, za}, materialIndex);
            appendTriangle(scene, {xa, haa, za}, {xa, hab, zb},
                           {xb, hbb, zb}, materialIndex);
        }
    }
}

Scene loadProjectScene(const std::string& inputFile)
{
    const std::filesystem::path inputPath(inputFile);
    const std::string xml = readTextFile(inputFile);
    const float sceneWidth = std::max(0.01f, xmlFloat(xml, "sceneSizeX", 8.0f));
    const float sceneDepth = std::max(0.01f, xmlFloat(xml, "sceneSizeY", 8.0f));
    const float sceneHeight = std::max(0.01f, xmlFloat(xml, "Height", 4.0f));
    const float voxelSize = std::max(0.01f, xmlFloat(xml, "voxelSize", 1.0f));
    const DemHeightField dem = readDemHeightField(xml, inputPath);
    Scene scene;
    scene.periodicNeighborCount = static_cast<uint32_t>(std::clamp(
        static_cast<int>(std::lround(xmlFloat(
            xml, "periodicNeighborCount", 0.0f))), 0, 20));
    scene.periodicSizeX = sceneWidth;
    scene.periodicSizeZ = sceneDepth;
    if (std::max(sceneWidth, sceneDepth) >= kLargeSceneExtent) {
        scene.visibilityRasterSize = kLargeSceneRasterSize;
        // Preserve the existing A-buffer memory footprint when the raster
        // doubles in each dimension. Large scenes gain four times the spatial
        // sampling without requiring a multi-gigabyte fragment buffer.
        scene.visibilityRasterLayers = kLargeSceneRasterLayers;
    }
    const FacetRTParameters radiativeParameters =
        facetRTParametersFromInput(xml, inputPath);
    scene.backgroundAngularStrength = std::clamp(
        xmlFloat(xml, "backgroundAngularStrength", 0.0f), 0.0f, 1.0f);
    const std::string backgroundModel = trim(
        firstXmlTag(xml, "backgroundHeterogeneity"));
    if (!backgroundModel.empty() && backgroundModel != "Hapke" &&
        backgroundModel != "hapke") {
        scene.backgroundAngularStrength = 0.0f;
    }
    const OptCoeff opticalCoefficients = loadOpticalCoefficients(xml, inputPath);

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
        const std::vector<std::string> meshNames =
            nameList(firstXmlTag(objectXml, "meshNames"));
        const std::vector<std::string> spectralNames =
            nameList(firstXmlTag(objectXml, "spectralNames"));
        const std::vector<std::string> thermalNames =
            nameList(firstXmlTag(objectXml, "thermalNames"));
        const auto physicalNames=nameList(firstXmlTag(objectXml,"bioNames"));
        const size_t bindingCount = std::max<size_t>(
            1U, std::max(meshNames.size(),
                         std::max(spectralNames.size(), thermalNames.size())));
        std::vector<ObjectMaterialBinding> bindings;
        bindings.reserve(bindingCount);
        for (size_t index = 0; index < bindingCount; ++index) {
            SurfaceParameters material = radiativeParameters.object;
            const std::string spectralName = spectralNames.empty()
                ? std::string{}
                : spectralNames[std::min(index, spectralNames.size() - 1U)];
            const std::string thermalName = thermalNames.empty()
                ? std::string{}
                : thermalNames[std::min(index, thermalNames.size() - 1U)];
            loadSurfaceParameters(xml, spectralName, thermalName,
                                  radiativeParameters.wavelengths,
                                  opticalCoefficients, inputPath, material);
            const auto physicalName=physicalNames.empty()?std::string{}:physicalNames[std::min(index,physicalNames.size()-1U)];
            const auto physical=namedXmlBlock(xml,"surfaceEnergy",physicalName);
            if(!physical.empty()) {
                material.photovoltaic=xmlFloat(physical,"photovoltaic",0)>0.5f;
                material.vegetation=xmlFloat(physical,"vegetation",1)>0.5f && !material.photovoltaic;
                material.pvEta25=xmlFloat(physical,"eta25",.22f);
                material.pvGamma=xmlFloat(physical,"gamma",-.0035f);
                material.pvBifaciality=xmlFloat(physical,"bifaciality",0);
                material.heatCapacityPerArea=xmlFloat(physical,"heatCapacityPerArea",60000);
                material.convectiveScale=xmlFloat(physical,"convectiveScale",1);
                if(material.photovoltaic && (!(material.pvEta25>=0 && material.pvEta25<=.5f) ||
                    !(material.pvGamma>=-.02f && material.pvGamma<=0) || !(material.pvBifaciality>=0 && material.pvBifaciality<=1) ||
                    !(material.heatCapacityPerArea>=0) || !(material.convectiveScale>0)))
                    throw std::runtime_error("Invalid photovoltaic material parameters: "+physicalName);
                if(material.photovoltaic && (material.tirTransmittance>1e-6f ||
                    std::any_of(material.energyTransmittance.begin(),material.energyTransmittance.end(),[](float t){return t>1e-6f;})))
                    throw std::runtime_error("Photovoltaic surface must be opaque: "+physicalName);
            }
            const uint32_t materialIndex =
                static_cast<uint32_t>(scene.materials.size());
            scene.materials.push_back(std::move(material));
            bindings.push_back({
                meshNames.empty()
                    ? std::string{}
                    : meshNames[std::min(index, meshNames.size() - 1U)],
                materialIndex
            });
        }
        std::string modelFile = trim(firstXmlTag(objectXml, "fileName"));
        if (modelFile.empty()) {
            modelFile = trim(firstXmlTag(objectXml, "objectfile"));
        }
        const ObjGeometry geometry = modelFile.empty()
            ? cubeGeometry(objectXml)
            : readObjGeometry(resolveProjectAsset(inputPath, modelFile));
        for (Placement placement :
             readPlacements(inputPath, objectXml, sceneWidth, sceneDepth)) {
            placement.z += dem.heightAboveMinimum(
                placement.x, placement.y, sceneWidth, sceneDepth);
            appendGeometry(scene, geometry, placement, sceneWidth, sceneDepth,
                           sceneHeight, bindings);
        }
    }

    scene.leafFacetCount = scene.facetCount();
    const uint32_t groundMaterialIndex =
        static_cast<uint32_t>(scene.materials.size());
    scene.materials.push_back(radiativeParameters.ground);
    appendProjectGround(scene, sceneWidth, sceneDepth, voxelSize,
                        scene.visibilityRasterSize / 4U,
                        groundMaterialIndex, dem);
    const uint32_t initialRasterLayers = scene.visibilityRasterLayers;
    scene.visibilityRasterLayers = adaptiveVisibilityRasterLayers(scene);
    if (scene.visibilityRasterLayers != initialRasterLayers) {
        std::cout << "facetrt: automatically increased A-buffer layers from "
                  << initialRasterLayers << " to "
                  << scene.visibilityRasterLayers << " for "
                  << scene.facetCount() << " facets" << std::endl;
    }
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
    finalizeReflectanceTextureFactors(scene);
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
        throw std::runtime_error("project.json contains no OBJ fileName and fallback OBJ could not be created");
    }
    const float hx = length * 0.5f;
    const float hz = width * 0.5f;
    output << "# Generated from project.json cube dimensions\n"
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
        std::cerr << "facetrt: project.json has no OBJ; using the first cube shape as a generated scene\n";
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
        throw std::invalid_argument("FacetRT requires project.json");
    }
    const std::string resultFile = outputFileFromInput(inputPath, outputFile, "facetrt.json");
    const std::filesystem::path input(inputPath);
    const Scene scene = input.extension() == ".obj" || input.extension() == ".OBJ"
        ? loadTreeWithSoil(inputPath)
        : loadProjectScene(inputPath);
    const std::string xml =
        input.extension() == ".obj" || input.extension() == ".OBJ"
        ? std::string{} : readTextFile(inputPath);
    const FacetRTParameters parameters =
        xml.empty() ? FacetRTParameters{} : facetRTParametersFromInput(xml, input);
    const Progress progress = [](int value, const std::string& stage) {
        std::cout << "PROGRESS\t" << value << '\t' << stage << std::endl;
    };
    const RunResult result = runGpu(
        scene, shaderDirectory, progress,
        xml.empty() ? nullptr : &parameters);
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
        throw std::invalid_argument("FacetEB requires project.json");
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
    bool saveRadiationProcess = trim(firstXmlTag(xml, "isRadiationProcess")) == "1";
    bool saveEnergyProcess = trim(firstXmlTag(xml, "isEnergyProcess")) == "1";
    if (!saveRadiationProcess && !saveEnergyProcess &&
        trim(firstXmlTag(xml, "isProcess")) == "1") {
        saveEnergyProcess = true;
    }
    const bool saveProcess = saveRadiationProcess || saveEnergyProcess;
    const uint32_t maximumCouplingIterations = static_cast<uint32_t>(std::clamp(
        static_cast<int>(std::lround(xmlFloat(xml, "couplingIterations", 20.0f))),
        1, 100));
    const float temperatureTolerance = std::clamp(
        xmlFloat(xml, "temperatureTolerance", 0.05f), 0.001f, 10.0f);
    const float temperatureRelaxation = std::clamp(
        xmlFloat(xml, "temperatureRelaxation", 0.5f), 0.05f, 1.0f);
    const size_t spectralAccelerationWidth = static_cast<size_t>(std::clamp(
        static_cast<int>(std::lround(
            xmlFloat(xml, "spectralAccelerationWidth", 100.0f))),
        1, 1000));
    const int vegetationTemperatureMethod = std::clamp(
        static_cast<int>(std::lround(xmlFloat(
            xml, "vegetationTemperatureMethod", 0.0f))), 0, 1);
    const float latitude = xmlFloat(xml, "Latitude", 40.0f);
    const float longitude = xmlFloat(xml, "Longitude", 116.0f);
    const float skyTemperature = std::clamp(
        xmlFloat(xml, "skyTemperature", 250.0f), 150.0f, 350.0f);
    const float directShortwaveFraction = std::clamp(
        xmlFloat(xml, "directScatteringRatio", 0.8f), 0.0f, 1.0f);
    const FacetRTParameters radiativeParameters =
        facetRTParametersFromInput(xml, input);
    const ShortwaveSpectrum shortwaveSpectrum = loadShortwaveSpectrum(
        xml, input, directShortwaveFraction);

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
        const double localHours = fractionalDay * 24.0;
        const int hour = static_cast<int>(std::floor(localHours));
        const int minute = static_cast<int>(std::floor(
            (localHours - static_cast<double>(hour)) * 60.0));
        spa_data data{};
        data.year = 2019;
        data.month = month;
        data.day = day;
        // VoxelEB uses the meteorological timestamp at whole-minute precision.
        data.hour = std::clamp(hour, 0, 23);
        data.minute = std::clamp(minute, 0, 59);
        data.second = 0;
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
    config.rasterWidth = scene.visibilityRasterSize;
    config.rasterHeight = scene.visibilityRasterSize;
    config.maxFragmentsPerPixel = scene.visibilityRasterLayers;
    config.periodicNeighborCount = scene.periodicNeighborCount;
    config.periodicSizeX = scene.periodicSizeX;
    config.periodicSizeZ = scene.periodicSizeZ;
    config.gpuIndex = static_cast<uint32_t>(std::max(
        0, static_cast<int>(std::lround(xmlFloat(xml, "GPU", 0.0f)))));

    std::cout << "PROGRESS\t3\t加载面元场景与气象驱动" << std::endl;
    std::cout << "PROGRESS\t5\t温度方法：土壤按材质 0/1/2，植被="
              << (vegetationTemperatureMethod == 0
                      ? "Ball-Berry 经验法" : "Farquhar 机制法")
              << std::endl;
    facetvk::FacetrtVulkan model;
    model.initialize(config, shaderDirectory);
    model.setGeometry(scene.vertices);
    std::cout << "PROGRESS\t10\t构建共享面元可见性图" << std::endl;
    const facetvk::GraphDiagnostics graph =
        model.buildVisibilityGraph(makeHemisphereDirections());

    const size_t surfaceCount = static_cast<size_t>(scene.facetCount()) * 2U;
    std::vector<float> temperature(
        surfaceCount, std::numeric_limits<float>::quiet_NaN());

    RunResult latest;
    latest.backend = "Vulkan GPU Facet RT-EB coupled";
    latest.graph = graph;
    // Only the final state needs these summary arrays; do not retain a second
    // copy of the preceding node throughout the next solve.
    std::vector<float> latestNetRadiation;
    std::vector<float> latestSensibleHeat;
    std::vector<float> latestLatentHeat;
    std::vector<float> latestPhotovoltaicPower;
    std::vector<float> latestStorageHeat;
    uint32_t latestCouplingIterations = 0;
    float latestTemperatureDelta = 0.0f;
    std::string latestTime;

    const std::filesystem::path stepDirectory = outputDirectory / ".facet_steps";
    std::filesystem::create_directories(stepDirectory);
    if (saveProcess) {
        std::filesystem::create_directories(outputDirectory / "process");
    }

    std::ofstream pvSummary(outputDirectory/"photovoltaic_summary.csv");
    pvSummary << "node,time,area_m2,temperature_C,incident_W,absorbed_W,electric_W,net_longwave_W,sensible_W,storage_W,max_residual_W_m2,energy_kWh\n";
    double pvEnergyKwh=0;
    const char* stepSyncEnvironment = std::getenv("STREAMSIM_FACET_STEP_SYNC");
    const bool synchronizeStepOutput = stepSyncEnvironment &&
        std::string(stepSyncEnvironment) == "1";
    if (synchronizeStepOutput) {
        // Geometry is shared by all nodes and is available before the first
        // image/process preview. The final summary replaces this file later.
        std::ofstream geometry(resultFile, std::ios::binary | std::ios::trunc);
        geometry << std::setprecision(9)
                 << "{\n  \"backend\": \"Vulkan GPU Facet RT-EB coupled\",\n"
                 << "  \"facetCount\": " << scene.facetCount() << ",\n"
                 << "  \"leafFacetCount\": " << scene.leafFacetCount << ",\n"
                 << "  \"vertexPositions\": [";
        for (size_t index = 0; index < scene.vertices.size(); ++index) {
            if (index) geometry << ',';
            const auto& vertex = scene.vertices[index];
            geometry << vertex.position[0] << ',' << vertex.position[1]
                     << ',' << vertex.position[2];
        }
        geometry << "]\n}\n";
        geometry.close();
        if (!geometry) throw std::runtime_error("FacetEB cannot write shared geometry");
    }
    const auto totalBegin = Clock::now();
    for (int node = startNode; node < endNode; ++node) {
        const MeteoStep& meteo = meteorology[static_cast<size_t>(node)];
        updateSolarPosition(meteo);
        const std::string token = timeToken(meteo.julianTime);
        {
        // All per-node output/solver scratch arrays die before image export.
        const std::vector<float> sunlit =
            model.computeSunlitFraction(sunDirection(scene));

        if (node == startNode) {
            // FacetEB temperatures are solved states.  XML thermal-material
            // temperatures are inputs only for the RT modes, so use the
            // current air temperature solely as the EB solver's neutral seed.
            const float initialTemperature = std::clamp(
                meteo.airTemperature + 273.15f, 220.0f, 340.0f);
            std::fill(
                temperature.begin(), temperature.end(), initialTemperature);
        }
        const std::vector<float> previousTemperature = temperature;

        const float totalShortwave = std::max(0.0f, meteo.shortwave);
        const float cosineSolarZenith =
            std::max(0.0f, sunDirection(scene).y);
        std::vector<float> absorbedShortwave(surfaceCount, 0.0f);
        std::vector<float> incidentShortwave(surfaceCount,0), photovoltaicPower(surfaceCount,0), photovoltaicSlope(surfaceCount,0);
        facetvk::SolveResult shortwaveSolution;
        shortwaveSolution.radiosity.assign(surfaceCount, 0.0f);
        facetvk::SolveResult diffuseSolution;
        diffuseSolution.radiosity.assign(surfaceCount, 0.0f);

        // FacetEB uses the same shortwave acceleration definition as VoxelEB:
        // integrate direct and diffuse irradiance inside each configured
        // wavelength window, solve once with the window-centre optics, then
        // accumulate all windows into the broadband absorbed shortwave flux.
        const size_t shortwaveSampleCount = shortwaveSpectrum.sampleCount();
        const float directSpectrumIntegral = shortwaveSampleCount >= 2U
            ? shortwaveSpectrum.integrate(
                shortwaveSpectrum.direct, 0U, shortwaveSampleCount - 1U)
            : 0.0f;
        const float directNormalScale =
            cosineSolarZenith > 1.0e-4f && directSpectrumIntegral > 1.0e-8f
            ? std::min(
                1400.0f,
                totalShortwave * directSpectrumIntegral /
                    cosineSolarZenith) / directSpectrumIntegral
            : 0.0f;
        if (totalShortwave > 0.0f && shortwaveSampleCount >= 2U) {
            for (size_t firstSample = 0U;
                 firstSample + 1U < shortwaveSampleCount;
                 firstSample += spectralAccelerationWidth) {
                const size_t lastSample = std::min(
                    firstSample + spectralAccelerationWidth,
                    shortwaveSampleCount - 1U);
                const size_t centreSample =
                    (firstSample + lastSample) / 2U;
                const float directFraction = shortwaveSpectrum.integrate(
                    shortwaveSpectrum.direct, firstSample, lastSample);
                const float diffuseFraction = shortwaveSpectrum.integrate(
                    shortwaveSpectrum.diffuse, firstSample, lastSample);
                if (directFraction + diffuseFraction <= 1.0e-10f) {
                    continue;
                }
                const float windowDirectNormal =
                    directNormalScale * directFraction;
                const float windowDiffuse =
                    totalShortwave * diffuseFraction;
                const std::vector<facetvk::SurfaceOptics> windowOptics =
                    makeEnergyShortwaveOptics(
                        scene, sunlit, windowDirectNormal, centreSample);
                const facetvk::SolveResult windowSolution = model.solve(
                    windowOptics, windowDiffuse, kIterations, 1.0f);
                const std::vector<float> windowIncident =
                    model.incidentIrradiance(
                        windowSolution, windowOptics, windowDiffuse);
                const facetvk::SolveResult windowDiffuseSolution =
                    model.solve(
                        withoutDirectLight(windowOptics), windowDiffuse,
                        kIterations, 1.0f);
                for (size_t side = 0; side < surfaceCount; ++side) {
                    const float absorptance = std::clamp(
                        1.0f - windowOptics[side].reflectance -
                            windowOptics[side].transmittance,
                        0.0f, 1.0f);
                    incidentShortwave[side] += windowIncident[side];
                    absorbedShortwave[side] +=
                        absorptance * windowIncident[side];
                    shortwaveSolution.radiosity[side] +=
                        windowSolution.radiosity[side];
                    diffuseSolution.radiosity[side] +=
                        windowDiffuseSolution.radiosity[side];
                }
                shortwaveSolution.iterations = std::max(
                    shortwaveSolution.iterations,
                    windowSolution.iterations);
                shortwaveSolution.maxDelta = std::max(
                    shortwaveSolution.maxDelta,
                    windowSolution.maxDelta);
                diffuseSolution.iterations = std::max(
                    diffuseSolution.iterations,
                    windowDiffuseSolution.iterations);
                diffuseSolution.maxDelta = std::max(
                    diffuseSolution.maxDelta,
                    windowDiffuseSolution.maxDelta);
            }
        }

        const float skyLongwave = meteo.longwave > 0.0f
            ? meteo.longwave
            : kStefanBoltzmann * std::pow(skyTemperature, 4.0f);
        const float airTemperature =
            std::clamp(meteo.airTemperature + 273.15f, 220.0f, 340.0f);
        const float aerodynamicConductance =
            5.8f + 4.1f * std::sqrt(std::max(0.1f, meteo.windSpeed));
        const float airPressure = std::max(100.0f, meteo.pressure);
        const float airDensity = airPressure * 100.0f /
            (287.05f * airTemperature);
        const float aerodynamicResistance = airDensity * 1005.0f /
            std::max(0.1f, aerodynamicConductance);

        std::vector<float> netRadiation(surfaceCount);
        std::vector<float> sensibleHeat(surfaceCount);
        std::vector<float> latentHeat(surfaceCount);
        std::vector<float> latentHeatSlope(surfaceCount);
        std::vector<float> storageHeat(surfaceCount);
        std::vector<float> storageHeatSlope(surfaceCount);
        uint32_t couplingIterations = 0;
        float maximumTemperatureDelta = 0.0f;

        std::vector<float> radiativeNet(surfaceCount);
        const auto evaluateEnergyFluxes =
            [&](const std::vector<facetvk::SurfaceOptics>& longwaveOptics,
                const std::vector<float>& longwaveIncident) {
                for (size_t side = 0; side < surfaceCount; ++side) {
                    const float emissivity = std::clamp(
                        1.0f - longwaveOptics[side].reflectance -
                        longwaveOptics[side].transmittance,
                        0.0f, 1.0f);
                    radiativeNet[side] =
                        absorbedShortwave[side] +
                        emissivity * longwaveIncident[side] -
                        longwaveOptics[side].emission;
                }
                for (uint32_t facet = 0;
                     facet < scene.facetCount(); ++facet) {
                    const size_t side0 =
                        static_cast<size_t>(facet) * 2U;
                    const SurfaceParameters& surface =
                        surfaceForFacet(scene, facet);
                    if (facet < scene.leafFacetCount) {
                        const float facetNetRadiation =
                            radiativeNet[side0] +
                            radiativeNet[side0 + 1U];
                        const float facetSensibleHeat =
                            2.0f * aerodynamicConductance * surface.convectiveScale *
                            (temperature[side0] - airTemperature);
                        // Both vegetation methods remain dynamically coupled to
                        // energy balance. Ball-Berry uses the configured baseline
                        // resistance; Farquhar additionally responds to absorbed PAR
                        // and leaf temperature, matching the two VoxelEB pipelines.
                        float leafResistance = surface.surfaceResistance;
                        if (vegetationTemperatureMethod == 1) {
                            const float absorbedPar = std::max(
                                0.0f, absorbedShortwave[side0] +
                                      absorbedShortwave[side0 + 1U]);
                            const float lightOpening = 0.25f +
                                0.75f * absorbedPar / (absorbedPar + 180.0f);
                            const float temperatureCelsius =
                                temperature[side0] - 273.15f;
                            const float temperatureResponse = std::exp(
                                -std::pow((temperatureCelsius - 25.0f) / 18.0f, 2.0f));
                            leafResistance = std::clamp(
                                surface.surfaceResistance /
                                    std::max(0.08f, lightOpening * temperatureResponse),
                                40.0f, 100000.0f);
                        }
                        const float facetLatentHeat = (surface.vegetation ? 2.0f : 0.0f) *
                            latentHeatFlux(
                                temperature[side0], airDensity,
                                airPressure, meteo.vaporPressure,
                                aerodynamicResistance,
                                leafResistance);
                        const float facetLatentHeatSlope = (surface.vegetation ? 2.0f : 0.0f) *
                            latentHeatFluxDerivative(
                                temperature[side0], airDensity,
                                airPressure, meteo.vaporPressure,
                                aerodynamicResistance,
                                leafResistance);
                        const float facetStorageSlope =
                            std::max(0.0f, surface.heatCapacityPerArea) /
                            dTime;
                        const float facetStorageHeat =
                            facetStorageSlope *
                            (temperature[side0] -
                             previousTemperature[side0]);
                        const float effectiveIncident=incidentShortwave[side0]+surface.pvBifaciality*incidentShortwave[side0+1U];
                        const float eta=surface.pvEta25*(1.0f+surface.pvGamma*(temperature[side0]-298.15f));
                        const float available=absorbedShortwave[side0]+absorbedShortwave[side0+1U];
                        const float rawPower=eta*effectiveIncident;
                        const float pvPower=surface.photovoltaic?std::clamp(rawPower,0.0f,available):0.0f;
                        const float pvSlope=surface.photovoltaic && rawPower>0 && rawPower<available ?
                            surface.pvEta25*surface.pvGamma*effectiveIncident:0.0f;
                        for (uint32_t localSide = 0;
                             localSide < 2U; ++localSide) {
                            const size_t side = side0 + localSide;
                            photovoltaicPower[side]=pvPower; photovoltaicSlope[side]=pvSlope;
                            netRadiation[side] =
                                facetNetRadiation;
                            sensibleHeat[side] =
                                facetSensibleHeat;
                            latentHeat[side] =
                                facetLatentHeat;
                            latentHeatSlope[side] =
                                facetLatentHeatSlope;
                            storageHeat[side] =
                                facetStorageHeat;
                            storageHeatSlope[side] =
                                facetStorageSlope;
                        }
                    } else {
                        for (uint32_t localSide = 0;
                             localSide < 2U; ++localSide) {
                            const size_t side = side0 + localSide;
                            netRadiation[side] =
                                radiativeNet[side];
                            sensibleHeat[side] =
                                aerodynamicConductance *
                                (temperature[side] -
                                    airTemperature);
                            const bool drySoil =
                                surface.soilMoisture < 0.03f;
                            latentHeat[side] = drySoil ? 0.0f :
                                latentHeatFlux(
                                    temperature[side], airDensity,
                                    airPressure, meteo.vaporPressure,
                                    aerodynamicResistance,
                                    surface.surfaceResistance);
                            latentHeatSlope[side] = drySoil ? 0.0f :
                                latentHeatFluxDerivative(
                                    temperature[side], airDensity,
                                    airPressure, meteo.vaporPressure,
                                    aerodynamicResistance,
                                    surface.surfaceResistance);
                            if (surface.soilTemperatureMethod == 0) {
                                storageHeatSlope[side] = 0.0f;
                                storageHeat[side] = 0.35f * radiativeNet[side];
                            } else {
                                storageHeatSlope[side] =
                                    soilStorageCoefficient(surface, dTime) *
                                    (surface.soilTemperatureMethod == 2 ? 1.5f : 1.0f);
                                storageHeat[side] =
                                    storageHeatSlope[side] *
                                    (temperature[side] -
                                     previousTemperature[side]);
                            }
                        }
                    }
                }
            };

        for (uint32_t coupling = 0;
             coupling < maximumCouplingIterations; ++coupling) {
            const std::vector<facetvk::SurfaceOptics> longwaveOptics =
                makeEnergyLongwaveOptics(scene, temperature);
            const facetvk::SolveResult longwaveSolution =
                model.solve(
                    longwaveOptics, skyLongwave, kIterations, 1.0f);
            const std::vector<float> longwaveIncident =
                model.incidentIrradiance(
                    longwaveSolution, longwaveOptics, skyLongwave);
            evaluateEnergyFluxes(longwaveOptics, longwaveIncident);

            maximumTemperatureDelta = 0.0f;
            for (uint32_t facet = 0;
                 facet < scene.facetCount(); ++facet) {
                const size_t side0 =
                    static_cast<size_t>(facet) * 2U;
                if (facet < scene.leafFacetCount) {
                    const float emissivity = std::clamp(
                        1.0f - longwaveOptics[side0].reflectance -
                        longwaveOptics[side0].transmittance,
                        0.0f, 1.0f);
                    const float residual =
                        netRadiation[side0] -
                        sensibleHeat[side0] -
                        latentHeat[side0] -
                        storageHeat[side0] - photovoltaicPower[side0];
                    const float derivative =
                        8.0f * emissivity * kStefanBoltzmann *
                        std::pow(temperature[side0], 3.0f) +
                        2.0f * aerodynamicConductance * surfaceForFacet(scene,facet).convectiveScale +
                        latentHeatSlope[side0] +
                        storageHeatSlope[side0] + photovoltaicSlope[side0];
                    const float candidate = std::clamp(
                        temperature[side0] +
                            residual / std::max(1.0f, derivative),
                        220.0f, 360.0f);
                    const float updated = temperature[side0] +
                        temperatureRelaxation *
                            (candidate - temperature[side0]);
                    maximumTemperatureDelta = std::max(
                        maximumTemperatureDelta,
                        std::abs(updated - temperature[side0]));
                    temperature[side0] = updated;
                    temperature[side0 + 1U] = updated;
                } else {
                    for (uint32_t localSide = 0;
                         localSide < 2U; ++localSide) {
                        const size_t side = side0 + localSide;
                        const float emissivity = std::clamp(
                            1.0f -
                            longwaveOptics[side].reflectance -
                            longwaveOptics[side].transmittance,
                            0.0f, 1.0f);
                        const float residual =
                            netRadiation[side] -
                            sensibleHeat[side] -
                            latentHeat[side] -
                            storageHeat[side];
                        const float derivative =
                            4.0f * emissivity *
                            kStefanBoltzmann *
                            std::pow(temperature[side], 3.0f) +
                            aerodynamicConductance +
                            latentHeatSlope[side] +
                            storageHeatSlope[side];
                        const float candidate = std::clamp(
                            temperature[side] +
                                residual /
                                    std::max(1.0f, derivative),
                            220.0f, 360.0f);
                        const float updated = temperature[side] +
                            temperatureRelaxation *
                                (candidate - temperature[side]);
                        maximumTemperatureDelta = std::max(
                            maximumTemperatureDelta,
                            std::abs(
                                updated - temperature[side]));
                        temperature[side] = updated;
                    }
                }
            }
            couplingIterations = coupling + 1U;
            if (maximumTemperatureDelta <= temperatureTolerance) {
                break;
            }
        }

        const std::vector<facetvk::SurfaceOptics> finalLongwaveOptics =
            makeEnergyLongwaveOptics(scene, temperature);
        const facetvk::SolveResult finalLongwaveSolution =
            model.solve(
                finalLongwaveOptics, skyLongwave, kIterations, 1.0f);
        const std::vector<float> finalLongwaveIncident =
            model.incidentIrradiance(
                finalLongwaveSolution, finalLongwaveOptics, skyLongwave);
        evaluateEnergyFluxes(
            finalLongwaveOptics, finalLongwaveIncident);

        std::ofstream pvCsv(outputDirectory/("photovoltaic_node_"+std::to_string(node)+".csv"));
        pvCsv << "area_m2,temperature_C,incident_W_m2,absorbed_W_m2,electric_W_m2,residual_W_m2,latent_W_m2,effective_W_m2,net_longwave_W_m2,sensible_W_m2,storage_W_m2,facet_index\n";
        double pa=0,pt=0,pg=0,pabs=0,pp=0,plw=0,ph=0,ps=0,pres=0;
        for(uint32_t f=0;f<scene.leafFacetCount;++f){
            const auto& mat=surfaceForFacet(scene,f);if(!mat.photovoltaic)continue;
            const size_t k=2U*f;const float a=facetArea(scene,f),ab=absorbedShortwave[k]+absorbedShortwave[k+1];
            const float inc=incidentShortwave[k]+incidentShortwave[k+1],power=photovoltaicPower[k];
            const float lw=netRadiation[k]-ab,res=netRadiation[k]-sensibleHeat[k]-latentHeat[k]-storageHeat[k]-power;
            pvCsv << a << ',' << temperature[k]-273.15f << ',' << inc << ',' << ab << ',' << power << ',' << res << ',' << latentHeat[k] << ','
                  << incidentShortwave[k]+mat.pvBifaciality*incidentShortwave[k+1] << ',' << lw << ',' << sensibleHeat[k] << ',' << storageHeat[k] << ',' << f << '\n';
            pa+=a;pt+=a*(temperature[k]-273.15f);pg+=a*inc;pabs+=a*ab;pp+=a*power;plw+=a*lw;ph+=a*sensibleHeat[k];ps+=a*storageHeat[k];pres=std::max(pres,double(std::abs(res)));
        }
        if(pa>0){pvEnergyKwh+=pp*dTime/3600000.;pvSummary << node << ',' << token << ',' << pa << ',' << pt/pa << ',' << pg << ',' << pabs << ',' << pp << ',' << plw << ',' << ph << ',' << ps << ',' << pres << ',' << pvEnergyKwh << '\n';}
        const facetvk::SolveResult radiativeSolution =
            combineRadiativeSolutions(
                shortwaveSolution, finalLongwaveSolution);
        // Image bands must remain spectral even though the energy balance uses
        // broadband shortwave/longwave fluxes. Optical bands are normalized to
        // apparent reflectance; thermal bands retain spectral radiance.
        std::vector<std::vector<float>> spectralOutput(
            radiativeParameters.wavelengths.size(),
            std::vector<float>(surfaceCount, 0.0f));
        const size_t bandCount = radiativeParameters.wavelengths.size();
        // Energy balance above uses integrated broadband shortwave/longwave
        // fluxes. Sensor products are a separate calculation: solve every
        // requested wavelength independently so a selected band is never an
        // interpolation of neighbouring bands.
        for (size_t band = 0; band < bandCount; ++band) {
            const float wavelength = radiativeParameters.wavelengths[band];
            const bool optical = isOpticalWavelength(wavelength);
            const float directSpectralIrradiance = optical
                ? totalShortwave * shortwaveSpectrum.directAt(wavelength) * 0.001f
                : 0.0f;
            const float diffuseSpectralIrradiance = optical
                ? totalShortwave * shortwaveSpectrum.diffuseAt(wavelength) * 0.001f
                : 0.0f;
            const std::vector<facetvk::SurfaceOptics> spectralOptics =
                makeStateSpectralOptics(
                    scene, sunlit, radiativeParameters, band,
                    temperature, directSpectralIrradiance);
            const float skySpectralRadiance = optical
                ? diffuseSpectralIrradiance * 0.5f
                : planckRadiance(wavelength, skyTemperature);
            const facetvk::SolveResult spectralSolution = model.solve(
                spectralOptics, skySpectralRadiance, kIterations, 1.0f);
            if (optical) {
                const float referenceIrradiance =
                    directSpectralIrradiance * cosineSolarZenith +
                    diffuseSpectralIrradiance * 0.5f;
                if (referenceIrradiance > 1.0e-8f) {
                    for (size_t side = 0; side < surfaceCount; ++side) {
                        spectralOutput[band][side] = std::clamp(
                            spectralSolution.radiosity[side] /
                                referenceIrradiance,
                            0.0f, 1.0f);
                    }
                }
            } else {
                spectralOutput[band] = spectralSolution.radiosity;
            }
        }

        const std::filesystem::path binaryPath =
            stepDirectory / ("energy_T=" + token + ".bin");
        std::ofstream binary(binaryPath, std::ios::binary | std::ios::trunc);
        if (!binary) {
            throw std::runtime_error(
                "FacetEB cannot write time-step energy file");
        }
        for (size_t side = 0; side < surfaceCount; ++side) {
            const float values[9] = {
                temperature[side],
                radiativeSolution.radiosity[side],
                shortwaveSolution.radiosity[side],
                finalLongwaveSolution.radiosity[side],
                netRadiation[side],
                sensibleHeat[side],
                latentHeat[side],
                storageHeat[side],
                sunlit[side]
            };
            binary.write(reinterpret_cast<const char*>(values), sizeof(values));
            for (const std::vector<float>& bandValues : spectralOutput) {
                const float value = bandValues[side];
                binary.write(
                    reinterpret_cast<const char*>(&value), sizeof(value));
            }
        }

        const auto writeProcessMetadata = [&](const std::string& type) {
            // Radiation in FacetEB is a time sequence of FacetRT fields;
            // energy fluxes retain the FacetEB model name.
            const std::string processModel =
                type == "radiation" ? "facetrt" : "faceteb";
            const std::string processStem = processModel + (type == "photovoltaic" ? "_pv_T=" : "_T=") + token;
            const std::filesystem::path processDirectory = outputDirectory / "process";
            // PV results are also written when the optional RT/EB process flags
            // are off. Ensure their destination exists at the point of use.
            std::filesystem::create_directories(processDirectory);
            const std::filesystem::path processBinaryPath =
                processDirectory / (processStem + ".bin");
            const std::filesystem::path metadataPath =
                processDirectory / (processStem + ".json");
            std::ofstream processBinary(
                processBinaryPath, std::ios::binary | std::ios::trunc);
            if (!processBinary) {
                throw std::runtime_error(
                    "FacetEB cannot write process data: " +
                    processBinaryPath.string());
            }
            for (size_t side = 0; side < surfaceCount; ++side) {
                const float values[3] = {
                    type == "photovoltaic" ? photovoltaicPower[side] : type == "radiation" ? shortwaveSolution.radiosity[side]
                                        : latentHeat[side],
                    type == "photovoltaic" ? temperature[side] : type == "radiation" ? finalLongwaveSolution.radiosity[side]
                                        : sensibleHeat[side],
                    type == "photovoltaic" ? absorbedShortwave[side] : type == "radiation" ? netRadiation[side]
                                        : storageHeat[side]
                };
                processBinary.write(
                    reinterpret_cast<const char*>(values), sizeof(values));
            }
            processBinary.close();
            if (!processBinary) throw std::runtime_error("FacetEB cannot finish process binary");
            std::ofstream metadata(metadataPath, std::ios::trunc);
            metadata << std::setprecision(9)
                     << "{\n  \"kind\": \"facet-" << type << "-process\",\n"
                     << "  \"model\": \"" << processModel << "\",\n"
                     << "  \"processType\": \"" << type << "\",\n"
                     << "  \"geometry\": \"facet\",\n"
                     << "  \"node\": " << node << ",\n"
                     << "  \"julianTime\": " << meteo.julianTime << ",\n"
                     << "  \"time\": \"" << token << "\",\n"
                     << "  \"solarZenith\": " << scene.sunZenith << ",\n"
                     << "  \"solarAzimuth\": " << scene.sunAzimuth << ",\n"
                     << "  \"facetCount\": " << scene.facetCount() << ",\n"
                     << "  \"surfaceCount\": " << surfaceCount << ",\n"
                     << "  \"geometryFile\": \"../faceteb.json\",\n"
                     << "  \"dataFile\": \""
                     << processBinaryPath.filename().string() << "\",\n"
                     << "  \"dataType\": \"float32-little-endian\",\n"
                     << "  \"layout\": \"surface-interleaved\",\n"
                     << "  \"recordFloats\": 3,\n"
                     << "  \"couplingIterations\": " << couplingIterations << ",\n"
                     << "  \"temperatureDelta\": "
                     << maximumTemperatureDelta << ",\n  \"fields\": ";
            if (type == "photovoltaic") {
                metadata << "[{\"id\":\"photovoltaicPower\",\"label\":\"光伏功率 [W m⁻²]\",\"offset\":0},"
                            "{\"id\":\"temperature\",\"label\":\"温度 [K]\",\"offset\":1},"
                            "{\"id\":\"absorbedShortwave\",\"label\":\"单面吸收短波 [W m⁻²]\",\"offset\":2}]\n}\n";
            } else if (type == "radiation") {
                metadata << "[{\"id\":\"shortwaveRadiation\",\"label\":\"短波辐射 [W m⁻²]\",\"offset\":0},"
                            "{\"id\":\"longwaveRadiation\",\"label\":\"长波辐射 [W m⁻²]\",\"offset\":1},"
                            "{\"id\":\"netRadiation\",\"label\":\"净辐射 [W m⁻²]\",\"offset\":2}]\n}\n";
            } else {
                metadata << "[{\"id\":\"latentHeat\",\"label\":\"潜热 [W m⁻²]\",\"offset\":0},"
                            "{\"id\":\"sensibleHeat\",\"label\":\"显热 [W m⁻²]\",\"offset\":1},"
                            "{\"id\":\"surfaceHeatFlux\",\"label\":\"表面热通量 [W m⁻²]\",\"offset\":2}]\n}\n";
            }
        };
        if (saveRadiationProcess) writeProcessMetadata("radiation");
        if (saveEnergyProcess) writeProcessMetadata("energy");
        if (pa > 0) writeProcessMetadata("photovoltaic");

        binary.close();
        pvCsv.close();
        pvSummary.flush();
        if (!binary || !pvCsv || !pvSummary) {
            throw std::runtime_error("FacetEB cannot finish time-step output");
        }

        if (node == endNode - 1) {
            latest.sunlit = sunlit;
            latest.solution = radiativeSolution;
            latest.lightEnhancement = lightEnhancement(
                shortwaveSolution.radiosity, diffuseSolution.radiosity);
            latestNetRadiation = std::move(netRadiation);
            latestSensibleHeat = std::move(sensibleHeat);
            latestLatentHeat = std::move(latentHeat);
            latestPhotovoltaicPower = std::move(photovoltaicPower);
            latestStorageHeat = std::move(storageHeat);
            latestCouplingIterations = couplingIterations;
            latestTemperatureDelta = maximumTemperatureDelta;
            latestTime = token;
        }

        const int progress = 10 + static_cast<int>(
            86.0 * (node - startNode + 1) / (endNode - startNode));
        std::cout << "PROGRESS\t" << progress
                  << "\t面元 RT-EB 时间节点 " << (node - startNode + 1)
                  << "/" << (endNode - startNode) << " " << token
                  << "，迭代=" << couplingIterations
                  << "，最大温差=" << maximumTemperatureDelta << " K"
                  << std::endl;
        }
        if (synchronizeStepOutput) {
            // Backpressure: never overlap the next GPU solve with the current
            // node's CPU image buffers. EOF/error aborts instead of hanging.
            std::cout << "FACET_STEP\t" << node << '\t' << token << '\t'
                      << std::setprecision(9) << meteo.julianTime << std::endl;
            std::string acknowledgment;
            if (!std::getline(std::cin, acknowledgment) ||
                acknowledgment != "FACET_ACK\t" + std::to_string(node)) {
                throw std::runtime_error("FacetEB time-step output was interrupted or failed");
            }
        }
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
    writeArray("storageHeat", latestStorageHeat, true);
    writeArray("photovoltaicPower", latestPhotovoltaicPower, false);
    output << "}\n";

    std::cout << "PROGRESS\t100\t面元辐射传输与能量平衡耦合完成" << std::endl;
    std::cout << "RESULT\t" << resultFile << std::endl;
    return 0;
} catch (const std::exception& error) {
    std::cerr << "faceteb: " << error.what() << '\n';
    return 1;
}
