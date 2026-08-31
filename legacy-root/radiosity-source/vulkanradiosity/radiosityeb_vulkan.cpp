#include "radiosityeb_vulkan.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

float normalizedDot(const glm::vec3& normal, const facetvk::Direction& direction)
{
    const float normalLength = glm::length(normal);
    if (!(normalLength > 1.0e-8f)) {
        return 0.0f;
    }
    return (normal.x * direction.x + normal.y * direction.y + normal.z * direction.z) /
           normalLength;
}

} // namespace

facetvk::Direction RadiosityEBVulkan::directionFromAngles(float zenithRadians,
                                                           float azimuthRadians)
{
    return {std::sin(zenithRadians) * std::cos(azimuthRadians),
            std::sin(zenithRadians) * std::sin(azimuthRadians),
            std::cos(zenithRadians)};
}

std::vector<facetvk::Vertex> RadiosityEBVulkan::makeVertices(const RadiosityEBIO& model)
{
    const auto& facets = model.m_facetio->facets;
    std::vector<facetvk::Vertex> vertices;
    vertices.reserve(facets.size() * 3U);
    for (const Facet& facet : facets) {
        glm::vec3 normal = facet.pnorm;
        const float length = glm::length(normal);
        if (!(length > 1.0e-8f)) {
            throw std::runtime_error("A zero-area facet cannot be uploaded to Vulkan radiosity");
        }
        normal /= length;
        for (const glm::vec3& point : facet.points) {
            facetvk::Vertex vertex{};
            vertex.position[0] = point.x;
            vertex.position[1] = point.y;
            vertex.position[2] = point.z;
            vertex.normal[0] = normal.x;
            vertex.normal[1] = normal.y;
            vertex.normal[2] = normal.z;
            vertices.push_back(vertex);
        }
    }
    return vertices;
}

std::vector<facetvk::Direction> RadiosityEBVulkan::makeSkyDirections(
    const RadiosityEBIO& model)
{
    std::vector<facetvk::Direction> directions;
    directions.reserve(NSKY);
    for (uint32_t index = 0; index < NSKY; ++index) {
        directions.push_back(directionFromAngles(model.skyvza[index], model.skyvaa[index]));
    }
    return directions;
}

facetvk::GraphDiagnostics RadiosityEBVulkan::prepare(
    const std::shared_ptr<RadiosityEBIO>& model,
    const facetvk::Config& config,
    const std::string& shaderDirectory)
{
    if (!model || !model->m_facetio || !model->m_meshio) {
        throw std::invalid_argument("RadiosityEB model is not initialized");
    }
    m_gpu.initialize(config, shaderDirectory);
    m_gpu.setGeometry(makeVertices(*model));
    return m_gpu.buildVisibilityGraph(makeSkyDirections(*model));
}

std::vector<float> RadiosityEBVulkan::makeDirectIrradiance(
    const RadiosityEBIO& model,
    const facetvk::Direction& sun,
    const std::vector<float>& sunlitFraction,
    float beamIrradiance)
{
    const auto& facets = model.m_facetio->facets;
    if (sunlitFraction.size() != facets.size() * 2U) {
        throw std::invalid_argument("Sunlit fraction size does not match facet sides");
    }
    std::vector<float> result(sunlitFraction.size(), 0.0f);
    for (uint32_t facet = 0; facet < facets.size(); ++facet) {
        const float cosine = normalizedDot(facets[facet].pnorm, sun);
        result[facet * 2U] = beamIrradiance * std::max(cosine, 0.0f) *
                             sunlitFraction[facet * 2U];
        result[facet * 2U + 1U] = beamIrradiance * std::max(-cosine, 0.0f) *
                                  sunlitFraction[facet * 2U + 1U];
    }
    return result;
}

facetvk::SolveResult RadiosityEBVulkan::solveShortwaveBand(
    const std::shared_ptr<RadiosityEBIO>& model,
    uint32_t wavelengthIndex,
    float skyRadiosity,
    float beamIrradiance,
    uint32_t iterations,
    float relaxation)
{
    if (!model) {
        throw std::invalid_argument("RadiosityEB model is null");
    }
    const facetvk::Direction sun = directionFromAngles(model->sza * RD, model->saa * RD);
    const std::vector<float> sunlitFraction = m_gpu.computeSunlitFraction(sun);
    const std::vector<float> direct = makeDirectIrradiance(
        *model, sun, sunlitFraction, std::max(beamIrradiance, 0.0f));
    const std::vector<float> emission(m_gpu.surfaceCount(), 0.0f);
    return solve(model, wavelengthIndex, skyRadiosity, direct, emission,
                 iterations, relaxation);
}

