//
// Created by admin on 2024/1/24.
//

#include "compo.h"


bool Compo::createCompOptical(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RaytracingIO> &raytracingio)
{
    auto & meshio = raytracingio->m_meshio;
    meshio->spectrals.clear();
    meshio->thermals.clear();

    //--------------------------------------------------
    //--- Spectral
    //--------------------------------------------------
    int id = 0;
    for(auto &spectralxml: fileio->m_pRaytracingXml->spectralxmls ){
        if(spectralxml.type == spectralType::CUSTOM){
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});
        }
        meshio->spectralNames.insert({spectralxml.spectralName,id});
        id++;
    }
    //--------------------------------------------------
    //--- Thermal
    //--------------------------------------------------
    id = 0;
    for(auto &thermalxml: fileio->m_pRaytracingXml->thermalxmls ) {
        meshio->thermals.push_back(Thermal{thermalxml.sunlitTemperature, thermalxml.shadedTemperature});
        meshio->thermalNames.insert({thermalxml.thermalName,id});
        id++;
    }

    //// add none to avoid pass the empty data into the GPU
    if (!fileio->m_pRaytracingXml->sensorxml.isTemperature)
    {
        meshio->thermals.push_back(Thermal{300, 300});
        meshio->thermalNames.insert({"None",id});
        id++;
    }

    return true;
}

bool Compo::createCompOptical(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelrtIO> &modelio)
{
    auto & meshio = modelio->m_meshio;
    meshio->spectrals.clear();
    meshio->thermals.clear();

    //--------------------------------------------------
    //--- Spectral
    //--------------------------------------------------
    int id = 0;
    for(auto &spectralxml: fileio->m_pVoxelrtXml->spectralxmls ){
        if(spectralxml.type == spectralType::CUSTOM){
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});
        }
        meshio->spectralNames.insert({spectralxml.spectralName,id});
        id++;
    }
    //--------------------------------------------------
    //--- Thermal
    //--------------------------------------------------
    id = 0;
    for(auto &thermalxml: fileio->m_pVoxelrtXml->thermalxmls ) {
        meshio->thermals.push_back(Thermal{thermalxml.sunlitTemperature, thermalxml.shadedTemperature});
        meshio->thermalNames.insert({thermalxml.thermalName,id});
        id++;
    }
    //--------------------------------------------------
    //--- Canopy
    //--------------------------------------------------
    id = 0;
    for(auto &canopyxml: fileio->m_pVoxelrtXml->canopyxmls){

        ;
        meshio->canopies.push_back(canopyxml.canopy);
        meshio->canopyNames.insert({canopyxml.canopyName,id});
        id++;
    }

    return true;
}


float Compo::calctav(float alfa,float nr)
{
    float rd,pi,n2,np,nm,a,k,sa,b1,b2,b,a3,b3,tp1,tp2,tp3,tp4,ts,tp5,tp,tav;
    pi=3.1415926;
    rd          = pi/180;
    n2          = nr*nr;
    np          = n2+1;
    nm          = n2-1;
    a           = (nr+1)*(nr+1)/2;
    k           = -(n2-1)*(n2-1)/4;
    sa          = sin(alfa*rd);
    b1 = 0;
    if(alfa !=90) b1          = sqrt((sa*sa-np/2)*(sa*sa-np/2)+k);
    b2          = sa*sa-np/2;
    b           = b1-b2;
    b3          = b*b*b;
    a3          = a*a*a;
    ts          = (k*k/(6*b3)+k/b-b/2)-(k*k/(6*a3)+k/a-a/2);

    tp1         = -2*n2*(b-a)/(np*np);
    tp2         = -2*n2*np*log(b/a)/(nm*nm);
    tp3         = n2*(1/b-1/a)/2;
    tp4         = 16*n2*n2*(n2*n2+1)*log((2*np*b-nm*nm)/(2*np*a-nm*nm))/(np*np*np*nm*nm);
    tp5         = 16*n2*n2*n2*(1/(2*np*b-nm*nm)-1/(2*np*a-nm*nm))/(np*np*np);
    tp          = tp1+tp2+tp3+tp4+tp5;
    tav         = (ts+tp)/(2*sa*sa);

    return tav;
}

