//
// Created by bianzunjian on 2024/3/24.
//


//#pragma once
#include "fileio.h"
#include "../thirdparty/tinyxml.h"
#include "structs.h"
#include "../thirdparty//myfunction.h"
#include "stdlib.h"
#include <stack>
#include <string>
#include "xmlexamples.h"
//#include <iomanip>
#include "../thirdparty/nvmath.h"
//

MeteoMeta ext_meta;
AeroCond ext_aeroCond;

bool FileIO::readXml(std::string Path, std::string V) {

    // m_mode = Mode::eRadiosity;
    // m_pRadiosityXml = std::move(xmlexamples.m_pRadiosityXml);

    if (V == "eRadiosity")
    {
        m_mode = Mode::eRadiosity;
    }
    else if (V == "eRadiosityEB")
    {
        m_mode = Mode::eRadiosityEB;
    }
    else {
        return false;
    }
//    m_pRadiosityebXml = std::move(xmlexamples.m_pRadiosityEBXml);
//    return false;

    std::string filePath = Path + "\\Input.xml";
    TiXmlDocument mydoc(filePath.c_str()); // tinyxml.h
    bool isloadOk = mydoc.LoadFile();
    if (!isloadOk)
    {
        std::cout << "could not load the test file.Error:" << mydoc.ErrorDesc() << std::endl;
        exit(1);
    }
    RootElement = mydoc.RootElement(); // root node of xml


    if(m_mode == Mode::eRadiosity)
    {
        m_mode = Mode::eRadiosity;
        m_pRadiosityXml = std::make_shared<RadiosityXml>();
        m_pRadiosityXml->projectDir = Path;
        m_pRadiosityXml->definedDir = "../data/defined/";
        m_pRadiosityXml->settingxml = readSettingXML(RootElement->FirstChild("Control"), m_mode);
        m_pRadiosityXml->lightxml = readLightXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pRadiosityXml->sensorxml = readSensorXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pRadiosityXml->spectralxmls = readSpectralXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pRadiosityXml->thermalxmls = readThermalXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pRadiosityXml->scenexml = readSceneXML(RootElement->FirstChild("Scene"), m_mode);

    }else if(m_mode == Mode::eRadiosityEB){

        m_mode = Mode::eRadiosityEB;
        m_pRadiosityebXml = std::make_shared<RadiosityebXml>();
        m_pRadiosityebXml->projectDir = Path;
//        m_pRadiosityebXml->definedDir = "../../field/defined/";
        m_pRadiosityebXml->definedDir = "../data/defined/";
        m_pRadiosityebXml->aerocondxml= readAeroXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pRadiosityebXml->settingxml = readSettingXML(RootElement->FirstChild("Control"), m_mode);
        m_pRadiosityebXml->sensorxml = readSensorXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pRadiosityebXml->scenexml = readSceneXML(RootElement->FirstChild("Scene"), m_mode);
        m_pRadiosityebXml->spectralxmls = readSpectralXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pRadiosityebXml->canopyxmls = readCanopyXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pRadiosityebXml->propxmls = readPropertyXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pRadiosityebXml->aerocoeffxml = readAeroCoeffXml(RootElement->FirstChild("Attribute"), m_mode);
        m_pRadiosityebXml->meteoxml = readMeteoXML(RootElement->FirstChild("Meteorology"), m_mode);
        m_pRadiosityebXml->lightxml = readLightXML(RootElement->FirstChild("Geometry"), m_mode);

    }
    return true;
}

AeroCoeffXml FileIO::readAeroCoeffXml(TiXmlNode *node, Mode mode) {

    AeroCoeffXml coeffxml;
    TiXmlElement *AeroCoeffNode0 = node->FirstChildElement("Aerodynamics");
    TiXmlElement *AeroCoeffNode = AeroCoeffNode0->FirstChildElement("aerodynamics");

    coeffxml = {{
            stof(AeroCoeffNode->FirstChildElement("zo")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("d")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("Cd")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("rbc")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("CR")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("CD1")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("Psicor")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("CSSOIL")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("rbs")->GetText()),
            stof(AeroCoeffNode->FirstChildElement("rwc")->GetText())
        }
    };
    return coeffxml;
}


AeroCondXml FileIO::readAeroXML(TiXmlNode *node, Mode mode) {

    AeroCondXml aeroCondXml;
    TiXmlElement *AeroNode = node->FirstChildElement("Aerodynamics");
//    TiXmlElement *AeroNode0 = AeroNode->FirstChildElement("aerodynamics");
//    auto AeroNode0  = AeroNode->FirstChildElement("aerodynamics");
//    auto AeroNode0 = a->FirstChildElement("aeroName")->GetText();
//    aeroCondXml = {AeroType::one,
//                   {
//                           stof(AeroNode0->FirstChildElement("L")->GetText()),
//                           stof(AeroNode0->FirstChildElement("ustar")->GetText()),
//                           stof(AeroNode0->FirstChildElement("hc_veg")->GetText()),
//                           stof(AeroNode0->FirstChildElement("hc_build")->GetText()),
//                           stof(AeroNode0->FirstChildElement("lai")->GetText()),
//                           stof(AeroNode0->FirstChildElement("cover")->GetText())},
//                   AeroNode0->FirstChildElement("path")->GetText(),
//                   stof(AeroNode0->FirstChildElement("stepsizeatmos")->GetText())
//    };
    for (TiXmlElement *AeroNode0 = AeroNode->FirstChildElement("aerodynamics");
    AeroNode0 != NULL; AeroNode0 = AeroNode0->NextSiblingElement("aerodynamics")) {
        aeroCondXml = {AeroType::one,
                       {
                        stof(AeroNode0->FirstChildElement("L")->GetText()),
                        stof(AeroNode0->FirstChildElement("ustar")->GetText()),
                        stof(AeroNode0->FirstChildElement("hc_veg")->GetText()),
                        stof(AeroNode0->FirstChildElement("hc_build")->GetText()),
                        stof(AeroNode0->FirstChildElement("lai")->GetText()),
                        stof(AeroNode0->FirstChildElement("cover")->GetText())},
                       "1",
                       stof(AeroNode0->FirstChildElement("stepsizeatmos")->GetText())
        };
        ext_aeroCond = {
                stof(AeroNode0->FirstChildElement("L")->GetText()),
                stof(AeroNode0->FirstChildElement("ustar")->GetText()),
                stof(AeroNode0->FirstChildElement("hc_veg")->GetText()),
                stof(AeroNode0->FirstChildElement("hc_build")->GetText()),
                stof(AeroNode0->FirstChildElement("lai")->GetText()),
                stof(AeroNode0->FirstChildElement("cover")->GetText())};
    }


    return aeroCondXml;
}


