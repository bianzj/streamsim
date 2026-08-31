//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_STRUCTS_H
#define FIELD_RADIOSITY_STRUCTS_H

#include<iostream>
#include<vector>
#include<map>
#include<fstream>
#include <algorithm>
#include<string>
#include <memory>
#include "../thirdparty/magic_enum.hpp"
#include "../thirdparty/tiny_obj_loader.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

#define MXPIC 1024
#define MYPIC 1024
#define NSKY 40//
#define PI 3.141592653
#define RD 0.017453292

#define VNIRMAXBAND 2001
#define TIRMAXBAND 2162
#define LSTMAXBAND 2162

#define GROUP_SIZEXY 8 // Same group size as in compute shader
#define GROUP_SIZEX 64
#define MAX_FRAMES_IN_FLIGHT 10
#define TLASTNUM 8
#define SENSOR_HEIGHT 3000
#define SENSOR_FOV    0.5
#define ANGLE_COR 0.1
#define DEG2RAD  0.017453292
#define DIFFUSENUM 32
#define N1 2001
#define N2 161
#define NJ 1000
#define NB 10
#define NITE 100
#define RSS 50
#define WL 10.5

#define N1 2001
#define N2 161
#define RHOA 1.2047      // specific mass of air
#define AIRCP   1004        // specific heat of dry air
#define KAPPA 0.4        // Von Karman constant
#define GEARTH  9.81     // gravity acceleration
#define AHC 119.7117122  //
#define MAIR 28.96       // molecular mass of dry air
#define RGAS 8.31           // Molar gas constant
#define MH20 18          // molecular mass of water
#define MCO2 44          // Molecular mass of carbon dioxide
#define SIGMASB 5.67e-8   // stefan boltzman constant
#define STRESS 1         //
#define C2K    273.15    // melting point of water
#define KV 0.6396        //
#define GG 0.5       // leaf projection
#define CI 1.0      // clumping index
#define RAD_THRESHOLD 5    // radiance threshold
#define TMIN_THRESHOLD 220  // -50 C
#define TMAX_THRESHOLD 350  // +80 C
#define N_ITER 50
#define N_NODE 50 // pacth for each solution 48 for one day
#define RSS 50
#define WL 10.5
#define NBAD 50

enum IGBP{
    water,
    evergreen_needleleaf_forest,
    evergreen_broadleaf_forest,
    deciduous_needleleaf_forest,
    deciduous_broadleaf_forest,
    mixed_forest,
    closed_shrublands,
    open_shrublands,
    woody_savannas,
    savannas,
    grasslands,
    permanent_watlands,
    croplands,
    urban_and_builtup,
    cropland_vegetation_mosaic,
    snow_and_ice,
    barren_sparsely_vegetated,
    unclassified,
    fill_value
};

enum LIDF{
    planophile,
    erectrophile,
    plagiophile,
    extremophile,
    spherical,
    uniform
};


struct  Spectral{
    float reflectance;
    float transmittance;
};

struct Thermal{
    float sunlitTemperature;
    float shadedTemperature;
};

enum class Projection
{
    PARALLAL,
    PERSPECTIVE
};

enum class spectralType
{
    CUSTOM,
    LEAFBIO,
    SOILSET,
    BUILDUP,
    OTHER
};

enum class Type
{
    NO,
    SOIL,
    VEGETATION,
    BUILDING
};

struct Canopy{
    float lai;
    float density;
    float height;
    float width;
    float Gleaf;
    float LIDFa;
    float LIDFb;
    float hspot;
    float leafwidth;
    int type;
    int dist; // 0 barrain, 1 veg, 2 crop, 3 forest, 4 urban
};

struct AtomCoeff
{
    float wl[N1+N2];
    float fesky[N1+N2];
    float fesun[N1+N2];
};


struct MeshLink
{
    int type;
    int spectralId;
    int thermalId;
    int canopyId;
    // int leafbioId;
    // int soilsetId;
    int bioId;
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


enum Mode{
    eRadiosity,
    eRadiosityEB,
};


struct FluspectParam
{
    float Cab;
    float Cw;
    float Cdm;
    float Cs;
    float N;
    float refl_tir;
    float trans_tir;
};

struct BSMParam
{
    float SMC;
    float BSMBrightness;
    float BSMlat;
    float BSMlon;
    float refl_tir;
    float trans_tir;
};

struct LeafBio
{
    float Vcmax;		// maximum carboxylation capacity (at optimum temperature)
    float m;			// ball-berry stomatal conductance parameter "m"
    float BallBerry;   // "b"
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