facetvk::SolveResult RadiosityEBVulkan::solveThermal(
    const std::shared_ptr<RadiosityEBIO>& model,
    float skyRadiosity,
    const std::vector<float>& sideTemperaturesKelvin,
    uint32_t iterations,
    float relaxation)
{
    if (!model || sideTemperaturesKelvin.size() != m_gpu.surfaceCount()) {
        throw std::invalid_argument(
            "Thermal temperatures must contain exactly two sides per facet");
    }
    const auto& facets = model->m_facetio->facets;
    const auto& meshLinks = model->m_meshio->meshLinks;
    const auto& spectra = model->m_meshio->fixedSpectrals;
    std::vector<facetvk::SurfaceOptics> optics(m_gpu.surfaceCount());
    for (uint32_t facet = 0; facet < facets.size(); ++facet) {
        const int meshId = facets[facet].fsign;
        if (meshId < 0 || static_cast<size_t>(meshId) >= meshLinks.size()) {
            throw std::out_of_range("Facet mesh link is invalid");
        }
        const int spectralId = meshLinks[meshId].spectralId;
        if (spectralId < 0 || static_cast<size_t>(spectralId) >= spectra.size()) {
            throw std::out_of_range("Facet spectral link is invalid");
        }
        const float reflectance = spectra[spectralId].Refl_ir;
        const float transmittance = spectra[spectralId].Tran_ir;
        const float emissivity = 1.0f - reflectance - transmittance;
        if (emissivity < 0.0f) {
            throw std::invalid_argument("Thermal reflectance+transmittance exceeds one");
        }
        for (uint32_t side = 0; side < 2U; ++side) {
            const uint32_t surface = facet * 2U + side;
            const float temperature = sideTemperaturesKelvin[surface];
            if (!std::isfinite(temperature) || temperature <= 0.0f) {
                throw std::invalid_argument("Thermal temperatures must be positive Kelvin");
            }
            const float emitted = emissivity * SIGMASB * temperature * temperature *
                                  temperature * temperature;
            optics[surface] = {reflectance, transmittance, emitted, 0.0f};
        }
    }
    return m_gpu.solve(optics, skyRadiosity, iterations, relaxation);
}

facetvk::SolveResult RadiosityEBVulkan::solve(
    const std::shared_ptr<RadiosityEBIO>& model,
    uint32_t wavelengthIndex,
    float skyRadiosity,
    const std::vector<float>& directIrradiance,
    const std::vector<float>& emission,
    uint32_t iterations,
    float relaxation)
{
    if (!model || wavelengthIndex >= N1) {
        throw std::out_of_range("Invalid RadiosityEB model or wavelength index");
    }
    if (directIrradiance.size() != m_gpu.surfaceCount() ||
        emission.size() != m_gpu.surfaceCount()) {
        throw std::invalid_argument("Direct and emission arrays must contain two sides per facet");
    }

    const auto& facets = model->m_facetio->facets;
    const auto& meshLinks = model->m_meshio->meshLinks;
    const auto& spectra = model->m_meshio->fixedSpectrals;
    std::vector<facetvk::SurfaceOptics> optics(m_gpu.surfaceCount());
    for (uint32_t facet = 0; facet < facets.size(); ++facet) {
        const int meshId = facets[facet].fsign;
        if (meshId < 0 || static_cast<size_t>(meshId) >= meshLinks.size()) {
            throw std::out_of_range("Facet mesh link is invalid");
        }
        const int spectralId = meshLinks[meshId].spectralId;
        if (spectralId < 0 || static_cast<size_t>(spectralId) >= spectra.size()) {
            throw std::out_of_range("Facet spectral link is invalid");
        }
        const float reflectance = spectra[spectralId].Refl_[wavelengthIndex];
        const float transmittance = spectra[spectralId].Tran_[wavelengthIndex];
        for (uint32_t side = 0; side < 2U; ++side) {
            const uint32_t surface = facet * 2U + side;
            optics[surface] = {reflectance, transmittance, emission[surface],
                               directIrradiance[surface]};
        }
    }
    return m_gpu.solve(optics, skyRadiosity, iterations, relaxation);
}
