//
// Created by admin on 2024/1/25.
// For the kernel struct used both in the cpu and gpu part;
//

#ifndef FIELD_STRUCTS_CG_H
#define FIELD_STRUCTS_CG_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>
#include <map>

#include "nvvk/resourceallocator_vk.hpp"




struct alignas(16) SensorMatrix
{
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    float focalDist;
    float aperture;
    int n_wave{ 0 };
    int empty_{ 0 };

    glm::vec3 direction;
};

struct alignas(16) LightSet
{
    glm::vec3 direction{ 100, 100, 0 };
    float direct{ 0.9 };
    float diffuse{ 0.1 };
    float Rin{600};
    float Rli{450};
    float skyTemperature{ 150.1 };
    float solarTemperature{ 6000.0 };

};

struct  Spectral{
    float reflectance;
    float transmittance;
};

struct Thermal{
    float sunlitTemperature;
    float shadedTemperature;
};

struct alignas(16) RayRTSetting
{
    int frame{ 0 };
    int maxDepth{ 20 };
    int n_sample{ 32 };//64
    int n_wave{ 3 };

    glm::ivec2 imageSize{1000, 1000 }; // ->Sensor.resolution
    //nvmath::vec2f sceneSize_XYZ{ 100.0, 100.0 };
    int band1{ 1 };
    int band2{ 2 };
    int band3{ 3 };
    int isDisplay{ 0 };
    int isTemperature{ 0 };
    float fireflyClampThreshold{ 0 };
    int debugging_mode{ 0 };
    int periodicNeighborCount{ 0 };
    int skyboxEnabled{ 0 };
    int imageOffsetY{ 0 };
    glm::vec2 sceneSize{ 1.0f, 1.0f };
};


struct alignas(32)  VoxelLstSetting
{

    int frame{0};
    int maxIteration{3};
    int maxDepth{32};
    int maxStep{64};
    int n_sample{16};
    int n_wave{5};
    int n_jump{10};
    float scale{1};
    glm::ivec2 imageSize{100, 100};  // imageSize and VoxelSize do not put together
    int isDisplay{0};
    int isface{80};
    glm::ivec3 voxelSize{50, 0, 50};
    int islad{0};
    int voxelCount{0};
    int periodicNeighborCount{0};
    int skyboxEnabled{0};
};

struct alignas(32)  VoxelRTSetting
{

    int frame{0};
    int maxIteration{3};
    int maxDepth{16};
    int maxStep{64};
    int n_sample{16};
    int n_wave{5};
    int n_jump{10};
    float scale{1};
    glm::ivec2 imageSize{100, 100};  // imageSize and VoxelSize do not put together
    int isDisplay{0};
    int isface{80};
    glm::ivec3 voxelSize{50, 0, 50};
    int islad{0};
    int isTemperature{0};
    int voxelCount{0};
    int spectralBatchSize{1};
    int periodicNeighborCount{0};
    int skyboxEnabled{0};
};

// atmospheric radiative transfer linked to modtrain
struct AtomCond
{
    float wavelength{0};
    float direct{0};
    float diffuse{0};
};

// aerodynamic condition for resistance
struct AeroCond
{
    int type;  // 0 for vegtation, 1 for buildup
    float L;       // this is used for updating
    float ustar;   // this is used for updating
    float hc_veg;  // effective height
    float hc_build; // effective height
    float lai; // this is only used for the aeroy
    float leafwidth;
    float cover; // ???
};

struct Canopy
{
    float lai;  // this is used for the aerodynamic resistance inside a system
    float density; // gap property
    float height;  // this is used for the aerodynamic resistance inside a system
    float width;  // ???
    float G;      // gap property
    float LIDFa;  // For dynamic G
    float LIDFb;  // For dynamic G
    float hspot;      // for the hotspot
    float leafwidth; // for the aerodynamic resistance
    int structureType{0}; // 0: canopy, 1: rigid, 2: fire/smoke, 3: fog
    float extinction{0.0f};
    float scatteringAlbedo{0.0f};
    float asymmetry{0.0f};
    float emissionScale{0.0f};
    float fixedTemperature{0.0f};
};


struct InstanceLink
{
    uint32_t meshId;
};

struct  Instance
{
    int meshId;
    glm::mat4 object2worldMatrix;
    glm::mat4 world2objectMatrix;
};

struct VertexAttribute
{
    glm::vec3 pos;
    glm::vec3 nrm;
    glm::vec3 color;
    glm::vec2 texCoord;
};

///-----------------------------------------------------
/// Mesh seems like a geometry in a geometric optical model
///-----------------------------------------------------

struct MeshBuffer
{
    uint32_t nbIndices{ 0 };
    uint32_t nbVertices{ 0 };
    nvvk::Buffer   vertexBuffer;
    nvvk::Buffer   indexBuffer;
};

struct MeshLink
{
    int type;
    int spectralId;
    int thermalId;
    int canopyId;
    int bioId;
    float angularEffectStrength{0.0f};

    uint64_t vertexAddress;
    uint64_t indexAddress;
};

// voxel link
struct alignas(16) VoxelLink
{
    glm::ivec3 voxelId{0, 0, 0};
    int instanceId{0};
    int aeroId{0}; // this is used for a whole image or a aero divided image;
    int faceId{0}; //
    int isValid{0}; // is 0 go pass
    int empty_{0};// which faces ? 0 center 1, up, 2 bottom, 3 left, 4 right, 5, forward, 6 backward
    // now only 0 and 1 was used for the veg and soil, respectively;
    int hexId{-1}; // 异质性体元参数索引；-1 保持均匀体元行为
};

struct VoxelHex
{
    float ax{1.0f};
    float ay{1.0f};
    float az{1.0f};
    float rho{0.0f};
};

