//
// Created by bianzunjian on 2024/3/21.
//

#include "aerodynamic.h"

float psim(float z,float L)
{
    float xz,pm,temp;
    temp = 0.25;
    xz = (1-16*z/L);
    xz = pow(xz,temp);
    pm=0.0;
    if(L<-4)
    {
        pm = 2*log(0.5+0.5*xz)+log(0.5+0.5*xz*xz)-2*atan(xz)+PI/2;

    }
    if(L>4e3)
    {
        pm = -5*z/L;
    }
    return pm;
}
float psih(float z,float L)
{
    float xz,ph,temp;
    xz = (1-16*z/L);
    temp = 0.25;
    xz = pow(xz,temp);
    ph=0.0;
    if(L<-4)
    {
        ph = 2*log((1+xz*xz)*0.5);

    }
    if(L>4e3)
    {
        ph = -5*z/L;
    }
    return ph;
}

float phstar(float z,float zr,float d,float L)
{
    float xz,phs,temp;
    xz= (1-16.0*z/L);
    temp = 0.25;
    xz = pow(xz,temp);
    phs=0.0;
    if(L<-4)
    {
        phs = (z-d)/(zr-d)*(xz*xz-1)/(xz*xz+1);

    }
    if(L>4e3)
    {
        phs = -5*z/L;
    }
    return phs;
}

void Aerodynamic::aeresist(std::shared_ptr<RadiosityEBIO> &modelio)
{

   // auto &canopy = modelio->canopys;
    AeroCond &aerocond = modelio->aerocond;
    AeroCoeff &aerocoeff = modelio->aerocoeff;
    int k_node = modelio->m_knode;
    Meteo &meteo = modelio->meteos[k_node];
    //Resistance &resist = modelio->m_pDynamicVariable->resistance;
    MeteoMeta &meteod = modelio->meta;
    //AeroCond &aerocond = modelio->aerocond;
    auto npoly = modelio->m_npoly;
    auto &facetebs = modelio->m_facetio->facetEBs;
    auto &facets = modelio->m_facetio->facets;
    float lai_aero = aerocond.lai;
    float hc_aero = std::max(aerocond.hc_build, aerocond.hc_veg);


    float u = meteo.u;
    float z = meteod.z;
    float psicor = aerocoeff.Psicor;

    float kappa = 0.4;
    float cp = 1200;  //????????
    float pa = 1205;  //????????

    float d1, z0m1, d2, z0m2, z1, z2,rasoil;
    float d,z0m;
    float L = aerocond.L;
    float ustar,kh,zr,n;
    float pm_z,ph_z,pm_h,ph_zr,phs_zr,phs_h;
    if (lai_aero <= 0) {
        // bare soil: no vegetation
        d = 0;
        z0m = 0.01;
        float pm_z = psim(z - d, L);
        ustar = std::max(float(0.001), kappa * u / (log((z - d) / z0m) - pm_z));
        rasoil = 1.0 / (kappa * ustar) * log(z);
    }else {
        float sq = sqrt(aerocoeff.CD1 * lai_aero);
        float g1 = std::max(double(3.3), sqrt(aerocoeff.CSSOIL + aerocoeff.Cd * lai_aero / 2.0));
        n = aerocoeff.Cd * lai_aero / (2.0 * kappa * kappa);
        zr = 2.5 * hc_aero;
        d = hc_aero * (1 - (1 - exp(-sq)) / sq);          //0?????
        z0m = (hc_aero - d) * exp(-kappa * g1 + psicor);

         pm_z = psim(z - d, L);
         ph_z = psih(z - d, L);
         pm_h = psim(hc_aero - d, L);
         ph_zr = psih(zr - d, L);
         phs_zr = phstar(zr, zr, d, L);
         phs_h = phstar(hc_aero, zr, d, L);

        ustar = Utils::max(0.001, kappa * u / (log((z - d) / z0m) - pm_z));  //???????
        kh = kappa * ustar * (hc_aero - d);
        if (L < -4) kh = kappa * ustar * (zr - d) * sqrt(1 - 16 * (hc_aero - d) / L); //???????????
        if (L > 4e3) kh = kappa * ustar * (zr - d) / (1 + 5 * (hc_aero - d) / L);
    }


    aerocond.ustar = ustar;
    for(int k=0;k<npoly;k++) {

        int meshid = facets[k].fsign;
        int canopyId = modelio->m_meshio->meshLinks[meshid].canopyId;
        auto canopy = modelio->m_meshio->canopies[canopyId];
        auto type = modelio->m_meshio->meshLinks[meshid].type;


        if(canopy.lai < 0.01){
            facetebs[k].raa = rasoil;
        } else {

//        float lai = canopy.lai;
//        float hc = canopy.height;
        float lai = aerocond.lai;
        float hc = std::max(aerocond.hc_build, aerocond.hc_veg);


            float uh = Utils::max(ustar / kappa * (log((hc - d) / z0m) - pm_h), 0.01);   //??¦Ï???????????????????

            float rai, rar;
            if (z > zr) {
                rai = (1.0 / (kappa * ustar) * (log((z - d) / (zr - d)) - ph_z + ph_zr));  //??????????????¼£
            } else rai = 0;
            rar = 1.0 / (kappa * ustar) * ((zr - hc) / (zr - d)) - phs_zr + phs_h; // ????????????????¼£

            kh = kappa * ustar * (zr - d);
            float rac = hc * sinh(n) / (n * kh) * (log((exp(n) - 1) / (exp(n) + 1)) -
                                                    log((exp(n * (z0m + d) / hc) - 1) / (exp(n * (z0m + d) / hc) + 1)));
            float uz0 = uh * exp(n * ((z0m + d) / hc - 1));                      //???????????????i????????????
            float rbc = 70 / lai * sqrt(canopy.leafwidth / uz0);                     // ?????¼£??

            float rws = hc * sinh(n) / (n * kh) * (log((exp(n * (z0m + d) / hc) - 1) / (exp(n * (z0m + d) / hc) + 1)) -
                                                   log((exp(n * (.01) / hc) - 1) / (exp(n * (.01) / hc) + 1)));
            float rbs = aerocoeff.rbs;

            if(type > 1) { //veg
                // facetebs[k].raa = rai + rar + rac + rbc;
                facetebs[k].raa = rai + rar + rac + rws + rbc;

            }else if(type ==1) { //soil
                facetebs[k].raa = rai + rar + rac + rws + rbs;
            }

        }
    }




}







