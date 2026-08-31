//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_MODELIO_H
#define FIELD_RADIOSITY_MODELIO_H

#include "structs.h"
#include "facetio.h"

class ModelIO {

public:
    ModelIO();

    std::string projectdir;

    float m_xvw[3];
    SceneScale m_scenescale;
    float sza,saa,vza,vaa;
    float skyvza[NSKY], skyvaa[NSKY];
    int m_npoly;
    int m_npoint;
    float dx,dy;

    std::vector<std::shared_ptr<FacetIO>> m_vFacetio;

};


#endif //FIELD_RADIOSITY_MODELIO_H