static_assert(sizeof(VoxelLink) == 48, "VoxelLink must match GLSL std430 stride");
static_assert(sizeof(VoxelHex) == 16, "VoxelHex must match GLSL std430 stride");


struct VoxelDir
{
    float solar;
    float solar_last;
};

struct VoxelRad
{
    float cumulated;
    float induced;
    //float diffuse;
};

struct VoxelTempe
{
    float sunlit;
    float shaded;
};

struct VoxelNetRad
{
    float directVrad;   // direct radiance at VNIR bands
    float diffuseVrad;  // diffuse radiance at VNIR bands
    float directTrad;   // direct radiance at TIR bands
    float diffuseTrad;  // diffuse radiance at TIR bands
};

struct VoxelPnet
{
    float directPnet;
    float diffusePnet;
};

// aerodynamic resistance
struct VoxelRaa
{
    float raa;  // aerodynamic resistance
};




// surface resistance
struct VoxelRss
{
    float sunlit;   // surface resistance at sunlit surface in this voxel
    float shaded;   // surface resistance at shaded surface in this voxel
};

// Air density
struct VoxelAir
{
    float cs;       // Carbon surface
    float ci;       // Carbon inside
    float es;       // Water Surface
};

// Rn - H - LE - G = 0
struct VoxelHeatflux
{
    float Hsunlit;  // sensible heat flux of sunlit component
    float Hshaded;  // sensible heat flux of shaded component
    float LEsunlit; // latent heat flux of sunlit component
    float LEshaded; // latent heat flux of shaded component
    float Gsunlit;  // change in heat storage of sunlit component
    float Gshaded;  // change in heat storage of shaded component
    float GPPsunlit;  // gross leaf photosynthesis, umol CO2 m-2 leaf s-1
    float GPPshaded;
    float NPPsunlit;  // net leaf assimilation (GPP - dark respiration)
    float NPPshaded;
};

// temperature at LAST time nodes
struct TLAST
{
    float sunlit;   // sunlit temperature
    float shaded;   // shaded temperature
};

struct EBState
{
    uint32_t count;
};

struct alignas(16) FluidCellMeta
{
    int surfaceIndex{-1};
    int kind{0}; // 0 air, 1 solid, 2 vegetation, 3 fire, 4 fog
    float lad{0.0f};
    float padding{0.0f};
};

struct alignas(16) FluidParameters
{
    glm::ivec4 grid{1, 1, 1, 0};
    glm::vec4 spacingTime{1.0f, 1.0f, 1.0f, 5.0f}; // xyz spacing, w scalar dt
    glm::vec4 ambientWind{0.0f, 0.0f, 0.0f, 298.15f};
    glm::vec4 physics{0.033f, 0.3f, 1.0f, 0.1f};
    glm::vec4 sources{0.02f, 0.12f, 30.0f, 1.0f}; // smoke, lattice cap, speed cap, velocity scale
    glm::vec4 couplingSpacing{1.0f, 1.0f, 1.0f, 0.0f};
};


struct LeafBio
{
    float Vcmax;		// maximum carboxylation capacity (at optimum temperature)
    float m;			// ball-berry stomatal conductance parameter
    float BallBerry;
    float Type;
    float kV;			// extinction coefficient for a vertical profile
    float Rdparam;		// parameter for dark respiration
    float Tparam[5];
    float Tyear;
    float beta;
    float kNPQs;		// rate constant of sustained thermal dissipation
    float qLs;			// fraction of functional reaction voxelIds
    float stressfactor;
    int Tcor;
};

struct SoilSet
{
    int method;
    float rss;			// soil resistance for evaporation
    float cs;			// volumetric heat capacity of the soil
    float rhos;         // specific mass of the soil
    float lambdas;       //  heat conductivity of the soil
    float Tsoil;        // aeverage temperature 25
    float smc;			// volumetric soil moisture content 0.25
    float SatWater;
    int brdfModel{0};             // 0: Lambert, 1: Hapke
    float hapkeB0{1.0f};          // opposition-effect amplitude
    float hapkeH{0.1f};           // opposition-effect angular width
    float hapkeG{0.0f};           // Henyey-Greenstein asymmetry
    int thermalClass{0};          // 0: ground/building surface, 1: woody solid
    float convectiveScale{1.0f};  // small branches exchange heat faster than broad surfaces
    //BSMParam bsm;
};

// Open-water energy balance parameters.
struct WaterSet
{
    float rss;                    // surface resistance, normally 0 s/m
    float heatCapacity;           // volumetric heat capacity, J/(m3 K)
    float mixingDepth;            // effective mixed-layer depth, m
    float evaporationCoefficient; // 0..1 multiplier for latent heat flux
    int brdfModel{0};             // 0: Lambert, 1: Fresnel-Cox-Munk
    float refractiveIndex{1.333f};
    float slopeVariance{0.0f};     // <=0: derive from meteorological wind
    float diffuseFraction{0.02f};  // residual subsurface/Lambert component
};

struct BuildUp
{
    int method;
    float rss;
    float cs;          // 1180
    float rhos;       // 1900 -2600
    float lambdas;    // 1.0 -1.7
    float Tin;
};

struct Meteo
{
    float z;
    float t;		// time node
    float u;		// wind speed
    float p;		// air pressure
    float Rin;		// incoming shortwave radiation
    float Rli;		// incoming longwave radiation
    float Ta;		// air temperature
    float sm;		// volumetric soil moisture content
    float ea;		// atmospheric vapor pressure
    float Ca;		// CO2 concentration in the air
    float Oa;		// O2 concentration in the air
    float dTime {600};
};



#endif //FIELD_STRUCTS_CG_H