    FluspectParam fp;
};

struct SoilSet
{
    int method;
    float rss;			// soil resistance for evaporation
    float cs;			// volumetric heat capacity of the soil
    float rhos;
    float lambdas;
    float SMC;			// volumetric soil moisture content
    float csSoil;
    float rbs;
    float Tsoil;
    float satwater;

    BSMParam bsm;
};

struct FixedSpectral{

    float Refl_[N1];
    float Tran_[N1];
    float Refl_ir;
    float Tran_ir;
};

struct MeteoMeta
{
//    float t;
    float z;
    float u = 2.0f;  // default fixed wind speed (m/s)
    float Ta;
    float ea;
    float p;
    float Oa;
    float ca;
    float sm;
    float Rin;
    float Rli;
    float Tsold;
    float SatWater;
    float dTime;

    int startNode;
    int endNode;

};

struct SceneScale{
    float a;
    float b;
    float c;
    float d;
    float delx;
    float dely;
};

struct Angle{
    float sza;
    float saa;
    float vza;
    float vaa;
};



struct FacetLink{
    int typeId;
    int spectralId;
    int thermalId;
    int canopyId;
};

struct SettingXml
{
    bool isInfinite{false};
};

struct Background
{
    std::string spectralName;
    std::string thermalName;
    //std::string AeroCond;
    //spectralType bgSpectralType;
    std::string bgPropName;
    Type type;
    //  std::string bgAeroName;

    // for dem option
    bool isDEM;
    std::string DEMPath;
    glm::vec2 demResolution;   // to DEM resolution if(-1) change nothing;
};



// struct ObjEntity
// {
//     std::string objName;
//     std::string filePath;
//     std::vector<std::string> meshNames;
//     std::vector<std::string> spectralNames;
//     std::vector<std::string> thermalNames;
// //    std::vector<std::string> propNames;
// //    std::vector<std::string> canopyNames;
// //    std::vector<Type>      types;
// //    std::map<std::string, std::string> spectralNames; // { meshName: attributes }
// //    std::map<std::string, std::string> thermalNames;  // { meshName: attributes }
//     bool isLarge{false};
//     std::vector<glm::vec3> objDistributions;
//     std::vector<float> scales;
//     std::vector<float> rotations;
// };

struct ObjEntity
{
    std::string objName;
    std::string filePath;
    std::vector<std::string> meshNames;
    std::vector<std::string> spectralNames;
    std::vector<std::string> thermalNames;
    //    std::vector<std::string> propNames;
    //    std::vector<std::string> canopyNames;
    //    std::vector<Type>      types;
    //    std::map<std::string, std::string> spectralNames; // { meshName: attributes }
    //    std::map<std::string, std::string> thermalNames;  // { meshName: attributes }
    bool isLarge{false};
    std::vector<glm::vec3> objDistributions;
    std::vector<float> scales;
    std::vector<float> rotations;
    bool isdisfromFile;
    std::string distributefile;
};

// struct ObjEntityPlus
// {
//     std::string objName;
//     std::string filePath;
//     std::vector<std::string> meshNames;
//     std::vector<std::string> spectralNames;
//     std::vector<std::string> thermalNames;
//     std::vector<std::string> propNames;
//     std::vector<std::string> canopyNames;
//     std::vector<Type>      types;
// //    std::map<std::string, std::string> spectralNames; // { meshName: attributes }
// //    std::map<std::string, std::string> thermalNames;  // { meshName: attributes }
//     bool isLarge{false};
//     std::vector<glm::vec3> objDistributions;
//     std::vector<float> scales;
//     std::vector<float> rotations;
// };

struct ObjEntityPlus
{
    std::string objName;
    std::string filePath;
    std::vector<std::string> meshNames;
    std::vector<std::string> spectralNames;
    std::vector<std::string> thermalNames;
    std::vector<std::string> propNames;
    std::vector<std::string> canopyNames;
    std::vector<Type>      types;
    //    std::map<std::string, std::string> spectralNames; // { meshName: attributes }
    //    std::map<std::string, std::string> thermalNames;  // { meshName: attributes }
    bool isLarge{false};
    bool isdisfromFile;
    std::string distributefile;
    std::vector<glm::vec3> objDistributions;
    std::vector<float> scales;
    std::vector<float> rotations;
};

struct Facet
{
    int fsign;
    glm::vec3 points[3];
    float psize;
    glm::vec3 pnorm;
    glm::vec3 pcenter;

};

struct FacetRT{

