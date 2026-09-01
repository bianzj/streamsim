//
// Created by admin on 2024/1/24.
//
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include "fileio.h"
#pragma once
//#include "tinyxml.h"
#include "structs.h"
#include "myfunction.h"
#include "stdlib.h"
#include <stack>
#include <string>
#include "xmlexamples.h"

using namespace std;

bool FileIO::readXml(std::string Path, Mode mode) {
    m_mode = mode;
    m_inputDirectory = Path.empty() ? std::string() : std::filesystem::path(Path).parent_path().string();
    if (Path.empty()) {
        if (m_mode == Mode::eRaytracing) {
            m_pRaytracingXml = xmlexamples.m_pRaytracingXml;
        } else if (m_mode == Mode::eVoxelRT) {
            m_pVoxelrtXml = xmlexamples.m_pVoxelrtXml;
        } else {
            m_pVoxelebXml = xmlexamples.m_pVoxelebXml;
        }
        return true;
    }

    // std::string filePath = Path;
    TiXmlDocument mydoc(Path.c_str()); // tinyxml.h
    bool isloadOk = mydoc.LoadFile();
    if (!isloadOk)
    {
        std::cout << "could not load the test file.Error:" << mydoc.ErrorDesc() << std::endl;
        exit(1);
    }
    RootElement = mydoc.RootElement(); // root node of xml

    if(m_mode == Mode::eRaytracing)
    {
        m_mode = Mode::eRaytracing;
        m_pRaytracingXml = std::make_shared<RaytracingXml>();
        // m_pRaytracingXml->projectDir = Path;
        m_pRaytracingXml->settingxml = readSettingXML(RootElement->FirstChild("Control"), m_mode);
        m_pRaytracingXml->lightxml = readLightXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pRaytracingXml->sensorxml = readSensorXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pRaytracingXml->spectralxmls = readSpectralXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pRaytracingXml->thermalxmls = readThermalXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pRaytracingXml->scenexml = readSceneXML(RootElement->FirstChild("Scene"), m_mode);
    }else if(m_mode == Mode::eVoxelEB){

        m_mode = Mode::eVoxelEB;
        m_pVoxelebXml = std::make_shared<VoxelEBXml>();
        // m_pVoxelebXml->projectDir = Path;

        m_pVoxelebXml->settingxml = readSettingXML(RootElement->FirstChild("Control"), m_mode);
        m_pVoxelebXml->lightxml = readLightXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pVoxelebXml->sensorxml = readSensorXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pVoxelebXml->scenexml = readSceneXML(RootElement->FirstChild("Scene"), m_mode);
        m_pVoxelebXml->spectralxmls = readSpectralXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pVoxelebXml->thermalxmls = readThermalXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pVoxelebXml->canopyxmls = readCanopyXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pVoxelebXml->propxmls = readPropertyXML(RootElement->FirstChild("Attribute"), m_mode);
        // m_pVoxelebXml->atomcondxml = readAtomCondXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pVoxelebXml->meteoxml = readMeteoXML(RootElement->FirstChild("Meteorology"), m_mode);
        m_pVoxelebXml->aerocondxml= readAeroXML(RootElement->FirstChild("Attribute"), m_mode);
    }

    else if(m_mode == Mode::eVoxelRT){
        m_mode = Mode::eVoxelRT;
        m_pVoxelrtXml = std::make_shared<VoxelRTXml>();
        // m_pVoxelrtXml->projectDir = Path;
        m_pVoxelrtXml->settingxml = readSettingXML(RootElement->FirstChild("Control"), m_mode);
        m_pVoxelrtXml->lightxml = readLightXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pVoxelrtXml->sensorxml = readSensorXML(RootElement->FirstChild("Geometry"), m_mode);
        m_pVoxelrtXml->scenexml = readSceneXML(RootElement->FirstChild("Scene"), m_mode);
        m_pVoxelrtXml->spectralxmls = readSpectralXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pVoxelrtXml->canopyxmls = readCanopyXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pVoxelrtXml->propxmls = readPropertyXML(RootElement->FirstChild("Attribute"), m_mode);
        m_pVoxelrtXml->thermalxmls = readThermalXML(RootElement->FirstChild("Attribute"), m_mode);
    }
    return true;

}


AeroCondXml FileIO::readAeroXML(TiXmlNode *node, Mode mode) {

    AeroCondXml aeroCondXml;
    TiXmlElement *AeroNode = node->FirstChildElement("Aerodynamics");
//    aeroCondXml = {
//            AeroType::ONE,
//            {
//
//            }
//    };
//    aeroCondXml = {AeroType::ONE,
//                   {1, 10, 10, 3, 12.}, "", 1000};
    for (TiXmlElement *AeroNode0 = AeroNode->FirstChildElement(); AeroNode0 != NULL; AeroNode0 = AeroNode0->NextSiblingElement()) {

        aeroCondXml = {AeroType::ONE,
                       {stoi(AeroNode0->FirstChildElement("type")->GetText()),
                        stof(AeroNode0->FirstChildElement("L")->GetText()),
                        stof(AeroNode0->FirstChildElement("ustar")->GetText()),
                        stof(AeroNode0->FirstChildElement("hc_veg")->GetText()),
                        stof(AeroNode0->FirstChildElement("hc_build")->GetText()),
                        stof(AeroNode0->FirstChildElement("lai")->GetText()),
                        stof(AeroNode0->FirstChildElement("leaf_width")->GetText())},
                       "1",
                       stof(AeroNode0->FirstChildElement("stepsizeatmos")->GetText())
        };

    }
    return aeroCondXml;
}

MeteoXml FileIO::readMeteoXML(TiXmlNode *node, Mode mode) {


    MeteoXml meteoxml;
    MeteoMeta meta;
    meteoxml.startTimeNode = stoi(node->FirstChildElement("startTimeNode")->GetText());
    meteoxml.endTimeNode = stoi(node->FirstChildElement("endTimeNode")->GetText());
    meta.z = stof(node->FirstChildElement("z")->GetText());
    meta.Tsold = stof(node->FirstChildElement("Tsold")->GetText());
    meta.SatWater = stof(node->FirstChildElement("SatWater")->GetText());
    meta.dTime = stof(node->FirstChildElement("dTime")->GetText());
    meteoxml.meteofile = node->FirstChildElement("filePath")->GetText();
    meteoxml.meta = meta;

    m_meteoXml = meteoxml;
    return meteoxml;

}

AtomCondXml FileIO::readAtomCondXML(TiXmlNode *node, Mode mode) {

    AtomCondXml atomCondXml;
    TiXmlNode* lightNode = node->FirstChildElement("Light");
    for (TiXmlElement* pEle = lightNode->FirstChildElement(); pEle != NULL; pEle = pEle->NextSiblingElement())
    {
        atomCondXml.rlifile = pEle->FirstChildElement("eskyFileName")->GetText();
        atomCondXml.rinfile = pEle->FirstChildElement("esunFileName")->GetText();
        break;

    }


    return atomCondXml;
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
                    stof(soilNode->FirstChildElement("Tsoil")->GetText()),
                    stof(soilNode->FirstChildElement("SMC")->GetText()),
                    stof(soilNode->FirstChildElement("Satwater")->GetText())
//                    0.45
            };
            if (TiXmlElement* model = soilNode->FirstChildElement("brdfModel")) {
                const std::string value = model->GetText();
                propertyXml2.soilset.brdfModel =
                    (value == "Hapke" || value == "hapke" || value == "1") ? 1 : 0;
            }
            if (TiXmlElement* value = soilNode->FirstChildElement("hapkeB0"))
                propertyXml2.soilset.hapkeB0 = stof(value->GetText());
            if (TiXmlElement* value = soilNode->FirstChildElement("hapkeH"))
                propertyXml2.soilset.hapkeH = stof(value->GetText());
            if (TiXmlElement* value = soilNode->FirstChildElement("hapkeG"))
                propertyXml2.soilset.hapkeG = stof(value->GetText());
            propxmls.push_back(propertyXml2);
