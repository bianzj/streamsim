//
// Created by bianzunjian on 2024/3/24.
//

#ifndef FIELD_RADIOSITY_SCENE_H
#define FIELD_RADIOSITY_SCENE_H
#pragma once
#include "structs.h"
#include "fileio.h"
#include "xmlexamples.h"
#include "objloader.h"
#include "../radiosity/radiosityio.h"
#include "../radiosityeb/radiosityebio.h"
// #include "../thirdparty/libInterpolate/Interpolate.hpp"
#include "scifuns.h"



class Scene {


public:
    Scene() = default;
    bool createObjScene(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> & radiosityio);
    bool createObjScene(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> & radiosityio);
    bool createPrimScene(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> & radiosityio);
    bool fromobj2facet(std::shared_ptr<RadiosityIO> & radiosityio);
    bool fromobj2facet(std::shared_ptr<RadiosityEBIO> & radiosityio);
    void scale2(std::shared_ptr<RadiosityIO> modelio);
    void scale2(std::shared_ptr<RadiosityEBIO> modelio);

};


#endif //FIELD_RADIOSITY_SCENE_H
