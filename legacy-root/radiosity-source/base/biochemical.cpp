//
// Created by bianzunjian on 2024/3/21.
//

#include "biochemical.h"

void biochemical_Farquhar(std::shared_ptr<RadiosityEBIO> &modelio)
{
    float fPAR = 1;
   // auto &canopys = modelio->canopys;
   auto & canopys = modelio->m_meshio->canopies;
    AeroCoeff &aerocoeff = modelio->aerocoeff;
    int k_node = modelio->m_knode;
    Meteo &meteo = modelio->meteos[k_node];

    MeteoMeta &meteod = modelio->meta;
    auto &meshlinks = modelio->m_meshio->meshLinks;

    auto &leafbios = modelio->m_meshio->leafbios;
    auto &soilsets = modelio->m_meshio->soilsets;
//    Thermal &thermal = modelio->m_pDynamicVariable->thermal;
//    NetRad &netrad = modelio->m_pDynamicVariable->netrad;
//    BioState &biostate = modelio->m_pDynamicVariable->biostate;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    auto &facetebs = modelio->m_facetio->facetEBs;


    float rhoa = RHOA;                               // [kg m-3]       specific mass of air
    float Mair = MAIR;
    auto npoly = modelio->m_npoly;

    for(int k=0;k<npoly;k++) {

        int meshid = facets[k].fsign;
        auto type = meshlinks[meshid].type;
        int bioId = meshlinks[meshid].bioId;


        if (type == (int)Type::SOIL) {
            //----------------------
            //        SOIL
            //----------------------
            // facetebs[k].rss[0] = RSS;
            // facetebs[k].rss[1] = RSS;
            facetebs[k].rss[0] = 2000;
            facetebs[k].rss[1] = 2000;

        } else if (type == (int)Type::VEGETATION) {


            auto leafbio = leafbios[bioId];
            //---------------------
            //     vegetation
            //---------------------
            float Tc, Cs, Es, Q, eb, O, p, vegtype, Vcmax25, BallBerrySlope, RdPerVcmax25, kV, Tcor, stressfactor, BallBerry0, effcon;

            float Tsunlit, Tshaded;
            if (facetebs[k].thermals[0] < 100) {
                Tsunlit = facetebs[k].thermals[0] + 273.15;
                Tshaded = facetebs[k].thermals[1] + 273.15;
            } else {
                Tsunlit = facetebs[k].thermals[0];
                Tshaded = facetebs[k].thermals[1];
            }

            for (int kt = 0; kt < 2; kt++) {
                //---------------------------------------------
                //  Input from CPU parameters
                //---------------------------------------------
                //////// environmental parameter
                if (kt == 0) {
                    Tc = Tsunlit;
                } else {
                    Tc = Tshaded;
                }
                // Cs: carbon dioxide (CO2) concentration on leaf surface
                Cs = facetebs[k].cs[kt];
                // Es: Water (H2O) concentration on leaf surface
                Es = facetebs[k].es[kt];
                //  net radiation, PAR
                Q = facetebs[k].pnetrad[kt];
                // intial estimate of the vapour pressure in leaf boundary layer
                eb = meteod.ea;
                // concentration of O2
                O = meteod.Oa;
                //  air pressure
                p = meteo.p;
                //////// Biochemical parameter
                // C3 and C4
                vegtype = leafbio.Type;
                // maximum carboxylation capacity at 25 C
                Vcmax25 = leafbio.Vcmax;
                // Ball-Berry coefficient 'm' for stomatal regulation
                BallBerrySlope = leafbio.m;
                // respiration as fraction of Vcmax25
                RdPerVcmax25 = leafbio.Rdparam;
                kV = leafbio.kV;
                // temperature correction to Vcmax has to be applied.
                Tcor = leafbio.Tcor;
                // stress factor to reduce Vcmax
                stressfactor = leafbio.stressfactor;
                // (OPTIONAL) Ball-Berry intercept term 'b'
                BallBerry0 = 0.01;
                // due to vertical distribution
                //Vcmax25 = Vcmax25 * exp(kV * xl);
                //number of CO2 per electrons - typically 1/5 for C3 and 1/6 for C4
                if (vegtype == 3) effcon = 1.0 / 5;
                if (vegtype == 4) effcon = 1.0 / 6;
                // vector of 5 temperature correction parameters
                float slti = leafbio.Tparam[0];
                float shti = leafbio.Tparam[1];
                float Thl = leafbio.Tparam[2];
                float Thh = leafbio.Tparam[3];
                float Trdm = leafbio.Tparam[4];
                //-------------------------------------
                // Input using constant parameters at optimum temperature
                //-------------------------------------
                float Tref = 25 + 273.15; // absolute temperature at 25 oC
                float Kc25 = 350;         // kinetic coefficient (Km) for CO2
                float Ko25 = 450;         // kinetic coeeficient (Km) for  O2
                float spfy25 = 2600;      // specificity
                float Kpep25 = (Vcmax25 / 56) *
                               1e6; //  (C4) PEPcase rate constant for CO2, used here: Collatz et al: Vcmax25 = 39 umol m-1 s-1; kp = 0.7 mol m-1 s-1.
                float atheta = 0.8;
                //  electron transport and fluorescence
                float Kf = 0.05;         //  rate constant for fluorescence
                float Kd = Utils::max(0.8738,
                                      0.0301 * (Tc - 273.15) + 0.0773); //  rate constant for thermal deactivation at Fm
                float Kp = 4.0;        // rate constant for photochemisty
                float rhoa = 1.2047;   // specific mass of air
                float Mair = 28.96;    // molecular mass of dry air
                // convert all to bar: CO2 was supplied in ppm, O2 in permil, and pressure in mBar
                float ppm2bar = 1E-6 * (p * 1E-3);
                Cs = Cs * ppm2bar;
                O = (O * 1E-3) * (p * 1E-3);
                if (vegtype > 3.5) O = 0;
                Kc25 = Kc25 * 1e-6;
                Ko25 = Ko25 * 1e-3;
                // temperature corrections
                float qt, TH, TL;
                qt = 0.1 * (Tc - Tref) * Tcor;  // tempcorr = 0 or 1: this line dis/enables all Q10 operations
                TH = 1 + Tcor * exp(shti * (Tc - Thh));
                TL = 1 + Tcor * exp(slti * (Thl - Tc));

                float QTVc, Kc, Ko, Kpepcase, po0;
                QTVc = 2.1;       // Q10 base for Vcmax and Kc
                Kc = Kc25 * exp(log(2.1) * qt);
                Ko = Ko25 * exp(log(1.2) * qt);
                Kpepcase = Kpep25 * exp(log(1.8) * qt); // "pseudo first order rate constant for PEP carboxylase WRT pi

                // if(issunlit==0) voxelRss[bufferId0].sunlit = Kpepcase;
                // else voxelRss[bufferId0].shaded = Kpepcase;
                float Vcmax;
                if (vegtype < 3.5) {
                    Vcmax = Vcmax25 * exp(log(QTVc) * qt) / TH * stressfactor;
                } else if (vegtype > 3.5) {
                    Vcmax = Vcmax25 * exp(log(QTVc) * qt) / (TH * TL) * stressfactor;
                }
                // specificity (tau in Collatz e.a. 1991)
                float spfy = spfy25 * exp(log(0.75) * qt);
                // "Dark" Respiration
                float Rd = RdPerVcmax25 * Vcmax25 * exp(log(1.8) * qt) / (1 + exp(1.3 * (Tc - Trdm)));
                // calculation of potential electron transport rate
                po0 = Kp / (Kf + Kd + Kp);
                float Je, Gamma_star, MM_const, Vs_C3, minCi;
                Je = 0.5 * po0 * Q * fPAR;
                //  calculation of the intersection of enzyme and light limited curves
                Gamma_star = 0.5 * O / spfy; // compensation point in absence of Rd
                if (vegtype < 3.5) {
                    MM_const = (Kc * (1 + O / Ko));
                    Vs_C3 = (Vcmax25 / 2) * exp(log(1.8) * qt);
                    minCi = 0.3;
                } else if (vegtype > 3.5) {
                    MM_const = 0;
                    Vs_C3 = 0;
                    minCi = 0.1;
                }

                //-------------------------------
                // Calculation of Ci: BallBerry only for case with b = 0
                //-------------------------------
                float RH, Ci;
                RH = std::min(float(1.0), eb / SCI::es_fun(Tc - 273.15));
                //% EXPLANATION:   *at equilibrium* CO2_in = CO2_out => A = gs(Cs - Ci) [1]
                //%  so Ci = Cs - A/gs (at equilibrium)                                 [2]
                //%  Ball-Berry suggest: gs = m (A RH)/Cs + b   (also at equilib., see Leuning 1990)
                // %  if b = 0 we can rearrange B-B for the second term in [2]:  A/gs = Cs/(m RH)
                //%  Substituting into [2]
                //%  Ci = Cs - Cs/(m RH) = Cs ( 1- 1/(m RH)  [ the 1.6 converts from CO2- to H2O-diffusion ]
                Ci = float(Utils::max(minCi * Cs, Cs * (1 - 1.6 / (BallBerrySlope * RH))));

                //------------------------------
                // Caluculation of A: ComputeA
                //-----------------------------
                //float A = computeA(float(Ci),int(Type),float(1.0),Vs_C3,MM_const,Rd,Vcmax,Gamma_star,Je,effcon,atheta,Kpepcase);
                float Vc, Ve, Vs, CO2_per_electron;
                if (vegtype < 3.5) {
                    Vs = Vs_C3;
                    Vc = Vcmax * (Ci - Gamma_star) / (MM_const + Ci);
                    CO2_per_electron = (Ci - Gamma_star) / (Ci + 2 * Gamma_star) * effcon;
                    Ve = Je * CO2_per_electron;
                } else if (vegtype > 3.5) {
                    Vc = Vcmax;
                    Vs = Kpepcase * Ci;
                    CO2_per_electron = effcon;
                    Ve = Je * CO2_per_electron;
                }

                float V = SCI::sel_root(atheta, -(Vc + Ve), Vc * Ve, std::signbit(-Vc));
                float Ag = SCI::sel_root(0.98, -(V + Vs), V * Vs, -1);
                float A = Ag - Rd;
                // stomatal conductance (gs)
                float gs = float(1.6 * A * ppm2bar / (Cs - Ci));
                // tomatal resistance (rcw)

                float rcw = (rhoa / (Mair * 1E-3)) / (gs);
                if (A < 0) rcw = 0.625 * 1E6;
                if (isnan(rcw)) rcw = 0.625 * 1E6;

                // save back
                facetebs[k].rss[kt] = rcw;
                Ci = float(Ci * 1e6 / p * 1e3);
                if (!isnan(Ci)) facetebs[k].ci[kt] = Ci;

            }
        }
    }

}