    glm::vec2 radiosu[NB];
    glm::vec2 radiosh[NB];

};

struct FacetVF
{
    float xxx;  // project between the viewing and facet normal;
    glm::vec2 fsunlit;  // sunlit of two face
    glm::vec2 directvf; // fsunlit * xxx
    glm::vec2 diffusevf; //for sky proportion
    glm::vec2 pintsum;


//    int ja[NJ];
//    int jna[NJ];
    int jsum1 = 0;
    int ja1[NJ];
    int jna1[NJ];

    int jsum2 = 0;
    int ja2[NJ];
    int jna2[NJ];

};

struct FacetEB{
    glm::vec2 vnetrad;
    glm::vec2 tnetrad;
    glm::vec2 pnetrad;
    float raa;
    glm::vec2 rss;
    glm::vec2 thermals;
    glm::vec2 cs;  // ca,cs,ci co2 concentration in above canopy, leaf surface, inside leaf
    glm::vec2 es; // ea,es,ei h2o concentration in above canopy, leaf surface, inside leaf
    glm::vec2 ci;
    glm::vec2 H;
    glm::vec2 LE;
    glm::vec2 G;
    glm::vec2 Net;
};

struct OptCoeff
{
//    float * nr_, *kdm_, *kab_, *kw_, *ks_, *phiI_, *phiII_;
    std::vector<float> wl_;
    std::vector<float> nr_;
    std::vector<float> kab_;
    std::vector<float> kca_;
    std::vector<float> kdm_;
    std::vector<float> ks_;
    std::vector<float> kw_;
    std::vector<float> phiI_;
    std::vector<float> phiII_;
    std::vector<float> kcaV_;
    std::vector<float> kcaV2_;
    std::vector<float> kcaZ_;
    std::vector<float> kcant_;
    std::vector<float> gsv1_;
    std::vector<float> gsv2_;
    std::vector<float> gsv3_;
    std::vector<float> nw_;
    std::vector<float> phi_;
};



//struct FacetRT
//{
//    float radiosity[2];
//};

//struct FacetEB
//{
//    float netrad[2];
//};


struct SceneXml
{

    // scene information
    glm::vec3 sceneSize;       //
    glm::vec3 sceneOrigin;     // default : (0,0,0)
    glm::vec3 sMin{ -5,0,-5 };
    glm::vec3 sMax{ 5,5,5 };
    float stepsize_surface;


    //float stepSize;		       // DEM Sampling
    Background background;


    // Scene Component Information
    std::vector<ObjEntity> objEntities;
    std::vector<ObjEntityPlus> objEntityplus;


};

struct VertexAttribute
{
    glm::vec3 pos;
    glm::vec3 nrm;
    glm::vec3 color;
    glm::vec2 texCoord;
};


struct SensorXml
{
    std::string name;

