#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
using std::abs;
using std::isnan;
using std::isinf;
inline float max(float a, float b) { return std::max(a, b); }
inline float min(float a, float b) { return std::min(a, b); }
inline float clamp(float value, float low, float high) { return min(max(value, low), high); }
inline float sign(float value) { return value > 0 ? 1.0f : (value < 0 ? -1.0f : 0.0f); }
inline float uintBitsToFloat(unsigned bits) { float value; std::memcpy(&value, &bits, sizeof(value)); return value; }
#include "production_helpers.hpp"

constexpr unsigned TYPE_SOIL = 1, TYPE_VEGETATION = 2, TYPE_BUILDING = 3, TYPE_WATER = 4;
constexpr int gl_ScopeDevice = 0, gl_StorageSemanticsBuffer = 0, gl_SemanticsRelaxed = 0;
constexpr float RHOA = 1.2047f, MAIR = 28.96f, STRESS = 1.0f, KV = 0.6396f, R = 8.31f;
struct ivec3 { int x, y, z; ivec3(int x, int y, int z) : x(x), y(y), z(z) {} };
struct vec3 { float x = 0, y = 0, z = 0; vec3() = default; vec3(float x, float y, float z) : x(x), y(y), z(z) {} };
template<class T> struct CheckedBuffer {
    std::vector<T> data;
    T& operator[](unsigned index) { return data.at(index); }
};
struct Carbon { float GPPsunlit = 0, GPPshaded = 0, NPPsunlit = 0, NPPshaded = 0; };
struct Pair { float sunlit = 25, shaded = 25; };
struct Air { float es = 15.0f, cs = 400.0f, ci = 0; };
struct Pnet { float directPnet = 800, diffusePnet = 200; };
struct Canopy { float height = 10; bool participatingMedium = false; };
struct LeafBio {
    float qLs = 1, kNPQs = 0, Vcmax = 80, Rdparam = 0.015f, Type = 3;
    float beta = 0.507f, leafM = 9, Tyear = 25, kV = 0.6396f, stressfactor = 1;
    float Tparam[5] = {.2f, .3f, 288, 313, 328};
    int Tcor = 0;
};
struct Material { float rss = 50; };
struct VoxelLink { unsigned isValid = 1, instanceId = 0; ivec3 voxelId = {0, 9, 0}; int faceId = 0; };
struct InstanceLink { unsigned meshId = 0; };
struct MeshLink { unsigned spectralId = 0, canopyId = 0, bioId = 0, type = TYPE_VEGETATION; };
struct Setting { vec3 voxelSize = {2, 12, 2}; unsigned voxelCount = 1; float scale = 1; } setting;
struct Meteo { float Oa = 209, p = 1013.25f, ea = 15; };
struct Uniform { Meteo meteo; } ubom;
struct Invocation { unsigned x = 0; } gl_GlobalInvocationID;
CheckedBuffer<VoxelLink> voxelLinks;
CheckedBuffer<InstanceLink> instanceLinks;
CheckedBuffer<MeshLink> meshLinks;
CheckedBuffer<Carbon> voxelHeatFlux;
CheckedBuffer<Pair> voxelTempes, voxelRsss;
CheckedBuffer<Air> voxelAirs;
CheckedBuffer<Pnet> voxelPnets;
CheckedBuffer<Canopy> canopies;
CheckedBuffer<LeafBio> leafBios;
CheckedBuffer<Material> soilSets, waterSets;
bool isParticipatingMedium(const Canopy& canopy) { return canopy.participatingMedium; }
template<class... Args> void atomicStore(float& target, float value, Args...) { target = value; }
#include "production_dispatch.hpp"
#include "production_collatz.hpp"
#include "normal_domain_baseline.hpp"

