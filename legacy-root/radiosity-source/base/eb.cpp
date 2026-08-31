//
// Created by bianzunjian on 2024/3/21.
//

#include "eb.h"


void EB::rebalance(std::shared_ptr<RadiosityEBIO> &modelio){


    int k_node = modelio->m_knode;

    Meteo &meteo = modelio->meteos[k_node];


    //auto &canopy = modelio->canopys;
    float innovation;
    float raa,G,LE,H,rss,Net,bulk,Told,Tnew,res,lambda,s,ei,emis;

    // Temperature tempt;

    float e_to_q = MH20 / MAIR / meteo.p;
    // Meteo
    float Ta = meteo.Ta;
    float wc = 0.2;
    //int isclosed = 1;
    bool isclosed = true;
    auto npoly = modelio->m_npoly;
    auto &facets = modelio->m_facetio->facets;
    auto &facetebs = modelio->m_facetio->facetEBs;
    auto &meshlinks = modelio->m_meshio->meshLinks;
    auto &fixedSpectrals = modelio->m_meshio->fixedSpectrals;

    int n_bad = 0;
    int n_data = 0;
    isclosed = FALSE;
    for(int k=0;k<npoly;k++)
    {
        int meshId = facets[k].fsign;
        auto meshlink = meshlinks[meshId];
        int type = meshlink.type;
         raa = facetebs[k].raa;
        int spectralId = meshlink.spectralId;

        if (type >1)
        {
            int a = 0;
        }
        for(int kt = 0;kt<2;kt++){
             rss = facetebs[k].rss[kt];

             LE = facetebs[k].LE[kt];
             Told = facetebs[k].thermals[kt];
             H = facetebs[k].H[kt];
             G = facetebs[k].G[kt];
             Net = facetebs[k].Net[kt];
            bulk = LE + G;
            res = Net - bulk - H;
            lambda = (2.501-0.002361*(Told-273.15))*1E6;
            ei = SCI::es_fun(Told-273.15);
            s = SCI::s_fun(ei,Told-273.15);
            emis = 1.0 - fixedSpectrals[spectralId].Refl_ir;

            Tnew = Told + wc*res/((RHOA * AIRCP)/raa + RHOA*lambda*e_to_q*s/(raa+rss) + 4.0*emis*SIGMASB*std::pow(Told,3));
            // Tnew = Told + wc*res/((RHOA * AIRCP)/raa + RHOA*lambda*e_to_q*s/(raa) + 4.0*emis*SIGMASB*std::pow(Told,3));
//            innovation = raa/(RHOA * CP) *(Net - bulk);
//            Tnew = Ta + 273.15 + wc*innovation +(1-wc)*(Told -273.15 -Ta);

            facetebs[k].thermals[kt] = Tnew;
            if(k ==100){
                int a = 10;
            }

            if(res > RAD_THRESHOLD) n_bad++;
            n_data= n_data + 1;

            if(facetebs[k].thermals[kt] > TMAX_THRESHOLD )
            {
                facetebs[k].thermals[kt] = Ta + 2.0 + 273.15;
            }
            if(facetebs[k].thermals[kt] < TMIN_THRESHOLD)
            {
                facetebs[k].thermals[kt] = Ta + 2.0 + 273.15;
            }
            if(isnan(facetebs[k].thermals[kt]))
            {
                facetebs[k].thermals[kt] = Ta + 2.0 + 273.15;
            }
        }

        int a = 10;
    }

    if (n_bad < NBAD){
        isclosed = TRUE;
    }
    modelio->isclosed = isclosed;

}