SettingXml FileIO::readSettingXML(TiXmlNode *controlNode, Mode mode){

    SettingXml settingxml;
    if (stoi(controlNode->FirstChildElement("isInfinite")->GetText()) == 0){
        settingxml.isInfinite = false;
    }
    else{
        settingxml.isInfinite = true;
    }
    return settingxml;
}

bool FileIO::sonExists(std::string sonName, TiXmlElement* parentEle)
{
    //????????????§Ó??
    for (TiXmlElement* pEle = parentEle->FirstChildElement(); pEle; pEle = pEle->NextSiblingElement())///??????????§ß??
    {
        //recursive find sub node return node pointer
        if (!strcmp(sonName.c_str(), pEle->Value()))
        {
            return 1;
        }
    }
    return 0;
}



std::vector<SpectralXml> FileIO::readSpectralXML(TiXmlNode *node, Mode mode) {

    std::vector<SpectralXml> spectralxmls;
    TiXmlElement *spectralNode = node->FirstChildElement("Spectral");
    if (m_mode == Mode::eRadiosity) {
        for (TiXmlElement *Node = spectralNode->FirstChildElement("spectral");
             Node != NULL; Node = Node->NextSiblingElement("spectral")) {
            SpectralXml spectralXml;
            const char *nameAttribute = Node->Attribute("name");

            if (!nameAttribute) {
                std::cerr << "Error: <spectral> element is missing 'name' attribute." << std::endl;
                continue;
            }

            spectralXml.spectralName = nameAttribute;

            if (Node->Attribute("type") == std::string("custom")) {
                if (m_mode == Mode::eRadiosity) {
                    spectralXml.type = spectralType::CUSTOM;
                }
                if (m_mode == Mode::eRadiosityEB) {
                    spectralXml.type = spectralType::CUSTOM;
                }
                spectralXml.reflectances = {
                        myFunction::mySplitFloat(Node->FirstChildElement("reflectance")->GetText(), ",")};
                spectralXml.transmittance = {
                        myFunction::mySplitFloat(Node->FirstChildElement("transmittance")->GetText(), ",")};
                if (m_mode == Mode::eRadiosityEB) {
                    spectralXml.tau_tir = std::stof(Node->FirstChildElement("tau_TIR")->GetText());
                    spectralXml.refl_tir = std::stof(Node->FirstChildElement("ref_TIR")->GetText());
                }
                if (sonExists("spectral_file", Node->ToElement())){
                    std::string a = Node->FirstChildElement("spectral_file")->GetText();
                    spectralXml.path = a;
                }
            } else if (Node->Attribute("type") == std::string("Prospect")) {
                spectralXml.type = spectralType::LEAFBIO;
                spectralXml.reflectances = {
                        myFunction::mySplitFloat((Node->FirstChildElement("reflectance")->GetText()), ",")};
                spectralXml.transmittance = {
                        myFunction::mySplitFloat((Node->FirstChildElement("transmittance")->GetText()), ",")};
//????????????
                if (m_mode == Mode::eRadiosityEB) {
                    spectralXml.tau_tir = std::stof(Node->FirstChildElement("tau_TIR")->GetText());
                    spectralXml.refl_tir = std::stof(Node->FirstChildElement("ref_TIR")->GetText());
                }


                spectralXml.fp = {
                        stof(Node->FirstChildElement("Cab")->GetText()),
                        stof(Node->FirstChildElement("Cw")->GetText()),
                        stof(Node->FirstChildElement("Cdm")->GetText()),
                        stof(Node->FirstChildElement("Cs")->GetText()),
                        stof(Node->FirstChildElement("N")->GetText()),
                        std::stof(Node->FirstChildElement("ref_TIR")->GetText()),
                        std::stof(Node->FirstChildElement("tau_TIR")->GetText())
                };
            } else if (Node->Attribute("type") == std::string("BSM")) {
                spectralXml.type = spectralType::SOILSET;
                spectralXml.reflectances = {
                        myFunction::mySplitFloat((Node->FirstChildElement("reflectance")->GetText()), ",")};
                spectralXml.transmittance = {
                        myFunction::mySplitFloat((Node->FirstChildElement("transmittance")->GetText()), ",")};
//????????????
                if (m_mode == Mode::eRadiosityEB) {
                    spectralXml.tau_tir = stof(Node->FirstChildElement("tau_TIR")->GetText());
                    spectralXml.refl_tir = stof(Node->FirstChildElement("ref_TIR")->GetText());
                }

                spectralXml.bsm = {
                        stof(Node->FirstChildElement("SMC")->GetText()),
                        stof(Node->FirstChildElement("BSMBrightness")->GetText()),
                        stof(Node->FirstChildElement("BSMlat")->GetText()),
                        stof(Node->FirstChildElement("BSMlon")->GetText()),
                        spectralXml.refl_tir = stof(Node->FirstChildElement("ref_TIR")->GetText()),
                        spectralXml.tau_tir = stof(Node->FirstChildElement("tau_TIR")->GetText())
                };
            }
//        if (spectralXml.spectralName == "soil") {
//            spectralXml.path = "..\\..\\field\\defined\\soilnew.txt";
//        } else if (spectralXml.spectralName == "wall") {
//            spectralXml.path = "..\\..\\field\\defined\\VNIR_construction_tar_asphalt.txt";
//        } else if (spectralXml.spectralName == "roof") {
//            spectralXml.path = "..\\..\\field\\defined\\VNIR_construction_tar_asphalt.txt";
//        } else {}
            spectralxmls.push_back(spectralXml);
        }
    }
    if (m_mode == Mode::eRadiosityEB) {
        for (TiXmlElement *Node = spectralNode->FirstChildElement("spectral");
             Node != NULL; Node = Node->NextSiblingElement("spectral")) {
            SpectralXml spectralXml;
            const char *nameAttribute = Node->Attribute("name");

            if (!nameAttribute) {
                std::cerr << "Error: <spectral> element is missing 'name' attribute." << std::endl;
                continue;
            }

            spectralXml.spectralName = nameAttribute;

            if (Node->Attribute("type") == std::string("custom")) {
                if (m_mode == Mode::eRadiosity) {
                    spectralXml.type = spectralType::OTHER;
                }
                if (m_mode == Mode::eRadiosityEB) {
                    spectralXml.type = spectralType::OTHER;
                }
                spectralXml.reflectances = {
                        myFunction::mySplitFloat(Node->FirstChildElement("reflectance")->GetText(), ",")};
                spectralXml.transmittance = {
                        myFunction::mySplitFloat(Node->FirstChildElement("transmittance")->GetText(), ",")};
                if (m_mode == Mode::eRadiosityEB) {
                    spectralXml.tau_tir = std::stof(Node->FirstChildElement("tau_TIR")->GetText());
                    spectralXml.refl_tir = std::stof(Node->FirstChildElement("ref_TIR")->GetText());
                }
                if (sonExists("spectral_file", Node->ToElement())) {
                    std::string a = Node->FirstChildElement("spectral_file")->GetText();
                    spectralXml.path = a;
                }
            } else if (Node->Attribute("type") == std::string("Prospect")) {
                spectralXml.type = spectralType::LEAFBIO;
                spectralXml.reflectances = {
                        myFunction::mySplitFloat((Node->FirstChildElement("reflectance")->GetText()), ",")};
                spectralXml.transmittance = {
                        myFunction::mySplitFloat((Node->FirstChildElement("transmittance")->GetText()), ",")};
//????????????
                if (m_mode == Mode::eRadiosityEB) {
                    spectralXml.tau_tir = std::stof(Node->FirstChildElement("tau_TIR")->GetText());
                    spectralXml.refl_tir = std::stof(Node->FirstChildElement("ref_TIR")->GetText());
                }


                spectralXml.fp = {
                        stof(Node->FirstChildElement("Cab")->GetText()),
                        stof(Node->FirstChildElement("Cw")->GetText()),
                        stof(Node->FirstChildElement("Cdm")->GetText()),
                        stof(Node->FirstChildElement("Cs")->GetText()),
                        stof(Node->FirstChildElement("N")->GetText())
                };
            } else if (Node->Attribute("type") == std::string("BSM")) {
                spectralXml.type = spectralType::SOILSET;
                spectralXml.reflectances = {
                        myFunction::mySplitFloat((Node->FirstChildElement("reflectance")->GetText()), ",")};
                spectralXml.transmittance = {
                        myFunction::mySplitFloat((Node->FirstChildElement("transmittance")->GetText()), ",")};
//????????????
                if (m_mode == Mode::eRadiosityEB) {
                    spectralXml.tau_tir = stof(Node->FirstChildElement("tau_TIR")->GetText());
                    spectralXml.refl_tir = stof(Node->FirstChildElement("ref_TIR")->GetText());
                }

                spectralXml.bsm = {
                        stof(Node->FirstChildElement("SMC")->GetText()),
                        stof(Node->FirstChildElement("BSMBrightness")->GetText()),
                        stof(Node->FirstChildElement("BSMlat")->GetText()),
                        stof(Node->FirstChildElement("BSMlon")->GetText())
                };
            }
            spectralxmls.push_back(spectralXml);
        }
//    spectralxmls.push_back(spectralXml);
    }
    return spectralxmls;
}


