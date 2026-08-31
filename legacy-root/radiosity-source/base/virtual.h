//
// Created by jiank on 2024/3/25.
//

#ifndef FIELD_RADIOSITY_VIRTUAL_H
#define FIELD_RADIOSITY_VIRTUAL_H

#include "../radiosity/radiosityio.h"
#include "rt.h"
#include "eigen3/Eigen/Dense"

class Virtual
{
public:
    Virtual(){
        m_pRT = std::make_shared<RT>();
    };

    std::string affiliate = "";
    std::shared_ptr<RT> m_pRT;
    void observe(std::shared_ptr<RadiosityIO> &modelio);
    void observe(std::shared_ptr<RadiosityEBIO> &modelio);

    void hist(std::shared_ptr<RadiosityIO> &mio, float vza_d,float vaa_d,float sza_d,float saa_d,std::vector<int> &picv,std::vector<int> &pics, std::vector<float> &vs);
    void hist(std::shared_ptr<RadiosityEBIO> &mio, float vza_d,float vaa_d,float sza_d,float saa_d,std::vector<int> &picv,std::vector<int> &pics, std::vector<float> &vs);

    void hist(std::shared_ptr<RadiosityEBIO> &mio, float vza_d, float vaa_d, float sza_d, float saa_d, std::vector<int> &picv, std::vector<int> &pics, std::vector<int> &picvvs);

};


#endif //FIELD_RADIOSITY_VIRTUAL_H
