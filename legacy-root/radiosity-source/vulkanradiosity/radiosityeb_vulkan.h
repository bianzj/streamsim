#pragma once

#include "vulkan_radiosity.h"
#include "../radiosityeb/radiosityebio.h"

#include <memory>
#include <string>
#include <vector>

// Adapter between the existing two-sided Facet/RadiosityEB data and the
// raster-only Vulkan solver.  The visibility graph is built once and reused by
// every wavelength and meteorological time node.
class RadiosityEBVulkan {
public:
    facetvk::GraphDiagnostics prepare(const std::shared_ptr<RadiosityEBIO>& model,
                                      const facetvk::Config& config,
                                      const std::string& shaderDirectory);

    // skyRadiosity is the diffuse spectral sky boundary value. beamIrradiance
    // is measured normal to the solar beam (horizontal direct input must first
    // be divided by cos(solar zenith)).
    facetvk::SolveResult solveShortwaveBand(const std::shared_ptr<RadiosityEBIO>& model,
                                            uint32_t wavelengthIndex,
                                            float skyRadiosity,
                                            float beamIrradiance,
                                            uint32_t iterations = 64,
                                            float relaxation = 1.0f);

    facetvk::SolveResult solveThermal(const std::shared_ptr<RadiosityEBIO>& model,
                                      float skyRadiosity,
                                      const std::vector<float>& sideTemperaturesKelvin,
                                      uint32_t iterations = 64,
                                      float relaxation = 1.0f);

    // Shortwave entry for externally constructed direct and emission sources.
    facetvk::SolveResult solve(const std::shared_ptr<RadiosityEBIO>& model,
                               uint32_t wavelengthIndex,
                               float skyRadiosity,
                               const std::vector<float>& directIrradiance,
                               const std::vector<float>& emission,
                               uint32_t iterations = 64,
                               float relaxation = 1.0f);

    void destroy() { m_gpu.destroy(); }

private:
    static facetvk::Direction directionFromAngles(float zenithRadians,
                                                   float azimuthRadians);
    static std::vector<facetvk::Vertex> makeVertices(const RadiosityEBIO& model);
    static std::vector<facetvk::Direction> makeSkyDirections(const RadiosityEBIO& model);
    static std::vector<float> makeDirectIrradiance(const RadiosityEBIO& model,
                                                   const facetvk::Direction& sun,
                                                   const std::vector<float>& sunlitFraction,
                                                   float beamIrradiance);

    facetvk::VulkanRadiosity m_gpu;
};