////            continue;
//
        }
        if (sonExists("waterSet", Node)){
            PropertyXml waterProperty{};
            TiXmlElement* waterNode = Node->FirstChildElement("waterSet");
            waterProperty.name = waterNode->Attribute("name");
            waterProperty.type = Type::WATER;
            waterProperty.waterset = WaterSet{
                stof(waterNode->FirstChildElement("rss")->GetText()),
                stof(waterNode->FirstChildElement("heatCapacity")->GetText()),
                stof(waterNode->FirstChildElement("mixingDepth")->GetText()),
                stof(waterNode->FirstChildElement("evaporationCoefficient")->GetText())
            };
            if (TiXmlElement* model = waterNode->FirstChildElement("brdfModel")) {
                const std::string value = model->GetText();
                waterProperty.waterset.brdfModel =
                    (value == "CoxMunk" || value == "coxMunk"
                     || value == "cox-munk" || value == "1") ? 1 : 0;
            }
            if (TiXmlElement* value = waterNode->FirstChildElement("refractiveIndex"))
                waterProperty.waterset.refractiveIndex = stof(value->GetText());
            if (TiXmlElement* value = waterNode->FirstChildElement("slopeVariance"))
                waterProperty.waterset.slopeVariance = stof(value->GetText());
            if (TiXmlElement* value = waterNode->FirstChildElement("diffuseFraction"))
                waterProperty.waterset.diffuseFraction = stof(value->GetText());
            propxmls.push_back(waterProperty);
        }
    }
    return propxmls;

}

