//
// Created by bianzunjian on 2024/3/24.
//

#ifndef FIELD_RADIOSITY_RADIOSITY_H
#define FIELD_RADIOSITY_RADIOSITY_H

#include "../base/structs.h"
#include "../base/fileio.h"
#include "../base/geometry.h"
#include "../base/scene.h"
#include "../base/rt.h"
#include "../base/compo.h"
#include "../base/virtual.h"

class Radiosity {

public:
    Radiosity(){
        m_pGeometry = std::make_shared<Geometry>();
        m_pScene = std::make_shared<Scene>();
        m_pCompo = std::make_shared<Compo>();
        m_pRT = std::make_shared<RT>();
        m_pVirtual = std::make_shared<Virtual>();
    };


    std::shared_ptr<Geometry> m_pGeometry;
    std::shared_ptr<Scene> m_pScene;
    std::shared_ptr<Compo> m_pCompo;
    std::shared_ptr<RT> m_pRT;
    std::shared_ptr<Virtual> m_pVirtual;

    void upload(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> &modelio);
    //virtual void test();
//    void create( std::shared_ptr<RadiosityIO> &modelio);
    void run( std::shared_ptr<RadiosityIO> &modelio);
    void uploadSetting(std::shared_ptr<FileIO> &fileio,std::shared_ptr<RadiosityIO> &modelio);


};


#endif //FIELD_RADIOSITY_RADIOSITY_H
