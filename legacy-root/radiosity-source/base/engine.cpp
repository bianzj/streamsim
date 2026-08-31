//
// Created by bianzunjian on 2024/3/21.
//

#include "engine.h"

void Engine::init(Mode mode) {


    if(mode == Mode::eRadiosity){
        m_pRadiosity = std::make_shared<Radiosity>();
        m_pRadiosityio = std::make_shared<RadiosityIO>();
    }else if(mode == Mode::eRadiosityEB)
    {
        m_pRadiosityeb = std::make_shared<RadiosityEB>();
        m_pRadiosityebio = std::make_shared<RadiosityEBIO>();
    }



}

void Engine::input(std::string path, std::string V) {
    m_pFileio = std::make_shared<FileIO>();
    m_pFileio->readXml(path, V);
    m_mode = m_pFileio->m_mode;
    init(m_mode);

    if(m_mode == Mode::eRadiosity)
    {
        m_pRadiosity->upload(m_pFileio, m_pRadiosityio);
        m_pRadiosityio->definedDir = m_pFileio->m_pRadiosityXml->definedDir;
        m_pRadiosityio->projectDir = m_pFileio->m_pRadiosityXml->projectDir;
    }else if(m_mode == Mode::eRadiosityEB)
    {
        m_pRadiosityeb->upload(m_pFileio, m_pRadiosityebio);
        m_pRadiosityebio->definedDir = m_pFileio->m_pRadiosityebXml->definedDir;
        m_pRadiosityebio->projectDir = m_pFileio->m_pRadiosityebXml->projectDir;
    }

}

void Engine::run() {
    if(m_mode == Mode::eRadiosity) {
        m_pRadiosity->run(m_pRadiosityio);
    }else if(m_mode == Mode::eRadiosityEB)
    {
        m_pRadiosityeb->run(m_pRadiosityebio);
    }
}


void Engine::destroy() {

    std::cout<<"finished"<<std::endl;
}