bool FileIO::sonExists(std::string sonName, TiXmlElement* parentEle)
{
    //根据节点名称进行查询
    for (TiXmlElement* pEle = parentEle->FirstChildElement(); pEle; pEle = pEle->NextSiblingElement())///循环下面所有节点
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
            TiXmlElement *spectralFileElement = Node->FirstChildElement("spectral_file");
            const char *spectralFile = spectralFileElement ? spectralFileElement->GetText() : nullptr;
            // Constant values are valid in every mode; use an external spectrum only when a path exists.
            spectralXml.type = (m_mode == Mode::eVoxelEB && spectralFile && spectralFile[0] != '\0')
                ? spectralType::OTHER
                : spectralType::CUSTOM;
            spectralXml.reflectances = {myFunction::mySplitFloat(Node->FirstChildElement("reflectance")->GetText(), ",")};
            spectralXml.transmittance = {myFunction::mySplitFloat(Node->FirstChildElement("transmittance")->GetText(), ",")};
            if (m_mode == Mode::eVoxelEB || m_mode == Mode::eVoxelRT){
                spectralXml.tau_tir = stof(Node->FirstChildElement("tau_TIR")->GetText());
                spectralXml.refl_tir = stof(Node->FirstChildElement("ref_TIR")->GetText());
            }

            if (spectralFile && spectralFile[0] != '\0')
                spectralXml.path = spectralFile;
        } else if (Node->Attribute("type") == std::string("Prospect")) {
            spectralXml.type = spectralType::PROSPECT;
            spectralXml.reflectances = {myFunction::mySplitFloat((Node->FirstChildElement("reflectance")->GetText()), ",")};
            spectralXml.transmittance = {myFunction::mySplitFloat((Node->FirstChildElement("transmittance")->GetText()), ",")};
//红外波段只取一个值
            if (m_mode == Mode::eVoxelEB || m_mode == Mode::eVoxelRT){
                spectralXml.tau_tir = stof(Node->FirstChildElement("tau_TIR")->GetText());
                spectralXml.refl_tir = stof(Node->FirstChildElement("ref_TIR")->GetText());
            }


            spectralXml.fp = {
                    stof(Node->FirstChildElement("Cab")->GetText()),
                    stof(Node->FirstChildElement("Cw")->GetText()),
                    stof(Node->FirstChildElement("Cdm")->GetText()),
                    stof(Node->FirstChildElement("Cs")->GetText()),
                    stof(Node->FirstChildElement("N")->GetText())
            };
        }
        else if (Node->Attribute("type") == std::string("BSM")) {
            spectralXml.type = spectralType::BSM;
            spectralXml.reflectances = {
                    myFunction::mySplitFloat((Node->FirstChildElement("reflectance")->GetText()), ",")};
            spectralXml.transmittance = {
                    myFunction::mySplitFloat((Node->FirstChildElement("transmittance")->GetText()), ",")};
//红外波段只取一个值
            if (m_mode == Mode::eVoxelEB || m_mode == Mode::eVoxelRT){
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

    return spectralxmls;
}

std::vector<ThermalXml> FileIO::readThermalXML(TiXmlNode *node, Mode mode) {
    std::vector<ThermalXml> thermalXmls;
    if (node == nullptr) return thermalXmls;
    TiXmlElement* thermalNode = node->FirstChildElement("Thermal");
    if (thermalNode == nullptr) return thermalXmls;
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


SensorXml FileIO::readSensorXML(TiXmlNode *node, Mode mode){

    SensorXml sensorxml;
    sensorxml.projection = Projection::PARALLAL;
    sensorxml.position = {0.0f, 0.0f, 3000.0f};
    TiXmlElement* sensorEle = node->FirstChildElement("Sensor");

//赋值
    for (TiXmlElement* pEle = sensorEle->FirstChildElement(); pEle != NULL; pEle = pEle->NextSiblingElement())
    {
        // const char* typeAttr = pEle->Attribute("type");
        std::string sensorType;
        if (pEle->Attribute("type") == NULL)
        {
            sensorType = "Multispectral sensor";
        }
        else
        {
            sensorType = pEle->Attribute("type");
        }

        if (sensorType.empty() || sensorType == "Multispectral sensor")
        {
            sensorxml.name = pEle->Attribute("name");
            if (sonExists("projection", pEle))
            {
                std::string projection = pEle->FirstChildElement("projection")->GetText();
                std::transform(projection.begin(), projection.end(), projection.begin(),
                               [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
                if (projection == "perspective" || projection == "central" || projection == "center")
                {
                    sensorxml.projection = Projection::PERSPECTIVE;
                }
            }
            if (sonExists("sensorPosition", pEle))
            {
                const std::vector<float> position = myFunction::mySplitFloat(
                        pEle->FirstChildElement("sensorPosition")->GetText(), ",");
                if (position.size() >= 3)
                {
                    sensorxml.position = {position[0], position[1], std::max(0.01f, position[2])};
                }
            }
            if (sonExists("FOV", pEle))
            {
                sensorxml.sensorFov = std::clamp(std::stof(pEle->FirstChildElement("FOV")->GetText()), 0.1f, 120.0f);
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
                            nvmath::vec2f viewAngleTemp = {k, SAA};
                            glm::vec2 viewanglesk = {viewAngleTemp[0],
                                                     viewAngleTemp[1]};
                            sensorxml.viewAngles.push_back(viewanglesk);
                        }
                        for (int k = 0; k<=VzaMax; k += vzaStep){
                            nvmath::vec2f viewAngleTemp;
                            if (SAA > 180){
                                viewAngleTemp = {k, SAA-180};
                            }
                            else{
                                viewAngleTemp = {k, SAA+180};
                            }
                            glm::vec2 viewanglesk = {viewAngleTemp[0],
                                                     viewAngleTemp[1]};
                            sensorxml.viewAngles.push_back(viewanglesk);
                        }

                    }
                    if (stoi(subEle->FirstChildElement("CSPP")->GetText()) == 1){
                        int VzaMax = stoi(subEle->FirstChildElement("vzaMax")->GetText());
                        int vzaStep = stoi(subEle->FirstChildElement("vzaStep")->GetText());
                        for (int k = 0; k<=VzaMax; k += vzaStep){
                            nvmath::vec2f viewAngleTemp;
                            if (SAA < 270){
                                viewAngleTemp = {k, SAA+90};
                            }
                            else{
                                viewAngleTemp = {k, SAA-270};
                            }

                            glm::vec2 viewanglesk = {viewAngleTemp[0],
                                                     viewAngleTemp[1]};
                            sensorxml.viewAngles.push_back(viewanglesk);
                        }
                        for (int k = 0; k<=VzaMax; k += vzaStep){
                            nvmath::vec2f viewAngleTemp;
                            if (SAA < 90){
                                viewAngleTemp = {k, SAA+270};
                            }
                            else{
                                viewAngleTemp = {k, SAA-90};
                            }

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
                    if (isHemisphere == 1){
                        float theta0, theta1, theta;
                        float r0, r1;
                        int k0, k1, k00;
                        float pi = 3.1415926, dphi;

                        theta0 = pi / 2.0;
                        k0 = hemiAngleNum;
                        r0 = 2.0 * sin(theta0 / 2.0);

                        for (int k = 0; k < 10; k++) {
                            theta1 = theta0 - 2.0 * sin(theta0 / 2.0) * sqrt(pi / k0);
                            r1 = 2.0 * sin(theta1 / 2.0);
                            k1 = k0 * pow(r1 / r0, 2);

                            theta = (theta0 + theta1) / 2.0 * 180.0 / pi;

                            k00 = k0;
                            dphi = 360.0 / k00;
                            for (int kk = 0; kk < k00; kk++) {
                                // 生成每个 viewAngleTemp 值
                                glm::vec2 viewanglesk = {
                                        theta,                          // 每一个 VZA
                                        dphi / 2.0 + dphi * kk          // 每一个 VAA
                                };

                                // 插入到 sensorxml.viewAngles 中
                                sensorxml.viewAngles.push_back(viewanglesk);
                            }

                            if (k1 < 0) {
                                // 如果 k1 小于 0，插入 VZA = 0 和 VAA = 0 的角度
                                glm::vec2 viewanglesk = {0.0f, 0.0f};
                                sensorxml.viewAngles.push_back(viewanglesk);
                                break;
                            }

                            theta0 = theta1;
                            r0 = r1;
                            k0 = k1;


                        }
                    }
                }
            }
        }
        else if (sensorType == "hyperspectral sensor")
        {
            sensorxml.name = pEle->Attribute("name");
            sensorxml.projection = Projection::PARALLAL;
            if (sonExists("FOV", pEle))
            {
    // temp.FOV = std::stoi(pEle->FirstChildElement("FOV")->GetText());
            }
            else
            {
    //            temp.FOV = -1;
            }

            sensorxml.resolution = {std::stof(pEle->FirstChildElement("pixelResolutionX")->GetText()),
                                    std::stof(pEle->FirstChildElement("pixelResolutionY")->GetText())};

            // sensorxml.waves = myFunction::mySplitFloat(pEle->FirstChildElement("controlBand")->GetText(), ",");
            std::string bandsFilePath = pEle->FirstChildElement("bandsFilePath")->GetText();
            std::ifstream file(bandsFilePath);

            if (!file.is_open()) {
                std::cerr << "Can not open file: " << bandsFilePath << std::endl;
                //return 1;
            }
            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty()) {
                    try {
                        // 将字符串转换为float并添加到vector中
                        float num = std::stof(line);
                        sensorxml.waves.push_back(num);
                    }
                    catch (const std::invalid_argument& e) {
                        std::cerr << "无效的数字格式: " << line << std::endl;
                    }
                    catch (const std::out_of_range& e) {
                        std::cerr << "数字超出范围: " << line << std::endl;
                    }
                }
            }
            file.close();




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
                            nvmath::vec2f viewAngleTemp = {k, SAA};
                            glm::vec2 viewanglesk = {viewAngleTemp[0],
                                                     viewAngleTemp[1]};
                            sensorxml.viewAngles.push_back(viewanglesk);
                        }
                        for (int k = 0; k<=VzaMax; k += vzaStep){
                            nvmath::vec2f viewAngleTemp;
                            if (SAA > 180){
                                viewAngleTemp = {k, SAA-180};
                            }
                            else{
                                viewAngleTemp = {k, SAA+180};
                            }
                            glm::vec2 viewanglesk = {viewAngleTemp[0],
                                                     viewAngleTemp[1]};
                            sensorxml.viewAngles.push_back(viewanglesk);
                        }

                    }
                    if (stoi(subEle->FirstChildElement("CSPP")->GetText()) == 1){
                        int VzaMax = stoi(subEle->FirstChildElement("vzaMax")->GetText());
                        int vzaStep = stoi(subEle->FirstChildElement("vzaStep")->GetText());
                        for (int k = 0; k<=VzaMax; k += vzaStep){
                            nvmath::vec2f viewAngleTemp;
                            if (SAA < 270){
                                viewAngleTemp = {k, SAA+90};
                            }
                            else{
                                viewAngleTemp = {k, SAA-270};
                            }

                            glm::vec2 viewanglesk = {viewAngleTemp[0],
                                                     viewAngleTemp[1]};
                            sensorxml.viewAngles.push_back(viewanglesk);
                        }
                        for (int k = 0; k<=VzaMax; k += vzaStep){
                            nvmath::vec2f viewAngleTemp;
                            if (SAA < 90){
                                viewAngleTemp = {k, SAA+270};
                            }
                            else{
                                viewAngleTemp = {k, SAA-90};
                            }

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
                    if (isHemisphere == 1){
                        float theta0, theta1, theta;
                        float r0, r1;
                        int k0, k1, k00;
                        float pi = 3.1415926, dphi;

                        theta0 = pi / 2.0;
                        k0 = hemiAngleNum;
                        r0 = 2.0 * sin(theta0 / 2.0);

                        for (int k = 0; k < 10; k++) {
                            theta1 = theta0 - 2.0 * sin(theta0 / 2.0) * sqrt(pi / k0);
                            r1 = 2.0 * sin(theta1 / 2.0);
                            k1 = k0 * pow(r1 / r0, 2);

                            theta = (theta0 + theta1) / 2.0 * 180.0 / pi;

                            k00 = k0;
                            dphi = 360.0 / k00;
                            for (int kk = 0; kk < k00; kk++) {
                                // 生成每个 viewAngleTemp 值
                                glm::vec2 viewanglesk = {
                                        theta,                          // 每一个 VZA
                                        dphi / 2.0 + dphi * kk          // 每一个 VAA
                                };

                                // 插入到 sensorxml.viewAngles 中
                                sensorxml.viewAngles.push_back(viewanglesk);
                            }

                            if (k1 < 0) {
                                // 如果 k1 小于 0，插入 VZA = 0 和 VAA = 0 的角度
                                glm::vec2 viewanglesk = {0.0f, 0.0f};
                                sensorxml.viewAngles.push_back(viewanglesk);
                                break;
                            }

                            theta0 = theta1;
                            r0 = r1;
                            k0 = k1;


                        }
                    }
                }
            }
        }

    }
