//
// Created by bianzunjian on 2024/3/26.
//

#include "defined.h"


void DefinedIO::input(std::string m_dir)
{
    //--------------------------------
    // import the fluspect parameters;
    //--------------------------------

//    difineDir = m_dir+"predefine/";
//    std::string infileName = predifineDir + "optipar_fluspect.txt";
//    int num = 1;
    //wl = Utils::readascfile(infileName,0,0,num);
//    m_optCoeff.nr_ = Utils::readascfile(infileName, 0, 1, num);
//    m_optCoeff.kdm_ = Utils::readascfile(infileName, 0, 2, num);
//    m_optCoeff.kab_ = Utils::readascfile(infileName, 0, 3, num);
//    m_optCoeff.kw_ = Utils::readascfile(infileName, 0, 4, num);
//    m_optCoeff.ks_ = Utils::readascfile(infileName, 0, 5, num);
//    m_optCoeff.phiI_ = Utils::readascfile(infileName, 0, 6, num);
//    m_optCoeff.phiII_ = Utils::readascfile(infileName, 0, 7, num);

//    Utils::readascfileinout(infileName, 0, 1, m_optCoeff.nr_, num);
//    Utils::readascfileinout(infileName, 0, 2, m_optCoeff.kdm_, num);
//    Utils::readascfileinout(infileName, 0, 3, m_optCoeff.kab_, num);
//    Utils::readascfileinout(infileName, 0, 4, m_optCoeff.kw_, num);
//    Utils::readascfileinout(infileName, 0, 5, m_optCoeff.ks_, num);
//    Utils::readascfileinout(infileName, 0, 6, m_optCoeff.phiI_, num);
//    Utils::readascfileinout(infileName, 0, 7, m_optCoeff.phiII_, num);



    //----------------------------------
    //-- import leafbio and soilset
    //----------------------------------

//    std::string line;
//    std::vector<std::string> fields;
//    std::string DefinedIOpath = predifineDir + "DefinedIO.txt";
//    std::string deli(" ");
//    std::ifstream infile(DefinedIOpath.c_str());
//    if(infile.is_open())
//    {
//        getline(infile,line);
//        getline(infile,line);
//        fields = Utils::splitt(line,deli);
//        canopy.lai = std::atof(fields[0].c_str());
//        canopy.density = std::atof(fields[1].c_str());
//        canopy.height = std::atof(fields[2].c_str());
//        canopy.width = std::atof(fields[3].c_str());
//        canopy.Gleaf = std::atof(fields[4].c_str());
//        canopy.LIDFa = std::atof(fields[5].c_str());
//        canopy.LIDFb = std::atof(fields[6].c_str());
//        canopy.hspot = std::atof(fields[7].c_str());
//        canopy.leafwidth = std::atof(fields[8].c_str());
//        canopy.type = std::atof(fields[9].c_str());
//        canopy.dist = std::atof(fields[10].c_str());
//
//        getline(infile,line);
//        getline(infile,line);
//        fields = Utils::splitt(line,deli);
//        leafbio.fp.Cab = std::atof(fields[0].c_str());
//        leafbio.fp.Cw = std::atof(fields[1].c_str());
//        leafbio.fp.Cdm = std::atof(fields[2].c_str());
//        leafbio.fp.Cs = std::atof(fields[3].c_str());
//        leafbio.fp.N = std::atof(fields[4].c_str());
//        spectral.Refl_ir = std::atof(fields[5].c_str());
//        spectral.leafTran_ir = std::atof(fields[6].c_str());
//
//        getline(infile,line);
//        getline(infile,line);
//        fields = Utils::splitt(line,deli);
////        leafbio.Fqe[0] = std::atof(fields[0].c_str());
////        leafbio.Fqe[1] = std::atof(fields[1].c_str());
//        leafbio.Vcmax = std::atof(fields[0].c_str());
//        leafbio.m = std::atof(fields[1].c_str());
//        leafbio.Type = std::atof(fields[2].c_str());
//        leafbio.Tparam[0] = std::atof(fields[3].c_str());
//        leafbio.Tparam[1] = std::atof(fields[4].c_str());
//        leafbio.Tparam[2] = std::atof(fields[5].c_str());
//        leafbio.Tparam[3] = std::atof(fields[6].c_str());
//        leafbio.Tparam[4] = std::atof(fields[7].c_str());
//        leafbio.Rdparam = std::atof(fields[8].c_str());
//        leafbio.Tyear = std::atof(fields[9].c_str());
//        leafbio.beta = std::atof(fields[10].c_str());
//        leafbio.kNPQs = std::atof(fields[11].c_str());
//        leafbio.qLs = std::atof(fields[12].c_str());
//        leafbio.kV = std::atof(fields[13].c_str());
//        leafbio.stressfactor = std::atof(fields[14].c_str());
//
//        getline(infile,line);
//        getline(infile,line);
//        fields = Utils::splitt(line,deli);
//        soilset.bsm.BSMBrightness = std::atof(fields[0].c_str());
//        soilset.bsm.BSMlat = std::atof(fields[1].c_str());
//        soilset.bsm.BSMlon = std::atof(fields[2].c_str());
//        spectral.soilRefl_ir = std::atof(fields[3].c_str());
//        getline(infile,line);
//        getline(infile,line);
//        fields = Utils::splitt(line,deli);
//        soilset.rss = std::atof(fields[0].c_str());
//        soilset.cs = std::atof(fields[1].c_str());
//        soilset.rhos = std::atof(fields[2].c_str());
//        soilset.lambdas = std::atof(fields[3].c_str());
//        soilset.rbs = std::atof(fields[4].c_str());
//        soilset.SMC = std::atof(fields[5].c_str());
//        soilset.Tsoil = std::atof(fields[6].c_str());
//        soilset.satwater = std::atof(fields[7].c_str());
//
//        getline(infile,line);
//        getline(infile,line);
//        fields = Utils::splitt(line,deli);
//        m_aerocoeff.zo = std::atof(fields[0].c_str());
//        m_aerocoeff.d = std::atof(fields[1].c_str());
//        m_aerocoeff.rbc = std::atof(fields[2].c_str());
//        m_aerocoeff.CR = std::atof(fields[3].c_str());
//        m_aerocoeff.Cd = std::atof(fields[4].c_str());
//        m_aerocoeff.CD1 = std::atof(fields[5].c_str());
//        m_aerocoeff.Psicor = std::atof(fields[6].c_str());
//        m_aerocoeff.CSSOIL = std::atof(fields[7].c_str());
//        m_aerocoeff.rwc = std::atof(fields[8].c_str());
//        m_aerocoeff.rbs = std::atof(fields[9].c_str());
//
//
//        getline(infile,line);
//        getline(infile,line);
//        fields = Utils::splitt(line,deli);
//        meta.z = atof(fields[0].c_str());
//        meta.sm = atof(fields[1].c_str());
//        meta.ea = atof(fields[2].c_str());
//        meta.Ca = atof(fields[3].c_str());
//        meta.Oa = atof(fields[4].c_str());
//        meta.Tsold = atof(fields[5].c_str());
//        meta.SatWater = atof(fields[6].c_str());
//        meta.dTime = atof(fields[7].c_str());
//
//
//        //std::cout<<"FileInput has been read..."<<std::endl;
//
//    }else std::cout<<"Unable to open the fileinput "<<std::endl;
//    infile.close();
//
//    leafopt.fluspect(m_optCoeff,leafbio.fp,spectral);

    //-----------------------------------
    // import soil reflectance
    //------------------------------------
//    std::string infileName1 = predifineDir + "soilnew.txt";
//    //m_soilRefl_ = Utils::readascfile(infileName, 0, 1, num);
//    std::vector<float> m_soilRefl_;
//    Utils::readascfileinout(infileName1,0,1,m_soilRefl_,num);

//    std::string infileName1 = predifineDir + " ";
//    m_soilRefl_ = Utils::readascfile(infileName, 0, 1, num);

    //std::string infilesky = predifineDir +'Esky_.dat';
    for (int i = 0; i <  2001 ; i++)
    {
        m_atomcoeff.wl[i] = 400 + i;
//        spectral.wl_[i] = 400+i;
//        spectral.soilRefl_[i] = m_soilRefl_[i];
    }
    for (int i = 0; i < 126; i++)
    {
        m_atomcoeff.wl[i+2001] = 2500 + i * 100;
        //spectral.wl_[i+2001] =2500 + i * 100;
    }
    for (int i = 0; i < 35; i++)
    {
        m_atomcoeff.wl[i+2127] = 16000 + i * 1000;
        // spectral.wl_[i+2127] = 16000 + i * 1000;
    }



    float  *esun_, *esky_, *fesky_, *fesun_;
//    //int numm = 1;
//    // wave_ = Utils::infile2num_float(predifineDir+'Esk', 0, 0, num);
//    esun_ = Utils::readascfile(predifineDir+"Esun_.dat", 0, 0, num);
//    esky_ = Utils::readascfile(predifineDir+"Esky_.dat", 0, 0, num);
//    fesky_ = new float[num];
//    fesun_ = new float[num];
//
//    float TsEsky = 0, TlEsky = 0, TlEsun = 0, TsEsun = 0, tstot = 0, tltot = 0, temp1, temp2, step;
//    int b1 = N1;
//    int b2 = N1+N2;
//
//    // ???
//    for (int i = 0; i < b1 - 1; i++)
//    {
//        temp1 = (esky_[i] + esky_[i + 1]) / 2.0;
//        step =  m_atomcoeff.wl[i + 1] -  m_atomcoeff.wl[i];
//        temp2 = (esun_[i] + esun_[i + 1]) / 2.0;
//        TsEsky += temp1 * step;
//        TsEsun += temp2 * step;
//    }
//    tstot = (TsEsky + TsEsun) * 0.001;
//    for (int i = 0; i < b1; i++)
//    {
//        fesky_[i] = esky_[i] / tstot;
//        fesun_[i] = esun_[i] / tstot;
//    }
//    // ????
//    for (int j = b1; j < b2 - 1; j++)
//    {
//        temp1 = (esky_[j] + esky_[j + 1]) / 2.0;
//        step =  m_atomcoeff.wl[j + 1] -  m_atomcoeff.wl[j];
//        temp2 = (esun_[j] + esun_[j + 1]) / 2.0;
//        TlEsky += temp1 * step;
//        TlEsun += temp2 * step;
//    }
//    tltot = (TlEsky + TlEsun) * 0.001;
//    for (int i = b1; i < b2; i++)
//    {
//        fesky_[i] = esky_[i] / tltot;
//        fesun_[i] = esun_[i] / tltot;
//    }
//
//    for (int i = 0; i < b2; i++)
//    {
//        //m_atomcoeff.wl[i] =
//        m_atomcoeff.fesun[i] = fesun_[i];
//        m_atomcoeff.fesky[i] = fesky_[i];
//    }



    delete[] fesky_;
    delete[] fesun_;
    delete[] esun_;
    delete[] esky_;

}


