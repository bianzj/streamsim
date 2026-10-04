#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "production_soil.hpp"

#define TYPE_SOIL 1
#define TYPE_VEGETATION 2
#define TYPE_BUILDING 3
#define TYPE_WATER 4
constexpr int gl_ScopeDevice = 0, gl_StorageSemanticsBuffer = 0, gl_SemanticsRelaxed = 0;
template<class T> struct CheckedBuffer {
    std::vector<T> data;
    T& operator[](unsigned index) { return data.at(index); }
};
struct CarbonFlux { float GPPsunlit, GPPshaded, NPPsunlit, NPPshaded; };
struct Resistance { float sunlit, shaded; };
struct Material { float rss; };
struct Canopy { bool participatingMedium; };
bool isParticipatingMedium(const Canopy& canopy) { return canopy.participatingMedium; }
CheckedBuffer<CarbonFlux> voxelHeatFlux;
CheckedBuffer<Resistance> voxelRsss;
CheckedBuffer<Material> soilSets, waterSets;
CheckedBuffer<Canopy> canopies;
template<class... Args> void atomicStore(float& target, float value, Args...) { target = value; }
#include "production_biochemical.hpp"
struct Placement {
    float x, y, z;
    Placement(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};
struct PrimEntity {
    std::string primitiveName = "building", distributefile;
    std::vector<std::string> meshNames = {"building"}, spectralNames = {"concrete"},
        canopyNames = {"rigid"}, propNames = {"solid"};
    bool isdisFromFile = false;
    std::vector<Placement> primDistributions = {{0, 0, 0}};
    std::vector<float> scales = {1}, rotations = {0};
};
#include "production_building.hpp"

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool near(float a, float b, float tolerance = 0.005f) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tolerance;
}
static constexpr float depth[8] = {0.0f, 0.02f, 0.04f, 0.10f, 0.20f, 0.40f, 0.60f, 1.0f};
static float volumetricCapacity(float moisture, float cs, float density, float saturation) {
    return cs * density + 4.195e6f * soilMoistureFraction(moisture, saturation);
}
static double storageRate(const float* old, const float* updated, const float* moisture,
                          float dt, float cs, float density, float saturation) {
    // Independent conservation oracle: each node owns the interval between
    // adjacent midpoints, including the surface half-cell. Bottom T is fixed.
    double total = 0.0;
    for (int i = 0; i < 7; ++i) {
        const double width = i == 0 ? depth[1] * 0.5 : (depth[i + 1] - depth[i - 1]) * 0.5;
        total += volumetricCapacity(moisture[i], cs, density, saturation) * width *
                 (double(updated[i]) - old[i]) / dt;
    }
    return total;
}

