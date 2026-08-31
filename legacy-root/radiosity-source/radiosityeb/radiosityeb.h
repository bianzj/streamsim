//
// Created by bianzunjian on 2024/3/24.
//

#ifndef FIELD_RADIOSITY_RADIOSITYEB_H
#define FIELD_RADIOSITY_RADIOSITYEB_H

#include "../radiosityeb/radiosityebio.h"
#include "../radiosity/radiosity.h"
#include "../base/eb.h"
#include "../base/evapo.h"
#include "../base/aerodynamic.h"
#include "../base/biochemical.h"

class RadiosityEB{

public:
    RadiosityEB(){
        m_pEB = std::make_shared<EB>();
        m_pAero = std::make_shared<Aerodynamic>();
        m_pBio = std::make_shared<BioChemical>();
        m_pEvapo = std::make_shared<Evapo>();
        m_pGeometry = std::make_shared<Geometry>();
        m_pScene = std::make_shared<Scene>();
        m_pCompo = std::make_shared<Compo>();
        m_pRT = std::make_shared<RT>();
        m_pVirtual = std::make_shared<Virtual>();
    };

    void upload(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio);
    void run( std::shared_ptr<RadiosityEBIO> &modelio);

    bool init(std::shared_ptr<RadiosityEBIO> &modelio);
    bool uploadMeteo(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio);
    bool uploadAero(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &voxellstio);
    void uploadSetting(std::shared_ptr<FileIO> &fileio,std::shared_ptr<RadiosityEBIO> &modelio);
    bool uploadDefined(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio);
    bool uploadCanopy(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio);

    std::shared_ptr<EB> m_pEB;
    std::shared_ptr<Evapo> m_pEvapo;
    std::shared_ptr<Aerodynamic> m_pAero;
    std::shared_ptr<BioChemical> m_pBio;
    std::shared_ptr<Geometry> m_pGeometry;
    std::shared_ptr<Scene> m_pScene;
    std::shared_ptr<Compo> m_pCompo;
    std::shared_ptr<RT> m_pRT;
    std::shared_ptr<Virtual> m_pVirtual;

};


#endif //FIELD_RADIOSITY_RADIOSITYEB_H
