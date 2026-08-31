//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_EVAPO_H
#define FIELD_RADIOSITY_EVAPO_H

#include "../radiosityeb/radiosityebio.h"
#include "scifuns.h"


class Evapo {
public:


    Evapo(){};
    void evapotranspiration(std::shared_ptr<RadiosityEBIO> &m_pPixelio);
};


#endif //FIELD_RADIOSITY_EVAPO_H
