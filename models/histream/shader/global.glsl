#ifndef GLOBALS_GLSL
#define GLOBALS_GLSL 1

// global constants
#define POROSITY 1 //土壤的孔隙度因子
#define SSA 0.25 // single scattering albedo
#define GST 0.35
#define DENSITY 1

#define PI 3.14159265358979323
#define TWO_PI 6.28318530717958648
#define INFINITY 1e32
#define RngStateType uint // Random type
#define SHORTDISTANCE 0.01

/// type information
#define TYPE_NULL 0
#define TYPE_SOIL 1
#define TYPE_VEGETATION 2
#define TYPE_BUILDING 3
#define TYPE_WATER 4
#define AEROTYPE_VEGETATION 0
#define AEROTYPE_URBAN 1

/// scene object type
#define KIND_CUBE 0

//precision highp int;
precision highp float;

const float M_PI = 3.14159265358979323846;   // pi
const float M_TWO_PI = 6.28318530717958648;      // 2*pi
const float M_PI_2 = 1.57079632679489661923;   // pi/2
const float M_PI_4 = 0.785398163397448309616;  // pi/4
const float M_1_OVER_PI = 0.318309886183790671538;  // 1/pi
const float M_2_OVER_PI = 0.636619772367581343076;  // 2/pi
const float OUTER = 10000.0;
const float InvPI = 0.318309886183790671538;
#define EPSION_1 0.1
#define EPSION_2 0.01
#define EPSION_3 0.001

#define EPSION 0.00000001
#define GLEAF 0.5
#define HSPOT 0.5

#define GROUP_SIZEXY 8
#define GROUP_SIZEX 64
#define GROUP_SIZEXYZ 8

#define PID2 1.557963
#define TLASTNUM 8
#define MAXSTEP 8
#define DIFFUSE_ANGLE_NUM 50
#define BANDNUM 2162

#define NBAND 2002  // 2001 vnir+1 TIR 
#define RHOA 1.2047      // specific mass of air
#define CP   1004        // specific heat of dry air
#define KAPPA 0.4        // Von Karman constant
#define GEARTH  9.81     // gravity acceleration
#define AHC 119.7117122  //
#define MAIR 28.96       // molecular mass of dry air
#define MH20 18          // molecular mass of water
#define MCO2 44          // Molecular mass of carbon dioxide
#define SIGMASB 5.67e-8   // stefan boltzman constant
#define STRESS 1         // 
#define C2K    273.15    // melting point of water
#define KV 0.6396        // 
#define R  8.31           // Molar gas constant
#define LEAFG 0.5
#define CD   0.3         // drag coefficient for the vegetation
#define CR    0.35        // darg for the isolate tree
#define CD1 20.6       //fitting coefficients
#define PSICOR    0.2           // rOUGHNESS LAYER CORRECTION
#define CSSOIL 0.01 // DARY COEFFICIENT FOR SOIL

#define RAD_THRESHOLD 2    // radiance threshold
#define TMIN_THRESHOLD 220  // -50 C
#define TMAX_THRESHOLD 350  // +80 C