void Compo::fluspect(OptCoeff fluspectCoeff, FluspectParam fluspectParam, std::vector<Spectral>& spectrals)
{
    float Cab         = fluspectParam.Cab;
    float Cw          = fluspectParam.Cw;
    float Cdm         = fluspectParam.Cdm;
    float Cs          = fluspectParam.Cs;
    float N           = fluspectParam.N;
    std::vector<float> &nr_         = fluspectCoeff.nr_;
    std::vector<float> &kdm_        = fluspectCoeff.kdm_;
    std::vector<float> &kab_         = fluspectCoeff.kab_;
    std::vector<float> &kw_         = fluspectCoeff.kw_;
    std::vector<float> &ks_         = fluspectCoeff.ks_;
    std::vector<float> &phiI_       = fluspectCoeff.phiI_;
    std::vector<float> &phiII_      = fluspectCoeff.phiII_;

    float nr, Kdm, Kab, Kw, Ks, phiII, phiI, Kall, t1, t2, tau, kChlrel,
            t12, r12, t21, r21, denom, Ra, r, D, rq, tq, a, b, bNm1, bN2_g, a2, Rsub, Tsub,
            s, j, talf, ralf, tt, Ta;

    spectrals.clear();
    for(int i=0; i<N1; i++)
    {
        nr          = nr_[i];
        //
        Kdm         = kdm_[i];
        Kab         = kab_[i];
        Kw          = kw_[i];
        Ks          = ks_[i];
        phiI        = phiI_[i];
        phiII       = phiII_[i];

        // PROSPECT calculations
        Kall        = (Cab*Kab + Cdm*Kdm + Cw*Kw  + Cs*Ks)/N;

        // Non-conservative scattering (normal case)
        t1          = (1-Kall)*exp(-Kall);
        t2          = Kall*Kall*Utils::expint(Kall);
        tau         = 1;
        if(Kall > 0) tau      = t1+t2;
        kChlrel     = 0;
        if(Kall > 0) kChlrel  = Cab*Kab/(Kall*N);

        talf        = calctav(59,nr);
        ralf        = 1-talf;
        t12         = calctav(90,nr);
        r12         = 1-t12;
        t21         = t12/(nr*nr);
        r21         = 1-t21;

        // top layer
        denom       = 1-r21*r21*tau*tau;
        Ta          = talf*tau*t21/denom;
        Ra          = ralf+r21*tau*Ta;

        // deeper layers
        tt           = t12*tau*t21/denom;
        r           = r12+r21*tau*tt;

        // Stokes equations to compute properties of next N-1 layers (N real)
        // Normal case

        D           = sqrt((1+r+tt)*(1+r-tt)*(1-r+tt)*(1-r-tt));
        rq          = r*r;
        tq          = tt*tt;
        a           = (1+rq-tq+D)/(2*r);
        b           = (1-rq+tq+D)/(2*tt);

        bNm1        = pow(b,(N-1));
        bN2_g         = bNm1*bNm1;
        a2          = a*a;
        denom       = a2*bN2_g-1;
        Rsub        = a*(bN2_g-1)/denom;
        Tsub        = bNm1*(a2-1)/denom;

        s           = r/tt;                             // Conservative scattering (CS)
        if(Kall>0) s        = 2*a/(a*a-1)*log(b);   // Normal case overwrites CS case

        //			Case of zero absorption

        if(r+tt>=1)
        {
            Tsub     = tt/(tt+(1-tt)*(N-1));
            Rsub	    = 1-Tsub;
        }
        // Reflectance and transmittance of the leaf: combine top layer with next N-1 layers
        denom       = 1-Rsub*r;
        Spectral spectral;
        spectral.transmittance        = Ta*Tsub/denom;
        spectral.reflectance      = Ra+Ta*Rsub*tt/denom;
        spectrals.push_back(spectral);
    }
}