std::vector<ThermalXml> FileIO::readThermalXML(TiXmlNode *node, Mode mode) {



    std::vector<ThermalXml> thermalXmls;
    TiXmlElement* thermalNode = node->FirstChildElement("Thermal");
    for (TiXmlElement* Node = thermalNode->FirstChildElement("thermal"); Node != NULL; Node = Node->NextSiblingElement()){
        ThermalXml thermalxml;
        thermalxml = {
                Node->Attribute("name"),
                stof(Node->FirstChildElement("sunlitTemperature")->GetText()),
                stof(Node->FirstChildElement("shadedTemperature")->GetText())
        };
        thermalXmls.push_back(thermalxml);
    }
    return thermalXmls;
}

SensorXml FileIO::readSensorXML(TiXmlNode *node, Mode mode){

    SensorXml sensorxml;
    TiXmlElement* sensorEle = node->FirstChildElement("Sensor");
//???
    for (TiXmlElement* pEle = sensorEle->FirstChildElement(); pEle != NULL; pEle = pEle->NextSiblingElement())
    {
//        SensorStruct temp;
        sensorxml.name = pEle->Attribute("name");
//        sensorxml.projection = pEle->FirstChildElement("projWay")->GetText();
//        sensorxml.projection = Projection::PARALLAL;
        if (sonExists("FOV", pEle))
        {
//            temp.FOV = std::stoi(pEle->FirstChildElement("FOV")->GetText());
        }
        else
        {
//            temp.FOV = -1;
        }
        sensorxml.resolution = {std::stof(pEle->FirstChildElement("pixelResolutionX")->GetText()),
                                std::stof(pEle->FirstChildElement("pixelResolutionY")->GetText())};
//        temp.pixelResolutionX = std::stof(pEle->FirstChildElement("pixelResolutionX")->GetText());
//        temp.pixelResolutionY = std::stof(pEle->FirstChildElement("pixelResolutionY")->GetText());

        sensorxml.waves = myFunction::mySplitFloat(pEle->FirstChildElement("controlBand")->GetText(), ",");

        TiXmlElement* allViewAngleEle = pEle->FirstChildElement("viewAngle");
        for (TiXmlElement* subEle = allViewAngleEle->FirstChildElement(); subEle != NULL; subEle = subEle->NextSiblingElement())
        {
            std::string type = subEle->Attribute("type");
            if (type == "custom")
            {
                glm::vec2 viewanglesk;
                for (TiXmlElement* viewAngleIter = subEle->FirstChildElement(); viewAngleIter != NULL; viewAngleIter = viewAngleIter->NextSiblingElement())
                {
                    nvmath::vec2f viewAngleTemp = { myFunction::mySplitFloat(viewAngleIter->GetText(), ",")[0],
                                                    myFunction::mySplitFloat(viewAngleIter->GetText(), ",")[1] };
                    viewanglesk = {viewAngleTemp[0],
                                   viewAngleTemp[1]};
                    sensorxml.viewAngles.push_back(viewanglesk);
//                    temp.viewZenith.push_back(viewAngleTemp[0]);
//                    temp.viewAzimuth.push_back(viewAngleTemp[1]);
                }
            }
            else if (type == "BRF")
            {
                if (stoi(subEle->FirstChildElement("SPP")->GetText()) == 1){
                    int VzaMax = stoi(subEle->FirstChildElement("vzaMax")->GetText());
                    int vzaStep = stoi(subEle->FirstChildElement("vzaStep")->GetText());
                    for (int k = 0; k<=VzaMax; k += vzaStep){
                        nvmath::vec2f viewAngleTemp = {k, 0};
                        glm::vec2 viewanglesk = {viewAngleTemp[0],
                                                 viewAngleTemp[1]};
                        sensorxml.viewAngles.push_back(viewanglesk);
                    }

                }
            }
            else if (type == "albedo")
            {
                int isHemisphere = stoi(subEle->FirstChildElement("enabled")->GetText());
                int hemiAngleNum = stoi(subEle->FirstChildElement("angleNum")->GetText());
            }
        }

//        sensorDatasets.push_back(temp);
    }
// ????sensorxml?????bool isImage{true};
//    bool isAlbedo{false};
//    bool isTemperature{true};
//    bool isDisplay{false};????------------------------------------------------------------------------------

//    sensorxml.viewAngles = {{0, 0}};

    TiXmlNode *controlnode = RootElement->FirstChild("Control");
    if (sonExists("isAlbedo", controlnode->ToElement()))
    {
        sensorxml.isAlbedo = stoi(controlnode->FirstChildElement("isAlbedo")->GetText());
    }
    if (sonExists("isImage", controlnode->ToElement()))
    {
        sensorxml.isImage = stoi(controlnode->FirstChildElement("isImage")->GetText());
    }
    if (sonExists("isTemperature", controlnode->ToElement()))
    {
        sensorxml.isTemperature = stoi(controlnode->FirstChildElement("isTemperature")->GetText());
    }

    if (sonExists("isDisplay", controlnode->ToElement()))
    {
        sensorxml.isDisplay = stoi(controlnode->FirstChildElement("isDisplay")->GetText());
    }

    return sensorxml;


}
LightXml FileIO::readLightXML(TiXmlNode *geometryNode, Mode mode){

    LightXml lightxml;
//    AtomCondXml atomCondXml;
    TiXmlElement* lightEle = geometryNode->FirstChildElement("Light");
    for (TiXmlElement* pEle = lightEle->FirstChildElement(); pEle != NULL; pEle = pEle->NextSiblingElement())
    {

//        LightStruct temp;

        lightxml.name = pEle->Attribute("name");

//        temp.isSun = stoi(pEle->FirstChildElement("isSun")->GetText());
//        glm::vec2 sunAnglesk;
        TiXmlElement* lightAngleEle = pEle->FirstChildElement("lightAngle");
        for (TiXmlElement* lightAngleIter = lightAngleEle->FirstChildElement(); lightAngleIter != NULL; lightAngleIter = lightAngleIter->NextSiblingElement())
        {

            nvmath::vec2f lightAngleTemp = { myFunction::mySplitFloat(lightAngleIter->GetText(), ",")[0],
                                             myFunction::mySplitFloat(lightAngleIter->GetText(), ",")[1] };
            lightxml.solarAngle = {lightAngleTemp[0], lightAngleTemp[1]};

        }

        lightxml.skyTemperature = stof(pEle->FirstChildElement("skyTemperature")->GetText());
        if (sonExists("directScatteringRatio", pEle->ToElement())){
            float k;
            k = stof(pEle->FirstChildElement(("directScatteringRatio"))->GetText());
            lightxml.direct = k;
            lightxml.diffuse = 1 - lightxml.direct;
            lightxml.solarTemperature = 6000;
        }
        else{
            if (m_mode == Mode::eRadiosityEB){
//                lightxml.direct = 0.9;
//                lightxml.diffuse = 0.1;
                m_pRadiosityebXml->meteoxml.rinfile = pEle->FirstChildElement("esunFileName")->GetText();
                m_pRadiosityebXml->meteoxml.rlifile = pEle->FirstChildElement("eskyFileName")->GetText();
            }

        }
    }
//-----------------------------------------x+y = 1, x/y = k,y = x=k-kx,x/(1+k) =k,x=k(1+k)
//-----------------------------------------
//    lightxml.direct = 0.9;
//    lightxml.diffuse = 0.1;

    return lightxml;
}

