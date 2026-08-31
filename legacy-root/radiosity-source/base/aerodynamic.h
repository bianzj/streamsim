//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_AERODYNAMIC_H
#define FIELD_RADIOSITY_AERODYNAMIC_H


#include "structs.h"
#include "defined.h"
#include "../radiosityeb/radiosityebio.h"

class Aerodynamic
{
public:
    Aerodynamic(){};

    void aeresist(std::shared_ptr<RadiosityEBIO> &modelio);

};


#endif //FIELD_RADIOSITY_AERODYNAMIC_H