// const vec3 DIFFUSE_DIRECTIONS[32] = {
//     vec3(0.898904, 0.435512, 0.0479745),
//     vec3(0.898904, -0.435512, -0.0479745),
//     vec3(0.898904, 0.0479745, -0.435512),
//     vec3(0.898904, -0.0479745, 0.435512),
//     vec3(-0.898904, 0.435512, -0.0479745),
//     vec3(-0.898904, -0.435512, 0.0479745),
//     vec3(-0.898904, 0.0479745, 0.435512),
//     vec3(-0.898904, -0.0479745, -0.435512),
//     vec3(0.0479745, 0.898904, 0.435512),
//     vec3(-0.0479745, 0.898904, -0.435512),
//     vec3(-0.435512, 0.898904, 0.0479745),
//     vec3(0.435512, 0.898904, -0.0479745),
//     vec3(-0.0479745, -0.898904, 0.435512),
//     vec3(0.0479745, -0.898904, -0.435512),
//     vec3(0.435512, -0.898904, 0.0479745),
//     vec3(-0.435512, -0.898904, -0.0479745),
//     vec3(0.435512, 0.0479745, 0.898904),
//     vec3(-0.435512, -0.0479745, 0.898904),
//     vec3(0.0479745, -0.435512, 0.898904),
//     vec3(-0.0479745, 0.435512, 0.898904),
//     vec3(0.435512, -0.0479745, -0.898904),
//     vec3(-0.435512, 0.0479745, -0.898904),
//     vec3(0.0479745, 0.435512, -0.898904),
//     vec3(-0.0479745, -0.435512, -0.898904),
//     vec3(0.57735, 0.57735, 0.57735),
//     vec3(0.57735, 0.57735, -0.57735),
//     vec3(0.57735, -0.57735, 0.57735),
//     vec3(0.57735, -0.57735, -0.57735),
//     vec3(-0.57735, 0.57735, 0.57735),
//     vec3(-0.57735, 0.57735, -0.57735),
//     vec3(-0.57735, -0.57735, 0.57735),
//     vec3(-0.57735, -0.57735, -0.57735)
// };

const vec3 DIFFUSE_DIRECTIONS[50] = {
    vec3(0.000000, 1.000000, 0.000000),
    vec3(1.000000, 0.000000, 0.000000),
    vec3(0.473544, 0.850735, 0.228047),
    vec3(0.116956, 0.850735, 0.512417),
    vec3(-0.327703, 0.850735, 0.410926),
    vec3(-0.525595, 0.850735, 0.000000),
    vec3(-0.327703, 0.850735, -0.410926),
    vec3(0.116956, 0.850735, -0.512417),
    vec3(0.473544, 0.850735, -0.228047),
    vec3(0.868246, 0.475088, 0.142969),
    vec3(0.653120, 0.475088, 0.589682),
    vec3(0.230633, 0.475088, 0.849176),
    vec3(-0.265078, 0.475088, 0.839062),
    vec3(-0.676629, 0.475088, 0.562552),
    vec3(-0.873355, 0.475088, 0.107435),
    vec3(-0.792797, 0.475088, -0.381791),
    vec3(-0.460531, 0.475088, -0.749801),
    vec3(0.017949, 0.475088, -0.879755),
    vec3(0.490732, 0.475088, -0.730393),
    vec3(0.807710, 0.475088, -0.349136),
    vec3(0.990300, 0.000000, 0.138949),
    vec3(0.788150, 0.000000, 0.615483),
    vec3(0.374817, 0.000000, 0.927099),
    vec3(-0.138949, 0.000000, 0.990300),
    vec3(-0.615483, 0.000000, 0.788150),
    vec3(-0.927099, 0.000000, 0.374817),
    vec3(-0.990300, 0.000000, -0.138949),
    vec3(-0.788150, 0.000000, -0.615483),
    vec3(-0.374817, 0.000000, -0.927099),
    vec3(0.138949, 0.000000, -0.990300),
    vec3(0.615483, 0.000000, -0.788150),
    vec3(0.927099, 0.000000, -0.374817),
    vec3(0.860460, -0.475088, 0.184120),
    vec3(0.624322, -0.475088, 0.620091),
    vec3(0.189967, -0.475088, 0.859188),
    vec3(-0.304702, -0.475088, 0.825499),
    vec3(-0.702630, -0.475088, 0.529719),
    vec3(-0.877478, -0.475088, 0.065758),
    vec3(-0.773733, -0.475088, -0.419081),
    vec3(-0.424333, -0.475088, -0.770865),
    vec3(0.059790, -0.475088, -0.877905),
    vec3(0.524929, -0.475088, -0.706216),
    vec3(0.823408, -0.475088, -0.310308),
    vec3(0.440216, -0.850735, 0.287158),
    vec3(0.049961, -0.850735, 0.523215),
    vec3(-0.377916, -0.850735, 0.365280),
    vec3(-0.521214, -0.850735, -0.067718),
    vec3(-0.272028, -0.850735, -0.449723),
    vec3(0.182001, -0.850735, -0.493077),
    vec3(0.498979, -0.850735, -0.165135)
};

#endif
