//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_MODEL_H
#define FIELD_RADIOSITY_MODEL_H

#include<iostream>
#include<vector>
#include<map>
#include "modelio.h"
#include "rt.h"

class Model {
public:
    Model();


    void scale2(std::shared_ptr<ModelIO> modelio);

    RT m_rt;
};


#endif //FIELD_RADIOSITY_MODEL_H