// 补充，sensorxml里面的bool isImage{true};
//    bool isAlbedo{false};
//    bool isTemperature{true};
//    bool isDisplay{false};属性------------------------------------------------------------------------------

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
    if (sonExists("isProcess", controlnode->ToElement()))
    {
        sensorxml.isProcess = stoi(controlnode->FirstChildElement("isProcess")->GetText());
    }
    if (sonExists("isOrth", controlnode->ToElement()))
    {
        sensorxml.isOrth = stoi(controlnode->FirstChildElement("isOrth")->GetText());
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
    AtomCondXml atomCondXml;
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
            SAA = lightAngleTemp[1];

        }

        lightxml.skyTemperature = stof(pEle->FirstChildElement("skyTemperature")->GetText());
        if (sonExists("directScatteringRatio", pEle->ToElement())){
            if (m_mode == Mode::eVoxelEB) {
                m_pVoxelebXml->atomcondxml.rinfile = pEle->FirstChildElement("esunFileName")->GetText();
                m_pVoxelebXml->atomcondxml.rlifile = pEle->FirstChildElement("eskyFileName")->GetText();
            }
            float k;
            k = stof(pEle->FirstChildElement(("directScatteringRatio"))->GetText());
            lightxml.direct = k;
            lightxml.diffuse = 1 - lightxml.direct;
            lightxml.solarTemperature = 6000;
        }
        else{
            if (m_mode == Mode::eRaytracing){
//                break;
            }
            else if (m_mode == Mode::eVoxelEB){
                m_pVoxelebXml->atomcondxml.rinfile = pEle->FirstChildElement("esunFileName")->GetText();
                m_pVoxelebXml->atomcondxml.rlifile = pEle->FirstChildElement("eskyFileName")->GetText();
            }

        }
    }
//-----------------------------------------x+y = 1, x/y = k,y = x=k-kx,x/(1+k) =k,x=k(1+k)
//-----------------------------------------


    return lightxml;
}



SettingXml FileIO::readSettingXML(TiXmlNode *controlNode, Mode mode){

    SettingXml settingxml;
    settingxml.maxDepth = stoi(controlNode->FirstChildElement("rayTracingDepth")->GetText());
    settingxml.theGPU = stoi(controlNode->FirstChildElement("GPU")->GetText());
    settingxml.n_sample=32;
    if (sonExists("isUAVtrave", controlNode->ToElement())){
        settingxml.isUAVtrave = stoi(controlNode->FirstChildElement("isUAVtrave")->GetText());
    }



    char szFilePath[MAX_PATH + 1] = { 0 };
    GetModuleFileNameA(NULL, szFilePath, MAX_PATH);
    /*
    strrchr:函数功能：查找一个字符c在另一个字符串str中末次出现的位置（也就是从str的右侧开始查找字符c首次出现的位置），
    并返回这个位置的地址。如果未能找到指定字符，那么函数将返回NULL。
    使用这个地址返回从最后一个字符c到str末尾的字符串。
    */
    (strrchr(szFilePath, '\\'))[0] = 0; // 删除文件名，只获得路径字串//
    std::string exe_path = szFilePath;
    if (mode == Mode::eRaytracing)
    {
        m_pRaytracingXml->definedDir = exe_path;
        //controlNode->FirstChildElement("defineDir")->GetText();
        m_pRaytracingXml->projectDir = controlNode->FirstChildElement("outDir")->GetText();
    }
    if (mode == Mode::eVoxelRT)
    {
        m_pVoxelrtXml->definedDir = exe_path;//controlNode->FirstChildElement("defineDir")->GetText();
        m_pVoxelrtXml->projectDir = controlNode->FirstChildElement("outDir")->GetText();
    }
    if (mode == Mode::eVoxelEB)
    {
        m_pVoxelebXml->definedDir = exe_path;//controlNode->FirstChildElement("defineDir")->GetText();
        m_pVoxelebXml->projectDir = controlNode->FirstChildElement("outDir")->GetText();
    }

    return settingxml;
}

