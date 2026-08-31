/*
in this case, the leaf reflectance and transpiration will be modeled;

*/

#pragma once

#include "structs.h"
#include <math.h>



class LeafOpt
{
public:
    LeafOpt(){};

    float calctav(float alfa,float nr);
    void fluspect(OptCoeff fluspectCoeff, FluspectParam fluspectParam, FixedSpectral& spectral);

};