SceneXml FileIO::readSceneXML(TiXmlNode *sceneNode, Mode mode) {

    SceneXml sceneXml;
    sceneXml.sceneSize = {
            stof(sceneNode->FirstChildElement("sceneSizeX")->GetText()),
            stof(sceneNode->FirstChildElement("sceneSizeY")->GetText()),
            stof(sceneNode->FirstChildElement("Height")->GetText())
    };

// ------------------------------------------------------
    sceneXml.sceneOrigin = {0, 0, 0};
// ------------------------------------------------------

    if (sonExists("bgBioType", sceneNode->ToElement())) {
//        sceneXml.background.bgPropName = sceneNode->FirstChildElement("bgBioName")->GetText();
        sceneXml.background.bgPropName = sceneNode->FirstChildElement("bgBioName")->GetText();
    } else {
        sceneXml.background.bgPropName = "soilset";
    }

//    if (sceneXml.background.bgPropName == "soilSet") {
//        sceneXml.background.bgPropName = "soilset";
//    }

    if (sonExists("bgSpectral", sceneNode->ToElement())) {
        sceneXml.background.spectralName = sceneNode->FirstChildElement("bgSpectral")->GetText();
    }
    sceneXml.background.type = Type::SOIL;

//-------------------------------------------------------
    if (sonExists("bgThermal", sceneNode->ToElement())) {
        if (sceneNode->FirstChildElement("bgThermal")->GetText() != NULL) {
            sceneXml.background.thermalName = sceneNode->FirstChildElement("bgThermal")->GetText();
        }
    }
//-------------------------------------------------------
    sceneXml.background.isDEM = 0;
//    sceneXml.stepsize_surface = 1;
//??????dem?????????????false
    if (sonExists("isDEM", RootElement->FirstChildElement("Control"))) {
        auto a = stoi(RootElement->FirstChild("Control")->FirstChildElement("isDEM")->GetText());
//        if (a && strcmp(a, "false") == 0) {
//            sceneXml.background.isDEM = false;
//        } else {
//            sceneXml.background.isDEM = true;
//        }
        sceneXml.background.isDEM = a;
    }

    if (sceneXml.background.isDEM) {
        sceneXml.background.DEMPath = sceneNode->FirstChildElement("DEMPath")->GetText();
        sceneXml.background.demResolution = {
                stof(sceneNode->FirstChildElement("DEMresolutionX")->GetText()),
                stof(sceneNode->FirstChildElement("DEMresolutionY")->GetText())
        };
    }

    if (mode == Mode::eRadiosity) {
        std::vector<ObjEntity> ObjEntities;
        TiXmlElement *objNode = sceneNode->FirstChildElement("Object");
        for (TiXmlElement *node = objNode->FirstChildElement(); node != NULL; node = node->NextSiblingElement()) {
            ObjEntity objEntity;
            objEntity.objName = node->Attribute("objName");
            objEntity.filePath = node->FirstChildElement("objectfile")->GetText();
            objEntity.meshNames = {myFunction::mySplitStr(node->FirstChildElement("meshNames")->GetText(), ",")};
            objEntity.spectralNames = {
                    myFunction::mySplitStr(node->FirstChildElement("spectralNames")->GetText(), ",")};
            if (sonExists("thermalNames", node)) {
                objEntity.thermalNames = {
                        myFunction::mySplitStr(node->FirstChildElement("thermalNames")->GetText(), ",")};
            }
            objEntity.isLarge = stoi(node->FirstChildElement("isLarge")->GetText());

            objEntity.isdisfromFile = stoi(node->FirstChildElement("isdisfromFile")->GetText());
            if (objEntity.isdisfromFile == 1){
                objEntity.distributefile = node->FirstChildElement("objectPosition")->GetText();
            }
            if (objEntity.isdisfromFile == 0){
                objEntity.objDistributions = {{2.5,2.5,0},{1.5,1.5,0},{3.5,3.5,0},{1.5,3.5,0},{3.5,1.5,0}};
                objEntity.scales = {1,1,1,1,1};
                objEntity.rotations = {0,0,0,0,0};
            }
            objEntity.isLarge = stoi(node->FirstChildElement("isLarge")->GetText());
            ObjEntities.push_back(objEntity);
        }

        sceneXml.objEntities = ObjEntities;
    }

    if (mode == Mode::eRadiosityEB) {
        std::vector<ObjEntityPlus> ObjEntities;
        TiXmlElement *objNode = sceneNode->FirstChildElement("Object");
        for (TiXmlElement *node = objNode->FirstChildElement(); node != NULL; node = node->NextSiblingElement()) {
            ObjEntityPlus objEntity;
            objEntity.objName = node->Attribute("objName");
            objEntity.filePath = node->FirstChildElement("objectfile")->GetText();
            objEntity.meshNames = {myFunction::mySplitStr(node->FirstChildElement("meshNames")->GetText(), ",")};
            objEntity.spectralNames = {
                    myFunction::mySplitStr(node->FirstChildElement("spectralNames")->GetText(), ",")};
            if (sonExists("thermalNames", node)) {
                objEntity.thermalNames = {
                        myFunction::mySplitStr(node->FirstChildElement("thermalNames")->GetText(), ",")};
            }
            if (sonExists("propNames", node)) {
                objEntity.propNames = {
                        myFunction::mySplitStr(node->FirstChildElement("propNames")->GetText(), ",")};
            }
            if (sonExists("canopyNames", node)) {
                objEntity.canopyNames = {
                        myFunction::mySplitStr(node->FirstChildElement("canopyNames")->GetText(), ",")};
            }
            if (sonExists("types", node)) {
                auto a = node->FirstChildElement("types")->GetText();
                if(strcmp(node->FirstChildElement("types")->GetText(), "vegetation") == 0){
                    objEntity.types = {Type::VEGETATION};
                }
                if(strcmp(node->FirstChildElement("types")->GetText(), "soil") == 0) {
                    objEntity.types = {Type::SOIL};
                }
            }
            objEntity.isdisfromFile = stoi(node->FirstChildElement("isdisfromFile")->GetText());

            if (objEntity.isdisfromFile == 1){
                objEntity.distributefile = node->FirstChildElement("objectPosition")->GetText();
            }
            if (objEntity.isdisfromFile == 0){
                objEntity.objDistributions = {{2.5,2.5,0},{1.5,1.5,0},{3.5,3.5,0},{1.5,3.5,0},{3.5,1.5,0}};
                objEntity.scales = {1,1,1,1,1};
                objEntity.rotations = {0,0,0,0,0};
            }
            objEntity.isLarge = stoi(node->FirstChildElement("isLarge")->GetText());
            ObjEntities.push_back(objEntity);
        }
        sceneXml.objEntityplus = ObjEntities;
    }

    return sceneXml;
}