void biochemical_M12(std::shared_ptr<RadiosityEBIO> & modelio)
{


   // auto &canopys = modelio->canopys;
    auto & canopys = modelio->m_meshio->canopies;
    AeroCoeff &aerocoeff = modelio->aerocoeff;
    int k_node = modelio->m_knode;
    Meteo &meteo = modelio->meteos[k_node];

    MeteoMeta &meteod = modelio->meta;
    auto &meshlinks = modelio->m_meshio->meshLinks;

    auto &leafbios = modelio->m_meshio->leafbios;
    auto &soilsets = modelio->m_meshio->soilsets;
//    Thermal &thermal = modelio->m_pDynamicVariable->thermal;
//    NetRad &netrad = modelio->m_pDynamicVariable->netrad;
//    BioState &biostate = modelio->m_pDynamicVariable->biostate;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    auto &facetebs = modelio->m_facetio->facetEBs;


    float pmax, Vcmo, Tc, RH, O, SCOOP, Rdopt, Jmo, Vpmo,Tyear,
            Vpr, gbs, x, alpha, TREF, HARD, CRD, HAGSTAR, CGSTAR, HAJ, HDJ,
            DELTASJ, HAVCM, HDVC, DELTASVC, KCOP, HAKC, KOOP, HAKO, HAVPM,
            HDVP, DELTASVP, Q10KC, Q10KO, KPOP, Q10KP, dum1, dum2, Rd, SCO,
            Jmax, Vcmax, CKC, Kc, CKO, Ko, Vpmax, Kp, kf, kD, po0max, kPSII, fo0,
            kps, qLs, kd, NPQs, kds, kDs, Jms, po0, THETA, Q2, J, GSTAR, Ci, Cc,
            Wc, Wj, W, Ag, Ja, Cm, Rs, Rm, gam, Vpc, Vp, dum3, dum4, dum5, dum6,vcmax,
            dum7, dum8, dum9, dum10, a, b, c, d, Ac, Aj, ps, cs, A, p, Q, xl, rs, vcmo, Rdparam, Type, kNPQs, beta, leafM,kv;


    float rhoa = RHOA;                               // [kg m-3]       specific mass of air
    float Mair = MAIR;
    auto npoly = modelio->m_npoly;

    O = meteo.Oa;
    p = meteo.p * 100;


    for(int k=0;k<npoly;k++) {

        int meshid = facets[k].fsign;
        auto type = meshlinks[meshid].type;
        int bioId = meshlinks[meshid].bioId;


        if (type == (int)Type::SOIL) {
            //----------------------
            //        SOIL
            //----------------------
            // facetebs[k].rss[0] = RSS;
            // facetebs[k].rss[1] = RSS;
            facetebs[k].rss[0] = 2000;
            facetebs[k].rss[1] = 2000;

        } else if (type == (int)Type::VEGETATION) {

            //----------------------
            //        Veg
            //----------------------

            auto leafbio = leafbios[bioId];
            int canopyId = meshlinks[meshid].canopyId;
            auto canopy = canopys[canopyId];


            qLs = leafbio.qLs;
            NPQs = leafbio.kNPQs;
            vcmax = leafbio.Vcmax;
            Rdparam = leafbio.Rdparam;
            Type = leafbio.Type;
            beta = leafbio.beta;
            leafM = leafbio.m;
            kv = leafbio.kV;
            Tyear = leafbio.Tyear;
            float facetz = facets[k].pcenter[2];
            float canopyh = canopy.height;
            xl = -(canopyh-facetz)/canopyh;
            Vcmo = vcmax * exp(kv*xl);


            float Tsunlit, Tshaded;
            if (facetebs[k].thermals[0] < 100) {
                Tsunlit = facetebs[k].thermals[0] + 273.15;
                Tshaded = facetebs[k].thermals[1] + 273.15;
            } else {
                Tsunlit = facetebs[k].thermals[0];
                Tshaded = facetebs[k].thermals[1];
            }

            for (int kt = 0; kt < 2; kt++) {

                if (kt == 0) {
                    Tc = Tsunlit;
                    Q = facetebs[k].pnetrad[kt];
                } else {
                    Tc = Tshaded;
                    Q = facetebs[k].pnetrad[kt];
                }



                RH = facetebs[k].es[kt] / SCI::es_fun(Tc - 273.15);
                cs = facetebs[k].cs[kt] * p * 1e-11;

                O = meteo.Oa;
                O = O * p * 1e-8;
                Q = facetebs[k].pnetrad[kt];

                SCOOP = 2862;
                Rdopt = Rdparam * Vcmo;
                if (Type < 3.5) {
                    Jmo = Vcmo * 2.68;
                } else {
                    Jmo = Vcmo * 40 / 6.0;
                    Vpmo = Vcmo * 2.33;
                    Vpr = 80;
                    gbs = (0.0207 * Vcmo + 0.4806) * 1000.0;
                    x = 0.4;
                    alpha = 0;
                }

                //---------------------------------------------------------------------------------------------------------
                //% Parameters for voxelTempe corrections
                TREF = 25 +
                       273.15;                             // [K]            reference voxelTempe for photosynthetic processes

                HARD = 46.39;                                 // [kJ/mol]       activation energy of Rd
                CRD = 1000 * HARD /
                      (RGAS * TREF);                   // []             scaling factor in RD response to voxelTempe

                HAGSTAR = 37.83;                                 // [kJ/mol]       activation energy of Gamma_star
                CGSTAR = 1000 * HAGSTAR /
                         (RGAS * TREF);                // []             scaling factor in GSTAR response to voxelTempe

                // if (Type == 0)
                if (Type < 3.5){
                    // C3 species
                    HAJ = 49.88;                                 // [kJ/mol]       activation energy of Jm (Kattge & Knorr 2007)
                    HDJ = 200;                                   // [kJ/mol]       deactivation energy of Jm (Kattge & Knorr 2007)
                    DELTASJ = (-0.75 * Tyear + 660) /
                              1000;                // [kJ/mol/K]     entropy term for J  (Kattge and Knorr 2007)

                    HAVCM = 71.51;                                 // [kJ/mol]       activation energy of Vcm (Kattge and Knorr 2007)
                    HDVC = 200;                                   // [kJ/mol]       deactivation energy of Vcm (Kattge & Knorr 2007)
                    DELTASVC = (-1.07 * Tyear + 668) /
                               1000;                // [kJ/mol/K]     entropy term for Vcmax (Kattge and Knorr 2007)

                    KCOP = 404.9;                                 // [umol/mol]     Michaelis-Menten constant for CO2 at ref temp (Bernacchi et al 2001)
                    HAKC = 79.43;                                 // [kJ/mol]       activation energy of Kc (Bernacchi et al 2001)

                    KOOP = 278.4;                                 // [mmol/mol]     Michaelis-Menten constant for O2  at ref temp (Bernacchi et al 2001)
                    HAKO = 36.38;                                 // [kJ/mol]       activation energy of Ko (Bernacchi et al 2001)

                } else {
                    // C4 species (values can be different as noted by von Caemmerer 2000)
                    HAJ = 77.9;                                   // [kJ/mol]       activation energy of Jm  (Massad et al 2007)
                    HDJ = 191.9;                                 // [kJ/mol]       deactivation energy of Jm (Massad et al 2007)
                    DELTASJ = 0.627;                                 // [kJ/mol/K]     entropy term for Jm (Massad et al 2007). No data available on acclimation to voxelTempe.

                    HAVCM = 67.29;                                 // [kJ/mol]       activation energy of Vcm (Massad et al 2007)
                    HDVC = 144.57;                                // [kJ/mol]       deactivation energy of Vcm (Massad et al 2007)
                    DELTASVC = 0.472;                                 // [kJ/mol/K]     entropy term for Vcm (Massad et al 2007). No data available on acclimation to voxelTempe.

                    HAVPM = 70.37;                                 // [kJ/mol]       activation energy of Vpm  (Massad et al 2007)
                    HDVP = 117.93;                                // [kJ/mol]       deactivation energy of Vpm (Massad et al 2007)
                    DELTASVP = 0.376;                                 // [kJ/mol/K]     entropy term for Vpm (Massad et al 2007). No data available on acclimation to voxelTempe.

                    KCOP = 944.;                                  // [umol/mol]     Michaelis-Menten constant for CO2 at ref temp (Chen et al 1994; Massad et al 2007)
                    Q10KC = 2.1;                                   // []             Q10 for voxelTempe response of Kc (Chen et al 1994; Massad et al 2007)

                    KOOP = 633.;                                  // [mmol/mol]     Michaelis-Menten constant for O2 at ref temp (Chen et al 1994; Massad et al 2007)
                    Q10KO = 1.2;                                   // []             Q10 for voxelTempe response of Ko (Chen et al 1994; Massad et al 2007)

                    KPOP = 82.;                                   // [umol/mol]     Michaelis-Menten constant of PEP carboxylase at ref temp (Chen et al 1994; Massad et al 2007)
                    Q10KP = 2.1;                                   // []             Q10 for voxelTempe response of Kp (Chen et al 1994; Massad et al 2007)

                }

                //---------------------------------------------------------------------------------------------------------
                //% Corrections for effects of voxelTempe and non-stomatal limitations
                dum1 = RGAS / 1000 * Tc;                                  // [kJ/mol]
                dum2 = RGAS / 1000 * TREF;                               // [kJ/mol]

                Rd = Rdopt * exp(CRD - HARD /
                                       dum1);                  // [umol/m2/s]    mitochondrial respiration rates adjusted for voxelTempe (Bernacchi et al. 2001)
                SCO = SCOOP / exp(CGSTAR - HAGSTAR /
                                           dum1);            // []             Rubisco specificity for CO2 adjusted for voxelTempe (Bernacchi et al. 2001)

                Jmax = Jmo * exp(HAJ * (Tc - TREF) / (TREF * dum1));
                Jmax = Jmax * (1. + exp((TREF * DELTASJ - HDJ) / dum2));
                Jmax = Jmax / (1. + exp((Tc * DELTASJ - HDJ) /
                                        dum1));     // [umol e-/m2/s] max electron transport rate at leaf voxelTempe (Kattge and Knorr 2007; Massad et al. 2007)

                Vcmax = Vcmo * exp(HAVCM * (Tc - TREF) / (TREF * dum1));
                Vcmax = Vcmax * (1 + exp((TREF * DELTASVC - HDVC) / dum2));
                Vcmax = Vcmax / (1 + exp((Tc * DELTASVC - HDVC) /
                                         dum1));    // [umol/m2/s]    max carboxylation rate at leaf voxelTempe (Kattge and Knorr 2007; Massad et al. 2007)



                ////////////////////////////////////////////////////////////////////////////////////
                if (Type < 3.5)
                    {
                    // C3// species
                    CKC = 1000 * HAKC / (RGAS *
                                         TREF);                     // []             scaling factor in KC response to voxelTempe
                    Kc = KCOP * exp(CKC - HAKC / dum1) * 1e-11 *
                         p;     // [bar]          Michaelis constant of carboxylation adjusted for voxelTempe (Bernacchi et al. 2001)

                    CKO = 1000 * HAKO / (RGAS *
                                         TREF);                     // []             scaling factor in KO response to voxelTempe
                    Ko = KOOP * exp(CKO - HAKO / dum1) * 1e-8 *
                         p;      // [bar]          Michaelis constant of oxygenation adjusted for voxelTempe (Bernacchi et al. 2001)

                } else                                              // C4 species
                {
                    Vpmax = Vpmo * exp(HAVPM * (Tc - TREF) / (TREF * dum1));
                    Vpmax = Vpmax * (1 + exp((TREF * DELTASVP - HDVP) / dum2));
                    Vpmax = Vpmax / (1 + exp((Tc * DELTASVP - HDVP) /
                                             dum1));//% [umol/m2/s]    max carboxylation rate at leaf voxelTempe (Massad et al. 2007)
                    float temp = ((Tc - TREF) / 10.);
                    Kc = KCOP * pow(Q10KC, temp) * 1e-11 *
                         p;    // [bar]          Michaelis constant of carboxylation voxelTempe corrected (Chen et al 1994; Massad et al 2007)
                    temp = ((Tc - TREF) / 10.);
                    Ko = KOOP * pow(Q10KO, temp) * 1e-8 *
                         p;     // [bar]          Michaelis constant of oxygenation  voxelTempe corrected (Chen et al 1994; Massad et al 2007)
                    temp = ((Tc - TREF) / 10.);
                    Kp = KPOP * pow(Q10KP, temp) * 1e-11 *
                         p;    // [bar]          Michaelis constant of PEP carboxyl voxelTempe corrected (Chen et al 1994; Massad et al 2007)

                }

                //---------------------------------------------------------------------------------------------------------
                //% Define electron transport and fluorescence parameters
                kf = 3.E7;                                    // [s-1]         rate constant for fluorescence
                kD = 1.E8;                                   // [s-1]         rate constant for thermal deactivation at Fm
                kd = 1.95E8;                                    // [s-1]         rate constant of energy dissipation in closed RCs (for theta=0.7 under un-stressed conditions)
                po0max = 0.88;                                     // [mol e-/E]    maximum PSII quantum yield, dark-acclimated in the absence of stress (Pfundel 1998)
                kPSII = (kD + kf) * po0max /
                        (1. - po0max);             // [s-1]         rate constant for photochemisty (Genty et al. 1989)
                fo0 = kf / (kf + kPSII +
                            kD);                        // [E/E]         reference dark-adapted PSII fluorescence yield under un-stressed conditions

                kps = kPSII *
                      qLs;                              // [s-1]         rate constant for photochemisty under stressed conditions (Porcar-Castell 2011)
                kNPQs = NPQs * (kf +
                                kD);                           // [s-1]         rate constant of sustained thermal dissipation (Porcar-Castell 2011)
                kds = kd * qLs;
                kDs = kD + kNPQs;
                Jms = Jmax *
                      qLs;                               // [umol e-/m2/s] potential e-transport rate reduced for PSII photodamage
                po0 = kps / (kps + kf +
                             kDs);                       // [mol e-/E]    maximum PSII quantum yield, dark-acclimated in the presence of stress
                THETA = (kps - kds) /
                        (kps + kf + kDs);                  // []            convexity factor in J response to PAR

                //---------------------------------------------------------------------------------------------------------
                //% Calculation of electron transport rate
                Q2 = beta * Q * po0;
                J = (Q2 + Jms - sqrt(pow((Q2 + Jms), 2) - 4 * THETA * Q2 * Jms)) /
                    (2 * THETA); // [umol e-/m2/s]    electron transport rate under light-limiting conditions

                //---------------------------------------------------------------------------------------------------------
                //% Calculation of net photosynthesis
                if (Type < 3.5)                                           // C3 species, based on Farquhar model (Farquhar et al. 1980)
                {

                    GSTAR = 0.5 * O /
                            SCO;                             // [bar]             CO2 compensation point in the absence of mitochondrial respiration
                    Ci = cs * (1 - 1.6 / (leafbio.m * RH * leafbio.stressfactor));
                    if (Ci < GSTAR) Ci = GSTAR;
                    // [bar]             intercellular CO2 concentration from Ball-Berry model (Ball et al. 1987)
                    Cc = Ci;                                        // [bar]             CO2 concentration at carboxylation sites (neglecting mesophyll resistance)

                    Wc = Vcmax * Cc / (Cc + Kc * (1 + O / Ko));     // [umol/m2/s]       RuBP-limited carboxylation
                    Wj = J * Cc /
                         (4.5 * Cc + 10.5 * GSTAR);            // [umol/m2/s]       electr transp-limited carboxyl

                    W = std::min(Wc, Wj);                                // [umol/m2/s]       carboxylation rate
                    Ag = (1 - GSTAR / Cc) * W;                       // [umol/m2/s]       gross photosynthesis rate
                    A = Ag - Rd;                                   // [umol/m2/s]       net photosynthesis rate
                    Ja = J * W /
                         Wj;                                 // [umole-/m2/s]     actual linear electron transport rate

                } else                                             // C4 species, based on von Caemmerer model (von Caemmerer 2000)
                {
                    Ci = Utils::max(9.9e-6 * (p * 1e-5), cs * (1 - 1.6 / (leafbio.m * RH * leafbio.stressfactor)));
                    // [bar]             intercellular CO2 concentration from Ball-Berry model (Ball et al. 1987)
                    Cm = Ci;                                     // [bar]             mesophyll CO2 concentration (neglecting mesophyll resistance)
                    Rs = 0.5 *
                         Rd;                               // [umol/m2/s]       bundle sheath mitochondrial respiration (von Caemmerer 2000)
                    Rm = Rs;                                     // [umol/m2/s]       mesophyll mitochondrial respiration
                    gam = 0.5 /
                          SCO;                               // []                half the reciprocal of Rubisco specificity for CO2

                    Vpc = Vpmax * Cm / (Cm +
                                        Kp);                     // [umol/m2/s]       PEP carboxylation rate under limiting CO2 (saturating PEP)
                    Vp = std::min(Vpc, Vpr);                            // [umol/m2/s]       PEP carboxylation rate

                    // Complete model proposed by von Caemmerer (2000)
                    dum1 = alpha / 0.047;                           // dummy variables, to reduce computation time
                    dum2 = Kc / Ko;
                    dum3 = Vp - Rm + gbs * Cm;
                    dum4 = Vcmax - Rd;
                    dum5 = gbs * Kc * (1 + O / Ko);
                    dum6 = gam * Vcmax;
                    dum7 = x * J / 2. - Rm + gbs * Cm;
                    dum8 = (1. - x) * J / 3.;
                    dum9 = dum8 - Rd;
                    dum10 = dum8 + Rd * 7 / 3;

                    a = 1. - dum1 * dum2;
                    b = -(dum3 + dum4 + dum5 + dum1 * (dum6 + Rd * dum2));
                    c = dum4 * dum3 - dum6 * gbs * O + Rd * dum5;
                    Ac = (-b - sqrt(b * b - 4 * a * c)) /
                         (2 * a);           // [umol/m2/s]       CO2-limited net photosynthesis

                    a = 1. - 7 / 3 * gam * dum1;
                    b = -(dum7 + dum9 + gbs * gam * O * 7 / 3. + dum1 * gam * dum10);
                    c = dum7 * dum9 - gbs * gam * O * dum10;
                    Aj = (-b - sqrt(b * b - 4 * a * c)) / (2 *
                                                           a);           // [umol/m2/s]       light-limited net photosynthesis (assuming that an obligatory Q cycle operates)

                    if (Ac != Ac && Aj == Aj) A = Aj;
                    if (Aj != Aj && Ac == Ac) A = Ac;
                    if (Ac == Ac && Aj == Aj)
                        A = std::min(Ac, Aj);                             // [umol/m2/s]       net photosynthesis

                    Ja = J;                                      // [umole-/m2/s]     actual electron transport rate, CO2-limited


                    if (Ac == Ac && A == Ac) //IPL 03/09/2013
                    {

                        // ind=A==Ac;
                        a = x * (1 - x) / 6 / A;
                        b = (1 - x) / 3 * (gbs / A * (Cm - Rm / gbs - gam * O) - 1 - alpha * gam / 0.047) -
                            x / 2 * (1. + Rd / A);
                        c = (1 + Rd / A) * (Rm - gbs * Cm - 7 * gbs * gam * O / 3.0) +
                            (Rd + A) * (1 - 7 * alpha * gam / 3 / 0.047);
                        Ja = (-b + sqrt(b * b - 4 * a * c)) /
                             (2 * a);            // [umole-/m2/s]     actual electron transport rate, CO2-limited

                    }


                    // Simplified model (von Caemmerer 2000), should be chosen ONLY if computation times are excessive
                    //        dum3  =  Vp-Rm+gbs*Cm;
                    //        dum4  =  Vcmax-Rd;
                    //        dum7  =  x*J/2. - Rm + gbs*Cm;
                    //        dum8  =  (1.-x)*J/3.;
                    //        dum9  =  dum8 - Rd;
                    //
                    //        Ac    = min(dum3,dum4);                          // [umol/m2/s]       light saturated CO2 assimilation rate
                    //        Aj    = min(dum7,dum9);                          // [umol/m2/s]       light-limited CO2 assimilation rate
                    //
                    //        A     = min(Ac,Aj);                              // [umol/m2/s]       net photosynthesis rate
                    //        Ja  = J * A/ Aj;                               // [umole-/m2/s]     actual electron transport rate (simple empirical formulation based on results)

                }

                //---------------------------------------------------------------------------------------------------------
                //% Calculation of PSII quantum yield and fluorescence
                ps = Ja / (beta * Q);                            // [mol e-/E]    PSII photochemical quantum yield
                //fs   = MD12(ps,Ja,Jms,kps,kf,kds,kDs);            // [E/E]         PSII fluorescence yield
                // eta    = fs/fo0;                                   // []            scaled PSII fluorescence yield


                //% JP add
                // [g mol-1]      molecular mass of dry air

                float rss = 0.625 * (cs - Ci) / A * rhoa / Mair * 1E3 * 1e6 / p * 1E5;


                if (A <= 0) rss = 0.625 * 1E6;


                Ci = Ci * 1e6 / p * 1E5;


                facetebs[k].rss[kt] = rss;


                if (!isnan(Ci)) facetebs[k].ci[kt] = Ci;

            }
        }
    }

}


void BioChemical::suresist(std::shared_ptr<RadiosityEBIO> &modelio) {

   // biochemical_Farquhar(modelio);

   biochemical_M12(modelio);
}
