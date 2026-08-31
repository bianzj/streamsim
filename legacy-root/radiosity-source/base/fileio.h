//
// Created by bianzunjian on 2024/3/24.
//

#ifndef FIELD_RADIOSITY_FILEIO_H
#define FIELD_RADIOSITY_FILEIO_H

#include "structs.h"
#include "defined.h"
#include "xmlexamples.h"
#include "../thirdparty/tinyxml.h"


using namespace std;

class FileIO {
public:
    FileIO() = default;

    bool readXml(std::string path, std::string V);
    std::shared_ptr<RadiosityXml> m_pRadiosityXml;
    std::shared_ptr<RadiosityebXml> m_pRadiosityebXml;
    XmlExamples xmlexamples;

    bool sonExists(std::string sonName, TiXmlElement* parentEle);
    //    radiosity + EB
    SensorXml readSensorXML(TiXmlNode *node, Mode mode);
    LightXml readLightXML(TiXmlNode *node, Mode mode);
    SettingXml readSettingXML(TiXmlNode *node, Mode mode);
    SceneXml readSceneXML(TiXmlNode *node, Mode mode);
    std::vector<SpectralXml> readSpectralXML(TiXmlNode *node, Mode mode);
    std::vector<ThermalXml> readThermalXML(TiXmlNode *node, Mode mode);
    std::vector<CanopyXml> readCanopyXML(TiXmlNode *node, Mode mode);
    std::vector<PropertyXml> readPropertyXML(TiXmlNode *node, Mode mode);
    //    AtomCondXml readAtomCondXML(TiXmlNode *node, Mode mode);
    MeteoXml readMeteoXML(TiXmlNode *node, Mode mode);
    AeroCondXml readAeroXML(TiXmlNode *node, Mode mode);
    AeroCoeffXml readAeroCoeffXml(TiXmlNode *node, Mode mode);


    void readDefined(std::shared_ptr<DefinedIO> & defineio);
    void readMeteo(std::string path, std::vector<Meteo> &meteos, MeteoMeta &meta);
    void readAtom(std::string rinfile,std::string rlifile,AtomCoeff &atomcoeff);
    //    Radiosity EB


    TiXmlElement *RootElement;
    Mode m_mode;
};


#endif //FIELD_RADIOSITY_FILEIO_H
