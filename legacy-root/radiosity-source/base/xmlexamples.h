//
// Created by bianzunjian on 2024/3/22.
//

#ifndef FIELD_RADIOSITY_XMLEXAMPLES_H
#define FIELD_RADIOSITY_XMLEXAMPLES_H

#include <iostream>
#include <string>
#include <fstream>
#include <memory>
#include <numbers>
#include <algorithm>
#include <vector>
#include <gdal.h>
#include <random>
#include "utils.h"
#include "structs.h"
#include "objloader.h"

class XmlExamples {

public:
    XmlExamples(){
        createRadiosityXml();
        createRadiosityEBXml();
    }
    std::shared_ptr<RadiosityXml> m_pRadiosityXml;
    std::shared_ptr<RadiosityebXml> m_pRadiosityEBXml;

    void createRadiosityXml();
    void createRadiosityEBXml();

};


#endif //FIELD_RADIOSITY_XMLEXAMPLES_H
