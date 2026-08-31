//
// Created by admin on 2024/1/24.
//

#include "compo.h"


bool Compo::createCompOptical(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> &modelio)
{
    auto & meshio = modelio->m_meshio;
    meshio->spectrals.clear();
    meshio->thermals.clear();

    //--------------------------------------------------
    //--- Spectral
    //--------------------------------------------------
    int id = 0;
    for(auto &spectralxml: fileio->m_pRadiosityXml->spectralxmls ){
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
    for(auto &thermalxml: fileio->m_pRadiosityXml->thermalxmls ) {
        meshio->thermals.push_back(Thermal{thermalxml.sunlitTemperature, thermalxml.shadedTemperature});
        meshio->thermalNames.insert({thermalxml.thermalName,id});
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

void Compo::fluspect(OptCoeff fluspectCoeff, FluspectParam fluspectParam, FixedSpectral& spectrals)
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

//    spectrals.clear();
    FixedSpectral spectral;
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
//cout<<b<<endl;
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

        spectrals.Tran_[i]        = Ta*Tsub/denom;
        spectrals.Refl_[i]      = Ra+Ta*Rsub*tt/denom;

        // spectral.Tran_[i]        = Ta*Tsub/denom;
        // spectral.Refl_[i]      = Ra+Ta*Rsub*tt/denom;
        // spectrals.push_back(spectral);
//leafopt.refl = refl;
//leafopt.tran = tran;
//leafopt.kChlrel = kChlrel;

//t        =   tran;
//r        =   refl;
//
//I_rt     =   (r+t)<1;
//D(I_rt)  =   sqrt((1 + r(I_rt) + t(I_rt)) * ...
//                  (1 + r(I_rt) - t(I_rt)) * ...
//                  (1 - r(I_rt) + t(I_rt)) * ...
//                  (1 - r(I_rt) - t(I_rt)));
//a(I_rt)  =   (1 + r(I_rt).^2 - t(I_rt).^2 + D(I_rt)) / (2*r(I_rt));
//b(I_rt)  =   (1 - r(I_rt).^2 + t(I_rt).^2 + D(I_rt)) / (2*t(I_rt));
//a(~I_rt) =   1;
//b(~I_rt) =   1;
//
//I_a      =   a>1;
//s(I_a)   =   2*a(I_a) / (a(I_a).^2 - 1) * log(b(I_a));
//s(~I_a)  =   r(~I_a) / t(~I_a);
//
//k        =   (a-1) / (a+1) * log(b);
//kChl     =   kChlrel * k;
//k(j)        = 0;
//kChl(j)     = 0;
//
//if fqe > 0
//
//    %% Fluorescence
//    wle         = spectral.wlE';    % excitation wavelengths, transpose to column
//    wlf         = spectral.wlF';    % fluorescence wavelengths, transpose to column
//    wlp         = spectral.wlP;     % PROSPECT wavelengths, also a row vector
//
//    minwle      = min(wle);
//    maxwle      = max(wle);
//    minwlf      = min(wlf);
//    maxwlf      = max(wlf);
//
//    % indices of wle and wlf within wlp
//
//    Iwle        = find(wlp>=minwle & wlp<=maxwle);
//    Iwlf        = find(wlp>=minwlf & wlp<=maxwlf);
//
//    eps         = 2^(-ndub);
//
//    % initialisations
//    te          = 1-(k(Iwle)+s(Iwle)) * eps;
//    tf          = 1-(k(Iwlf)+s(Iwlf)) * eps;
//    re          = s(Iwle) * eps;
//    rf          = s(Iwlf) * eps;
//
//    sigmoid     = 1/(1+exp(-wlf/10)*exp(wle'/10));  % matrix computed as an outproduct
//
//    % Other factor .5 deleted, since these are the complete efficiencies
//    % for either PSI or PSII. not a linear combination
//    [MfI,  MbI]  = deal(fqe(1) * ((.5*phiI( Iwlf))*eps) * kChl(Iwle)'*sigmoid);
//    [MfII, MbII] = deal(fqe(2) * ((.5*phiII(Iwlf))*eps) * kChl(Iwle)'*sigmoid);
//
//    Ih          = ones(1,length(te));     % row of ones
//    Iv          = ones(length(tf),1);     % column of ones
//
//    % Doubling routine
//
//    for i = 1:ndub
//
//        xe = te/(1-re*re);  ten = te*xe;  ren = re*(1+ten);
//        xf = tf/(1-rf*rf);  tfn = tf*xf;  rfn = rf*(1+tfn);
//
//        A11  = xf*Ih + Iv*xe';           A12 = (xf*xe')*(rf*Ih + Iv*re');
//        A21  = 1+(xf*xe')*(1+rf*re');   A22 = (xf*rf)*Ih+Iv*(xe*re)';
//
//        MfnI   = MfI  * A11 + MbI  * A12;
//        MbnI   = MbI  * A21 + MfI  * A22;
//        MfnII  = MfII * A11 + MbII * A12;
//        MbnII  = MbII * A21 + MfII * A22;
//
//        te   = ten;  re  = ren;   tf   = tfn;   rf   = rfn;
//        MfI  = MfnI; MbI = MbnI;  MfII = MfnII; MbII = MbnII;
//
//    end
//}
//    leafopt.MbI  = MbI;
//    leafopt.MbII = MbII;
//    leafopt.MfI  = MfI;
    }
}


bool Compo::createCompOpticall(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio) {


    auto & meshio = modelio->m_meshio;
    auto & radiosityebxml = fileio->m_pRadiosityebXml;
    auto & definedio = modelio->m_definedio;
    int num = 0;



    int id = 0;
    for(auto &spectralxml: radiosityebxml->spectralxmls ){
//        if(spectralxml.type == spectralType::CUSTOM){
//            for(int i=0;i<spectralxml.reflectances.size();i++)
//                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});
//        }

        if(spectralxml.type == spectralType::LEAFBIO){

            // spectral
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});

            // fixedSpectral
            //meshio->fp = spectralxml.fp;
            FixedSpectral fixedSpectral{};
            fluspect(definedio->m_optCoeff, spectralxml.fp, fixedSpectral);
            fixedSpectral.Refl_ir = spectralxml.refl_tir;
            fixedSpectral.Tran_ir = spectralxml.tau_tir;
            meshio->fixedSpectrals.push_back(fixedSpectral);
            //meshio->fixedSpectrals.insert(meshio->fixedSpectrals.end(),spectral_.begin(),spectral_.end());
//            meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});
        }

        if(spectralxml.type == spectralType::SOILSET){

            // spectral
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});

            // fixedSpectral
            //meshio->fp = spectralxml.fp;
            FixedSpectral fixedSpectral{};
            bsm(definedio->m_optCoeff, spectralxml.bsm, fixedSpectral);
            fixedSpectral.Refl_ir = spectralxml.refl_tir;
            fixedSpectral.Tran_ir = spectralxml.tau_tir;
            meshio->fixedSpectrals.push_back(fixedSpectral);
            //meshio->fixedSpectrals.insert(meshio->fixedSpectrals.end(),spectral_.begin(),spectral_.end());
