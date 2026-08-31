//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_EB_H
#define FIELD_RADIOSITY_EB_H

#include "structs.h"
#include "scifuns.h"
#include "../radiosityeb/radiosityebio.h"

class EB {
public:
    EB(){};

    void rebalance(std::shared_ptr<RadiosityEBIO> &modelio);
};


#endif //FIELD_RADIOSITY_EB_H