void DefinedIO::destroy()
{

//    delete [] m_optCoeff.nr_;
//    delete [] m_optCoeff.kdm_;
//    delete [] m_optCoeff.kab_;
//    delete [] m_optCoeff.kw_;
//    delete [] m_optCoeff.ks_;
//    delete [] m_optCoeff.phiI_;
//    delete [] m_optCoeff.phiII_;
//    delete [] m_soilRefl_;
}

void DefinedIO::defineCanopy()
{
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::water,
                                            Canopy{-1, 0, 0, 0, 0, 0, 0, 0, 0, 0,0}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::evergreen_needleleaf_forest,
                                            Canopy{3.0, 1, 10, 5, 0.5, -0.35, -0.15, 0.2, 0.2, 1,3}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::evergreen_broadleaf_forest,
                                            Canopy{3.0, 1, 10, 5, 0.5, -0.35, -0.15, 0.2, 0.2, 2,3}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::deciduous_needleleaf_forest,
                                            Canopy{3.0, 1, 10, 5, 0.5, -0.35, -0.15, 0.2, 0.2, 3,3}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::deciduous_broadleaf_forest,
                                            Canopy{3.0, 1, 10, 5, 0.5, -0.35, -0.15, 0.2, 0.2, 4,3}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::mixed_forest,
                                            Canopy{3.0, 1, 10, 5, 0.5, -0.35, -0.15, 0.2, 0.2, 5,3}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::closed_shrublands,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 6,1}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::open_shrublands,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 7,1}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::woody_savannas,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 8,1}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::savannas,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 9,1}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::grasslands,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 10,1}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::permanent_watlands,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 11,0}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::croplands,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 12,2}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::urban_and_builtup,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 13,4}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::cropland_vegetation_mosaic,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 14,2}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::snow_and_ice,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 15,0}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::barren_sparsely_vegetated,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 16,1}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::unclassified,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 17,0}));
    m_mCanopy.insert(std::pair<int, Canopy>(IGBP::fill_value,
                                            Canopy{3.0, 1, 3, 3, 0.5, -0.35, -0.15, 0.2, 0.2, 255,0}));
}

