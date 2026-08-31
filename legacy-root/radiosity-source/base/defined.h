//
// Created by bianzunjian on 2024/3/26.
//

#ifndef TIRTEB_RADIOSITY_DEFINED_H
#define TIRTEB_RADIOSITY_DEFINED_H

#include "structs.h"
#include "leafopt.h"
#include "soilopt.h"
#include "utils.h"
#include <memory>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>

class DefinedIO {

public:
    DefinedIO() {

        defineCanopy();
        defineLeafbio();
    };


    void input(std::string path);
    void destroy();


    std::string definedDir="./predefine/";
    OptCoeff m_optCoeff;
    AeroCoeff m_aerocoeff;
    LeafBio leafbio;
    SoilSet soilset;
    Canopy canopy;
    MeteoMeta meta;
    Spectral spectral;
    FixedSpectral fixedSpectral;

    AtomCoeff m_atomcoeff;
    //
//    float lrho_ir;
//    float ltau_ir;
//    float rs_ir;

    //LeafOpt leafopt;

    void defineCanopy();
    void defineLeafbio();
    int year;
    int doy;
//    float* m_direct;
//    float* m_diffuse;
//    float* wl;
    std::map<int,Canopy> m_mCanopy;
    std::map<int,LeafBio> m_mLeafbio;
    std::map<int,FixedSpectral> m_mSpectral;

};


#endif //TIRTEB_RADIOSITY_DEFINED_H