std::vector<CanopyXml> FileIO::readCanopyXML(TiXmlNode *node, Mode mode) {

    std::vector<CanopyXml> CanopyXmls;
    TiXmlElement* canopyNode = node->FirstChildElement("Canopy");
    for (TiXmlElement* Node = canopyNode->FirstChildElement("canopy"); Node != NULL; Node = Node->NextSiblingElement()){
        CanopyXml canopyXml;
        canopyXml.canopyName = Node->Attribute("name");
        canopyXml.canopy = {
                stof(Node ->FirstChildElement("lai")->GetText()),
                stof(Node ->FirstChildElement("density")->GetText()),
                stof(Node ->FirstChildElement("hc")->GetText()),
                1,
                stof(Node ->FirstChildElement("G")->GetText()),
                stof(Node ->FirstChildElement("LIDFa")->GetText()),
                stof(Node ->FirstChildElement("LIDFb")->GetText()),
                stof(Node ->FirstChildElement("hspot")->GetText()),
                stof(Node ->FirstChildElement("leafwidth")->GetText())
        };
        CanopyXmls.push_back(canopyXml);
    }
//    CanopyXml canopyXml = {"crown", {1, 1, 1, 1, 0.5, -0.35, -0.15, 0.2, 0.2}};
//    CanopyXmls ={canopyXml};

    return CanopyXmls;
}

