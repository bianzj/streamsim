//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_SCIFUNS_H
#define FIELD_RADIOSITY_SCIFUNS_H

#include "structs.h"

namespace  SCI{

    float trianglearea(glm::vec3 a, glm::vec3 b, glm::vec3 c );

    float bemission(float T);

    float planck(float T,float wl);

    float StefanBoltzmann(float T);

    void cross(glm::vec3 a, glm::vec3 b, float * c );

    //--------------------------------------------------------------------------------------------
    //  SATE VAPOR USING A TEMPERATURE  T 300K
    //--------------------------------------------------------------------------------------------
    float es_fun(float T);


    //slope of the saturated pressure function
    float s_fun(float es, float T);



    //----------------------------------------------------------
    // quadratic formula, root of least magnitude: AX2 + BX + C = 0
    //    for the eqn ax^2 + bx + c,
    //    if dsign is:
    //       -1, 0: choose the smaller root
    //       +1: choose the larger root
    //----------------------------------------------------------
    float sel_root(float a,float b,float c,float design);

}


#endif //FIELD_RADIOSITY_SCIFUNS_H
