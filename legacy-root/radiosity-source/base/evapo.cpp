//
// Created by bianzunjian on 2024/3/21.
//

#include "evapo.h"

void Evapo::evapotranspiration(std::shared_ptr<RadiosityEBIO> &modelio) {


    int k_node = modelio->m_knode;
    Meteo &meteo = modelio->meteos[k_node];
    auto npoly = modelio->m_npoly;

   // auto &canopy = modelio->canopys;
    AeroCond &aerocond = modelio->aerocond;
    AeroCoeff &aerocoeff = modelio->aerocoeff;
    //Resistance &resist = modelio->m_pDynamicVariable->resistance;
    MeteoMeta &meteod = modelio->meta;
    //AeroCond &aerocond = modelio->aerocond;
    auto &facetebs = modelio->m_facetio->facetEBs;
    auto &facets = modelio->m_facetio->facets;
    auto &meshlinks = modelio->m_meshio->meshLinks;

    for (int k = 0; k < npoly; k++) {


        int meshId = facets[k].fsign;
        int canopyId = meshlinks[meshId].canopyId;
        auto canopy = modelio->m_meshio->canopies[canopyId];
        int type = meshlinks[meshId].type;



        float e_to_q = MH20 / MAIR / meteo.p;
        float ea = meteod.ea;
        float qa = ea * e_to_q;
        float lambda, Tc, ra, rs, ei, qi;   //      [J kg-1]  Evapor. heat (J kg-1)
        float Ta = meteo.Ta;
        float ca = meteod.ca;
        float ci = 0;
        if(k ==0){
            int a = 10;
        }

        for(int kt=0;kt<2;kt++)
        {
            //----------------------------
            // Leaf sunlit
            //----------------------------

            Tc = facetebs[k].thermals[kt];
            ra = facetebs[k].raa;
            rs = facetebs[k].rss[kt];
            lambda = (2.501 - 0.002361 * (Tc - 273.15)) * 1E6;
            ei = SCI::es_fun(Tc - 273.15);
            qi = ei * e_to_q;
            ci = facetebs[k].ci[kt];
            // facetebs[k].H[kt] = RHOA / (ra + rs) * lambda * (qi - qa);  // % [W m-2]   Latent heat flux
            // facetebs[k].LE[kt] = (RHOA * AIRCP) / ra * (Tc - 273.15 - Ta);           //% [W m-2]   Sensible heat flux
            facetebs[k].LE[kt] = RHOA / (ra + rs) * lambda * (qi - qa);  // % [W m-2]   Latent heat flux
            facetebs[k].H[kt] = (RHOA * AIRCP) / ra * (Tc - 273.15 - Ta);           //% [W m-2]   Sensible heat flux
            facetebs[k].Net[kt] = facetebs[k].vnetrad[kt] + facetebs[k].tnetrad[kt];
            if(type == (int)Type::SOIL) {
                // facetebs[k].G[kt] = facetebs[k].Net[kt] * 0.25;
                facetebs[k].G[kt] = facetebs[k].Net[kt] * 0.35;
            }else{
                facetebs[k].G[kt] = 0;
                facetebs[k].es[kt] = ea + (ei - ea) * ra / (ra + rs);
                facetebs[k].cs[kt] = ca + (ci - ca) * ra / (ra + rs);
//                facetebs[k].es[kt] = ea ;
//                facetebs[k].cs[kt] = ca ;
            }

        }


        // ec          = ea + (ei-ea)*ra./(ra+rs);         % [W m-2] vapour pressure at the leaf surface
        // Cc          = Ca - (Ca-Ci).*ra./(ra+rs);        % [umol m-2 s-1] CO2 concentration at the leaf surface
//        heatflux.Gsunlit = heatflux.Nsoilsunlit * 0.25;
//        heatflux.Gshaded = heatflux.Nsoilshaded * 0.25;
    }
}