SceneXml FileIO::readSceneXML(TiXmlNode *sceneNode, Mode mode) {

    SceneXml sceneXml;
    sceneXml.background.sceneSize = {
            stof(sceneNode->FirstChildElement("sceneSizeX")->GetText()),
            stof(sceneNode->FirstChildElement("sceneSizeY")->GetText()),
            stof(sceneNode->FirstChildElement("Height")->GetText())
    };
//    sceneSizeDatasets.sceneSizeX = stof(sceneNode->FirstChildElement("sceneSizeX")->GetText());
//    sceneSizeDatasets.sceneSizeY = stof(sceneNode->FirstChildElement("sceneSizeY")->GetText());
// ------------------------------------------------------
    sceneXml.background.sceneOrigin = {0, 0, 0};
// ------------------------------------------------------
    if (sonExists("voxelSize", sceneNode->ToElement()))
    {
        sceneXml.background.stepsize_surface = stof(sceneNode->FirstChildElement("voxelSize")->GetText());
        sceneXml.background.stepsize_height = stof(sceneNode->FirstChildElement("voxelSize")->GetText());
    }
    else
    {
        sceneXml.background.stepsize_surface = 1;
        sceneXml.background.stepsize_height = 1;
    }

    if (sonExists("voxelFillThreshold", sceneNode->ToElement())) {
        sceneXml.background.voxelFillThreshold = std::clamp(
            stof(sceneNode->FirstChildElement("voxelFillThreshold")->GetText()),
            0.0f, 1.0f);
    }

    if (sonExists("bgBioType", sceneNode->ToElement())) {
//        sceneXml.background.bgPropName = sceneNode->FirstChildElement("bgBioName")->GetText();
        sceneXml.background.bgPropName = sceneNode->FirstChildElement("bgBioName")->GetText();
        const std::string bgType = sceneNode->FirstChildElement("bgBioType")->GetText();
        if (bgType == "Water" || bgType == "water") {
            sceneXml.background.type = Type::WATER;
        }
    }
    else
    {
        sceneXml.background.bgPropName = "soilset";
    }


    if (sceneXml.background.bgPropName == "soilSet") {
        sceneXml.background.bgPropName = "soilset";
    }

    if (sonExists("bgSpectral", sceneNode->ToElement())) {
        sceneXml.background.bgSpectralName = sceneNode->FirstChildElement("bgSpectral")->GetText();
    }

//-------------------------------------------------------
    if (sonExists("bgThermal", sceneNode->ToElement())) {
        if(sceneNode->FirstChildElement("bgThermal")->GetText() != NULL){
            sceneXml.background.bgThermalName = sceneNode->FirstChildElement("bgThermal")->GetText();
        }
    }
//-------------------------------------------------------

//暂时没有dem这个功能，设置为false
    sceneXml.background.isDEM = {false};
    if (sonExists("DEM", sceneNode->ToElement()))
    {
        if (sceneNode->FirstChildElement("DEM")->GetText() != NULL){
            sceneXml.background.isDEM = {true};
            sceneXml.background.DEMFile = sceneNode->FirstChildElement("DEM")->GetText();
        }
    }



// 添加一个meteo的经纬度信息-----------------------------------------
    if (sonExists("Meteorology", RootElement->ToElement()))
    {
        TiXmlNode *MeteorologyNode = RootElement->FirstChild("Meteorology");
        if (sonExists("Latitude", MeteorologyNode->ToElement())) {
            sceneXml.background.lat = stof(MeteorologyNode->FirstChildElement("Latitude")->GetText());
        }
        if (sonExists("Longitude", MeteorologyNode->ToElement())) {
            sceneXml.background.lon = stof(MeteorologyNode->FirstChildElement("Longitude")->GetText());
        }
    }
//// obj文件和位置-----------------------
    if (m_mode == Mode::eVoxelEB)
    {
        sceneXml.objEntities = {};
//    PrimEntity treeEntity;
//    PrimEntity buildingEntity;
        std::vector<PrimEntity> PrimEntitys;
// 获取position
        if (sonExists("Object", sceneNode->ToElement())) {
            TiXmlElement *obj = sceneNode->FirstChildElement("Object");
            for (TiXmlElement *node = obj->FirstChildElement(); node != NULL; node = node->NextSiblingElement()) {
                PrimEntity entity;
                const bool voxelizeFromObj =
                    sonExists("fileName", node) || sonExists("objectfile", node);
                const char* objFileText = voxelizeFromObj
                    ? (sonExists("fileName", node)
                        ? node->FirstChildElement("fileName")->GetText()
                        : node->FirstChildElement("objectfile")->GetText())
                    : nullptr;
                entity.primitiveName = node->FirstChildElement("meshNames")->GetText();
                entity.meshNames = {node->FirstChildElement("meshNames")->GetText()};
                std::string name = node->FirstChildElement("types")->GetText();

                if (name == "Building") {
                    const auto spectra = myFunction::mySplitStr(node->FirstChildElement("spectralNames")->GetText(), ",");
                    const std::string wallSpectrum = spectra.empty() ? "soil" : spectra.front();
                    const std::string roofSpectrum = spectra.size() > 1 ? spectra[1] : wallSpectrum;
                    entity.meshNames = {"wall", "roof"};
                    entity.spectralNames = {wallSpectrum, roofSpectrum};
//                    entity.thermalNames = {"wall", "roof"};
                    entity.canopyNames = {node->FirstChildElement("canopyNames")->GetText(),
                                          node->FirstChildElement("canopyNames")->GetText()};
                    entity.propNames = {node->FirstChildElement("bioNames")->GetText(),
                                        node->FirstChildElement("bioNames")->GetText()};
                    entity.type = Type::BUILDING;
                    entity.isshapeFromFile = false;
                    entity.shapefile = " ";
                    entity.isheightFromFile = false;
                    entity.heightfile = "";
//                entity.isdisFromFile = false;
//                entity.distributefile = " ";
                } else if (name == "Vegetation") {
                    entity.canopyNames = {node->FirstChildElement("canopyNames")->GetText()};
                    entity.propNames = {node->FirstChildElement("bioNames")->GetText()};
                    entity.spectralNames = {node->FirstChildElement("spectralNames")->GetText()};

                    if (sonExists("thermalNames", node)){
                        entity.thermalNames = {node->FirstChildElement("thermalNames")->GetText()};
                    }
                    entity.type = Type::VEGETATION;
                    entity.isshapeFromFile = false;
                    entity.shapefile = " ";
                    entity.isheightFromFile = false;
                    entity.heightfile = "";
                    entity.isdisFromFile = true;
                    entity.distributefile = node->FirstChildElement("objectPosition")->GetText();
                } else if (name == "Water" || name == "water") {
                    entity.canopyNames = {""};
                    entity.propNames = {node->FirstChildElement("bioNames")->GetText()};
                    entity.spectralNames = {node->FirstChildElement("spectralNames")->GetText()};
                    entity.type = Type::WATER;
                    entity.isshapeFromFile = false;
                    entity.isheightFromFile = false;
                    entity.isdisFromFile = sonExists("objectPosition", node);
                    if (entity.isdisFromFile) {
                        entity.distributefile = node->FirstChildElement("objectPosition")->GetText();
                    }
                }


                std::string shapetype = node->FirstChildElement("shapeTypes")->GetText();
                cout << shapetype << endl;
                if (shapetype == "ellipsoid" || shapetype == "Ellipsoid") {
                    entity.shape = {ShapeType::ELLIPSOID,
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[2],
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[1],
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[0],
                                    glm::vec3(0, 0, 0)};
                } else if (shapetype == "cube" || shapetype == "Cube") {
                    entity.shape = {ShapeType::CUBE,
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[2],
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[1],
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[0],
                                    glm::vec3(0, 0, 0)};
                }
                if (voxelizeFromObj && objFileText && *objFileText) {
                    entity.voxelizeFromObj = true;
                    std::filesystem::path objPath(objFileText);
                    if (objPath.is_relative() && !m_inputDirectory.empty()) {
                        objPath = std::filesystem::path(m_inputDirectory) / objPath;
                    }
                    entity.objFile = objPath.lexically_normal().string();
                    entity.voxelFillThreshold = sceneXml.background.voxelFillThreshold;

                    entity.meshNames = myFunction::mySplitStr(
                        node->FirstChildElement("meshNames")->GetText(), ",");
                    entity.spectralNames = myFunction::mySplitStr(
                        node->FirstChildElement("spectralNames")->GetText(), ",");
                    if (sonExists("thermalNames", node)) {
                        entity.thermalNames = myFunction::mySplitStr(
                            node->FirstChildElement("thermalNames")->GetText(), ",");
                    }

                    const size_t bindingCount = std::max<size_t>(1, entity.meshNames.size());
                    const std::string canopyName = sonExists("canopyNames", node)
                        ? node->FirstChildElement("canopyNames")->GetText() : "";
                    const std::string propertyName = sonExists("bioNames", node)
                        ? node->FirstChildElement("bioNames")->GetText() : "";
                    entity.canopyNames.assign(bindingCount, canopyName);
                    entity.propNames.assign(bindingCount, propertyName);

                    entity.isdisFromFile = sonExists("objectPosition", node);
                    if (entity.isdisFromFile) {
                        std::filesystem::path distributionPath(
                            node->FirstChildElement("objectPosition")->GetText());
                        if (distributionPath.is_relative() && !m_inputDirectory.empty()) {
                            distributionPath =
                                std::filesystem::path(m_inputDirectory) / distributionPath;
                        }
                        entity.distributefile =
                            distributionPath.lexically_normal().string();
                    }
                }

                PrimEntitys.push_back(entity);
            }
        }
        sceneXml.primEntities = PrimEntitys;
    }



    if (mode == Mode::eRaytracing)
    {
        std::vector<ObjEntity> ObjEntities;
        TiXmlElement* objNode = sceneNode->FirstChildElement("Object");
        for (TiXmlElement* node = objNode->FirstChildElement(); node != NULL; node = node->NextSiblingElement())
        {
            ObjEntity objEntity;
            objEntity.objName = node->Attribute("objName");
            objEntity.filePath = node->FirstChildElement("fileName")->GetText();
//            objEntity.filePath = "D:\\work\\data\\field_aoyunlst\\field_data\\aoyun\\single_tree_a2_b3_q0.3_lai2.7.obj";
            // objEntity.meshNames = {"Crown"};
            objEntity.meshNames ={myFunction::mySplitStr(node->FirstChildElement("meshNames")->GetText(), ",")};
//            objEntity.types = {Type::SOIL, Type::VEGETATION, Type::VEGETATION};
            for (int i = 0; i < objEntity.meshNames.size(); i++)
            {
                objEntity.types.push_back(Type::VEGETATION);
            }
            // objEntity.types = {Type::VEGETATION};
            objEntity.spectralNames = {myFunction::mySplitStr(node->FirstChildElement("spectralNames")->GetText(), ",")};
//            objEntity.spectralNames = {"leaf"};
            if (sonExists("thermalNames", node)){
                objEntity.thermalNames = {myFunction::mySplitStr(node->FirstChildElement("thermalNames")->GetText(), ",")};
            }

//            objEntity.thermalNames = {"leaf"};
            if(sonExists("objectPosition", node))
            {
                objEntity.isFromFile = {true};
                objEntity.file = node->FirstChildElement("objectPosition")->GetText();
            }

//            objEntity.isFromFile = true;
//            objEntity.file = "D:/work/gray_rt/1/entity_0_position.txt";
            objEntity.objDistributions = {};
            objEntity.scales = {};
            objEntity.rotations = {};
            ObjEntities.push_back(objEntity);
        }
        sceneXml.objEntities = ObjEntities;
    }

    if (m_mode == Mode::eVoxelRT)
    {
        sceneXml.objEntities = {};
//    PrimEntity treeEntity;
//    PrimEntity buildingEntity;
        std::vector<PrimEntity> PrimEntitys;
// 获取position
        if (sonExists("Object", sceneNode->ToElement())) {
            TiXmlElement *obj = sceneNode->FirstChildElement("Object");
            for (TiXmlElement *node = obj->FirstChildElement(); node != NULL; node = node->NextSiblingElement()) {
                PrimEntity entity;
                const bool voxelizeFromObj =
                    sonExists("fileName", node) || sonExists("objectfile", node);
                const char* objFileText = voxelizeFromObj
                    ? (sonExists("fileName", node)
                        ? node->FirstChildElement("fileName")->GetText()
                        : node->FirstChildElement("objectfile")->GetText())
                    : nullptr;
                entity.primitiveName = node->FirstChildElement("meshNames")->GetText();
                entity.meshNames = {node->FirstChildElement("canopyNames")->GetText()};
                std::string name = node->FirstChildElement("types")->GetText();

                if (name == "building" || name == "Building") {
                    const auto spectra = myFunction::mySplitStr(node->FirstChildElement("spectralNames")->GetText(), ",");
                    const std::string wallSpectrum = spectra.empty() ? "soil" : spectra.front();
                    const std::string roofSpectrum = spectra.size() > 1 ? spectra[1] : wallSpectrum;
                    entity.meshNames = {"wall", "roof"};
                    entity.spectralNames = {wallSpectrum, roofSpectrum};
//                    entity.thermalNames = {"wall", "roof"};
                    entity.canopyNames = {node->FirstChildElement("canopyNames")->GetText(),
                                          node->FirstChildElement("canopyNames")->GetText()};
                    entity.propNames = {node->FirstChildElement("bioNames")->GetText(),
                                        node->FirstChildElement("bioNames")->GetText()};
                    entity.type = Type::BUILDING;
                    entity.isshapeFromFile = false;
                    entity.shapefile = " ";
                    entity.isheightFromFile = false;
                    entity.heightfile = "";
//                entity.isdisFromFile = false;
//                entity.distributefile = " ";
                } else if (name == "vegetation" || name == "Vegetation") {
                    entity.canopyNames = {node->FirstChildElement("canopyNames")->GetText()};
                    entity.propNames = {node->FirstChildElement("bioNames")->GetText()};
                    entity.spectralNames = {node->FirstChildElement("spectralNames")->GetText()};

                    if (sonExists("thermalNames", node)){
                        entity.thermalNames = {node->FirstChildElement("thermalNames")->GetText()};
                    }
                    entity.type = Type::VEGETATION;
                    entity.isshapeFromFile = false;
                    entity.shapefile = " ";
                    entity.isheightFromFile = false;
                    entity.heightfile = "";
                    entity.isdisFromFile = true;
                    entity.distributefile = node->FirstChildElement("objectPosition")->GetText();
                } else if (name == "water" || name == "Water") {
                    entity.canopyNames = {node->FirstChildElement("canopyNames")->GetText()};
                    entity.propNames = {node->FirstChildElement("bioNames")->GetText()};
                    entity.spectralNames = {node->FirstChildElement("spectralNames")->GetText()};
                    entity.type = Type::WATER;
                    entity.isshapeFromFile = false;
                    entity.isheightFromFile = false;
                    entity.isdisFromFile = sonExists("objectPosition", node);
                    if (entity.isdisFromFile) {
                        entity.distributefile = node->FirstChildElement("objectPosition")->GetText();
                    }
                }


                std::string shapetype = node->FirstChildElement("shapeTypes")->GetText();
                cout << shapetype << endl;
                if (shapetype == "ellipsoid" || shapetype == "Ellipsoid") {
                    entity.shape = {ShapeType::ELLIPSOID,
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[2],
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[1],
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[0],
                                    glm::vec3(0, 0, 0)};
                } else if (shapetype == "cube" || shapetype == "Cube") {
                    entity.shape = {ShapeType::CUBE,
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[2],
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[1],
                                    myFunction::mySplitFloat(node->FirstChildElement("shapes")->GetText(), ",")[0],
                                    glm::vec3(0, 0, 0)};
                }
                if (voxelizeFromObj && objFileText && *objFileText) {
                    entity.voxelizeFromObj = true;
                    std::filesystem::path objPath(objFileText);
                    if (objPath.is_relative() && !m_inputDirectory.empty()) {
                        objPath = std::filesystem::path(m_inputDirectory) / objPath;
                    }
                    entity.objFile = objPath.lexically_normal().string();
                    entity.voxelFillThreshold = sceneXml.background.voxelFillThreshold;

                    entity.meshNames = myFunction::mySplitStr(
                        node->FirstChildElement("meshNames")->GetText(), ",");
                    entity.spectralNames = myFunction::mySplitStr(
                        node->FirstChildElement("spectralNames")->GetText(), ",");
                    if (sonExists("thermalNames", node)) {
                        entity.thermalNames = myFunction::mySplitStr(
                            node->FirstChildElement("thermalNames")->GetText(), ",");
                    }

                    const size_t bindingCount = std::max<size_t>(1, entity.meshNames.size());
                    const std::string canopyName = sonExists("canopyNames", node)
                        ? node->FirstChildElement("canopyNames")->GetText() : "";
                    const std::string propertyName = sonExists("bioNames", node)
                        ? node->FirstChildElement("bioNames")->GetText() : "";
                    entity.canopyNames.assign(bindingCount, canopyName);
                    entity.propNames.assign(bindingCount, propertyName);

                    entity.isdisFromFile = sonExists("objectPosition", node);
                    if (entity.isdisFromFile) {
                        std::filesystem::path distributionPath(
                            node->FirstChildElement("objectPosition")->GetText());
                        if (distributionPath.is_relative() && !m_inputDirectory.empty()) {
                            distributionPath =
                                std::filesystem::path(m_inputDirectory) / distributionPath;
                        }
                        entity.distributefile =
                            distributionPath.lexically_normal().string();
                    }
                }

                PrimEntitys.push_back(entity);
            }
        }
        sceneXml.primEntities = PrimEntitys;
    }
    return sceneXml;
}


