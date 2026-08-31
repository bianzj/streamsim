//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_BIOCHEMICAL_H
#define FIELD_RADIOSITY_BIOCHEMICAL_H


#include "structs.h"
#include "defined.h"
#include "scifuns.h"
#include "../radiosityeb/radiosityebio.h"

class BioChemical
{
public:
    BioChemical(){};
    void suresist(std::shared_ptr<RadiosityEBIO> &modelio);
};



#endif //FIELD_RADIOSITY_BIOCHEMICAL_H