static void require(bool condition, const char* message) {
    if(!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool near(float actual, double expected, double relative = 2e-5, double absolute = 1e-7) {
    return std::isfinite(actual) && std::fabs(actual - expected) <= absolute + relative * std::fabs(expected);
}
static void reset(float type = 3) {
    setting = Setting{}; ubom = Uniform{}; gl_GlobalInvocationID = Invocation{};
    voxelLinks.data = {VoxelLink{}}; instanceLinks.data = {InstanceLink{}};
    meshLinks.data = {MeshLink{}}; voxelHeatFlux.data = {Carbon{}};
    voxelTempes.data = {Pair{}}; voxelRsss.data = {Pair{}};
    voxelAirs.data = {Air{}}; voxelPnets.data = {Pnet{}};
    canopies.data = {Canopy{}}; leafBios.data = {LeafBio{}};
    leafBios[0].Type = type;
    if(type == 4) leafBios[0].leafM = 4;
    soilSets.data = {Material{}}; waterSets.data = {Material{}};
}
static void finiteResult() {
    const auto& carbon = voxelHeatFlux[0];
    if(!std::isfinite(carbon.GPPsunlit) || !std::isfinite(carbon.GPPshaded))
        std::fprintf(stderr, "context Type=%g T=%g PAR=%g/%g es=%g cs=%g qLs=%g Vcmax=%g stress=%g m=%g outputs=%g/%g\n",
            leafBios[0].Type, voxelTempes[0].sunlit, voxelPnets[0].directPnet,
            voxelPnets[0].diffusePnet, voxelAirs[0].es, voxelAirs[0].cs, leafBios[0].qLs,
            leafBios[0].Vcmax, leafBios[0].stressfactor, leafBios[0].leafM,
            carbon.GPPsunlit, carbon.GPPshaded);
    require(std::isfinite(carbon.GPPsunlit) && std::isfinite(carbon.GPPshaded) &&
        std::isfinite(carbon.NPPsunlit) && std::isfinite(carbon.NPPshaded), "carbon output is nonfinite");
    require(carbon.GPPsunlit >= 0 && carbon.GPPshaded >= 0, "gross assimilation is negative");
    require(std::isfinite(voxelRsss[0].sunlit) && voxelRsss[0].sunlit >= 0 &&
        std::isfinite(voxelRsss[0].shaded) && voxelRsss[0].shaded >= 0, "resistance is invalid");
    require(std::isfinite(voxelAirs[0].ci) && voxelAirs[0].ci >= 0, "intercellular CO2 is invalid");
}
static void missingResult() {
    require(std::isnan(voxelHeatFlux[0].GPPsunlit) && std::isnan(voxelHeatFlux[0].GPPshaded) &&
        std::isnan(voxelHeatFlux[0].NPPsunlit) && std::isnan(voxelHeatFlux[0].NPPshaded) &&
        std::isnan(voxelRsss[0].sunlit) && std::isnan(voxelRsss[0].shaded),
        "invalid input must remain an explicit missing result");
}

int main() {
    require(near(biochemicalElectronTransport(100, 200, 0), 20000.0 / 300.0), "theta=0 is not the exact linear limit");
    require(near(biochemicalElectronTransport(100, 200, 1e-10f), 20000.0 / 300.0), "theta->0 is discontinuous");
    require(near(biochemicalElectronTransport(1e-12f, 200, .7f), 1e-12, 1e-5, 1e-18), "weak light cancelled to zero");
    require(biochemicalElectronTransport(0, 200, .7f) == 0, "night transports electrons");
    require(near(biochemicalElectronTransport(1e30f, 1e30f, .7f), 1e30 * 2.0 / (2.0 + std::sqrt(1.2)), 5e-6, 0), "large excitation overflowed");
    require(near(biochemicalQuadraticRoot(1, -1e8f, 1, -1), 1e-8, 1e-5, 1e-14), "small quadratic root cancelled");
    require(near(biochemicalQuadraticRoot(0, 2, -1, -1), .5), "linear limit is wrong");
    require(near(biochemicalQuadraticRoot(1e-30f, 2, -1, 1), .5), "nearly linear root is unstable");
    require(std::isnan(biochemicalQuadraticRoot(1, 0, 1, -1)), "negative discriminant was fabricated into a root");
    require(std::isnan(biochemicalQuadraticRoot(0, 0, 0, -1)), "indeterminate equation was fabricated into a root");

    double maximumBaselineRelativeDifference = 0;
    for(const auto& example : normalDomainBaseline) {
        reset(example.type); voxelTempes[0].sunlit = voxelTempes[0].shaded = 298.15f;
        voxelAirs[0].es = es_fun(25) * example.rh; ubom.meteo.ea = voxelAirs[0].es;
        voxelAirs[0].cs = example.co2;
        voxelPnets[0] = {example.directPar, example.directPar * .2f};
        if(example.method == 0) productionCollatz(); else productionDispatch();
        finiteResult();
        const auto& carbon = voxelHeatFlux[0]; const auto& resistance = voxelRsss[0];
        const float actual[] = {carbon.GPPsunlit, carbon.GPPshaded, carbon.NPPsunlit,
            carbon.NPPshaded, resistance.sunlit, resistance.shaded, voxelAirs[0].ci};
        for(unsigned field = 0; field < 7; ++field) {
            const double relative = std::fabs(actual[field] - example.expected[field]) /
                std::max(1e-4, double(std::fabs(example.expected[field])));
            maximumBaselineRelativeDifference = std::max(maximumBaselineRelativeDifference, relative);
            require(near(actual[field], example.expected[field], 2e-4, 2e-5),
                "normal-domain physical output changed beyond float32 root tolerance");
        }
    }

    reset(); productionDispatch(); finiteResult();
    const float daylightC3 = voxelHeatFlux[0].GPPsunlit;
    reset(4); productionDispatch(); finiteResult();
    const float daylightC4 = voxelHeatFlux[0].GPPsunlit;
    require(daylightC3 > 1 && daylightC4 > 1, "daylight photosynthesis is unexpectedly weak");
    unsigned supportCases = 0;
    for(float type : {3.0f, 4.0f}) for(float temperature : {-40.0f, -10.0f, 0.0f, 25.0f, 45.0f})
    for(float light : {0.0f, 1e-8f, .001f, 1.0f, 200.0f, 2000.0f, 1e6f})
    for(float rh : {0.0f, 1e-6f, .01f, .5f, 1.0f, 1.2f})
    for(float co2 : {0.0f, 1e-6f, 1.0f, 400.0f, 1000.0f}) {
        reset(type);
        voxelTempes[0].sunlit = voxelTempes[0].shaded = temperature;
        voxelPnets[0].directPnet = light; voxelPnets[0].diffusePnet = light * .2f;
        voxelAirs[0].es = es_fun(temperature) * rh; voxelAirs[0].cs = co2;
        productionDispatch(); finiteResult(); ++supportCases;
        if(light == 0 || rh == 0 || co2 == 0) {
            require(voxelHeatFlux[0].GPPsunlit == 0 && voxelHeatFlux[0].GPPshaded == 0,
                "closed stomata/night produced gross assimilation");
            require(voxelHeatFlux[0].NPPsunlit <= 0 && voxelRsss[0].sunlit == 625000,
                "respiration/closed resistance limit is wrong");
        }
    }
    for(float type : {3.0f, 4.0f}) for(float qLs : {0.0f, 1e-8f, .1f, 1.0f})
    for(float capacity : {0.0f, .001f, 80.0f, 300.0f}) for(float stress : {0.0f, 1e-6f, .5f, 1.0f})
    for(float slope : {0.0f, 1e-6f, 4.0f, 9.0f}) {
        reset(type); leafBios[0].qLs = qLs; leafBios[0].Vcmax = capacity;
        leafBios[0].stressfactor = stress; leafBios[0].leafM = slope;
        productionDispatch(); finiteResult(); ++supportCases;
        if(qLs == 0 || capacity == 0 || stress == 0 || slope == 0)
            require(voxelHeatFlux[0].GPPsunlit == 0 && voxelHeatFlux[0].GPPshaded == 0,
                "zero capacity/stress generated carbon");
    }
    reset(); voxelPnets[0].directPnet = 0; voxelPnets[0].diffusePnet = 600;
    productionDispatch(); finiteResult();
    require(near(voxelHeatFlux[0].GPPsunlit, voxelHeatFlux[0].GPPshaded) &&
        voxelHeatFlux[0].GPPshaded > 1, "pure diffuse illumination is not used consistently");
    reset(); voxelPnets[0].diffusePnet = 0; productionDispatch(); finiteResult();
    require(voxelHeatFlux[0].GPPsunlit > 1 && voxelHeatFlux[0].GPPshaded == 0,
        "direct-only light reached shaded leaves");
    reset(); leafBios[0].kV = 0; productionDispatch();
    const float homogeneousCapacity = voxelHeatFlux[0].GPPsunlit;
    reset(); leafBios[0].kV = 5; productionDispatch();
    require(voxelHeatFlux[0].GPPsunlit < homogeneousCapacity, "material kV was ignored");
    reset(); voxelTempes[0].sunlit = 25; voxelTempes[0].shaded = 298.15f;
    voxelPnets[0].directPnet = 0; productionDispatch(); finiteResult();
    require(near(voxelHeatFlux[0].GPPsunlit, voxelHeatFlux[0].GPPshaded), "mixed Kelvin/Celsius temperatures were coupled");

    unsigned invalidCases = 0;
    for(int invalid = 0; invalid < 14; ++invalid) {
        reset();
        switch(invalid) {
            case 0: leafBios[0].qLs = -1; break;
            case 1: leafBios[0].qLs = 2; break;
            case 2: leafBios[0].kNPQs = -1; break;
            case 3: leafBios[0].Vcmax = -1; break;
            case 4: leafBios[0].Rdparam = -1; break;
            case 5: leafBios[0].beta = 2; break;
            case 6: leafBios[0].leafM = -1; break;
            case 7: leafBios[0].stressfactor = -1; break;
            case 8: leafBios[0].Type = 5; break;
            case 9: canopies[0].height = 0; break;
            case 10: voxelAirs[0].cs = -1; break;
            case 11: voxelAirs[0].es = -1; break;
            case 12: ubom.meteo.p = 0; break;
            case 13: leafBios[0].Tyear = std::numeric_limits<float>::quiet_NaN(); break;
        }
        productionDispatch(); missingResult(); ++invalidCases;
    }
    // A genuinely non-real C4 carboxylation equation must remain missing,
    // rather than clamping its strongly negative discriminant to zero.
    reset(4); voxelTempes[0].sunlit = voxelTempes[0].shaded = 65;
    voxelAirs[0].cs = 1e-6f; voxelAirs[0].es = es_fun(65) * 1e-6f;
    voxelPnets[0].directPnet = 1e-8f; voxelPnets[0].diffusePnet = 2e-9f;
    productionDispatch(); missingResult(); ++invalidCases;
    reset(); leafBios[0].kNPQs = 1e38f; voxelPnets[0] = {0, 0};
    productionDispatch(); missingResult(); ++invalidCases;
    reset(); voxelPnets[0].directPnet = -1; // The sum is still positive.
    productionDispatch(); missingResult(); ++invalidCases;
    reset(); voxelPnets[0].diffusePnet = std::numeric_limits<float>::infinity();
    productionDispatch(); missingResult(); ++invalidCases;
    for(float pressure : {3e38f, 1e-38f}) {
        reset(); ubom.meteo.p = pressure;
        productionDispatch(); missingResult(); ++invalidCases;
    }

    reset(); productionCollatz(); finiteResult();
    const float collatzC3 = voxelHeatFlux[0].GPPsunlit;
    reset(4); productionCollatz(); finiteResult();
    const float collatzC4 = voxelHeatFlux[0].GPPsunlit;
    require(collatzC3 > 1 && collatzC4 > 1, "Collatz daylight input scale is wrong");
    unsigned collatzCases = 0;
    for(float type : {3.0f, 4.0f}) for(float temperature : {-40.0f, -10.0f, 0.0f, 25.0f, 45.0f, 65.0f})
    for(float light : {0.0f, 1e-8f, .001f, 1.0f, 200.0f, 2000.0f, 1e6f})
    for(float rh : {0.0f, 1e-6f, .01f, .5f, 1.0f, 1.2f})
    for(float co2 : {0.0f, 1e-6f, 1.0f, 400.0f, 1000.0f}) {
        reset(type); leafBios[0].Tcor = 1;
        voxelTempes[0].sunlit = voxelTempes[0].shaded = temperature;
        voxelPnets[0].directPnet = light; voxelPnets[0].diffusePnet = light * .2f;
        ubom.meteo.ea = es_fun(temperature) * rh; voxelAirs[0].cs = co2;
        productionCollatz(); finiteResult(); ++collatzCases;
        if(light == 0 || rh == 0 || co2 == 0) {
            require(voxelHeatFlux[0].GPPsunlit == 0 && voxelHeatFlux[0].GPPshaded == 0,
                "Collatz closed stomata/night produced gross assimilation");
            require(voxelHeatFlux[0].NPPsunlit <= 0 && voxelRsss[0].sunlit == 625000,
                "Collatz respiration/closed resistance limit is wrong");
        }
    }
    for(float type : {3.0f, 4.0f}) for(float capacity : {0.0f, .001f, 80.0f, 300.0f})
    for(float stress : {0.0f, 1e-6f, .5f, 1.0f}) for(float slope : {0.0f, 1e-6f, 4.0f, 9.0f}) {
        reset(type); leafBios[0].Vcmax = capacity;
        leafBios[0].stressfactor = stress; leafBios[0].leafM = slope;
        productionCollatz(); finiteResult(); ++collatzCases;
        if(capacity == 0 || stress == 0 || slope == 0)
            require(voxelHeatFlux[0].GPPsunlit == 0 && voxelHeatFlux[0].GPPshaded == 0,
                "Collatz zero capacity/stress generated carbon");
    }
    for(int invalid = 0; invalid < 14; ++invalid) {
        reset();
        switch(invalid) {
            case 0: leafBios[0].Tcor = 2; break;
            case 1: leafBios[0].Tparam[0] = -1; break;
            case 2: leafBios[0].Tparam[3] = leafBios[0].Tparam[2]; break;
            case 3: leafBios[0].Vcmax = -1; break;
            case 4: leafBios[0].Rdparam = -1; break;
            case 5: leafBios[0].kV = -1; break;
            case 6: leafBios[0].leafM = -1; break;
            case 7: leafBios[0].stressfactor = -1; break;
            case 8: leafBios[0].Type = 3.5f; break;
            case 9: canopies[0].height = 0; break;
            case 10: voxelAirs[0].cs = -1; break;
            case 11: ubom.meteo.ea = -1; break;
            case 12: ubom.meteo.p = 0; break;
            case 13: leafBios[0].Tparam[4] = std::numeric_limits<float>::quiet_NaN(); break;
        }
        productionCollatz(); missingResult(); ++invalidCases;
    }
    reset(); voxelPnets[0].directPnet = -1;
    productionCollatz(); missingResult(); ++invalidCases;
    reset(); voxelPnets[0].diffusePnet = std::numeric_limits<float>::infinity();
    productionCollatz(); missingResult(); ++invalidCases;
    for(float pressure : {3e38f, 1e-38f}) {
        reset(); ubom.meteo.p = pressure;
        productionCollatz(); missingResult(); ++invalidCases;
    }
    reset(); voxelPnets[0].directPnet = 0; voxelPnets[0].diffusePnet = 600;
    productionCollatz(); finiteResult();
    require(near(voxelHeatFlux[0].GPPsunlit, voxelHeatFlux[0].GPPshaded) &&
        voxelHeatFlux[0].GPPshaded > 1, "Collatz pure diffuse illumination is not used");
    reset(); voxelPnets[0].diffusePnet = 0; productionCollatz(); finiteResult();
    require(voxelHeatFlux[0].GPPsunlit > 1 && voxelHeatFlux[0].GPPshaded == 0,
        "Collatz direct-only light reached shaded leaves");
    reset(); voxelTempes[0].sunlit = 25; voxelTempes[0].shaded = 298.15f;
    voxelPnets[0].directPnet = 0; productionCollatz(); finiteResult();
    require(near(voxelHeatFlux[0].GPPsunlit, voxelHeatFlux[0].GPPshaded), "Collatz mixed temperature units were coupled");
    for(unsigned type : {TYPE_SOIL, TYPE_BUILDING, TYPE_WATER}) {
        reset(); leafBios.data.clear(); meshLinks[0].type = type;
        productionDispatch();
        require(voxelHeatFlux[0].GPPsunlit == 0 && voxelHeatFlux[0].GPPshaded == 0 &&
            voxelHeatFlux[0].NPPsunlit == 0 && voxelHeatFlux[0].NPPshaded == 0,
            "nonvegetation carbon was not zeroed");
        require(voxelRsss[0].sunlit == 50, "nonvegetation material resistance is wrong");
    }
    reset(); leafBios.data.clear(); canopies[0].participatingMedium = true;
    productionDispatch();
    reset(); leafBios.data.clear(); voxelLinks[0].isValid = 0; productionDispatch();
    reset(); leafBios.data.clear(); gl_GlobalInvocationID.x = 1; productionDispatch();
    for(unsigned type : {TYPE_SOIL, TYPE_BUILDING, TYPE_WATER}) {
        reset(); leafBios.data.clear(); meshLinks[0].type = type;
        productionCollatz();
        require(voxelHeatFlux[0].GPPsunlit == 0 && voxelHeatFlux[0].GPPshaded == 0 &&
            voxelHeatFlux[0].NPPsunlit == 0 && voxelHeatFlux[0].NPPshaded == 0,
            "Collatz nonvegetation carbon was not zeroed");
        require(voxelRsss[0].sunlit == 50, "Collatz nonvegetation resistance is wrong");
    }
    reset(); leafBios.data.clear(); canopies[0].participatingMedium = true; productionCollatz();
    reset(); leafBios.data.clear(); voxelLinks[0].isValid = 0; productionCollatz();
    reset(); leafBios.data.clear(); gl_GlobalInvocationID.x = 1; productionCollatz();
    std::printf("{\"farquhar_c3_gpp\":%.9g,\"farquhar_c4_gpp\":%.9g,\"collatz_c3_gpp\":%.9g,\"collatz_c4_gpp\":%.9g,\"farquhar_support_cases\":%u,\"collatz_support_cases\":%u,\"invalid_cases\":%u,\"normal_domain_baseline_cases\":32,\"baseline_max_relative_difference\":%.9g,\"checked_material_dispatch\":true,\"production_glsl_float32\":true}\n",
        daylightC3, daylightC4, collatzC3, collatzC4, supportCases, collatzCases, invalidCases,
        maximumBaselineRelativeDifference);
}