void DefinedIO::defineLeafbio() {

    FluspectParam fp{58.0, 0.013, 0.0036,    0.0,  1.86,0.025,0 };
    float tp[5]{0.2,0.3,281,308,328};
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::water,
                                              LeafBio{ 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::evergreen_needleleaf_forest,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,278,308,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::evergreen_broadleaf_forest,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,288,313,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::deciduous_needleleaf_forest,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,278,303,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::deciduous_broadleaf_forest,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,283,311,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::mixed_forest,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,281,307,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::closed_shrublands,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,278,313,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::open_shrublands,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,278,313,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::woody_savannas,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,288,303,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::savannas,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,288,303,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::grasslands,
                                              LeafBio{ 35.8, 4, 0.01, 1, 0.6396, 0.015, 0.2,0.3,288,313,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::permanent_watlands,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,288,303,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::croplands,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,281,308,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::urban_and_builtup,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,283,311,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::cropland_vegetation_mosaic,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,281,308,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::snow_and_ice,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,281,308,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::barren_sparsely_vegetated,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,281,308,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::unclassified,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,281,308,328,15,0.507,0,1,1,0,fp}));
    m_mLeafbio.insert(std::pair<int, LeafBio>(IGBP::fill_value,
                                              LeafBio{ 80, 9, 0.01, 0, 0.6396, 0.015, 0.2,0.3,281,308,328,15,0.507,0,1,1,0,fp}));



}
