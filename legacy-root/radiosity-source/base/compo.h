//
// Created by bianzunjian on 2024/3/24.
//

#ifndef FIELD_RADIOSITY_COMPO_H
#define FIELD_RADIOSITY_COMPO_H

#include "structs.h"
#include "fileio.h"
#include "../radiosity/radiosityio.h"
#include "../radiosityeb/radiosityebio.h"
#include "utils.h"


//------------------------------------
//--- Different between GPU and CPU
//------------------------------------


class Compo {

public:
    Compo() = default;


    bool createCompOptical(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> &modelio);


    bool createCompOpticall(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio);


    //bool createCompProperty(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio);

    float calctav(float alfa,float nr);
    void fluspect(OptCoeff fluspectCoeff, FluspectParam fluspectParam, FixedSpectral& spectral);


    void bsm(OptCoeff bsmCoeff, BSMParam bsm, FixedSpectral &spectral);
};


#endif //FIELD_RADIOSITY_COMPO_H