int main() {
    float old[8], moisture[8], updated[8], flux = 0.0f;
    constexpr float dt = 1800.0f, cs = 1180.0f, density = 1800.0f, saturation = 0.45f;
    for (int i = 0; i < 8; ++i) { old[i] = 30.0f - 10.0f * depth[i]; moisture[i] = 0.25f; }
    Soilheatflux(old, moisture, 30.0f, 20.0f, flux, updated, dt, cs, density, 0.8f, saturation);
    require(near(flux, 8.0f), "steady linear gradient must conduct 8 W/m2 through both boundaries");
    for (int i = 0; i < 8; ++i) require(near(old[i], updated[i], 0.00001f), "steady linear profile changed");
    const float steadyFlux = flux;
    for (float& temperature : old) temperature = 25.0f;
    Soilheatflux(old, moisture, 25.0f, 25.0f, flux, updated, dt, cs, density, 0.8f, saturation);
    require(near(flux, 0.0f), "isothermal column must have zero heat flux");

    // Heating a previously uniform column must conserve energy over the full
    // nonuniform mesh, not merely return its storage while dropping bottom G.
    old[6] = 28.0f; // Existing deep heat must also leave through the bottom.
    Soilheatflux(old, moisture, 35.0f, 25.0f, flux, updated, dt, cs, density, 0.8f, saturation);
    const float transientFlux = flux;
    const float bottomFlux = 0.8f * (updated[6] - updated[7]) / (depth[7] - depth[6]);
    const double transientStorage = storageRate(old, updated, moisture, dt, cs, density, saturation);
    require(std::fabs(flux - transientStorage - bottomFlux) < 0.015,
            "transient G must equal control-volume storage plus lower-boundary flux");
    require(updated[0] == 35.0f && updated[7] == 25.0f, "Dirichlet boundaries drifted");
    for (float temperature : updated) require(temperature >= 24.999f && temperature <= 35.001f,
                                             "implicit heating violated the maximum principle");

    // Check each interior control volume as well as global conservation: this
    // rejects the doubled widths formerly used on the nonuniform mesh.
    for (int i = 1; i < 7; ++i) {
        const double storage = volumetricCapacity(moisture[i], cs, density, saturation) *
            (depth[i + 1] - depth[i - 1]) * 0.5 * (double(updated[i]) - old[i]) / dt;
        const double incoming = 0.8 * (updated[i - 1] - updated[i]) / (depth[i] - depth[i - 1]);
        const double outgoing = 0.8 * (updated[i] - updated[i + 1]) / (depth[i + 1] - depth[i]);
        require(std::fabs(storage - incoming + outgoing) < 0.015, "an interior volume does not conserve energy");
    }

    require(bottomFlux > 1.0f, "transient test must exercise a nonzero lower-boundary flux");
    for (float& temperature : old) temperature = 25.0f;
    Soilheatflux(old, moisture, 35.0f, 25.0f, flux, updated, dt, cs, density, 0.8f, saturation);
    const float uniformTransientFlux = flux;
    const float derivative = soilHeatFluxSurfaceDerivative(moisture, dt, cs, density, 0.8f, saturation);
    float plus = 0.0f, minus = 0.0f;
    Soilheatflux(old, moisture, 35.1f, 25.0f, plus, updated, dt, cs, density, 0.8f, saturation);
    Soilheatflux(old, moisture, 34.9f, 25.0f, minus, updated, dt, cs, density, 0.8f, saturation);
    const float finiteDifference = (plus - minus) / 0.2f;
    require(near(derivative, finiteDifference, 0.006f), "budget conduction derivative disagrees with finite differences");

    float lowCapacity = 0.0f, highCapacity = 0.0f, lowDensity = 0.0f, highDensity = 0.0f;
    Soilheatflux(old, moisture, 35.0f, 25.0f, lowCapacity, updated, dt, cs * 0.5f, density, 0.8f, saturation);
    Soilheatflux(old, moisture, 35.0f, 25.0f, highCapacity, updated, dt, cs * 2.0f, density, 0.8f, saturation);
    Soilheatflux(old, moisture, 35.0f, 25.0f, lowDensity, updated, dt, cs, density * 0.5f, 0.8f, saturation);
    Soilheatflux(old, moisture, 35.0f, 25.0f, highDensity, updated, dt, cs, density * 2.0f, 0.8f, saturation);
    require(highCapacity > lowCapacity && highDensity > lowDensity,
            "configured specific heat and density must affect transient flux");
    float dryFlux = 0.0f, wetFlux = 0.0f, limitedFlux = 0.0f, lowConductivity = 0.0f, highConductivity = 0.0f;
    for (float& water : moisture) water = 0.0f;
    Soilheatflux(old, moisture, 35.0f, 25.0f, dryFlux, updated, dt, cs, density, 0.8f, saturation);
    for (float& water : moisture) water = 25.0f;
    Soilheatflux(old, moisture, 35.0f, 25.0f, wetFlux, updated, dt, cs, density, 0.8f, saturation);
    Soilheatflux(old, moisture, 35.0f, 25.0f, limitedFlux, updated, dt, cs, density, 0.8f, 0.10f);
    Soilheatflux(old, moisture, 35.0f, 25.0f, lowConductivity, updated, dt, cs, density, 0.2f, saturation);
    Soilheatflux(old, moisture, 35.0f, 25.0f, highConductivity, updated, dt, cs, density, 1.6f, saturation);
    require(wetFlux > limitedFlux && limitedFlux > dryFlux && highConductivity > lowConductivity,
            "SMC, saturation and conductivity must affect transient conduction");
    require(near(wetFlux, uniformTransientFlux), "percentage and fraction SMC must be equivalent");
    float insulated = 0.0f;
    Soilheatflux(old, moisture, 35.0f, 25.0f, insulated, updated, dt, cs, density, 0.0f, saturation);
    require(near(insulated, volumetricCapacity(0.25f, cs, density, saturation) * 0.01f * 10.0f / dt),
            "zero conductivity must retain only surface-cell storage");

    struct ConductionCase { float cs, rho, conductivity, saturation, moisture, dt; };
    const ConductionCase cases[] = {
        {900.0f, 2300.0f, 1.5f, 0.0f, 25.0f, 1800.0f}, // dry building
        {1700.0f, 600.0f, 0.15f, 0.0f, 0.0f, 300.0f}, // wood
        {1180.0f, 1800.0f, 0.8f, 0.45f, 25.0f, 60.0f},
        {1180.0f, 1800.0f, 0.8f, 0.10f, 25.0f, 1800.0f},
        {1180.0f, 1800.0f, 5.0f, 0.45f, 0.25f, 86400.0f},
        {1180.0f, 1800.0f, 0.0f, 0.45f, 0.25f, 1800.0f}
    };
    for (const auto& material : cases) {
        for (float& water : moisture) water = material.moisture;
        const float exact = soilHeatFluxSurfaceDerivative(moisture, material.dt,
            material.cs, material.rho, material.conductivity, material.saturation);
        Soilheatflux(old, moisture, 35.5f, 25.0f, plus, updated, material.dt,
            material.cs, material.rho, material.conductivity, material.saturation);
        Soilheatflux(old, moisture, 34.5f, 25.0f, minus, updated, material.dt,
            material.cs, material.rho, material.conductivity, material.saturation);
        require(near(exact, plus - minus, max(0.01f, exact * 0.0002f)),
                "a material/timestep-specific derivative disagrees with finite differences");
    }

    // Farquhar nonvegetation dispatch must retain material resistance and clear
    // carbon even with stale output. Index 2 is deliberately beyond leaf id 0.
    soilSets.data = {{100.0f}, {200.0f}, {900.0f}};
    waterSets.data = {{10.0f}, {20.0f}, {30.0f}};
    canopies.data = {{false}, {true}};
    voxelHeatFlux.data.resize(1); voxelRsss.data.resize(1);
    for (const unsigned type : {TYPE_SOIL, TYPE_BUILDING, TYPE_WATER, 0}) {
        voxelHeatFlux.data[0] = {11.0f, 12.0f, 13.0f, 14.0f};
        voxelRsss.data[0] = {-1.0f, -1.0f};
        require(!productionMaterialDispatch(type, 2, 0), "nonvegetation entered the leaf-material branch");
        const auto& carbon = voxelHeatFlux.data[0];
        require(carbon.GPPsunlit == 0 && carbon.GPPshaded == 0 && carbon.NPPsunlit == 0 && carbon.NPPshaded == 0,
                "nonvegetation retained carbon flux");
        if (type == TYPE_SOIL || type == TYPE_BUILDING)
            require(voxelRsss.data[0].sunlit == 900 && voxelRsss.data[0].shaded == 900, "solid material resistance lost");
        if (type == TYPE_WATER)
            require(voxelRsss.data[0].sunlit == 30 && voxelRsss.data[0].shaded == 30, "water material resistance lost");
    }
    require(productionMaterialDispatch(TYPE_VEGETATION, 0, 0), "vegetation failed to enter Farquhar branch");
    require(!productionMaterialDispatch(TYPE_VEGETATION, 2, 0, 1),
            "radiation-only fog/fire medium entered leaf biochemistry");

    // Compile the production primitive-building parser too: its wall/roof
    // split must accept a single schema binding, and consume real placements.
    PrimEntity building;
    preparePrimitiveBuilding(building);
    require(building.meshNames.size() == 2 && building.spectralNames[1] == "concrete" &&
            building.propNames[1] == "solid", "single building binding did not expand to wall and roof");
    building.spectralNames[1] = "roof";
    building.isdisFromFile = true;
    building.distributefile = "building_positions.txt";
    { std::ofstream positions(building.distributefile);
      positions << "# x z height [scale] [rotation]\n8 3 0\n2 6 1 1.25 30 // second placement\n"; }
    preparePrimitiveBuilding(building);
    require(building.spectralNames[1] == "roof" && building.primDistributions.size() == 2 &&
            building.primDistributions[0].x == 8 && building.primDistributions[0].y == 3 &&
            building.primDistributions[1].z == 1 && building.scales[0] == 1 &&
            building.scales[1] == 1.25f && building.rotations[1] == 30,
            "building wall/roof or placement fields were lost");
    for (const char* invalid : {"8 3\n", "8 3 0 0 0\n", "8 3 0 bad\n", "# no instances\n"}) {
        { std::ofstream positions(building.distributefile); positions << invalid; }
        bool rejected = false;
        try { preparePrimitiveBuilding(building); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "malformed building position data was accepted");
    }
    building.meshNames.resize(3);
    bool rejected = false;
    try { preparePrimitiveBuilding(building); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "unsupported primitive mesh bindings were not rejected safely");
    std::printf("{\"passed\":true,\"steady_flux_W_m2\":%.8g,\"transient_flux_W_m2\":%.8g,"
                "\"storage_W_m2\":%.8g,\"bottom_flux_W_m2\":%.8g,\"derivative_W_m2_K\":%.8g,"
                "\"finite_difference_W_m2_K\":%.8g,\"material_derivative_cases\":6,"
                "\"nonvegetation_dispatch_cases\":4,\"radiation_only_medium_skipped\":true,"
                "\"primitive_building_inputs_passed\":true}\n",
                steadyFlux, transientFlux, transientStorage, bottomFlux, derivative, finiteDifference);
}