std::vector<PropertyXml> FileIO::readPropertyXML(TiXmlNode *node, Mode mode) {

    std::vector<PropertyXml> propxmls;
    TiXmlElement* chemstry_node = node->FirstChildElement("Biochemistry");
    for (TiXmlElement* Node = chemstry_node->FirstChildElement(); Node != NULL; Node = Node->NextSiblingElement()){
        PropertyXml propertyXml;
        if (sonExists("leafBio", Node)){
            TiXmlElement* leafNode = Node->FirstChildElement("leafBio");
            propertyXml.name = leafNode->Attribute("name");
//            propertyXml.name = "tree";
            if (propertyXml.name == "leafBio"){
                propertyXml.name = "leafbio";
            }
            propertyXml.type = Type::VEGETATION;
            propertyXml.leafbio = LeafBio{
                    stof(leafNode->FirstChildElement("Vcmax")->GetText()),
                    stof(leafNode->FirstChildElement("m")->GetText()),
                    stof(leafNode->FirstChildElement("BallBerry")->GetText()),
                    stof(leafNode->FirstChildElement("Type")->GetText()),
                    stof(leafNode->FirstChildElement("kV")->GetText()),
                    stof(leafNode->FirstChildElement("Rdparam")->GetText()),
                    {
                            myFunction::mySplitFloat(leafNode->FirstChildElement("Tparam")->GetText(), ",")[0],
                            myFunction::mySplitFloat(leafNode->FirstChildElement("Tparam")->GetText(), ",")[1],
                            myFunction::mySplitFloat(leafNode->FirstChildElement("Tparam")->GetText(), ",")[2],
                            myFunction::mySplitFloat(leafNode->FirstChildElement("Tparam")->GetText(), ",")[3],
                            myFunction::mySplitFloat(leafNode->FirstChildElement("Tparam")->GetText(), ",")[4],
                    },
                    stof(leafNode->FirstChildElement("Tyear")->GetText()),
                    stof(leafNode->FirstChildElement("beta")->GetText()),
                    stof(leafNode->FirstChildElement("kNPQs")->GetText()),
                    stof(leafNode->FirstChildElement("qLs")->GetText()),
                    stof(leafNode->FirstChildElement("stressfactor")->GetText()),
                    (stoi(leafNode->FirstChildElement("Tcor")->GetText()))
            };
            propxmls.push_back(propertyXml);
//            continue;
        }
        if (sonExists("soilSet", Node)){
            PropertyXml propertyXml2;
            TiXmlElement* soilNode = Node->FirstChildElement("soilSet");
            propertyXml2.name = soilNode->Attribute("name");
//            propertyXml2.name = "soilset";
            if (propertyXml2.name == "soil"){
                propertyXml2.name = "soilset";
            }
            propertyXml2.type = Type::SOIL;
            propertyXml2.soilset = SoilSet{
                    stoi(soilNode->FirstChildElement("method")->GetText()),
                    stof(soilNode->FirstChildElement("rss")->GetText()),
                    stof(soilNode->FirstChildElement("cs")->GetText()),
                    stof(soilNode->FirstChildElement("rhos")->GetText()),
                    stof(soilNode->FirstChildElement("lambdas")->GetText()),
                    stof(soilNode->FirstChildElement("SMC")->GetText()),
                    0.01,
                    stof(soilNode->FirstChildElement("rbs")->GetText())
//                    0.45
            };
            propxmls.push_back(propertyXml2);
////            continue;
//
        }
    }
    return propxmls;

}

