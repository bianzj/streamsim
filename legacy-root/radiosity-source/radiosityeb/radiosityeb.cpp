//
// Created by bianzunjian on 2024/3/24.
//

#include "radiosityeb.h"


void RadiosityEB::upload(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio){

    //Radiosity::upload(fileio, modelio);
    //auto &mio = modelio->radiosityio;

    uploadMeteo(fileio,modelio);
    uploadAero(fileio,modelio);
    uploadDefined(fileio,modelio);
    uploadSetting(fileio, modelio);

    m_pCompo->createCompOpticall(fileio, modelio);
    m_pScene->createObjScene(fileio,modelio);
    m_pScene->fromobj2facet(modelio);
    m_pGeometry->createGeometry(fileio,modelio);
    m_pGeometry->calcSkyAngle(modelio->skyvza,modelio->skyvaa);
    init(modelio);

}

void RadiosityEB::uploadSetting(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio) {
    modelio->isInfinite = fileio->m_pRadiosityebXml->settingxml.isInfinite;
    modelio->latlon = fileio->m_pRadiosityebXml->latlon;
}

bool RadiosityEB::uploadAero(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio) {


    if(fileio->m_pRadiosityebXml->aerocondxml.aerotype==AeroType::one) {
        modelio->aerocond = (fileio->m_pRadiosityebXml->aerocondxml.aerocond);
        modelio->aerocoeff = (fileio->m_pRadiosityebXml->aerocoeffxml.aerocoeff);
    }else if (fileio->m_pRadiosityebXml->aerocondxml.aerotype==AeroType::image)
    {
        int a = 10;
    }else if(fileio->m_pRadiosityebXml->aerocondxml.aerotype==AeroType::gridCal){
        int b = 10;
    }

    return false;


}

bool RadiosityEB::uploadDefined(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio)
{

    fileio->readDefined(modelio->m_definedio);

    return false;
}

bool  RadiosityEB::uploadMeteo(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio){

    // Utils::readascfileinout(meteofile,0,1,)
    fileio->readMeteo(fileio->m_pRadiosityebXml->meteoxml.meteofile,modelio->meteos,modelio->meta);
    fileio->readAtom(fileio->m_pRadiosityebXml->meteoxml.rinfile,fileio->m_pRadiosityebXml->meteoxml.rlifile,modelio->atomcoeff);

    return true;
}

bool  RadiosityEB::uploadCanopy(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio) {

    // Utils::readascfileinout(meteofile,0,1,)
//    modelio->aerocond = (fileio->m_pRadiosityebXml->aerocondxml.aerocond);
    //modelio->canopys = fileio->m_pRadiosityebXml->canopyxmls.data();

    return true;
}

void RadiosityEB::run(std::shared_ptr<RadiosityEBIO> &modelio) {

    m_pRT->diffuseproject(modelio);


    // m_pRT->directproject(modelio,modelio->sza,modelio->saa);
    for(int knode = modelio->meta.startNode;knode<modelio->meta.endNode;knode = knode + 1)
    {
        modelio->m_knode = knode;
        Angle angle;
        m_pGeometry->calcSolarAngle(modelio->latlon[0],modelio->latlon[1],modelio->meteos[knode].t,angle,0);
        modelio->sza = angle.sza;
        modelio->saa = angle.saa;

        m_pRT->directproject(modelio,modelio->sza,modelio->saa);
        m_pRT->netrad_shortwave(modelio);

        int kite = 0;
        for(kite = 0;kite < NITE; kite++)
        {
            m_pRT->netrad_longwave(modelio);
            m_pAero->aeresist(modelio);
            m_pBio->suresist(modelio);
            m_pEvapo->evapotranspiration(modelio);
            m_pEB->rebalance(modelio);

            if(kite == 90)
            {
                int aa = 10;
            }

            if(modelio->isclosed) {
               // std::cout << kite << std::endl;
                break;
            } else
            {
              //  std::cout << kite << std::endl;
            }
        }
        std::cout<<std::endl;
        std::cout<<"------------"<<modelio->meteos[knode].t<<" "<<kite<<"-------------"<<std::endl;
        m_pRT->radiosity(modelio);
        m_pRT->writerad(modelio);
        m_pVirtual->observe(modelio);



    }
}

bool RadiosityEB::init(std::shared_ptr<RadiosityEBIO> &modelio) {

    auto npoly = modelio->m_npoly;
    auto &facetebs = modelio->m_facetio->facetEBs;
    auto &meteo = modelio->meteos[0];
    modelio->n_node = modelio->meteos.size();
    for(int k=0;k<npoly;k++)
    {
        facetebs[k].thermals[0] = meteo.Ta + 5 + 273.15;
        facetebs[k].thermals[1] = meteo.Ta + 0.5 + 273.15;
        facetebs[k].cs[0] = meteo.Ca;
        facetebs[k].cs[1] = meteo.Ca;
        facetebs[k].es[0] = meteo.ea;
        facetebs[k].es[1] = meteo.ea;
    }

    return false;
}

