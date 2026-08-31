//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_ENGINE_H
#define FIELD_RADIOSITY_ENGINE_H


#include "../radiosity/radiosity.h"
#include "../radiosity/radiosityio.h"
#include "../radiosityeb/radiosityeb.h"
#include "../radiosityeb/radiosityebio.h"

class Engine {

public:

    Engine() = default;


    void input(std::string path, std::string V);
    void run();
    void destroy();


    void init(Mode mode);
    Mode m_mode;
    std::shared_ptr<FileIO>       m_pFileio;
    std::shared_ptr<Radiosity>    m_pRadiosity;
    std::shared_ptr<RadiosityIO>  m_pRadiosityio;
    std::shared_ptr<RadiosityEB>    m_pRadiosityeb;
    std::shared_ptr<RadiosityEBIO>  m_pRadiosityebio;


};


#endif //FIELD_RADIOSITY_ENGINE_H