    glm::vec2 resolution;			   // { WIDTH, HEIGHT }
    //std::vector<int> wavelengthInds;	   // int -> index; LST:6/1000; ray:0-num
    std::vector<glm::vec2> viewAngles; // { zenith, azimuth }
    std::vector<float> waves;
    bool isImage{true};
    bool isAlbedo{false};
    bool isTemperature{true};
    bool isDisplay{false};
};

struct ObjMesh
{
    int meshId;
    uint32_t nIndices;
    uint32_t nVertices;
    std::vector<VertexAttribute> vertices;
    std::vector<uint32_t>        indices;
};

struct alignas(16) SensorMatrix
{
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    float focalDist;
    float aperture;
    int empty{ 0 };
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
    float skyTemperature{ 250.1 };
    float solarTemperature{ 6000.0 };

};

//struct FluspectParam
//{
//    float Cab;
//    float Cw;
//    float Cdm;
//    float Cs;
//    float N;
////    float refl_tir;
////    float trans_tir;
//};

struct AeroCoeff
{
    float zo;
    float d;
    float Cd;		// drag coefficient for the vegetation
    float rbc;		//  for leaf boundary resistance
    float CR;		// drag coefficient for a isolated tree
    float CD1;		// fitting parameter
    float Psicor;	// roughness layer correction
    float CSSOIL;	// drag coefficient for soil
    float rbs;		// for soil boundary resistance
    float rwc;		// for Aerodynamic resistance Within Canopy
};

enum class ShapeType
{
    CUBE,
    ELLIPSOID,
    PLANE
};

//struct OptCoeff
//{
////    float * nr_, *kdm_, *kab_, *kw_, *ks_, *phiI_, *phiII_;
//    std::vector<float> nr_;
//    std::vector<float> kdm_;
//    std::vector<float> kab_;
//    std::vector<float> kw_;
//    std::vector<float> ks_;
//    std::vector<float> phiI_;
//    std::vector<float> phiII_;
//
//};

//struct OptCoeff
//{
////    float * nr_, *kdm_, *kab_, *kw_, *ks_, *phiI_, *phiII_;
//    std::vector<float> wl_;
//    std::vector<float> nr_;
//    std::vector<float> kab_;
//    std::vector<float> kca_;
//    std::vector<float> kdm_;
//    std::vector<float> ks_;
//    std::vector<float> kw_;
//    std::vector<float> phiI_;
//    std::vector<float> phiII_;
//    std::vector<float> kcaV_;
//    std::vector<float> kcaV2_;
//    std::vector<float> kcaZ_;
//    std::vector<float> kcant_;
//    std::vector<float> gsv1_;
//    std::vector<float> gsv2_;
//    std::vector<float> gsv3_;
//    std::vector<float> nw_;
//    std::vector<float> phi_;
//};


//struct BSMParam
//{
//    float SMC;
//    float BSMBrightness;
//    float BSMlat;
//    float BSMlon;
////    float refl_tir;
////    float trans_tir;
//};

struct SpectralXml
{
    std::string spectralName;
    spectralType type;
//    int n_band;
    std::vector<float> reflectances;
    std::vector<float> transmittance;
    FluspectParam fp;
    BSMParam bsm;
    std::string path;
    float tau_tir;
    float refl_tir;
};

struct ThermalXml
{
    std::string thermalName;
    float sunlitTemperature;
    float shadedTemperature;
};

struct LightXml {
    std::string name;
    glm::vec2 solarAngle;
    float direct;
    float diffuse;
    float skyTemperature{250};
    float solarTemperature{6000};
    //   float directdiffuseratio{1.0};
};

struct RadiosityXml{
    std::string projectDir;
    std::string definedDir;

    SettingXml settingxml;

    LightXml lightxml;
    SensorXml sensorxml;

    SceneXml scenexml;

    std::vector<SpectralXml> spectralxmls;
    std::vector<ThermalXml> thermalxmls;
};

//struct Canopy
//{
//    float lai;
//    float density;
//    float height;
//    float width;
//    float G;
//    float LIDFa;
//    float LIDFb;
//    float hspot; // for the hotspot
//    float leafwidth; // for the aerodynamic boundary
//};


struct CanopyXml
{
    std::string canopyName;
    Canopy canopy;
};

struct PropertyXml{
    std::string name;
    Type type;
    LeafBio leafbio;
    SoilSet soilset;
};

enum AeroType
{
    one,  // one for all
    image,  // one aerocond for the whole scene
    gridCal,   // different aerocond for each grid
};

struct AeroCond
{
    float L;
    float ustar;
    float hc_veg;
    float hc_build;
    float lai;
    float cover;
};

struct MeteoXml
{
//    float z;
//    float Tsold;
//    float satWater;
//    float dTime;
//    float Lat;
//    float Lon;
    std::string meteofile;
    std::string rinfile;
    std::string rlifile;
    AeroCond aerocond;
    MeteoMeta meta;
    //std::vector<Meteo> meteos;

};

struct AeroCondXml
{
    AeroType aerotype;
    AeroCond aerocond; // one for all
    std::string path;
    float stepsize_atmosphere;
};

struct AeroCoeffXml
{
    AeroCoeff aerocoeff;
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
    // float Tsold;	//
    // float SatWater;
    float dTime;
};

// AtomCond AtomCoef
struct AtomCond
{
    float wavelength[N1+N2];
    float direct[N1+N1];
    float diffuse[N1+N2];
};

struct RadiosityebXml{
    std::string projectDir;
    std::string definedDir;

    SettingXml settingxml;

    LightXml lightxml;
    SensorXml sensorxml;

    SceneXml scenexml;
    std::vector<CanopyXml> canopyxmls;
    std::vector<PropertyXml> propxmls;
    std::vector<SpectralXml> spectralxmls;
    std::vector<ThermalXml> thermalxmls;

    MeteoXml meteoxml;
    AeroCondXml aerocondxml;
    AeroCoeffXml aerocoeffxml;

    glm::vec2 latlon;
};

#endif //FIELD_RADIOSITY_STRUCTS_H