//            meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});
        }

        if(spectralxml.type == spectralType::OTHER){

            //spectral
            for(int i=0;i<spectralxml.reflectances.size();i++)
                meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});

            // fixedSpectral
            //------------------------------------
            FixedSpectral fixedSpectral{};
            std::string infileName1 = spectralxml.path;
            //m_soilRefl_ = Utils::readascfile(infileName, 0, 1, num);
            std::vector<float> refl_;
            Utils::readascfileinout(infileName1,0,1,refl_,num);
            for(int k = 0;k<N1;k++){
                fixedSpectral.Refl_[k] = refl_[k];
                fixedSpectral.Tran_[k] = 0;
            }
            fixedSpectral.Refl_ir = spectralxml.refl_tir;
            fixedSpectral.Tran_ir = spectralxml.tau_tir;
            meshio->fixedSpectrals.push_back(fixedSpectral);
            //   meshio->fp = spectralxml.fp;
            //fluspect(definedio->m_optCoeff,meshio->fp,meshio->spectrals);
//            meshio->spectrals.push_back(Spectral{spectralxml.reflectances[i],spectralxml.transmittance[i]});
        }

        meshio->spectralNames.insert({spectralxml.spectralName,id});
        id++;
    }

    id = 0;
    for(auto &canopyxml: radiosityebxml->canopyxmls){

        ;
        meshio->canopies.push_back(canopyxml.canopy);
        meshio->canopyNames.insert({canopyxml.canopyName,id});
        id++;
    }

    int id1 = 0,id2 = 0;
    for(auto &propxml: fileio->m_pRadiosityebXml->propxmls){

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
    }



    return false;
}



void Compo::bsm(OptCoeff bsmCoeff,BSMParam bsm,FixedSpectral& spectral){

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
        //Spectral spectral{rwet,0};
        //spectrals.push_back(spectral);
        spectral.Refl_[k] = rwet;
        spectral.Tran_[k] = 0;

    }


}






