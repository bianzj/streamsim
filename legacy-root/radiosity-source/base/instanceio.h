//
// Created by jiank on 2024/3/25.
//

#ifndef TIRTEB_RADIOSITY_INSTANCEIO_H
#define TIRTEB_RADIOSITY_INSTANCEIO_H

#include "structs.h"

class InstanceIO {
public:
    InstanceIO()=default;



    std::vector<Instance>      instances;
    std::vector<InstanceLink> instanceLinks;


    std::vector<Spectral> spectrals;
    std::vector<Thermal> thermals;

};


#endif //TIRTEB_RADIOSITY_INSTANCEIO_H
