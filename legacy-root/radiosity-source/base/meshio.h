//
// Created by jiank on 2024/3/25.
//

#ifndef FIELD_RADIOSITY_MESHIO_H
#define FIELD_RADIOSITY_MESHIO_H

#include "structs.h"

class MeshIO {

public:
    MeshIO() = default;

    std::vector<Spectral> spectrals;
    std::vector<Thermal> thermals;
    std::vector<FixedSpectral> fixedSpectrals;
    std::vector<Canopy> canopies;
    std::vector<LeafBio> leafbios;
    std::vector<SoilSet> soilsets;

    FluspectParam fp;
    BSMParam bsm;

    std::vector<ObjMesh>      objMeshes;
//    std::vector<PrimMesh>     primMeshes;
    //std::vector<MeshBuffer>   meshBuffers;
    std::vector<MeshLink>     meshLinks;

    std::map<std::string,int>  spectralNames;
    std::map<std::string,int>  thermalNames;
    std::map<std::string,int> canopyNames;
    std::map<std::string,int> leafbioNames;
    std::map<std::string,int> soilsetNames;
    std::map<std::string,int> aeroNames;

};


#endif //FIELD_RADIOSITY_MESHIO_H
