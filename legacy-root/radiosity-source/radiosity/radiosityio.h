//
// Created by bianzunjian on 2024/3/24.
//

#ifndef FIELD_RADIOSITY_RADIOSITYIO_H
#define FIELD_RADIOSITY_RADIOSITYIO_H


#include "../base/structs.h"
#include "../base/fileio.h"
#include "../base/facetio.h"
#include "../base/virtualio.h"
#include "../base/instanceio.h"
#include "../base/meshio.h"
#include <iostream>

class RadiosityIO {

public:
    RadiosityIO(){
        m_meshio = std::make_shared<MeshIO>();
        m_facetio = std::make_shared<FacetIO>();
        m_instanceio = std::make_shared<InstanceIO>();
        m_virtualio = std::make_shared<VirtualIO>();
    };

    std::string definedDir;
    std::string projectDir;

    int n_modelmesh = 0;
    int n_instance = 0;
    glm::vec3 sceneSize;
    glm::vec3 sceneOrigin;
    glm::vec3 sMin;
    glm::vec3 sMax;

    SensorMatrix sensor;   // original sensor become the angular sensor and spectral wavesets
    LightSet     light;
    std::vector<Angle> angles;
    std::vector<float> waves;

    int n_wave;
    int n_angle;
    glm::ivec2 imageSize;
    int n_sample;
    int maxDepth;
    bool isTemperature;
    bool isDisplay;
    bool isImage;
    bool isAlbedo;
    bool isInfinite;

    std::shared_ptr<FacetIO> m_facetio;
    std::shared_ptr<MeshIO> m_meshio;
    std::shared_ptr<InstanceIO> m_instanceio;
    std::shared_ptr<VirtualIO> m_virtualio;


    glm::vec3 m_xvw;
    SceneScale m_scenescale;
    float sza,saa,vza,vaa;
    float skyvza[NSKY], skyvaa[NSKY];
    int m_npoly;
    int m_npoint;
    float dx,dy;

//    int *ja, *jna,*vfum;


};


#endif //FIELD_RADIOSITY_RADIOSITYIO_H