bool Compo::createCompProperty(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &modelio) {

    auto & meshio = modelio->m_meshio;
    auto & definedio = modelio->m_defined;
    int num = 0;

    int id = 0;
    for(auto &spectralxml: fileio->m_pVoxelebXml->spectralxmls ){

        if(spectralxml.type == spectralType::PROSPECT){

            // spectral
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});

            // fixedSpectral
            meshio->fp = spectralxml.fp;
            std::vector<Spectral> spectral_;
            fluspect(definedio->m_fluspectCoeff,meshio->fp,spectral_);
            spectral_.push_back(Spectral{spectralxml.refl_tir,spectralxml.tau_tir});
            meshio->fixedSpectrals.insert(meshio->fixedSpectrals.end(),spectral_.begin(),spectral_.end());

        }

        if(spectralxml.type == spectralType::BSM){

            // spectral
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});

            // fixedSpectral
            meshio->fp = spectralxml.fp;
            std::vector<Spectral> spectral_;
            //fluspect(definedio->m_fluspectCoeff,meshio->fp,spectral_);
            bsm(definedio->m_fluspectCoeff,spectralxml.bsm,spectral_);
            spectral_.push_back(Spectral{spectralxml.refl_tir,spectralxml.tau_tir});
            meshio->fixedSpectrals.insert(meshio->fixedSpectrals.end(),spectral_.begin(),spectral_.end());

        }

        if(spectralxml.type == spectralType::OTHER){

            //spectral
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});

            // fixedSpectral
            //------------------------------------
            std::string infileName1 = spectralxml.path;
            //m_soilRefl_ = Utils::readascfile(infileName, 0, 1, num);
            std::vector<float> refl_;
            Utils::readascfileinout(infileName1,0,1,refl_,num);
            for(int k = 0;k<N1;k++){
                Spectral spectral{refl_[k],0};
                meshio->fixedSpectrals.push_back(spectral);
            }
            meshio->fixedSpectrals.push_back(Spectral{spectralxml.refl_tir,spectralxml.tau_tir});

        }

        if(spectralxml.type == spectralType::CUSTOM){

            //spectral
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});

            // fixedSpectral
            //------------------------------------
            //std::string infileName1 = spectralxml.path;
            //m_soilRefl_ = Utils::readascfile(infileName, 0, 1, num);
            //std::vector<float> refl_;
            //Utils::readascfileinout(infileName1,0,1,refl_,num);

            for(int k = 0;k<N1;k++){
                Spectral spectral{spectralxml.reflectances[0],spectralxml.transmittance[0]};
                meshio->fixedSpectrals.push_back(spectral);
            }
            meshio->fixedSpectrals.push_back(Spectral{spectralxml.refl_tir,spectralxml.tau_tir});

        }

        meshio->spectralNames.insert({spectralxml.spectralName,id});
        id++;
    }

    id = 0;
    for(auto &canopyxml: fileio->m_pVoxelebXml->canopyxmls){
        meshio->canopies.push_back(canopyxml.canopy);
        meshio->canopyNames.insert({canopyxml.canopyName,id});
        id++;
    }

    // The VoxelEB descriptor layout is fixed, so water-only scenes still need
    // valid placeholder buffers for resources unused by water shaders.
    if (meshio->canopies.empty()) {
        meshio->canopies.push_back(Canopy{});
        meshio->canopyNames.insert({"__default_canopy", 0});
    }

    int id1 = 0,id2 = 0,id3 = 0;
    for(auto &propxml: fileio->m_pVoxelebXml->propxmls){

        if(propxml.type == Type::VEGETATION) {
            meshio->leafbios.push_back(propxml.leafbio);
            meshio->leafbioNames.insert({propxml.name, id1});
            id1++;
        }
        else if(propxml.type == Type::SOIL){
            meshio->soilsets.push_back(propxml.soilset);
            meshio->soilsetNames.insert({propxml.name, id2});
            id2++;
        }
        else if(propxml.type == Type::WATER){
            meshio->watersets.push_back(propxml.waterset);
            meshio->watersetNames.insert({propxml.name, id3});
            id3++;
        }
    }

    if (meshio->leafbios.empty()) {
        meshio->leafbios.push_back(LeafBio{});
        meshio->leafbioNames.insert({"__default_leaf", 0});
    }
    if (meshio->soilsets.empty()) {
        meshio->soilsets.push_back(SoilSet{});
        meshio->soilsetNames.insert({"__default_soil", 0});
    }

    // Descriptor bindings are fixed; keep compatibility with scenes that have no water.
    if (meshio->watersets.empty()) {
        meshio->watersets.push_back(WaterSet{0.0f, 4.186e6f, 1.0f, 1.0f});
        meshio->watersetNames.insert({"water", 0});
    }

    return false;
}



void Compo::bsm(OptCoeff bsmCoeff,BSMParam bsm,std::vector<Spectral>& spectrals){

    std::vector<float> &gsv1_ = bsmCoeff.gsv1_;
    std::vector<float> &gsv2_ = bsmCoeff.gsv2_;
    std::vector<float> &gsv3_ = bsmCoeff.gsv3_;
    std::vector<float> &kw_ = bsmCoeff.kw_;
    std::vector<float> &nw_ = bsmCoeff.nw_;

    float B = bsm.BSMBrightness;
    float lat = bsm.BSMlat;
    float lon = bsm.BSMlon;
    float SMC = bsm.SMC;
    float SMCp = 0.25;
    float film = 0.015;
    float rd = 3.1415926/180.0;

    float f1 = B * sin(lat*rd);
    float f2 = B * cos(lat*rd)* sin(lon*rd);
    float f3 = B * cos(lat*rd) * cos(lon*rd);

    std::vector<float> rdry_,rwet_;
    for(int k =0;k<gsv1_.size();k++){
        rdry_.push_back(gsv1_[k]*f1+gsv2_[k]*f2+gsv3_[k]*f3);
    }

    for(int k =0;k<gsv1_.size();k++) {
        float rdry = rdry_[k];
        float tw = exp(-kw_[k] * film);
        float rbac = 1 - (1-rdry_[k]) * (rdry_[k] * calctav(90,2.0/nw_[k]) / calctav(90,2.0) + 1-rdry_[k]);
        float p = 1-calctav(90,nw_[k])/nw_[k]/nw_[k];
        float Rw = 1-calctav(40,nw_[k]);
        float   Radd   = (1-Rw) * (1-p) * rbac /(1 - p * rbac);
        float     mu  = (SMC - 0.05)/ SMCp;
        float fdry = exp(-mu);
        float  fmul = (exp(tw * mu) - 1) * fdry;
        float rwet = rdry * fdry + Rw * (1 - fdry) + Radd * fmul;
     //   rwet_.push_back(rwet);
         Spectral spectral{rwet,0};
         spectrals.push_back(spectral);
    }

}
