//
// Created by bianzunjian on 2024/3/24.
//

#include "radiosity.h"

void Radiosity::upload(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> &modelio) {


    m_pCompo->createCompOptical(fileio, modelio);
    m_pScene->createObjScene(fileio,modelio);
    m_pScene->fromobj2facet(modelio);
    m_pGeometry->createGeometry(fileio,modelio);
    m_pGeometry->calcSkyAngle(modelio->skyvza,modelio->skyvaa);
    uploadSetting(fileio,modelio);

}




void Radiosity::run(std::shared_ptr<RadiosityIO> &modelio) {

    m_pRT->directproject(modelio,modelio->sza,modelio->saa);


//     m_pRT->diffuseproject(modelio);
//     m_pRT->directproject(modelio,modelio->sza,modelio->saa);
//
//     m_pRT->radiosity(modelio);
//
//     m_pRT->writerad(modelio);
//
//     m_pVirtual->observe(modelio);
//
//
//
//
//         //m_pRT->directproject(modelio,vza,vaa);
//
// //    m_pRT->directproject(modelio,vza,vaa);
}

void Radiosity::uploadSetting(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> &modelio) {
    modelio->isInfinite = fileio->m_pRadiosityXml->settingxml.isInfinite;
}