MeteoXml FileIO::readMeteoXML(TiXmlNode *node, Mode mode) {


    MeteoXml meteoxml;
    MeteoMeta meta;
    meta.startNode = stoi(node->FirstChildElement("startTimeNode")->GetText());
    meta.endNode = stoi(node->FirstChildElement("endTimeNode")->GetText());
    meta.z = stof(node->FirstChildElement("z")->GetText());
    meta.Tsold = stof(node->FirstChildElement("Tsold")->GetText());
    meta.SatWater = stof(node->FirstChildElement("SatWater")->GetText());
    meta.dTime = stof(node->FirstChildElement("dTime")->GetText());
    meteoxml.meteofile = node->FirstChildElement("filePath")->GetText();
    meteoxml.meta = meta;
    m_pRadiosityebXml->latlon = glm::vec2(
            stof(node->FirstChildElement("Latitude")->GetText()),
            stof(node->FirstChildElement("Longitude")->GetText())
            );
//    m_meteoXml = meteoxml;
    ext_meta = meta;
    meteoxml.aerocond = ext_aeroCond;
    return meteoxml;

}


void FileIO::readDefined(std::shared_ptr<DefinedIO> & definedio) {

    //std::string optfile = "D:\\work\\field\\field\\defined\\optipar.txt";

    definedio->definedDir = m_pRadiosityebXml->definedDir;
    std::string predifineDir = definedio->definedDir;
    std::string infileName = predifineDir + "optipar.txt";
    int num = 1;
    Utils::readascfileinout(infileName, 0, 0, definedio->m_optCoeff.wl_, num);
    Utils::readascfileinout(infileName, 0, 1, definedio->m_optCoeff.nr_, num);
    Utils::readascfileinout(infileName, 0, 2, definedio->m_optCoeff.kab_, num);
    Utils::readascfileinout(infileName, 0, 3, definedio->m_optCoeff.kca_, num);
    Utils::readascfileinout(infileName, 0, 4, definedio->m_optCoeff.ks_, num);
    Utils::readascfileinout(infileName, 0, 5, definedio->m_optCoeff.kw_, num);
    Utils::readascfileinout(infileName, 0, 6, definedio->m_optCoeff.kdm_, num);
    Utils::readascfileinout(infileName, 0, 7, definedio->m_optCoeff.phiI_, num);
    Utils::readascfileinout(infileName, 0, 8, definedio->m_optCoeff.phiII_, num);
    Utils::readascfileinout(infileName, 0, 9, definedio->m_optCoeff.kcaV_, num);
    Utils::readascfileinout(infileName, 0, 10, definedio->m_optCoeff.kcaZ_, num);
    Utils::readascfileinout(infileName, 0, 11, definedio->m_optCoeff.kcant_, num);
    Utils::readascfileinout(infileName, 0, 12, definedio->m_optCoeff.kcaV2_, num);
    Utils::readascfileinout(infileName, 0, 13, definedio->m_optCoeff.phi_, num);
    Utils::readascfileinout(infileName, 0, 14, definedio->m_optCoeff.gsv1_, num);
    Utils::readascfileinout(infileName, 0, 15, definedio->m_optCoeff.gsv2_, num);
    Utils::readascfileinout(infileName, 0, 16, definedio->m_optCoeff.gsv3_, num);
    Utils::readascfileinout(infileName, 0, 17, definedio->m_optCoeff.nw_, num);
    // return false;
}

