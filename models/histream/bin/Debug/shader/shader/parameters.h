#ifndef STRUCTURE
#define STRUCTURE 1

// #ifdef __cplusplus
// #include <stdint.h>
// #include "nvmath/nvmath.h"
// // GLSL Type
// using ivec2 = nvmath::vec2i;
// using vec2  = nvmath::vec2f;
// using vec3  = nvmath::vec3f;
// using vec4  = nvmath::vec4f;
// using mat4  = nvmath::mat4f;
// using uint  = unsigned int;
// #endif

#include "global.glsl"

//////////////////////////////////////////////////////////////
//   Parameters used in Input
//////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------
// Entity /Component Property
//--------------------------------------------------------------------------------------------

// Wavelength link to sunlight source and camera metric as well as optical material


// reflectance and transmittance
struct SpectralMaterial
{
	float reflectance;
	float transmittance;
};
// sunlit and shaded temperature
struct ThermalMaterial
{
	float sunlitTemperature;
	float shadedTemperature;
};

// canopy information
struct Canopy
{
	float lai;
	float density;
	float height;
	float width;
	float G;
	float LIDFa;
	float LIDFb;
	float hspot;
	float leafwidth;
};

// sphere instance defination
struct Sphere
{
  vec3  center;
  float radius;
};

// aabb instance defination
struct AllAabb
{
  vec3 minimum;
  int empty;
  vec3 maximum;
};

struct hitValue
{
	float id;
};

//--------------------------------------------------------------------------------------------
// Entity / Component
//--------------------------------------------------------------------------------------------

struct VertexAttribute
{
	vec3 position;
	vec3 NRM;
	vec3 color;
	vec2 txt;
};

struct SensorMatrix
{
	mat4 viewInverse;
	mat4 projInverse;
	float focalDist;
	float aperture;
	int n_wave;
	int empty;
	vec3 direction;
};


struct LightSet
{
	vec3  direction;
	float direct;
	float diffuse;
	float Rin;
	float Rli;
	float skyTemperature;
	float solarTemperature;
};

struct AtomCond
{
	float wavelength;
	float direct;
	float diffuse;
};

// obj model link
struct MeshLink
{
	int type;
	int spectralId;
	int thermalId;
	int canopyId;
	int bioId;
    // int leafbioId;
    // int soilsetId;
	uint64_t vertexAddress;
	uint64_t indexAddress;
};

// obj instance link
struct InstanceLink
{
	int meshId;
};


// voxel link
struct VoxelLink
{
    ivec3 voxelId;
    int instanceId;
	int aeroId;
	int faceId;
	int isValid;
	int empty_;
};

// transmittance
struct VoxelDir
{
    float solar;
    float solar_last;
};

struct VoxelTempe
{
    float sunlit;
    float shaded;
};

// radiance
struct VoxelRad
{
    float cumulated;
    float induced;
};

// net radiance
struct VoxelNetRad
{
    float directVrad;
    float diffuseVrad;
    float directTrad;
    float diffuseTrad;
};

struct VoxelPnet
{
    float directPnet;
    float diffusePnet;
};


struct VoxelAir
{
    float cs;       // Carbon surface
    float ci;       // Carbon inside
    float es;       // Water Surface
};

struct VoxelRaa
{
	float raa;
};

struct VoxelRss
{
    float sunlit;   // surface resistance at sunlit surface in this voxel
    float shaded;   // surface resistance at shaded surface in this voxel
};

struct AeroCond
{
	int type;
	float L;
	float ustar;
    float hc_veg;
    float hc_build;
    float lai;
	float leafwidth;
    float cover;
};

// Rn - H - LE - G = 0. For water, G stores mixed-layer heat storage Qw.
struct VoxelHeatFlux
{
    float Hsunlit;  // sensible heat flux of sunlit component
    float Hshaded;  // sensible heat flux of shaded component
    float LEsunlit; // latent heat flux of sunlit component
    float LEshaded; // latent heat flux of shaded component
    float Gsunlit;  // change in heat storage of sunlit component
    float Gshaded;  // change in heat storage of shaded component
};

struct VoxelTlast
{
	float sunlit;
	float shaded;
};

struct EBState
{
	int count;
};




struct LeafBio
{
    float Vcmax;		// maximum carboxylation capacity (at optimum temperature)
    float leafM;			// ball-berry stomatal conductance parameter
    float BallBerry;
    float Type;
    float kV;			// extinction coefficient for a vertical profile
    float Rdparam;		// parameter for dark respiration
    float Tparam[5];
    float Tyear;
    float beta;
    float kNPQs;		// rate constant of sustained thermal dissipation
    float qLs;			// fraction of functional reaction centers
    float stressfactor;
    int Tcor;

    //FluspectParam fp;
};

struct SoilSet
{
    int method;
    float rss;			// soil resistance for evaporation
    float cs;			// volumetric heat capacity of the soil
    float rhos;
    float lambdas;
	float Tsoil;  // aeverage temperature 25
    float SMC;			// volumetric soil moisture content 0.25
//    float csSoil;
  //  float rbs;      // boundary prop

    float SatWater;
    int brdfModel;
    float hapkeB0;
    float hapkeH;
    float hapkeG;

    //BSMParam bsm;
};

struct WaterSet
{
    float rss;
    float heatCapacity;
    float mixingDepth;
    float evaporationCoefficient;
    int brdfModel;
    float refractiveIndex;
    float slopeVariance;
    float diffuseFraction;
};


struct Meteo
{
	float z;
	float t;
	float u;
	float p;
	float Rin;
	float Rli;
	float Ta;
	float sm;
	float ea;
	float Ca;
	float Oa;
	float dtime;
};




#endif
