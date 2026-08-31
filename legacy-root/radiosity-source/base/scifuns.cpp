//
// Created by bianzunjian on 2024/3/21.
//


#include "scifuns.h"


float SCI::trianglearea(glm::vec3 a, glm::vec3 b, glm::vec3 c) {

    float usize=0,vsize=0,wsize=0;
    for(int j=0;j<3;j++)
    {
        usize = usize + (b[j]-a[j])*(b[j]-a[j]);
        vsize = vsize + (c[j]-a[j])*(c[j]-a[j]);
        wsize = wsize + (c[j]-b[j])*(c[j]-b[j]);

    }
    usize = std::sqrt(usize);
    vsize = std::sqrt(vsize);
    wsize = std::sqrt(wsize);
    float p = 0.5*(usize+vsize+wsize);
    float area = std::sqrt(p*(p-usize)*(p-vsize)*(p-wsize));
    return area;


}

float SCI::bemission(float T) {

    float ts = 0, sigma = 5.6696e-8;
    float eb;
    if(T<100) ts=T+273.15;
    else ts=T;

    eb = sigma*ts*ts*ts*ts;
    //eb[i+npoly] = eb[i];

    return eb;
}
float SCI::planck(float wl,float T)
{
    float eb;
    float c1=11910.439340652;
    float c2=14388.291040407;
    float ts=0;
    float wll= wl/1000;

    if(T<100) ts=T+273.15;
    else ts=T;
    eb = c1/(pow(wll,5)*(exp(c2/ts/wll)-1))*10000;
    // eb[i+npoly]=eb[i];

    return eb;
}


void SCI::cross(glm::vec3 vec1, glm::vec3 vec2, float * norm )
{
    float vec[3];
    vec[0]=vec1.y*vec2.z-vec1.z*vec2.y;
    vec[1]=vec1.z*vec2.x-vec1.x*vec2.z;
    vec[2]=vec1.x*vec2.y-vec1.y*vec2.x;

    float su=0;
    for(int k=0;k<3;k++)
    {
        su = su+vec[k]*vec[k];
    }
    su = sqrt(su);
    for(int k=0;k<3;k++)
    {
        norm[k] = vec[k]*1.0/su;
    }
}

//--------------------------------------------------------------------------------------------
//  SATE VAPOR USING A TEMPERATURE  T 300K
//--------------------------------------------------------------------------------------------
float SCI::es_fun(float T)
{
    float a = 7.5;
    float b = 237.3;
    float temp = a*T/(b+T);
    return 6.107*pow(10,temp);
}

//slope of the saturated pressure function
float SCI::s_fun(float es, float T)
{
    return es*2.3026*7.5*237.3/((237.3+T)*(237.3+T));
}


//----------------------------------------------------------
// quadratic formula, root of least magnitude: AX2 + BX + C = 0
//    for the eqn ax^2 + bx + c,
//    if dsign is:
//       -1, 0: choose the smaller root
//       +1: choose the larger root
//----------------------------------------------------------
float SCI::sel_root(float a,float b,float c,float design)
{
    float x;
    if(a ==0)
    {
        x = -c/b;
    }else
    {
        if(design ==0)
        {
            design = -1;
        }
        x = (-b + design *sqrt(b*b - 4*a*c))/(2*a);
    }
    return x;
}



