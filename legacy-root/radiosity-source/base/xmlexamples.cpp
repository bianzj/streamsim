//
// Created by bianzunjian on 2024/3/22.
//

#include "xmlexamples.h"

void XmlExamples::createRadiosityXml() {

    m_pRadiosityXml=std::make_shared<RadiosityXml>();

    m_pRadiosityXml->projectDir = "D:/data/Beijing/building_shadow/aabb/beijing/";
    m_pRadiosityXml->definedDir = "D:\\code\\radiosity\\data/defined/";

//    m_pRadiosityXml->settingxml.n_sample=32;
//    m_pRadiosityXml->settingxml.maxDepth = 32;
//    m_pRadiosityXml->settingxml.theGPU = 0;
    m_pRadiosityXml->settingxml.isInfinite = false;

    m_pRadiosityXml->sensorxml.name = "UAV";
    m_pRadiosityXml->sensorxml.resolution = glm::vec2{1024,1024};
    m_pRadiosityXml->sensorxml.isImage = true;
    m_pRadiosityXml->sensorxml.isAlbedo = false;
    m_pRadiosityXml->sensorxml.isDisplay = false;
    m_pRadiosityXml->sensorxml.isTemperature = true;

    m_pRadiosityXml->sensorxml.viewAngles = {{0,0}};
//    for(int kvza = 0;kvza<60;kvza=kvza+10)
//    {
//        for(int kvaa = 0;kvaa<=360;kvaa=kvaa+30){
//            m_pRadiosityXml->sensorxml.viewAngles.push_back({kvza,kvaa});
//        }
//    }

    m_pRadiosityXml->sensorxml.waves = {650,850,10500};
//    m_pRadiosityXml->sensorxml.projection = Projection::PARALLAL;


    m_pRadiosityXml->lightxml.name = "Solar";
    m_pRadiosityXml->lightxml.solarAngle = glm::vec2{45,45};
    m_pRadiosityXml->lightxml.direct = 0.9;
    m_pRadiosityXml->lightxml.diffuse = 0.1;
    m_pRadiosityXml->lightxml.skyTemperature = 250;
    m_pRadiosityXml->lightxml.solarTemperature = 6000;


    m_pRadiosityXml->scenexml.sceneSize=glm::vec3{0,0,0};
    m_pRadiosityXml->scenexml.sceneOrigin={0,0,0};
    m_pRadiosityXml->scenexml.sMin={0,0,0};
    m_pRadiosityXml->scenexml.sMax={0,0,0};
    m_pRadiosityXml->scenexml.background = {"soil","K310","soilset"};


    ObjEntity objEntity = ObjEntity{"objTree",m_pRadiosityXml->projectDir + "/building.obj",
                                {"wall_0"},{"leaf"},{"K300"},
                                false,{{0,0,0}},
                                {1,},{0,}};

    // std::string objFilePath = "D:/data/Beijing/building_shadow/aabb/aoyun//building.obj";
    // ObjLoader loader;
    // loader.getModelMeshNames(objFilePath);
    // ObjEntity objEntity;
    // objEntity.objName = "beijing";
    // objEntity.filePath = objFilePath;
    // objEntity.meshNames = loader.m_meshNames;
    // objEntity.isLarge = false;
    // for (int i = 0; i < objEntity.meshNames.size(); i++)
    // {
    //     objEntity.spectralNames.push_back("leaf");
    //     objEntity.thermalNames.push_back("K300");
    //     objEntity.objDistributions.push_back({0,0,0});
    //     objEntity.scales.push_back({1});
    //     objEntity.rotations.push_back({0});
    // }


    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> distrib(1, 360);
    // for(int kx = 0;kx <=5;kx++){
    //     for(int ky = 0; ky<=5;ky++)
    //     {
    //         int random_number = distrib(gen);
    //         objEntity.objDistributions.push_back({kx,ky,0});
    //         objEntity.scales.push_back(1);
    //         objEntity.rotations.push_back(random_number);
    //     }
    // }


    //PrimEntity primEntity;
    //    primEntity= PrimEntity{"primTree",{"crown"},{"leaf"},{"K300"},{"crown"},
    //                                {ShapeType::ELLIPSOID},{{5,5,5,glm::vec3(0,0,0)}},
    //                                {{10,40,0},{40,10,0}},{1,1,1},{0,0,0}};

    m_pRadiosityXml->scenexml.objEntities = {objEntity};
   // m_pRadiosityXml->scenexml.primEntities = {primEntity};
    m_pRadiosityXml->scenexml.background.isDEM = {false};
    m_pRadiosityXml->scenexml.background.DEMPath = "";
    m_pRadiosityXml->scenexml.background.demResolution = {10,10};

    SpectralXml spectralXml1;
    spectralXml1.spectralName="soil";
    spectralXml1.type = spectralType::CUSTOM;
    spectralXml1.reflectances = {0.23,0.25,0.05};
    spectralXml1.transmittance ={0.23,0.25,0};
    SpectralXml spectralXml2{};
    spectralXml2.spectralName="leaf";
    spectralXml2.type = spectralType::CUSTOM;
    spectralXml2.reflectances = {0.13,0.15,0.025};
    spectralXml2.transmittance ={0.13,0.15,0};
    m_pRadiosityXml->spectralxmls = {spectralXml1,spectralXml2};


    m_pRadiosityXml->thermalxmls = {{"K310",320,300},{"K300",305,295}};

}