void FileIO::readMeteo(std::string meteofile, std::vector<Meteo> &meteos,  MeteoMeta &meta)
{

    //auto & meteofile = m_pRadiosityebXml->meteoxml.meteofile;



    std::ifstream infile(meteofile);
    std::vector<std::string> fields;
    std::string deli(" "), line;




    float Tsold = 25;
    float SatWater = 0.45;
    float dTime = 1800;
    dTime = 3600;


    std::getline(infile, line);
    fields = Utils::splitt(line, deli);
    int n_node = std::stoi(fields[0].c_str());
//    meta.z = std::atof(fields[1].c_str());
//    meta.u = std::atof(fields[2].c_str());
//    meta.Ta = std::atof(fields[3].c_str());
//    meta.ea = std::atof(fields[4].c_str());
//    meta.p = std::atof(fields[5].c_str());
//    meta.Oa = std::atof(fields[6].c_str());
//    meta.ca = std::atof(fields[7].c_str());
//    meta.sm = std::atof(fields[8].c_str());
//    meta.Rin = std::atof(fields[9].c_str());
//    meta.Rli = std::atof(fields[10].c_str());
//    meta.Tsold = std::atof(fields[11].c_str());
//    meta.SatWater = std::atof(fields[12].c_str());
//    meta.dTime = std::atof(fields[13].c_str());

    meta.z = ext_meta.z;//15;
//    meta.sm = ext_meta.sm;//0.25;
    meta.sm = 0.25;//0.25;
//    meta.ea = ext_meta.ea;//15;
    meta.ea = 15;//15;
//    meta.ca = ext_meta.ca;//380
    meta.ca = 380;//380
//    meta.Oa= ext_meta.Oa;//209
    meta.Oa = 209;//209

    meta.dTime = 600;
    dTime = meta.dTime;

    meta.Tsold = ext_meta.Tsold;//25
    meta.SatWater = ext_meta.SatWater;//0.45;
    meta.dTime = ext_meta.dTime;//1800;
//    meta.startNode = std::atof(fields[14].c_str());
//    meta.endNode = std::atof(fields[15].c_str());
    meta.startNode = ext_meta.startNode;
    meta.endNode = ext_meta.endNode;

    int meteoNum = 0;
    float z = ext_meta.z;
    float sm = meta.sm;
    float Ca = meta.ca;
    float Oa = meta.Oa;

    for (int i = 0; i < n_node; i++)
    {
        std::getline(infile, line);
        fields = Utils::splitt(line, deli);

        Meteo mi;
        mi.t = std::atof(fields[0].c_str());
        mi.Ta = std::atof(fields[1].c_str());
        mi.ea = std::atof(fields[2].c_str());
        mi.p = std::atof(fields[3].c_str());

        if(meta.u > 0){
            mi.u = meta.u;
        }else{
            mi.u = std::atof(fields[4].c_str());
        }

        mi.Rin = std::atof(fields[5].c_str());
        mi.Rli = std::atof(fields[6].c_str());
        mi.sm = sm;
        mi.z = z;
        mi.Ca = Ca;
        mi.Oa = Oa;
        mi.dTime = dTime;

        meteos.emplace_back(mi);
        //m_meteoParams.emplace_back(mi);
    }

//    meta = fileio->m_pVoxelLstXml->meteoxml.meta;





}


void FileIO::readAtom(std::string rinfile,std::string rlifile,AtomCoeff &atomcoeff) {

    for (int i = 0; i <  2001 ; i++)
    {
        atomcoeff.wl[i] = 400 + i;
    }
    for (int i = 0; i < 126; i++)
    {
        atomcoeff.wl[i + 2001] = 2500 + i * 100;
    }
    for (int i = 0; i < 35; i++)
    {
        atomcoeff.wl[i + 2127] = 16000 + i * 1000;
    }


    // atomCond;
    float  *esun_, *esky_, *fesky_, *fesun_;
    int num = 1;
    // wave_ = Utils::infile2num(predifineDir+'Esk', 0, 0, num);
    esun_ = Utils::readascfile(rinfile, 0, 0, num);
    esky_ = Utils::readascfile(rlifile, 0, 0, num);
    fesky_ = new float[num];
    fesun_ = new float[num];

    float TsEsky = 0, TlEsky = 0, TlEsun = 0, TsEsun = 0, tstot = 0, tltot = 0, temp1, temp2, step;
    int b1 = N1;
    int b2 = N1+N2;

    // ???
    for (int i = 0; i < b1 - 1; i++)
    {
        temp1 = (esky_[i] + esky_[i + 1]) / 2.0;
        step = atomcoeff.wl[i + 1] - atomcoeff.wl[i];
        temp2 = (esun_[i] + esun_[i + 1]) / 2.0;
        TsEsky += temp1 * step;
        TsEsun += temp2 * step;
    }
    tstot = (TsEsky + TsEsun) * 0.001;
    for (int i = 0; i < b1; i++)
    {
        fesky_[i] = esky_[i] / tstot;
        fesun_[i] = esun_[i] / tstot;
    }
    // ????
    for (int j = b1; j < b2 - 1; j++)
    {
        temp1 = (esky_[j] + esky_[j + 1]) / 2.0;
        step = atomcoeff.wl[j + 1] - atomcoeff.wl[j];
        temp2 = (esun_[j] + esun_[j + 1]) / 2.0;
        TlEsky += temp1 * step;
        TlEsun += temp2 * step;
    }
    tltot = (TlEsky + TlEsun) * 0.001;
    for (int i = b1; i < b2; i++)
    {
        fesky_[i] = esky_[i] / tltot;
        fesun_[i] = esun_[i] / tltot;
    }

    for (int i = 0; i < b2; i++)
    {
        atomcoeff.fesun[i] = fesun_[i];
        atomcoeff.fesky[i] = fesky_[i];
    }

    delete[] fesky_;
    delete[] fesun_;
    delete[] esun_;
    delete[] esky_;


}