void FileIO::readDefined(std::shared_ptr<DefinedIO> & definedio) {

    if (m_mode == Mode::eVoxelRT){
        definedio->definedDir = m_pVoxelrtXml->definedDir;
    }
    if (m_mode == Mode::eVoxelEB) {
        definedio->definedDir = m_pVoxelebXml->definedDir;
    }
    std::string predifineDir = definedio->definedDir + "\\defined\\";
    std::string infileName = predifineDir + "optipar.txt";
    int num = 1;
    Utils::readascfileinout(infileName,0,0,definedio->m_fluspectCoeff.wl_,num);
    Utils::readascfileinout(infileName,0,1,definedio->m_fluspectCoeff.nr_,num);
    Utils::readascfileinout(infileName,0,2,definedio->m_fluspectCoeff.kab_,num);
    Utils::readascfileinout(infileName,0,3,definedio->m_fluspectCoeff.kca_,num);
    Utils::readascfileinout(infileName,0,4,definedio->m_fluspectCoeff.ks_,num);
    Utils::readascfileinout(infileName,0,5,definedio->m_fluspectCoeff.kw_,num);
    Utils::readascfileinout(infileName,0,6,definedio->m_fluspectCoeff.kdm_,num);
    Utils::readascfileinout(infileName,0,7,definedio->m_fluspectCoeff.phiI_,num);
    Utils::readascfileinout(infileName,0,8,definedio->m_fluspectCoeff.phiII_,num);
    Utils::readascfileinout(infileName,0,9,definedio->m_fluspectCoeff.kcaV_,num);
    Utils::readascfileinout(infileName,0,10,definedio->m_fluspectCoeff.kcaZ_,num);
    Utils::readascfileinout(infileName,0,11,definedio->m_fluspectCoeff.kcant_,num);
    Utils::readascfileinout(infileName,0,12,definedio->m_fluspectCoeff.kcaV2_,num);
    Utils::readascfileinout(infileName,0,13,definedio->m_fluspectCoeff.phi_,num);
    Utils::readascfileinout(infileName,0,14,definedio->m_fluspectCoeff.gsv1_,num);
    Utils::readascfileinout(infileName,0,15,definedio->m_fluspectCoeff.gsv2_,num);
    Utils::readascfileinout(infileName,0,16,definedio->m_fluspectCoeff.gsv3_,num);
    Utils::readascfileinout(infileName,0,17,definedio->m_fluspectCoeff.nw_,num);
    // return false;
}