void XmlExamples::createRadiosityEBXml() {

    m_pRadiosityEBXml=std::make_shared<RadiosityebXml>();

    m_pRadiosityEBXml->projectDir = "D:\\code\\radiosity\\data/example/";
    m_pRadiosityEBXml->definedDir = "D:\\code\\radiosity\\data/defined/";

//    m_pRadiosityEBXml->settingxml.n_sample=32;
//    m_pRadiosityEBXml->settingxml.maxDepth = 32;
//    m_pRadiosityEBXml->settingxml.theGPU = 0;
    m_pRadiosityEBXml->settingxml.isInfinite = false;

    m_pRadiosityEBXml->sensorxml.name = "UAV";
    m_pRadiosityEBXml->sensorxml.resolution = glm::vec2{1024,1024};
    m_pRadiosityEBXml->sensorxml.isImage = true;
    m_pRadiosityEBXml->sensorxml.isAlbedo = false;
    m_pRadiosityEBXml->sensorxml.isDisplay = false;
    m_pRadiosityEBXml->sensorxml.isTemperature = true;
    m_pRadiosityEBXml->sensorxml.viewAngles = {{0,0}};
    m_pRadiosityEBXml->sensorxml.waves = {650,850,10500};
//    m_pRadiosityEBXml->sensorxml.projection = Projection::PARALLAL;


    m_pRadiosityEBXml->lightxml.name = "Solar";
    m_pRadiosityEBXml->lightxml.solarAngle = glm::vec2{25,10};
    m_pRadiosityEBXml->lightxml.direct = 0.9;
    m_pRadiosityEBXml->lightxml.diffuse = 0.1;
    m_pRadiosityEBXml->lightxml.skyTemperature = 250;
    m_pRadiosityEBXml->lightxml.solarTemperature = 6000;


    m_pRadiosityEBXml->scenexml.sceneSize=glm::vec3{20,20,0};
    m_pRadiosityEBXml->scenexml.sceneOrigin={0,0,0};
    m_pRadiosityEBXml->scenexml.sMin={5.5,5.5,0}; // this seems no works here
    m_pRadiosityEBXml->scenexml.sMax={0.5,0.5,0}; // this seems no works here
    m_pRadiosityEBXml->scenexml.background = {"soil","K310","soilset"};

//    ObjEntity objEntity = ObjEntity{"objTree","/Users/bianzj/work/radiosity/data/example/speed_tree.obj",
//                                    {"Leaf1","Trunk","Bough"},{"leaf","soil","leaf"},{"K300","K310","K300"},
//                                    false,{{2.5,2.5,0},{1.5,1.5,0},{3.5,3.5,0},{1.5,3.5,0}},
//                                    {1,1,1,1,1},{0,0,0,0,0}};
    // ObjEntityPlus objEntity = ObjEntityPlus{"objTree","D:\\code\\radiosity\\data/example/corn_day70.obj",
    //                                 {"corn"},{"leaf"},{"K300"},{"leafbio"},{"crown"},{Type::VEGETATION}
    //                                 ,false,
    //     {{2.5,2.5,0},{1.5,1.5,0},{3.5,3.5,0},{1.5,3.5,0},{3.5,1.5,0}},
    //                                 {1,1,1,1,1},{0,0,0,0,0}};

    ObjEntityPlus objEntity = ObjEntityPlus{"objTree","D:/code/radiosity/data/example/single_tree_LAI_4.obj",
                                    {"Crown"},{"leaf"},{"K300"},{"leafbio"},{"crown"},{Type::VEGETATION}
        ,false,
{},
        {},{}};

    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> distrib(1, 360);
    // for(int kx = 0;kx <=5;kx++){
    //     for(int ky = 0; ky<=5;ky++)
    //     {
    //         int random_number = distrib(gen);
    //         objEntity.objDistributions.push_back({kx,ky,0});
    //         objEntity.scales.push_back(1);
    //         objEntity.rotations.push_back(random_number);
    //     }
    // }

    std::string distFileName = "D:\\code\\radiosity\\data/example\\entity_0_position.txt";
    float  *xx, *yy;
    int num = 1;
    // wave_ = Utils::infile2num(predifineDir+'Esk', 0, 0, num);
    xx = Utils::readascfile(distFileName, 0, 0, num);
    yy = Utils::readascfile(distFileName, 0, 1, num);
    for(int i = 0; i < num; i = i + 1){
        objEntity.objDistributions.emplace_back(xx[i], yy[i], 0);
        objEntity.scales.emplace_back(1);
        objEntity.rotations.emplace_back(0);
    }

    //PrimEntity primEntity;
//    primEntity= PrimEntity{"primTree",{"crown"},{"leaf"},{"K300"},{"crown"},
//                                {ShapeType::ELLIPSOID},{{5,5,5,glm::vec3(0,0,0)}},
//                                {{10,40,0},{40,10,0}},{1,1,1},{0,0,0}};
    m_pRadiosityEBXml->scenexml.objEntityplus = {objEntity};

    m_pRadiosityEBXml->scenexml.background.isDEM = {false};
    m_pRadiosityEBXml->scenexml.background.DEMPath = R"(D:\code\radiosity\data\example\generated_dem.tif)";
    m_pRadiosityEBXml->scenexml.background.demResolution = {1,1};

//    SpectralXml spectralXml1;
//    spectralXml1.spectralName="soil";
//    spectralXml1.type = spectralType::CUSTOM;
//    spectralXml1.reflectances = {0.23,0.25,0.05};
//    spectralXml1.transmittance ={0.23,0.25,0};
//    SpectralXml spectralXml2{};
//    spectralXml2.spectralName="leaf";
//    spectralXml2.type = spectralType::CUSTOM;
//    spectralXml2.reflectances = {0.13,0.15,0.025};
//    spectralXml2.transmittance ={0.13,0.15,0};
//    m_pRadiosityEBXml->spectralxmls = {spectralXml1,spectralXml2};

    SpectralXml spectralXml1;
    spectralXml1.spectralName="soil";
    // spectralXml1.type = spectralType::SOILSET;
    spectralXml1.type = spectralType::OTHER;
    spectralXml1.reflectances = {0.23,0.25,0.04};
    spectralXml1.transmittance ={0.23,0.25,0};
    spectralXml1.path = "D:\\code\\radiosity\\data\\example\\soilnew_high.txt";
    // spectralXml1.bsm = {0.25,0.5,25,45,0.05,0};
    spectralXml1.tau_tir = 0;
    spectralXml1.refl_tir = 0.04;

    SpectralXml spectralXml2{};
    spectralXml2.spectralName="leaf";
    spectralXml2.type = spectralType::LEAFBIO;
    spectralXml2.reflectances = {0.13,0.15,0.02};
    spectralXml2.transmittance ={0.13,0.15,0};
    spectralXml2.fp = {80,0.009,0.012,0,1.46};
    spectralXml2.tau_tir = 0;
    spectralXml2.refl_tir = 0.02;
    m_pRadiosityEBXml->spectralxmls = {spectralXml1,spectralXml2};

    m_pRadiosityEBXml->thermalxmls = {{"K310",320,300},{"K300",305,295}};

    PropertyXml propertyXml1 = {"soilset",Type::SOIL,
                                LeafBio{25,8,0.01,1,0.6396,0.025,{0.2,0.3,288,313,328},25,0.4,0,1,1,0},
                                SoilSet{1,2000,1180,1800,1.55,0.25,25,0.45}};
    PropertyXml propertyXml2 = {"leafbio",Type::VEGETATION,
                                LeafBio{80,9,0.01,3,0.6396,0.015,{0.2,0.3,288,313,328},25,0.507,0,1,1,0},
                                SoilSet{0,500,1180,1800,1.55,0.25,25,0.45}};
    m_pRadiosityEBXml->propxmls ={propertyXml1,propertyXml2};


    m_pRadiosityEBXml->canopyxmls ={{"crown",{4,1.0,6,1,0.5,-0.35,-0.15,0.1,0.2}}};

    m_pRadiosityEBXml->aerocondxml = {AeroType::one,{0,10,3,0.36,2,0.2},"",1000};

    m_pRadiosityEBXml->aerocoeffxml = {{10,1.34, 0.3 ,10, 0.35,   20.6 ,  0.2 , 0.01,  10.0 , 0.0  }};

    m_pRadiosityEBXml->meteoxml.meteofile ="D:\\code\\radiosity\\data/example/15m_meteo_corrected_sameWind.txt";
    m_pRadiosityEBXml->meteoxml.rlifile ="D:\\code\\radiosity\\data/example/Esky_scope.dat";
    m_pRadiosityEBXml->meteoxml.rinfile ="D:\\code\\radiosity\\data/example/Esun_scope.dat";

    m_pRadiosityEBXml->latlon = glm::vec2(40.3574,115.7923);

}