void FileIO::readMeteo(std::shared_ptr<DefinedIO> &defineio,int & n_node,
                       std::vector<Meteo> &meteos, std::vector<AtomCond> &wavesets) {


    auto & meteofile = m_pVoxelebXml->meteoxml.meteofile;



    std::ifstream infile(meteofile);
    std::vector<std::string> fields;
    std::string deli(" "), line;

    std::getline(infile, line);
    fields = Utils::splitt(line, deli);
    n_node = std::stoi(fields[0].c_str());

    int meteoNum = 0;
    float z = m_meteoXml.meta.z;//15;
    float sm = m_meteoXml.meta.sm;//0.25;
    float ea = m_meteoXml.meta.ea;//15;
    float Ca = m_meteoXml.meta.Ca;//380
    float Oa = m_meteoXml.meta.Oa;//209
    float Tsold = m_meteoXml.meta.Tsold;//
    float SatWater = m_meteoXml.meta.SatWater;//0.45;
    float dTime = m_meteoXml.meta.dTime;//1800;

    for (int i = 0; i < n_node; i++)
    {
        std::getline(infile, line);
        fields = Utils::splitt(line, deli);

        Meteo mi;
        mi.t = std::atof(fields[0].c_str());
        mi.Ta = std::atof(fields[1].c_str());
        mi.ea = std::atof(fields[2].c_str());
        mi.p = std::atof(fields[3].c_str());
        mi.u = std::atof(fields[4].c_str());
        mi.Rin = std::atof(fields[5].c_str());
        mi.Rli = std::atof(fields[6].c_str());
        mi.sm = sm;
        mi.z = z;
//        mi.ea = ea;
        mi.Ca = Ca;
        mi.Oa = Oa;
        mi.dTime = dTime;

        meteos.emplace_back(mi);
        //m_meteoParams.emplace_back(mi);
    }

    ///-----------------------------------------------------------------------------
    ///   WaveLength and atmospheric condition
    ///----------------------------------------------------------------------------

    wavesets.resize(N1+N2);
    for (int i = 0; i <  2001 ; i++)
    {
        wavesets[i].wavelength = 400 + i;
    }
    for (int i = 0; i < 126; i++)
    {
        wavesets[i+2001].wavelength = 2500 + i * 100;
    }
    for (int i = 0; i < 35; i++)
    {
        wavesets[i+2127].wavelength = 16000 + i * 1000;
    }


    ///-----------------------------------------------------------------------------
    ///   Atmospheric condition in radiative transfer domain
    ///----------------------------------------------------------------------------
    float  *esun_, *esky_, *fesky_, *fesun_;
    int num = 1;
    // wave_ = Utils::infile2num(predifineDir+'Esk', 0, 0, num);
    esun_ = Utils::readascfile(m_pVoxelebXml->atomcondxml.rinfile, 0, 0, num);
    esky_ = Utils::readascfile(m_pVoxelebXml->atomcondxml.rlifile, 0, 0, num);
    fesky_ = new float[num];
    fesun_ = new float[num];

    float TsEsky = 0, TlEsky = 0, TlEsun = 0, TsEsun = 0, tstot = 0, tltot = 0, temp1, temp2, step;
    int b1 = N1;
    int b2 = N1+N2;

    // �̲�
    for (int i = 0; i < b1 - 1; i++)
    {
        temp1 = (esky_[i] + esky_[i + 1]) / 2.0;
        step =  wavesets[i+1].wavelength -  wavesets[i].wavelength;
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
    // ����
    for (int j = b1; j < b2 - 1; j++)
    {
        temp1 = (esky_[j] + esky_[j + 1]) / 2.0;
        step =   wavesets[j+1].wavelength -  wavesets[j].wavelength;
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
        wavesets[i].direct = fesun_[i];
        wavesets[i].diffuse = fesky_[i];
    }

    delete[] fesky_;
    delete[] fesun_;
    delete[] esun_;
    delete[] esky_;

}



//void FileIO::writeENVIdata(std::string projectDir, float *pData, int width, int height, int band,
//                           Angle &angle, float t) {
//
//    std::ostringstream  oss_x;
//    oss_x << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.vza;
//    std::ostringstream  oss_y;
//    oss_y << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.vaa;
//    std::ostringstream  oss_z;
//    oss_z << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.sza;
//    std::ostringstream  oss_w;
//    oss_w << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.saa;
//    std::ostringstream  oss_t;
//    oss_t << std::fixed << std::setprecision(2)<<std::setfill('0')<<t;
//
////    std::string outPath = projectDir + "/results/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
////                          "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".tif";
//    std::string tifName,hdrName;
//    if(t > 0){
//        tifName = projectDir + "/results/t=" + oss_t.str() +"_VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".img";
//        hdrName = projectDir + "/results/t=" + oss_t.str() +"_VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".hdr";
//    }else {
//        tifName = projectDir +"/results/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".img";
//        hdrName = projectDir +"/results/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".hdr";
//    }
//    std::ofstream outfilet1(tifName.c_str(), std::ios::binary);
//    outfilet1.write(reinterpret_cast<const char*>(pData), sizeof(float) * width * height * band);
//    outfilet1.close();
//
//    std::ofstream outfile(hdrName);
//    if (outfile.is_open())
//    {
//        outfile << "ENVI" << std::endl;
//        outfile << "description = {" << std::endl;
//        outfile << " File Imported into ENVI.} " << std::endl;
//        outfile << "samples = " << width << std::endl;
//        outfile << "lines   = " << height << std::endl;
//        outfile << "bands   =  " << band << std::endl;
//        outfile << "header offset = 0" << std::endl;
//        outfile << "file type = ENVI Standard" << std::endl;
//        outfile << "data type = 4" << std::endl;
//        outfile << "interleave = bsp" << std::endl;
//        outfile << "sensor type = unknown" << std::endl;
//        outfile << "byte order = 0" << std::endl;
//        outfile << "wavelength units = Unknown" << std::endl;
//        outfile.close();
//    }
//    outfile.close();
//
//
//}

//void FileIO::readCustomSpectral(TiXmlElement* pEle)
//{
//    ManualSpectralStruct temp;
//    temp.name = pEle->Attribute("name");;
//
//    std::vector<float> reflectanceVec = myFunction::mySplitFloat(pEle->FirstChildElement("reflectance")->GetText(), ",");
//    std::vector<float> transmittanceVec = myFunction::mySplitFloat(pEle->FirstChildElement("transmittance")->GetText(), ",");
//    for (int iter = 0; iter < reflectanceVec.size(); iter++)
//    {
//        OpticalMaterial optical;
//        optical.band = sensorDatasets[0].controlBand[iter];
//        optical.reflectance = reflectanceVec[iter];
//        optical.transmittance = transmittanceVec[iter];
//        temp.m_opticalMaterials.push_back(optical);
//    }
//    manualSpectralDatasets.push_back(temp);
//}






//void FileIO::writeENVIdata(std::string projectDir, float *pData, int width, int height, int band,
//                           Angle &angle, float t) {
//
//    std::ostringstream  oss_x;
//    oss_x << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.vza;
//    std::ostringstream  oss_y;
//    oss_y << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.vaa;
//    std::ostringstream  oss_z;
//    oss_z << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.sza;
//    std::ostringstream  oss_w;
//    oss_w << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.saa;
//    std::ostringstream  oss_t;
//    oss_t << std::fixed << std::setprecision(2)<<std::setfill('0')<<t;
//
////    std::string outPath = projectDir + "/results/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
////                          "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".tif";
//    std::string tifName,hdrName;
//    if(t > 0){
//        tifName = projectDir + "/results/t=" + oss_t.str() +"_VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                              "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".img";
//        hdrName = projectDir + "/results/t=" + oss_t.str() +"_VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                               "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".hdr";
//    }else {
//        tifName = projectDir +"/results/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                              "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".img";
//        hdrName = projectDir +"/results/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                              "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".hdr";
//    }
//    std::ofstream outfilet1(tifName.c_str(), std::ios::binary);
//    outfilet1.write(reinterpret_cast<const char*>(pData), sizeof(float) * width * height * band);
//    outfilet1.close();
//
//    std::ofstream outfile(hdrName);
//    if (outfile.is_open())
//    {
//        outfile << "ENVI" << std::endl;
//        outfile << "description = {" << std::endl;
//        outfile << " File Imported into ENVI.} " << std::endl;
//        outfile << "samples = " << width << std::endl;
//        outfile << "lines   = " << height << std::endl;
//        outfile << "bands   =  " << band << std::endl;
//        outfile << "header offset = 0" << std::endl;
//        outfile << "file type = ENVI Standard" << std::endl;
//        outfile << "data type = 4" << std::endl;
//        outfile << "interleave = bsp" << std::endl;
//        outfile << "sensor type = unknown" << std::endl;
//        outfile << "byte order = 0" << std::endl;
//        outfile << "wavelength units = Unknown" << std::endl;
//        outfile.close();
//    }
//    outfile.close();


//}

void FileIO::writeENVIdata(std::string projectDir, float *pData, int width, int height, int band,
                           Angle &angle, float t, int k) {

    std::ostringstream  oss_x;
    oss_x << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.vza;
    std::ostringstream  oss_y;
    oss_y << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.vaa;
    std::ostringstream  oss_z;
    oss_z << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.sza;
    std::ostringstream  oss_w;
    oss_w << std::fixed << std::setprecision(2)<<std::setfill('0')<<angle.saa;
    std::ostringstream  oss_t;
    oss_t << std::fixed << std::setprecision(3)<<std::setfill('0')<<t;
    std::ostringstream  oss_k;
    oss_k << std::fixed << std::setprecision(0)<<std::setfill('0')<<k;

//    std::string outPath = projectDir + "/results/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
//                          "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".tif";
    std::string tifName,hdrName;
    if(t >= 0){
        tifName = projectDir + "/t=" + oss_t.str() +"_VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".img";
        hdrName = projectDir + "/t=" + oss_t.str() +"_VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".hdr";
    }else if(k >= 0) {
        tifName = projectDir +"/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + "_"+oss_k.str()+".img";
        hdrName = projectDir +"/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + "_"+oss_k.str()+".hdr";
    }else
    {
        tifName = projectDir +"/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".img";
        hdrName = projectDir +"/VZA=" + oss_x.str() + "_VAA=" + oss_y.str() +
                  "_SZA=" + oss_z.str() + "_SAA=" + oss_w.str() + ".hdr";
    }
    std::ofstream outfilet1(tifName.c_str(), std::ios::binary);
    outfilet1.write(reinterpret_cast<const char*>(pData), sizeof(float) * width * height * band);
    outfilet1.close();

    std::ofstream outfile(hdrName);
    if (outfile.is_open())
    {
        outfile << "ENVI" << std::endl;
        outfile << "description = {" << std::endl;
        outfile << " File Imported into ENVI.} " << std::endl;
        outfile << "samples = " << width << std::endl;
        outfile << "lines   = " << height << std::endl;
        outfile << "bands   =  " << band << std::endl;
        outfile << "header offset = 0" << std::endl;
        outfile << "file type = ENVI Standard" << std::endl;
        outfile << "data type = 4" << std::endl;
        outfile << "interleave = bsp" << std::endl;
        outfile << "sensor type = unknown" << std::endl;
        outfile << "byte order = 0" << std::endl;
        outfile << "wavelength units = Unknown" << std::endl;
        outfile.close();
    }
    outfile.close();


}
