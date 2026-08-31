//
// Created by admin on 2024/1/24.
//

#include "scene.h"


bool Scene::createObjScene(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RaytracingIO> &raytracingio) {


    float x = fileio->m_pRaytracingXml->scenexml.background.sceneSize.x; // lenght
    float y = fileio->m_pRaytracingXml->scenexml.background.sceneSize.y; // width
    float z = fileio->m_pRaytracingXml->scenexml.background.sceneSize.z; // height
/*    raytracingio->sMin = glm::vec3(-x / 2.0, 0, -y / 2.0);
    raytracingio->sMax = glm::vec3(x / 2.0, z, y / 2.0);*/
    raytracingio->sceneSize = glm::vec3(x,y,z);
    raytracingio->sceneOrigin = fileio->m_pRaytracingXml->scenexml.background.sceneOrigin;

    raytracingio->voxelSize = glm::vec3(x,z,y);
    raytracingio->voxelOrigin =  glm::vec3(0,0,0);


    bool isInterp = false;
    auto & meshio = raytracingio->m_meshio;
    auto & instanceio = raytracingio->m_instanceio;
    auto & scenexml = fileio->m_pRaytracingXml->scenexml;

    //auto & sceneio = raytracingio->m_sceneio;

    ObjLoader loader;

    //-------------------------
    //-- Background Mesh
    //-------------------------
    if(scenexml.background.isDEM) {
        loader.creatBackgroundFromDEM(scenexml.background.DEMFile, scenexml.background.sceneSize);
        std::string objDir = fileio->m_pRaytracingXml->projectDir + "/dem.obj";
        outputObjMesh(loader.m_objmesh, objDir);
        isInterp = true;
//        return false;
    }else{
        loader.createBackground(fileio->m_pRaytracingXml->scenexml.background.sceneSize);
    }
    //n_modelmesh = 0;
    int & n_modelmesh = raytracingio->n_modelmesh;
    loader.m_objmesh.meshId = n_modelmesh;
    meshio->objMeshes.emplace_back(loader.m_objmesh);

//    std::string outpath = "D:/data/sim_albedo/demTest.obj";
//    outputObjMesh(loader.m_objmesh, outpath);
    //-------------------------
    //-- Background MeshLink
    //-------------------------
    MeshLink bgMeshLink{};
    bgMeshLink.type = int(Type::SOIL);
    std::string bgSpectralName = fileio->m_pRaytracingXml->scenexml.background.bgSpectralName;
    bgMeshLink.spectralId =  meshio->spectralNames.find(bgSpectralName)->second;
    std::string bgThermalName;
    int bgThermalIndex = 0;
    if (fileio->m_pRaytracingXml->sensorxml.isTemperature == true)
    {
        bgThermalName =  fileio->m_pRaytracingXml->scenexml.background.bgThermalName;
        bgThermalIndex = meshio->thermalNames.find(bgThermalName)->second;
    }
    bgMeshLink.thermalId = bgThermalIndex;
    meshio->meshLinks.emplace_back(bgMeshLink);
    //-------------------------
    //-- Background Instance
    //-------------------------
    Instance bgInstance{};
    bgInstance.meshId = static_cast<uint32_t>(n_modelmesh);
    n_modelmesh++;
    glm::mat4 bgunit = glm::mat4(1.0f);
    glm::vec3 bgShift = glm::vec3{0, 0, 0};
    glm::vec3 bgScale = glm::vec3{1.0, 1.0, 1.0};
    glm::mat4 bgMat = glm::scale(bgunit, bgScale) * glm::translate(bgunit,bgShift);
    bgInstance.object2worldMatrix = bgMat;
    bgInstance.world2objectMatrix = glm::transpose(glm::inverse(bgMat));
    instanceio->instances.emplace_back(bgInstance);


    //-------------------------
    //-- Background InstanceLink
    //-------------------------
    InstanceLink bgInstanceLink{};
    bgInstanceLink.meshId = bgInstance.meshId;
    instanceio->instanceLinks.emplace_back(bgInstanceLink);

    /// ------------------------------------
    /// Scene
    ///-------------------------------------



   // outputModel(loader.m_objmesh, m_pRaytracingXml->setting.outDir + "/background.obj");
   // std::cout << "Export background.obj" << std::endl;



    //-------------------------
    //-- Obj Models
    //-------------------------
    int n_obj = fileio->m_pRaytracingXml->scenexml.objEntities.size();
    for (int kobj = 0; kobj < n_obj; kobj++)
    {
        auto &objEntity = fileio->m_pRaytracingXml->scenexml.objEntities[kobj];



        if(objEntity.isFromFile == true){
            int n_dis = 0;
            float *tempx, *tempy,*tempz;
            tempx = Utils::readascfile(objEntity.file,0,0,n_dis);
            tempy = Utils::readascfile(objEntity.file,0,1,n_dis);
            tempz = Utils::readascfile(objEntity.file,0,2,n_dis);

            if (isInterp)
            {
                loader.interpolateZValues(scenexml.background.sceneSize,
                    tempx, tempy, tempz, n_dis);
            }


            float *tempScale, *tempRotation;
            tempScale = Utils::readascfileWithDefault(objEntity.file,0,3,n_dis, 1.0);
            tempRotation = Utils::readascfileWithDefault(objEntity.file,0,4,n_dis, 0.0);

            objEntity.objDistributions.resize(n_dis);
            objEntity.scales.resize(n_dis);
            objEntity.rotations.resize(n_dis);
            for(int kin = 0;kin<n_dis;kin++)
            {
                objEntity.objDistributions[kin]=(glm::vec3(tempx[kin],tempy[kin],tempz[kin]));
                // objEntity.scales[kin] = 1.0;
                // objEntity.rotations[kin] = 0.0;
                objEntity.scales[kin] = tempScale[kin];
                objEntity.rotations[kin] = tempRotation[kin];
            }
        }

        std::string fileName = objEntity.filePath;
        std::string objName = objEntity.objName;
        int n_mesh = objEntity.meshNames.size();
        for (int kmesh = 0; kmesh < n_mesh; kmesh++)
        {
            /// ------------------------------------
            /// model/mesh
            ///-------------------------------------
            std::string meshName = objEntity.meshNames[kmesh];
            std::string spectralName = objEntity.spectralNames[kmesh];
            std::string thermalName;

            ObjLoader loader1;
            loader1.loadMesh(fileName, meshName);
            loader1.m_objmesh.meshId = n_modelmesh;
            meshio->objMeshes.emplace_back(loader1.m_objmesh);

            /// ------------------------------------
            /// Mesh Link
            ///-------------------------------------

            int spectralIndex = meshio->spectralNames.find(spectralName)->second;
            int thermalIndex = 0;
            if (fileio->m_pRaytracingXml->sensorxml.isTemperature == true)
            {
                thermalName = objEntity.thermalNames[kmesh];
                thermalIndex = meshio->thermalNames.find(thermalName)->second;
            }
            MeshLink meshLink{};
            meshLink.type = int(objEntity.types[kmesh]);
            meshLink.spectralId = spectralIndex;
            meshLink.thermalId = thermalIndex;
            meshio->meshLinks.emplace_back(meshLink);

            int n_instancet = objEntity.objDistributions.size();
/*            if(n_instancet > 100){
                n_instancet = 100;
            }*/

            for (int kinstance = 0; kinstance < n_instancet; kinstance++)
            {
                /// ------------------------------------
                /// Instance
                ///-------------------------------------
                Instance instance{};
                instance.meshId = static_cast<uint32_t>(n_modelmesh);
                glm::vec3 shift0 = objEntity.objDistributions[kinstance];
                float scale0 = objEntity.scales[kinstance];
                float angle0 = objEntity.rotations[kinstance];

                //nvmath::vec3f shift = nvmath::vec3f{ shift0.x - x/2.0,shift0.z,shift0.y - z/2.0 };
                glm::vec3 shift;
                if (isInterp == true)
                {
                    shift = nvmath::vec3f{shift0.x - x / 2.0, shift0.z - loader.centerElevation, shift0.y - y / 2.0};
                }
                else
                {
                    shift = glm::vec3{shift0.x - x / 2.0, shift0.z, shift0.y - y / 2.0};
                }


                glm::mat4 unit = glm::mat4(1.0f);
                glm::vec3 scale = glm::vec3(scale0);
                glm::mat4 rotation = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0, 1.0, 0.0));
                glm::mat4 mat = glm::translate(unit, shift) * rotation * glm::scale(unit, scale);
                instance.object2worldMatrix = mat;
                instance.world2objectMatrix = glm::transpose(glm::inverse(mat));
                instanceio->instances.emplace_back(instance);

                /// ------------------------------------
                /// InstanceLink
                ///-------------------------------------
                InstanceLink instanceLink{};
                instanceLink.meshId = instance.meshId;
                instanceio->instanceLinks.emplace_back(instanceLink);
            }
            n_modelmesh++;
        }

        int a = 10;
    }



    return true;
}



//-----------------------------------------------------------
//--- n_moxelmesh,n_instance,n_voxel
//--- primMeshes, meshlink, instance, instanceLink, nanovdb, voxellink
//------------------------------------------------------------

bool Scene::createPrimObjScene(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &voxellstio) {


    auto &scenexml = fileio->m_pVoxelebXml->scenexml;
    auto &voxellstxml = fileio->m_pVoxelebXml;
    auto &meshio = voxellstio->m_meshio;
    auto &instanceio = voxellstio->m_instanceio;
    auto &voxelio = voxellstio->m_voxelio;
    auto &background = fileio->m_pVoxelebXml->scenexml.background;
    nanovdb::GridBuilder<int32_t> nanoBuilder(-1);


    /// ------------------------------------
    /// Background
    ///-------------------------------------
    createPrimObj_Background(background, nanoBuilder, voxellstio);

    /// ------------------------------------
    /// voxel Components
    ///-------------------------------------
    for (int kVoxelModel = 0; kVoxelModel < scenexml.primEntities.size(); kVoxelModel++)
    {
        auto &voxelEntity = scenexml.primEntities[kVoxelModel];

        if (voxelEntity.type == Type::VEGETATION)
        {
            if(voxelEntity.isshapeFromFile == true)
            {
                createPrimObj_Crowns(voxelEntity, nanoBuilder, voxellstio);
            }else {
                createPrimObj_Crown(voxelEntity, nanoBuilder, voxellstio);
            }
        }else if(voxelEntity.type == Type::BUILDING)
        {
            createPrimObj_Building(voxelEntity, nanoBuilder, voxellstio);
        }else if(voxelEntity.type == Type::WATER)
        {
            createPrimObj_Crown(voxelEntity, nanoBuilder, voxellstio);
        }

    }
    voxellstio->m_voxelio->nanoHandle = nanoBuilder.getHandle<>();


    return true;
}

bool Scene::createPrimObj_Crown(PrimEntity & voxelEntity,nanovdb::GridBuilder<int32_t> &nanoBuilder,std::shared_ptr<VoxelebIO> &modelio){

    auto acc = nanoBuilder.getAccessor();
    auto &meshio = modelio->m_meshio;
    auto &instanceio = modelio->m_instanceio;
    auto &voxelio = modelio->m_voxelio;
    int &n_modelmesh = modelio->n_modelmesh;
    int &n_instance = modelio->n_instance;
    int &n_voxel = modelio->n_voxel;

    VoxelDesigner loader;
    /// ------------------------------------
    /// voxel model/mesh
    ///-------------------------------------
    std::string modelName = voxelEntity.primitiveName;
    Type type = voxelEntity.type;
    Shape shape = voxelEntity.shape;
    //meshio->types[0] = voxelEntity.types[0];
    // std::string aeroName = voxelEntity.aeroNames[0];


    PrimMesh currentVoxelModelXYZ1;
    currentVoxelModelXYZ1 = loader.createTriEntity(shape, modelio->stepsize_surface);
    PrimMesh currentVoxelModel1 = XYZ2XZY(currentVoxelModelXYZ1, 0); // (1,1,0) => (1,0,1) with  height = 0

    // bool a = loader.outputPrimMesh("D:/data/field_data/Sim_homo_LAI_0.0_timeSeries/crown.obj", currentVoxelModel1);

    currentVoxelModel1.meshId = n_modelmesh;
    meshio->primMeshes.emplace_back(currentVoxelModel1); //xzy
    std::string meshName1 = voxelEntity.meshNames[0];
    std::string spectralName1 = voxelEntity.spectralNames[0];
    std::string canopyName1 = voxelEntity.canopyNames[0];
    std::string propName1 = voxelEntity.propNames[0];
    MeshLink meshlink1;
    meshlink1.spectralId = meshio->spectralNames.find(spectralName1)->second;
    meshlink1.thermalId = 0;
    meshlink1.canopyId = type == Type::WATER ? 0 : meshio->canopyNames.find(canopyName1)->second;
    if (type == Type::VEGETATION) {
        // meshlink1.leafbioId = meshio->leafbioNames.find(propName)->second;
        meshlink1.bioId = meshio->leafbioNames.find(propName1)->second;
    } else if (type == Type::SOIL || type == Type::BUILDING) {
        //  meshlink1.soilsetId = meshio->soilsetNames.find(propName)->second;
        meshlink1.bioId = meshio->soilsetNames.find(propName1)->second;
    } else if (type == Type::WATER) {
        meshlink1.bioId = meshio->watersetNames.find(propName1)->second;
    }
    //  meshlink1.aeroId = meshio->aeroNames.find(aeroName)->second;
    meshlink1.type = (int) type;
    meshio->meshLinks.emplace_back(meshlink1);



    if(voxelEntity.isdisFromFile == true){
        int n_dis = 0;
        float *tempx, *tempy,*tempz;
        tempx = Utils::readascfile(voxelEntity.distributefile,0,0,n_dis);
        tempy = Utils::readascfile(voxelEntity.distributefile,0,1,n_dis);
        tempz = Utils::readascfile(voxelEntity.distributefile,0,2,n_dis);
        voxelEntity.primDistributions.resize(n_dis);
        voxelEntity.scales.resize(n_dis);
        voxelEntity.rotations.resize(n_dis);
        for(int kin = 0;kin<n_dis;kin++)
        {
            voxelEntity.primDistributions[kin]=(glm::vec3(tempx[kin],tempy[kin],tempz[kin]));
            voxelEntity.scales[kin] = 1.0;
            voxelEntity.rotations[kin] = 0.0;
        }
    }

    // instance

    for (int kinstance = 0; kinstance < voxelEntity.primDistributions.size(); kinstance++) {
        // voxel instance1��change postion,

        glm::ivec3 shift0;
        shift0 = {voxelEntity.primDistributions[kinstance].x / modelio->stepsize_surface,
                  voxelEntity.primDistributions[kinstance].z / modelio->stepsize_surface,
                  voxelEntity.primDistributions[kinstance].y / modelio->stepsize_surface};

        ///-----------------------------------------------------------------------------
        ///
        ///------------------------------------------------------------------------------
        glm::ivec3 semiRange = glm::vec3{floor(modelio->voxelSize_XZY.x / 2.0+0.5), 0,
                                         floor(modelio->voxelSize_XZY.z / 2.0+0.5)};;

        float scale0 = voxelEntity.scales[kinstance];
        float angle0 = voxelEntity.rotations[kinstance];
        glm::vec3 shift = glm::vec3(shift0) - glm::vec3(semiRange);  // (5,0,5)

        //
        glm::mat4 unit = glm::mat4(1.0f);
        glm::vec3 scale = glm::vec3(scale0);
        glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 mat = glm::scale(unit, scale) * glm::translate(unit, shift);

        Instance instance1{};
        instance1.meshId = static_cast<uint32_t>(n_modelmesh);
        instance1.object2worldMatrix = mat;
        instance1.world2objectMatrix = glm::transpose(glm::inverse(mat));
        instanceio->instances.emplace_back(instance1);
        // voxel Instance Link
        InstanceLink instancelink1{};
        instancelink1.meshId = instance1.meshId;
        instanceio->instanceLinks.emplace_back(instancelink1);


        int isValid = 0;

        for (int kvoxel = 0; kvoxel < currentVoxelModel1.voxelIds.size(); kvoxel++) {
            // this is what we did in the shader;
            // glm::ivec3 pos =  mat * glm::vec4(currentVoxelModel.voxelIds[kvoxel],1.0);
            glm::ivec3 Id = shift0 + glm::ivec3(currentVoxelModel1.voxelIds[kvoxel]);
            int test = acc.getValue(nanovdb::Coord(Id.x, Id.y, Id.z));

            // the first bufferid;

            if (test < 0) {

                acc.setValue(nanovdb::Coord(Id.x, Id.y, Id.z), n_voxel);
                glm::ivec3 voxelPos = glm::ivec3(Id.x * 1.0, Id.y * 1.0, Id.z * 1.0);  //(5,0,5) with height = 0

                VoxelLink voxelLink{};
                voxelLink.voxelId = voxelPos;          //(5,0,5) with height = 0
                voxelLink.instanceId = n_instance;
                voxelLink.aeroId = 0;
                voxelLink.faceId = 0; // center
                voxelLink.isValid = 1; // center
                modelio->m_voxelio->voxellinks.emplace_back(voxelLink);
                n_voxel++;
                isValid = 1;

            }
        }
        if (isValid == 0) {
            continue;
        }

        n_instance++;

        //std::string info = "voxel entity " + std::to_string(kVoxelModel) + " done.\n";
        //LOGI(info.c_str());
        // float tt = acc.getValue(nanovdb::Coord(24, 0, 24));
    }
    n_modelmesh++;
    return true;
}

bool Scene::createPrimObj_Crowns(PrimEntity & voxelEntity,nanovdb::GridBuilder<int32_t> &nanoBuilder,std::shared_ptr<VoxelebIO> &modelio){

    auto acc = nanoBuilder.getAccessor();
    auto &meshio = modelio->m_meshio;
    auto &instanceio = modelio->m_instanceio;
    auto &voxelio = modelio->m_voxelio;
    int &n_modelmesh = modelio->n_modelmesh;
    int &n_instance = modelio->n_instance;
    int &n_voxel = modelio->n_voxel;

    VoxelDesigner loader;
    /// ------------------------------------
    /// voxel model/mesh
    ///-------------------------------------
    std::string modelName = voxelEntity.primitiveName;
    Type type = voxelEntity.type;

    //meshio->types[0] = voxelEntity.types[0];
    // std::string aeroName = voxelEntity.aeroNames[0];


    int n_dis = 0;
    float *tempx, *tempy,*tempz,*temp1, *temp2,*temp3,*temps,*tempsc,*tempr;
    temps = Utils::readascfile(voxelEntity.shapefile,0,0,n_dis);
    temp1 = Utils::readascfile(voxelEntity.shapefile,0,1,n_dis);
    temp2 = Utils::readascfile(voxelEntity.shapefile,0,2,n_dis);
    temp3 = Utils::readascfile(voxelEntity.shapefile,0,3,n_dis);
    tempx = Utils::readascfile(voxelEntity.shapefile,0,4,n_dis);
    tempy = Utils::readascfile(voxelEntity.shapefile,0,5,n_dis);
    tempz = Utils::readascfile(voxelEntity.shapefile,0,6,n_dis);
    tempsc = Utils::readascfile(voxelEntity.shapefile,0,7,n_dis);
    tempr = Utils::readascfile(voxelEntity.shapefile,0,8,n_dis);

    for(int k = 0;k<n_dis;k++) {


        Shape shape = Shape{ShapeType(temps[k]),temp1[k],temp2[k],temp3[k],{tempx[k],tempy[k],tempz[k]}};


        PrimMesh currentVoxelModelXYZ1;
        currentVoxelModelXYZ1 = loader.createTriEntity(shape, modelio->stepsize_surface);
        PrimMesh currentVoxelModel1 = XYZ2XZY(currentVoxelModelXYZ1, 1); // (1,1,0) => (1,0,1) with  height = 0
        currentVoxelModel1.meshId = n_modelmesh;
        meshio->primMeshes.emplace_back(currentVoxelModel1); //xzy
        std::string meshName1 = voxelEntity.meshNames[0];
        std::string spectralName1 = voxelEntity.spectralNames[0];
        std::string canopyName1 = voxelEntity.canopyNames[0];
        std::string propName1 = voxelEntity.propNames[0];
        MeshLink meshlink1;
        meshlink1.spectralId = meshio->spectralNames.find(spectralName1)->second;
        meshlink1.thermalId = 0;
        meshlink1.canopyId = meshio->canopyNames.find(canopyName1)->second;
        if (type == Type::VEGETATION) {
            // meshlink1.leafbioId = meshio->leafbioNames.find(propName)->second;
            meshlink1.bioId = meshio->leafbioNames.find(propName1)->second;
        } else if (type == Type::SOIL || type == Type::BUILDING) {
            //  meshlink1.soilsetId = meshio->soilsetNames.find(propName)->second;
            meshlink1.bioId = meshio->soilsetNames.find(propName1)->second;
        }
        //  meshlink1.aeroId = meshio->aeroNames.find(aeroName)->second;
        meshlink1.type = (int) type;
        meshio->meshLinks.emplace_back(meshlink1);

        // instance


        for (int kinstance = 0; kinstance < 1; kinstance++) {
            // voxel instance1��change postion,

            glm::ivec3 shift0;
            shift0 = {shape.pos.x / modelio->stepsize_surface,
                      shape.pos.z / modelio->stepsize_surface,
                      shape.pos.y / modelio->stepsize_surface};

            ///-----------------------------------------------------------------------------
            /// Attention!!!!
            ///------------------------------------------------------------------------------
            glm::ivec3 semiRange = glm::vec3{floor(modelio->voxelSize_XZY.x / 2.0+0.5), 0,
                                             floor(modelio->voxelSize_XZY.z / 2.0+0.5)};;

            float scale0 = 1;
            float angle0 = 0;
            glm::vec3 shift = glm::vec3(shift0) - glm::vec3(semiRange);  // (5,0,5)

            //
            glm::mat4 unit = glm::mat4(1.0f);
            glm::vec3 scale = glm::vec3(scale0);
            glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0, 1.0, 0.0));
            glm::mat4 mat = glm::scale(unit, scale) * glm::translate(unit, shift);

            Instance instance1{};
            instance1.meshId = static_cast<uint32_t>(n_modelmesh);
            instance1.object2worldMatrix = mat;
            instance1.world2objectMatrix = glm::transpose(glm::inverse(mat));
            instanceio->instances.emplace_back(instance1);
            // voxel Instance Link
            InstanceLink instancelink1{};
            instancelink1.meshId = instance1.meshId;
            instanceio->instanceLinks.emplace_back(instancelink1);


            int isValid = 0;

            for (int kvoxel = 0; kvoxel < currentVoxelModel1.voxelIds.size(); kvoxel++) {
                // this is what we did in the shader;
                // glm::ivec3 pos =  mat * glm::vec4(currentVoxelModel.voxelIds[kvoxel],1.0);
                glm::ivec3 Id = shift0 + glm::ivec3(currentVoxelModel1.voxelIds[kvoxel]);
                int test = acc.getValue(nanovdb::Coord(Id.x, Id.y, Id.z));

                // the first bufferid;

                if (test < 0) {

                    acc.setValue(nanovdb::Coord(Id.x, Id.y, Id.z), n_voxel);
                    glm::ivec3 voxelPos = glm::ivec3(Id.x * 1.0, Id.y * 1.0, Id.z * 1.0);  //(5,0,5) with height = 0

                    VoxelLink voxelLink{};
                    voxelLink.voxelId = voxelPos;          //(5,0,5) with height = 0
                    voxelLink.instanceId = n_instance;
                    voxelLink.aeroId = 0;
                    voxelLink.faceId = 0; // center
                    voxelLink.isValid = 1; // center
                    modelio->m_voxelio->voxellinks.emplace_back(voxelLink);
                    n_voxel++;
                    isValid = 1;

                }
            }
            if (isValid == 0) {
                continue;
            }

            n_instance++;

            //std::string info = "voxel entity " + std::to_string(kVoxelModel) + " done.\n";
            //LOGI(info.c_str());
            // float tt = acc.getValue(nanovdb::Coord(24, 0, 24));
        }
        n_modelmesh++;
    }
    return true;
}

bool Scene::createPrimObj_Building(PrimEntity & voxelEntity,nanovdb::GridBuilder<int32_t> &nanoBuilder, std::shared_ptr<VoxelebIO> &modelio) {


    auto acc = nanoBuilder.getAccessor();
    auto &meshio = modelio->m_meshio;
    auto &instanceio = modelio->m_instanceio;
    auto &voxelio = modelio->m_voxelio;
    int &n_modelmesh = modelio->n_modelmesh;
    int &n_instance = modelio->n_instance;
    int &n_voxel = modelio->n_voxel;

    VoxelDesigner loader;
    /// ------------------------------------
    /// voxel model/mesh
    ///-------------------------------------
    std::string modelName = voxelEntity.primitiveName;
    Type type = voxelEntity.type;
    Shape shape = voxelEntity.shape;
    //meshio->types[0] = voxelEntity.types[0];
    // std::string aeroName = voxelEntity.aeroNames[0];


    PrimMesh currentVoxelModelXYZ1;
    if (voxelEntity.isheightFromFile == 1) {
        currentVoxelModelXYZ1 = loader.createTriEntitiesFromTif_wall(voxelEntity.heightfile, modelio->voxelSize_XZY,modelio->stepsize_height);
    } else {
        currentVoxelModelXYZ1 = loader.createTriCube_wall(shape, modelio->stepsize_surface);
    }

    PrimMesh currentVoxelModel1 = XYZ2XZY(currentVoxelModelXYZ1, 1); // (1,1,0) => (1,0,1) with  height = 0
    currentVoxelModel1.meshId = n_modelmesh;
    meshio->primMeshes.emplace_back(currentVoxelModel1); //xzy
    std::string meshName1 = voxelEntity.meshNames[0];
    std::string spectralName1 = voxelEntity.spectralNames[0];
    std::string canopyName1 = voxelEntity.canopyNames[0];
    std::string propName1 = voxelEntity.propNames[0];
    MeshLink meshlink1;
    meshlink1.spectralId = meshio->spectralNames.find(spectralName1)->second;
    meshlink1.thermalId = 0;
    meshlink1.canopyId = meshio->canopyNames.find(canopyName1)->second;
    if (type == Type::VEGETATION) {
        // meshlink1.leafbioId = meshio->leafbioNames.find(propName)->second;
        meshlink1.bioId = meshio->leafbioNames.find(propName1)->second;
    } else if (type == Type::SOIL || type == Type::BUILDING) {
        //  meshlink1.soilsetId = meshio->soilsetNames.find(propName)->second;
        meshlink1.bioId = meshio->soilsetNames.find(propName1)->second;
    }
    //  meshlink1.aeroId = meshio->aeroNames.find(aeroName)->second;
    meshlink1.type = (int) type;
    meshio->meshLinks.emplace_back(meshlink1);

    PrimMesh currentVoxelModel2{};
    PrimMesh currentVoxelModelXYZ2;
    if (voxelEntity.isheightFromFile == 1) {
        currentVoxelModelXYZ2 = loader.createTriEntitiesFromTif_roof(voxelEntity.heightfile, modelio->voxelSize_XZY,modelio->stepsize_height);
    } else {
        currentVoxelModelXYZ2 = loader.createTriCube_roof(shape, modelio->stepsize_surface);
    }

    currentVoxelModel2 = XYZ2XZY(currentVoxelModelXYZ2, 1); // (1,1,0) => (1,0,1) with  height = 0
    currentVoxelModel2.meshId = n_modelmesh + 1;
    meshio->primMeshes.emplace_back(currentVoxelModel2); //xzy

    /// ------------------------------------
    /// voxel model/mesh
    ///-------------------------------------
    std::string meshName2 = voxelEntity.meshNames[1];
    std::string spectralName2 = voxelEntity.spectralNames[1];
    std::string canopyName2 = voxelEntity.canopyNames[1];
    std::string propName2 = voxelEntity.propNames[1];
    MeshLink meshlink2;
    meshlink2.spectralId = meshio->spectralNames.find(spectralName2)->second;
    meshlink2.thermalId = 0;
    meshlink2.canopyId = meshio->canopyNames.find(canopyName2)->second;
    if (type == Type::VEGETATION) {
        // meshlink1.leafbioId = meshio->leafbioNames.find(propName)->second;
        meshlink2.bioId = meshio->leafbioNames.find(propName2)->second;
    } else if (type == Type::SOIL || type == Type::BUILDING) {
        //  meshlink1.soilsetId = meshio->soilsetNames.find(propName)->second;
        meshlink2.bioId = meshio->soilsetNames.find(propName2)->second;
    }
    //  meshlink1.aeroId = meshio->aeroNames.find(aeroName)->second;
    meshlink2.type = (int) type;
    meshio->meshLinks.emplace_back(meshlink2);


    // instance

    for (int kinstance = 0; kinstance < voxelEntity.primDistributions.size(); kinstance++) {
        // voxel instance1��change postion,

        glm::ivec3 shift0;
        shift0 = {voxelEntity.primDistributions[kinstance].x / modelio->stepsize_surface,
                  voxelEntity.primDistributions[kinstance].z / modelio->stepsize_surface,
                  voxelEntity.primDistributions[kinstance].y / modelio->stepsize_surface};


        ///-----------------------------------------------------------------------------
        /// Attention!!!!
        ///------------------------------------------------------------------------------
        glm::ivec3 semiRange = glm::vec3{floor(modelio->voxelSize_XZY.x / 2.0+0.5), 0,
                                         floor(modelio->voxelSize_XZY.z / 2.0+0.5)};;

        float scale0 = voxelEntity.scales[kinstance];
        float angle0 = voxelEntity.rotations[kinstance];
        glm::vec3 shift = glm::vec3(shift0) - glm::vec3(semiRange);  // (5,0,5)

        //
        glm::mat4 unit = glm::mat4(1.0f);
        glm::vec3 scale = glm::vec3(scale0);
        glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 mat = glm::scale(unit, scale) * glm::translate(unit, shift);

        Instance instance1{};
        instance1.meshId = static_cast<uint32_t>(n_modelmesh);
        instance1.object2worldMatrix = mat;
        instance1.world2objectMatrix = glm::transpose(glm::inverse(mat));
        instanceio->instances.emplace_back(instance1);
        // voxel Instance Link
        InstanceLink instancelink1{};
        instancelink1.meshId = instance1.meshId;
        instanceio->instanceLinks.emplace_back(instancelink1);

        Instance instance2{};
        instance2.meshId = static_cast<uint32_t>(n_modelmesh + 1);
        instance2.object2worldMatrix = mat;
        instance2.world2objectMatrix = glm::transpose(glm::inverse(mat));
        instanceio->instances.emplace_back(instance2);
        // voxel Instance Link
        InstanceLink instancelink2{};
        instancelink2.meshId = instance2.meshId;
        instanceio->instanceLinks.emplace_back(instancelink2);


        int isValid = 0;
        if ((voxelEntity.meshNames.size() == 2) && (voxelEntity.type == Type::BUILDING)) {

            for (int kvoxel = 0; kvoxel < currentVoxelModel1.voxelIds.size(); kvoxel++) {
                // this is what we did in the shader;
                // glm::ivec3 pos =  mat * glm::vec4(currentVoxelModel.voxelIds[kvoxel],1.0);
                glm::ivec3 Id = shift0 + glm::ivec3(currentVoxelModel1.voxelIds[kvoxel]);
                int test = acc.getValue(nanovdb::Coord(Id.x, Id.y, Id.z));

                // the first bufferid;

                if (test < 0) {
                    ///-----------------------------------------------------------------------------
                    /// Attention!!!! only the first buffer is collected.
                    ///------------------------------------------------------------------------------

                    acc.setValue(nanovdb::Coord(Id.x, Id.y, Id.z), n_voxel);
                    glm::ivec3 voxelPos = glm::ivec3(Id.x * 1.0, Id.y * 1.0, Id.z * 1.0);  //(5,0,5) with height = 0


                    for (int kf = 0; kf < 4; kf++) {
                        VoxelLink voxelLink{};
                        voxelLink.voxelId = voxelPos;          //(5,0,5) with height = 0
                        voxelLink.instanceId = n_instance;
                        voxelLink.aeroId = 0;
                        voxelLink.faceId = kf+1; // center
                        voxelLink.isValid = currentVoxelModel1.isValids[kvoxel].values[kf]; // center
                        modelio->m_voxelio->voxellinks.emplace_back(voxelLink);

                        n_voxel++;
                        isValid = 1;
                    }

                    for (int kf = 4; kf < 5; kf++) {
                        VoxelLink voxelLink{};
                        voxelLink.voxelId = voxelPos;          //(5,0,5) with height = 0
                        voxelLink.instanceId = n_instance + 1;
                        voxelLink.aeroId = 0;
                        voxelLink.faceId = kf+1; // center
                        voxelLink.isValid = currentVoxelModel1.isValids[kvoxel].values[kf];
                        modelio->m_voxelio->voxellinks.emplace_back(voxelLink);

                        n_voxel++;
                        isValid = 1;
                    }


                }

            }

            if (isValid == 0) {
                continue;
            }


            if (voxelEntity.meshNames.size() == 2) {
                n_instance = n_instance + 2;
            } else {
                n_instance++;
            }

        }


        if (voxelEntity.meshNames.size() == 2) {
            n_modelmesh = n_modelmesh + 2;
        } else {
            n_modelmesh++;
        }
        //std::string info = "voxel entity " + std::to_string(kVoxelModel) + " done.\n";
        //LOGI(info.c_str());
        // float tt = acc.getValue(nanovdb::Coord(24, 0, 24));
    }
    return true;
}

bool Scene::createPrimObj_Background(Background & background,nanovdb::GridBuilder<int32_t> &nanoBuilder, std::shared_ptr<VoxelebIO> &modelio){

    auto acc = nanoBuilder.getAccessor();
    auto &meshio = modelio->m_meshio;
    auto &instanceio = modelio->m_instanceio;
    auto &voxelio = modelio->m_voxelio;
    auto &surfio = modelio->m_surfio;


    modelio->sceneSize_XYZ =  background.sceneSize;
    modelio->sceneOrigin_XYZ = background.sceneOrigin;
    modelio->voxelSize_XZY = glm::ivec3(background.sceneSize.x / background.stepsize_surface,
                                        0,
                                        background.sceneSize.y / background.stepsize_surface);
    modelio->voxelOrigin_XZY = glm::ivec3(background.sceneOrigin.x / background.stepsize_surface,
                                          0,
                                          background.sceneOrigin.y / background.stepsize_surface);

    modelio->stepsize_surface = background.stepsize_surface;
    modelio->stepsize_height = background.stepsize_height;
    modelio->lat = background.lat;
    modelio->lon = background.lon;
//    modelio->stepsize_surface = 1.0;
    //modelio->n_surface = modelio->voxelSize_XZY.x * modelio->voxelSize_XZY.y;

    ///----------------------------------------------------------------
    /// From now on, using voxel space
    ///----------------------------------------------------------------

    int &n_modelmesh = modelio->n_modelmesh;
    int &n_instance = modelio->n_instance;
    int &n_voxel = modelio->n_voxel;

    /// ------------------------------------
    /// BACKGROUND mesh
    ///-------------------------------------

    VoxelDesigner loader;
    PrimMesh bgModelXYZ;
    if ((background.isDEM == true) && (background.DEMFile != "")) {
        std::cout<<("----------------error--------------------");
    } else {
        bgModelXYZ = loader.createTriBackground(modelio->sceneSize_XYZ.x, modelio->sceneSize_XYZ.y, background.stepsize_surface);
    }


    bgModelXYZ.meshId = n_modelmesh;
    PrimMesh bgModel = XYZ2XZY(bgModelXYZ);
    meshio->primMeshes.emplace_back(bgModel);

    // background model link
    MeshLink bgMeshLink;
    bgMeshLink.thermalId = 0;
    bgMeshLink.canopyId = 0;
    std::string bgSpectralName = background.bgSpectralName;
    bgMeshLink.spectralId = meshio->spectralNames.find(bgSpectralName)->second;
//    std::string bgThermalName;
//    int bgThermalIndex = 0;
//    if (fileio->m_pVoxelebXml->sensorxml.isTemperature == true)
//    {
//        bgThermalName = scenexml.background.bgThermalName;
//        bgThermalIndex = meshio->thermalNames.find(bgThermalName)->second;
//    }

    int bgCanopyindex = 0;
    //  std::string bgCanopyName = scenexml.background.canopyName;
    //  bgMeshLink.canopyId = meshio->canopyNames.find(bgCanopyName)->second;
    int bgPropIndex = 0;
    std::string bgPropName = background.bgPropName;
    if (background.type == Type::WATER) {
        bgPropIndex = meshio->watersetNames.find(bgPropName)->second;
    } else {
        bgPropIndex = meshio->soilsetNames.find(bgPropName)->second;
    }
    bgMeshLink.bioId = bgPropIndex;
    bgMeshLink.type = static_cast<int>(background.type);
    meshio->meshLinks.emplace_back(bgMeshLink);

    /// ------------------------------------
    /// BACKGROUND instance
    /// Voxelsize is XZY
    /// iN cg space: (voxelsize.x - voxelsize.x/2, voxelsize.y, voxelsize.z - voxelsize.z/2)
    ///-------------------------------------
    Instance bgInstance{};
    bgInstance.meshId = static_cast<uint32_t>(n_modelmesh);
    glm::mat4 bgunit = glm::mat4(1.0f);
    // here for 0-9 will become -5 - 4
    glm::vec3 bgShift = glm::vec3{-floor(modelio->voxelSize_XZY.x / 2.0+0.5), 0 - loader.minElevation,
                                  -floor(modelio->voxelSize_XZY.z / 2.0+0.5)};

//    glm::vec3 bgShift = glm::vec3{0,0,0};

    glm::vec3 bgScale = glm::vec3{1.0, 1.0, 1.0};
    glm::mat4 bgMat = glm::scale(bgunit, bgScale) * glm::translate(bgunit, bgShift);
    bgInstance.object2worldMatrix = bgMat;
    bgInstance.world2objectMatrix = glm::transpose(glm::inverse(bgMat));
    instanceio->instances.emplace_back(bgInstance);


    // background instanceLink
    InstanceLink bgInstanceLink{};
    bgInstanceLink.meshId = bgInstance.meshId;
    instanceio->instanceLinks.emplace_back(bgInstanceLink);

    //--------------------------------------------
    // background voxelLink
    //--------------------------------------------
    for (int kvoxel = 0; kvoxel < bgModel.voxelIds.size(); kvoxel++) {
        glm::ivec3 voxelId = glm::ivec3(bgModel.voxelIds[kvoxel]);// + nvmath::vec3i(bgShift);
        int test = acc.getValue(nanovdb::Coord(voxelId.x, voxelId.y, voxelId.z)); //(1,0,1)

        if (test < 0) {
            acc.setValue(nanovdb::Coord(voxelId.x, voxelId.y, voxelId.z), n_voxel);
            VoxelLink voxellink{};
            glm::ivec3 voxelId_ = glm::ivec3(voxelId.x * 1.0, voxelId.y * 1.0, voxelId.z * 1.0);
            voxellink.voxelId = voxelId_;
            voxellink.instanceId = n_instance;
            voxellink.faceId = 5;
            voxellink.isValid = 1;
            voxellink.aeroId = 0;

            voxelio->voxellinks.emplace_back(voxellink);
            n_voxel++;
        }
    }
    n_instance++;
    n_modelmesh++;


    if(background.isLad==true){
        // read lad tif
        int width = 0,height = 0, nband = 1;
        Utils::readImageinout1(background.ladfile,surfio->lads,width, height,nband);
    }else{
        surfio->lads.emplace_back(0);
    }

    /// ------------------------------------
    ///  Divided  Background, Need to be provided
    ///-------------------------------------

    /// ------------------------------------
    ///  Divided  Background, Need to be provided
    ///-------------------------------------

    return true;
}

//-----------------------------------------------------------
//--- n_moxelmesh,n_instance,n_voxel
//--- primMeshes, meshlink, instance, instanceLink, nanovdb, voxellink
//------------------------------------------------------------

bool Scene::createPrimObjScene(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelrtIO> &voxellstio) {


    auto &scenexml = fileio->m_pVoxelrtXml->scenexml;
    auto &voxellstxml = fileio->m_pVoxelrtXml;
    auto &meshio = voxellstio->m_meshio;
    auto &instanceio = voxellstio->m_instanceio;
    auto &voxelio = voxellstio->m_voxelio;
    auto &background = fileio->m_pVoxelrtXml->scenexml.background;
    nanovdb::GridBuilder<int32_t> nanoBuilder(-1);


    /// ------------------------------------
    /// Background
    ///-------------------------------------
    createPrimObj_Background(background, nanoBuilder, voxellstio);

    /// ------------------------------------
    /// voxel Components
    ///-------------------------------------
    for (int kVoxelModel = 0; kVoxelModel < scenexml.primEntities.size(); kVoxelModel++)
    {
        auto &voxelEntity = scenexml.primEntities[kVoxelModel];


        if (voxelEntity.type == Type::VEGETATION)
        {
            if(voxelEntity.isshapeFromFile == true)
            {
                createPrimObj_Crowns(voxelEntity, nanoBuilder, voxellstio);
            }else {
                createPrimObj_Crown(voxelEntity, nanoBuilder, voxellstio);
            }
        }else if(voxelEntity.type == Type::BUILDING)
        {
            createPrimObj_Building(voxelEntity, nanoBuilder, voxellstio);
        }

    }
    voxellstio->m_voxelio->nanoHandle = nanoBuilder.getHandle<>();


    return true;
}

bool Scene::createPrimObj_Crown(PrimEntity & voxelEntity,nanovdb::GridBuilder<int32_t> &nanoBuilder,std::shared_ptr<VoxelrtIO> &modelio){

    auto acc = nanoBuilder.getAccessor();
    auto &meshio = modelio->m_meshio;
    auto &instanceio = modelio->m_instanceio;
    auto &voxelio = modelio->m_voxelio;
    int &n_modelmesh = modelio->n_modelmesh;
    int &n_instance = modelio->n_instance;
    int &n_voxel = modelio->n_voxel;

    VoxelDesigner loader;
    /// ------------------------------------
    /// voxel model/mesh
    ///-------------------------------------
    std::string modelName = voxelEntity.primitiveName;
    Type type = voxelEntity.type;
    Shape shape = voxelEntity.shape;
    //meshio->types[0] = voxelEntity.types[0];
    // std::string aeroName = voxelEntity.aeroNames[0];


    PrimMesh currentVoxelModelXYZ1;
    currentVoxelModelXYZ1 = loader.createTriEntity(shape, modelio->stepsize_surface);
    PrimMesh currentVoxelModel1 = XYZ2XZY(currentVoxelModelXYZ1, 0); // (1,1,0) => (1,0,1) with  height = 0
    currentVoxelModel1.meshId = n_modelmesh;
    meshio->primMeshes.emplace_back(currentVoxelModel1); //xzy
    std::string meshName1 = voxelEntity.meshNames[0];
    std::string spectralName1 = voxelEntity.spectralNames[0];
    std::string thermalName1 = voxelEntity.thermalNames[0];
    std::string canopyName1 = voxelEntity.canopyNames[0];
    std::string propName1 = voxelEntity.propNames[0];
    MeshLink meshlink1;
    meshlink1.spectralId = meshio->spectralNames.find(spectralName1)->second;
    meshlink1.thermalId = meshio->thermalNames.find(thermalName1)->second;
    meshlink1.canopyId = meshio->canopyNames.find(canopyName1)->second;
//    if (type == Type::VEGETATION) {
//        // meshlink1.leafbioId = meshio->leafbioNames.find(propName)->second;
//        meshlink1.bioId = meshio->leafbioNames.find(propName1)->second;
//    } else if (type == Type::SOIL || type == Type::BUILDING) {
//        //  meshlink1.soilsetId = meshio->soilsetNames.find(propName)->second;
//        meshlink1.bioId = meshio->soilsetNames.find(propName1)->second;
//    }
    //  meshlink1.aeroId = meshio->aeroNames.find(aeroName)->second;
    meshlink1.type = (int) type;
    meshio->meshLinks.emplace_back(meshlink1);



    if(voxelEntity.isdisFromFile == true){
        int n_dis = 0;
        float *tempx, *tempy,*tempz;
        tempx = Utils::readascfile(voxelEntity.distributefile,0,0,n_dis);
        tempy = Utils::readascfile(voxelEntity.distributefile,0,1,n_dis);
        tempz = Utils::readascfile(voxelEntity.distributefile,0,2,n_dis);
        voxelEntity.primDistributions.resize(n_dis);
        voxelEntity.scales.resize(n_dis);
        voxelEntity.rotations.resize(n_dis);
        for(int kin = 0;kin<n_dis;kin++)
        {
            voxelEntity.primDistributions[kin]=(glm::vec3(tempx[kin],tempy[kin],tempz[kin]));
            voxelEntity.scales[kin] = 1.0;
            voxelEntity.rotations[kin] = 0.0;
        }
    }

    // instance

    for (int kinstance = 0; kinstance < voxelEntity.primDistributions.size(); kinstance++) {
        // voxel instance1��change postion,

        glm::ivec3 shift0;
        shift0 = {voxelEntity.primDistributions[kinstance].x / modelio->stepsize_surface,
                  voxelEntity.primDistributions[kinstance].z / modelio->stepsize_surface,
                  voxelEntity.primDistributions[kinstance].y / modelio->stepsize_surface};

        ///-----------------------------------------------------------------------------
        ///
        ///------------------------------------------------------------------------------
        glm::ivec3 semiRange = glm::vec3{floor(modelio->voxelSize_XZY.x / 2.0+0.5), 0,
                                         floor(modelio->voxelSize_XZY.z / 2.0+0.5)};;

        float scale0 = voxelEntity.scales[kinstance];
        float angle0 = voxelEntity.rotations[kinstance];
        glm::vec3 shift = glm::vec3(shift0) - glm::vec3(semiRange);  // (5,0,5)

        //
        glm::mat4 unit = glm::mat4(1.0f);
        glm::vec3 scale = glm::vec3(scale0);
        glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 mat = glm::scale(unit, scale) * glm::translate(unit, shift);

        Instance instance1{};
        instance1.meshId = static_cast<uint32_t>(n_modelmesh);
        instance1.object2worldMatrix = mat;
        instance1.world2objectMatrix = glm::transpose(glm::inverse(mat));
        instanceio->instances.emplace_back(instance1);
        // voxel Instance Link
        InstanceLink instancelink1{};
        instancelink1.meshId = instance1.meshId;
        instanceio->instanceLinks.emplace_back(instancelink1);


        int isValid = 0;

        for (int kvoxel = 0; kvoxel < currentVoxelModel1.voxelIds.size(); kvoxel++) {
            // this is what we did in the shader;
            // glm::ivec3 pos =  mat * glm::vec4(currentVoxelModel.voxelIds[kvoxel],1.0);
            glm::ivec3 Id = shift0 + glm::ivec3(currentVoxelModel1.voxelIds[kvoxel]);
            int test = acc.getValue(nanovdb::Coord(Id.x, Id.y, Id.z));

            // the first bufferid;

            if (test < 0) {

                acc.setValue(nanovdb::Coord(Id.x, Id.y, Id.z), n_voxel);
                glm::ivec3 voxelPos = glm::ivec3(Id.x * 1.0, Id.y * 1.0, Id.z * 1.0);  //(5,0,5) with height = 0

                VoxelLink voxelLink{};
                voxelLink.voxelId = voxelPos;          //(5,0,5) with height = 0
                voxelLink.instanceId = n_instance;
                voxelLink.aeroId = 0;
                voxelLink.faceId = 0; // center
                voxelLink.isValid = 1; // center
                modelio->m_voxelio->voxellinks.emplace_back(voxelLink);
                n_voxel++;
                isValid = 1;

            }
        }
        if (isValid == 0) {
            continue;
        }

        n_instance++;

        //std::string info = "voxel entity " + std::to_string(kVoxelModel) + " done.\n";
        //LOGI(info.c_str());
        // float tt = acc.getValue(nanovdb::Coord(24, 0, 24));
    }
    n_modelmesh++;
    return true;
}

bool Scene::createPrimObj_Crowns(PrimEntity & voxelEntity,nanovdb::GridBuilder<int32_t> &nanoBuilder,std::shared_ptr<VoxelrtIO> &modelio){

    auto acc = nanoBuilder.getAccessor();
    auto &meshio = modelio->m_meshio;
    auto &instanceio = modelio->m_instanceio;
    auto &voxelio = modelio->m_voxelio;
    int &n_modelmesh = modelio->n_modelmesh;
    int &n_instance = modelio->n_instance;
    int &n_voxel = modelio->n_voxel;

    VoxelDesigner loader;
    /// ------------------------------------
    /// voxel model/mesh
    ///-------------------------------------
    std::string modelName = voxelEntity.primitiveName;
    Type type = voxelEntity.type;

    //meshio->types[0] = voxelEntity.types[0];
    // std::string aeroName = voxelEntity.aeroNames[0];


    int n_dis = 0;
    float *tempx, *tempy,*tempz,*temp1, *temp2,*temp3,*temps,*tempsc,*tempr;
    temps = Utils::readascfile(voxelEntity.shapefile,0,0,n_dis);
    temp1 = Utils::readascfile(voxelEntity.shapefile,0,1,n_dis);
    temp2 = Utils::readascfile(voxelEntity.shapefile,0,2,n_dis);
    temp3 = Utils::readascfile(voxelEntity.shapefile,0,3,n_dis);
    tempx = Utils::readascfile(voxelEntity.shapefile,0,4,n_dis);
    tempy = Utils::readascfile(voxelEntity.shapefile,0,5,n_dis);
    tempz = Utils::readascfile(voxelEntity.shapefile,0,6,n_dis);
    tempsc = Utils::readascfile(voxelEntity.shapefile,0,7,n_dis);
    tempr = Utils::readascfile(voxelEntity.shapefile,0,8,n_dis);

    for(int k = 0;k<n_dis;k++) {


        Shape shape = Shape{ShapeType(temps[k]),temp1[k],temp2[k],temp3[k],{tempx[k],tempy[k],tempz[k]}};


        PrimMesh currentVoxelModelXYZ1;
        currentVoxelModelXYZ1 = loader.createTriEntity(shape, modelio->stepsize_surface);
        PrimMesh currentVoxelModel1 = XYZ2XZY(currentVoxelModelXYZ1, 1); // (1,1,0) => (1,0,1) with  height = 0
        currentVoxelModel1.meshId = n_modelmesh;
        meshio->primMeshes.emplace_back(currentVoxelModel1); //xzy
        std::string meshName1 = voxelEntity.meshNames[0];
        std::string spectralName1 = voxelEntity.spectralNames[0];
        std::string canopyName1 = voxelEntity.canopyNames[0];
        std::string propName1 = voxelEntity.propNames[0];
        MeshLink meshlink1;
        meshlink1.spectralId = meshio->spectralNames.find(spectralName1)->second;
        meshlink1.thermalId = 0;
        meshlink1.canopyId = meshio->canopyNames.find(canopyName1)->second;
//        if (type == Type::VEGETATION) {
//            // meshlink1.leafbioId = meshio->leafbioNames.find(propName)->second;
//            meshlink1.bioId = meshio->leafbioNames.find(propName1)->second;
//        } else if (type == Type::SOIL || type == Type::BUILDING) {
//            //  meshlink1.soilsetId = meshio->soilsetNames.find(propName)->second;
//            meshlink1.bioId = meshio->soilsetNames.find(propName1)->second;
//        }
        //  meshlink1.aeroId = meshio->aeroNames.find(aeroName)->second;
        meshlink1.type = (int) type;
        meshio->meshLinks.emplace_back(meshlink1);

        // instance


        for (int kinstance = 0; kinstance < 1; kinstance++) {
            // voxel instance1��change postion,

            glm::ivec3 shift0;
            shift0 = {shape.pos.x / modelio->stepsize_surface,
                      shape.pos.z / modelio->stepsize_surface,
                      shape.pos.y / modelio->stepsize_surface};

            ///-----------------------------------------------------------------------------
            /// Attention!!!!
            ///------------------------------------------------------------------------------
            glm::ivec3 semiRange = glm::vec3{floor(modelio->voxelSize_XZY.x / 2.0+0.5), 0,
                                             floor(modelio->voxelSize_XZY.z / 2.0+0.5)};;

            float scale0 = 1;
            float angle0 = 0;
            glm::vec3 shift = glm::vec3(shift0) - glm::vec3(semiRange);  // (5,0,5)

            //
            glm::mat4 unit = glm::mat4(1.0f);
            glm::vec3 scale = glm::vec3(scale0);
            glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0, 1.0, 0.0));
            glm::mat4 mat = glm::scale(unit, scale) * glm::translate(unit, shift);

            Instance instance1{};
            instance1.meshId = static_cast<uint32_t>(n_modelmesh);
            instance1.object2worldMatrix = mat;
            instance1.world2objectMatrix = glm::transpose(glm::inverse(mat));
            instanceio->instances.emplace_back(instance1);
            // voxel Instance Link
            InstanceLink instancelink1{};
            instancelink1.meshId = instance1.meshId;
            instanceio->instanceLinks.emplace_back(instancelink1);


            int isValid = 0;

            for (int kvoxel = 0; kvoxel < currentVoxelModel1.voxelIds.size(); kvoxel++) {
                // this is what we did in the shader;
                // glm::ivec3 pos =  mat * glm::vec4(currentVoxelModel.voxelIds[kvoxel],1.0);
                glm::ivec3 Id = shift0 + glm::ivec3(currentVoxelModel1.voxelIds[kvoxel]);
                int test = acc.getValue(nanovdb::Coord(Id.x, Id.y, Id.z));

                // the first bufferid;

                if (test < 0) {

                    acc.setValue(nanovdb::Coord(Id.x, Id.y, Id.z), n_voxel);
                    glm::ivec3 voxelPos = glm::ivec3(Id.x * 1.0, Id.y * 1.0, Id.z * 1.0);  //(5,0,5) with height = 0

                    VoxelLink voxelLink{};
                    voxelLink.voxelId = voxelPos;          //(5,0,5) with height = 0
                    voxelLink.instanceId = n_instance;
                    voxelLink.aeroId = 0;
                    voxelLink.faceId = 0; // center
                    voxelLink.isValid = 1; // center
                    modelio->m_voxelio->voxellinks.emplace_back(voxelLink);
                    n_voxel++;
                    isValid = 1;

                }
            }
            if (isValid == 0) {
                continue;
            }

            n_instance++;

            //std::string info = "voxel entity " + std::to_string(kVoxelModel) + " done.\n";
            //LOGI(info.c_str());
            // float tt = acc.getValue(nanovdb::Coord(24, 0, 24));
        }
        n_modelmesh++;
    }
    return true;
}

bool Scene::createPrimObj_Building(PrimEntity & voxelEntity,nanovdb::GridBuilder<int32_t> &nanoBuilder, std::shared_ptr<VoxelrtIO> &modelio) {


    auto acc = nanoBuilder.getAccessor();
    auto &meshio = modelio->m_meshio;
    auto &instanceio = modelio->m_instanceio;
    auto &voxelio = modelio->m_voxelio;
    int &n_modelmesh = modelio->n_modelmesh;
    int &n_instance = modelio->n_instance;
    int &n_voxel = modelio->n_voxel;

    VoxelDesigner loader;
    /// ------------------------------------
    /// voxel model/mesh
    ///-------------------------------------
    std::string modelName = voxelEntity.primitiveName;
    Type type = voxelEntity.type;
    Shape shape = voxelEntity.shape;
    //meshio->types[0] = voxelEntity.types[0];
    // std::string aeroName = voxelEntity.aeroNames[0];


    PrimMesh currentVoxelModelXYZ1;
    if (voxelEntity.isheightFromFile == 1) {
        currentVoxelModelXYZ1 = loader.createTriEntitiesFromTif_wall(voxelEntity.heightfile, modelio->voxelSize_XZY,modelio->stepsize_height);
    } else {
        currentVoxelModelXYZ1 = loader.createTriCube_wall(shape, modelio->stepsize_surface);
    }

    PrimMesh currentVoxelModel1 = XYZ2XZY(currentVoxelModelXYZ1, 1); // (1,1,0) => (1,0,1) with  height = 0
    currentVoxelModel1.meshId = n_modelmesh;
    meshio->primMeshes.emplace_back(currentVoxelModel1); //xzy
    std::string meshName1 = voxelEntity.meshNames[0];
    std::string spectralName1 = voxelEntity.spectralNames[0];
    std::string canopyName1 = voxelEntity.canopyNames[0];
//    std::string propName1 = voxelEntity.propNames[0];
    MeshLink meshlink1;
    meshlink1.spectralId = meshio->spectralNames.find(spectralName1)->second;
    meshlink1.thermalId = 0;
    meshlink1.canopyId = meshio->canopyNames.find(canopyName1)->second;
//    if (type == Type::VEGETATION) {
//        // meshlink1.leafbioId = meshio->leafbioNames.find(propName)->second;
//        meshlink1.bioId = meshio->leafbioNames.find(propName1)->second;
//    } else if (type == Type::SOIL || type == Type::BUILDING) {
//        //  meshlink1.soilsetId = meshio->soilsetNames.find(propName)->second;
//        meshlink1.bioId = meshio->soilsetNames.find(propName1)->second;
//    }
    //  meshlink1.aeroId = meshio->aeroNames.find(aeroName)->second;
    meshlink1.type = (int) type;
    meshio->meshLinks.emplace_back(meshlink1);

    PrimMesh currentVoxelModel2{};
    PrimMesh currentVoxelModelXYZ2;
    if (voxelEntity.isheightFromFile == 1) {
        currentVoxelModelXYZ2 = loader.createTriEntitiesFromTif_roof(voxelEntity.heightfile, modelio->voxelSize_XZY,modelio->stepsize_height);
    } else {
        currentVoxelModelXYZ2 = loader.createTriCube_roof(shape, modelio->stepsize_surface);
    }

    currentVoxelModel2 = XYZ2XZY(currentVoxelModelXYZ2, 1); // (1,1,0) => (1,0,1) with  height = 0
    currentVoxelModel2.meshId = n_modelmesh + 1;
    meshio->primMeshes.emplace_back(currentVoxelModel2); //xzy

    /// ------------------------------------
    /// voxel model/mesh
    ///-------------------------------------
    std::string meshName2 = voxelEntity.meshNames[1];
    std::string spectralName2 = voxelEntity.spectralNames[1];
    std::string canopyName2 = voxelEntity.canopyNames[1];
//    std::string propName2 = voxelEntity.propNames[1];
    MeshLink meshlink2;
    meshlink2.spectralId = meshio->spectralNames.find(spectralName2)->second;
    meshlink2.thermalId = 0;
    meshlink2.canopyId = meshio->canopyNames.find(canopyName2)->second;
//    if (type == Type::VEGETATION) {
//        // meshlink1.leafbioId = meshio->leafbioNames.find(propName)->second;
//        meshlink2.bioId = meshio->leafbioNames.find(propName2)->second;
//    } else if (type == Type::SOIL || type == Type::BUILDING) {
//        //  meshlink1.soilsetId = meshio->soilsetNames.find(propName)->second;
//        meshlink2.bioId = meshio->soilsetNames.find(propName2)->second;
//    }
    //  meshlink1.aeroId = meshio->aeroNames.find(aeroName)->second;
    meshlink2.type = (int) type;
    meshio->meshLinks.emplace_back(meshlink2);


    // instance

    for (int kinstance = 0; kinstance < voxelEntity.primDistributions.size(); kinstance++) {
        // voxel instance1��change postion,

        glm::ivec3 shift0;
        shift0 = {voxelEntity.primDistributions[kinstance].x / modelio->stepsize_surface,
                  voxelEntity.primDistributions[kinstance].z / modelio->stepsize_surface,
                  voxelEntity.primDistributions[kinstance].y / modelio->stepsize_surface};


        ///-----------------------------------------------------------------------------
        /// Attention!!!!
        ///------------------------------------------------------------------------------
        glm::ivec3 semiRange = glm::vec3{floor(modelio->voxelSize_XZY.x / 2.0+0.5), 0,
                                         floor(modelio->voxelSize_XZY.z / 2.0+0.5)};;

        float scale0 = voxelEntity.scales[kinstance];
        float angle0 = voxelEntity.rotations[kinstance];
        glm::vec3 shift = glm::vec3(shift0) - glm::vec3(semiRange);  // (5,0,5)

        //
        glm::mat4 unit = glm::mat4(1.0f);
        glm::vec3 scale = glm::vec3(scale0);
        glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 mat = glm::scale(unit, scale) * glm::translate(unit, shift);

        Instance instance1{};
        instance1.meshId = static_cast<uint32_t>(n_modelmesh);
        instance1.object2worldMatrix = mat;
        instance1.world2objectMatrix = glm::transpose(glm::inverse(mat));
        instanceio->instances.emplace_back(instance1);
        // voxel Instance Link
        InstanceLink instancelink1{};
        instancelink1.meshId = instance1.meshId;
        instanceio->instanceLinks.emplace_back(instancelink1);

        Instance instance2{};
        instance2.meshId = static_cast<uint32_t>(n_modelmesh + 1);
        instance2.object2worldMatrix = mat;
        instance2.world2objectMatrix = glm::transpose(glm::inverse(mat));
        instanceio->instances.emplace_back(instance2);
        // voxel Instance Link
        InstanceLink instancelink2{};
        instancelink2.meshId = instance2.meshId;
        instanceio->instanceLinks.emplace_back(instancelink2);


        int isValid = 0;
        if ((voxelEntity.meshNames.size() == 2) && (voxelEntity.type == Type::BUILDING)) {

            for (int kvoxel = 0; kvoxel < currentVoxelModel1.voxelIds.size(); kvoxel++) {
                // this is what we did in the shader;
                // glm::ivec3 pos =  mat * glm::vec4(currentVoxelModel.voxelIds[kvoxel],1.0);
                glm::ivec3 Id = shift0 + glm::ivec3(currentVoxelModel1.voxelIds[kvoxel]);
                int test = acc.getValue(nanovdb::Coord(Id.x, Id.y, Id.z));

                // the first bufferid;

                if (test < 0) {
                    ///-----------------------------------------------------------------------------
                    /// Attention!!!! only the first buffer is collected.
                    ///------------------------------------------------------------------------------

                    acc.setValue(nanovdb::Coord(Id.x, Id.y, Id.z), n_voxel);
                    glm::ivec3 voxelPos = glm::ivec3(Id.x * 1.0, Id.y * 1.0, Id.z * 1.0);  //(5,0,5) with height = 0


                    for (int kf = 0; kf < 4; kf++) {
                        VoxelLink voxelLink{};
                        voxelLink.voxelId = voxelPos;          //(5,0,5) with height = 0
                        voxelLink.instanceId = n_instance;
                        voxelLink.aeroId = 0;
                        voxelLink.faceId = kf+1; // center
                        voxelLink.isValid = currentVoxelModel1.isValids[kvoxel].values[kf]; // center
                        modelio->m_voxelio->voxellinks.emplace_back(voxelLink);

                        n_voxel++;
                        isValid = 1;
                    }

                    for (int kf = 4; kf < 5; kf++) {
                        VoxelLink voxelLink{};
                        voxelLink.voxelId = voxelPos;          //(5,0,5) with height = 0
                        voxelLink.instanceId = n_instance + 1;
                        voxelLink.aeroId = 0;
                        voxelLink.faceId = kf+1; // center
                        voxelLink.isValid = currentVoxelModel1.isValids[kvoxel].values[kf];
                        modelio->m_voxelio->voxellinks.emplace_back(voxelLink);

                        n_voxel++;
                        isValid = 1;
                    }


                }

            }

            if (isValid == 0) {
                continue;
            }


            if (voxelEntity.meshNames.size() == 2) {
                n_instance = n_instance + 2;
            } else {
                n_instance++;
            }

        }


        if (voxelEntity.meshNames.size() == 2) {
            n_modelmesh = n_modelmesh + 2;
        } else {
            n_modelmesh++;
        }
        //std::string info = "voxel entity " + std::to_string(kVoxelModel) + " done.\n";
        //LOGI(info.c_str());
        // float tt = acc.getValue(nanovdb::Coord(24, 0, 24));
    }
    return true;
}

bool Scene::createPrimObj_Background(Background & background,nanovdb::GridBuilder<int32_t> &nanoBuilder, std::shared_ptr<VoxelrtIO> &modelio){

    auto acc = nanoBuilder.getAccessor();
    auto &meshio = modelio->m_meshio;
    auto &instanceio = modelio->m_instanceio;
    auto &voxelio = modelio->m_voxelio;
    auto &surfio = modelio->m_surfio;


    modelio->sceneSize_XYZ =  background.sceneSize;
    modelio->sceneOrigin_XYZ = background.sceneOrigin;
    modelio->voxelSize_XZY = glm::ivec3(background.sceneSize.x / background.stepsize_surface,
                                        0,
                                        background.sceneSize.y / background.stepsize_surface);
    modelio->voxelOrigin_XZY = glm::ivec3(background.sceneOrigin.x / background.stepsize_surface,
                                          0,
                                          background.sceneOrigin.y / background.stepsize_surface);

    modelio->stepsize_surface = background.stepsize_surface;
    modelio->stepsize_height = background.stepsize_height;
    modelio->lat = background.lat;
    modelio->lon = background.lon;
//    modelio->stepsize_surface = 1.0;
    //modelio->n_surface = modelio->voxelSize_XZY.x * modelio->voxelSize_XZY.y;

    ///----------------------------------------------------------------
    /// From now on, using voxel space
    ///----------------------------------------------------------------

    int &n_modelmesh = modelio->n_modelmesh;
    int &n_instance = modelio->n_instance;
    int &n_voxel = modelio->n_voxel;

    /// ------------------------------------
    /// BACKGROUND mesh
    ///-------------------------------------

    VoxelDesigner loader;
    PrimMesh bgModelXYZ;
    if ((background.isDEM == true) && (background.DEMFile != "")) {
        std::cout<<("----------------error--------------------");
    } else {
        bgModelXYZ = loader.createTriBackground(modelio->sceneSize_XYZ.x, modelio->sceneSize_XYZ.y, background.stepsize_surface);
    }


    bgModelXYZ.meshId = n_modelmesh;
    PrimMesh bgModel = XYZ2XZY(bgModelXYZ);
    meshio->primMeshes.emplace_back(bgModel);

    // background model link
    MeshLink bgMeshLink;
    std::string bgThermalName = background.bgThermalName;
    bgMeshLink.thermalId =  meshio->thermalNames.find(bgThermalName)->second;
    bgMeshLink.canopyId = 0;
    std::string bgSpectralName = background.bgSpectralName;
    bgMeshLink.spectralId = meshio->spectralNames.find(bgSpectralName)->second;
//    std::string bgThermalName;
//    int bgThermalIndex = 0;
//    if (fileio->m_pVoxelebXml->sensorxml.isTemperature == true)
//    {
//        bgThermalName = scenexml.background.bgThermalName;
//        bgThermalIndex = meshio->thermalNames.find(bgThermalName)->second;
//    }

    int bgCanopyindex = 0;
    //  std::string bgCanopyName = scenexml.background.canopyName;
    //  bgMeshLink.canopyId = meshio->canopyNames.find(bgCanopyName)->second;
    int bgPropIndex = 0;
    std::string bgPropName = background.bgPropName;
    //bgPropIndex = meshio->soilsetNames.find(bgPropName)->second;
    //bgMeshLink.bioId = bgPropIndex;
    bgMeshLink.type = (int) Type::SOIL;
    meshio->meshLinks.emplace_back(bgMeshLink);

    /// ------------------------------------
    /// BACKGROUND instance
    /// Voxelsize is XZY
    /// iN cg space: (voxelsize.x - voxelsize.x/2, voxelsize.y, voxelsize.z - voxelsize.z/2)
    ///-------------------------------------
    Instance bgInstance{};
    bgInstance.meshId = static_cast<uint32_t>(n_modelmesh);
    glm::mat4 bgunit = glm::mat4(1.0f);
    // here for 0-9 will become -5 - 4
    glm::vec3 bgShift = glm::vec3{-floor(modelio->voxelSize_XZY.x / 2.0+0.5), 0 - loader.minElevation,
                                  -floor(modelio->voxelSize_XZY.z / 2.0+0.5)};

//    glm::vec3 bgShift = glm::vec3{0,0,0};

    glm::vec3 bgScale = glm::vec3{1.0, 1.0, 1.0};
    glm::mat4 bgMat = glm::scale(bgunit, bgScale) * glm::translate(bgunit, bgShift);
    bgInstance.object2worldMatrix = bgMat;
    bgInstance.world2objectMatrix = glm::transpose(glm::inverse(bgMat));
    instanceio->instances.emplace_back(bgInstance);


    // background instanceLink
    InstanceLink bgInstanceLink{};
    bgInstanceLink.meshId = bgInstance.meshId;
    instanceio->instanceLinks.emplace_back(bgInstanceLink);

    //--------------------------------------------
    // background voxelLink
    //--------------------------------------------
    for (int kvoxel = 0; kvoxel < bgModel.voxelIds.size(); kvoxel++) {
        glm::ivec3 voxelId = glm::ivec3(bgModel.voxelIds[kvoxel]);// + nvmath::vec3i(bgShift);
        int test = acc.getValue(nanovdb::Coord(voxelId.x, voxelId.y, voxelId.z)); //(1,0,1)

        if (test < 0) {
            acc.setValue(nanovdb::Coord(voxelId.x, voxelId.y, voxelId.z), n_voxel);
            VoxelLink voxellink{};
            glm::ivec3 voxelId_ = glm::ivec3(voxelId.x * 1.0, voxelId.y * 1.0, voxelId.z * 1.0);
            voxellink.voxelId = voxelId_;
            voxellink.instanceId = n_instance;
            voxellink.faceId = 5;
            voxellink.isValid = 1;
            voxellink.aeroId = 0;

            voxelio->voxellinks.emplace_back(voxellink);
            n_voxel++;
        }
    }
    n_instance++;
    n_modelmesh++;


    if(background.isLad==true){
        // read lad tif
        int width = 0,height = 0, nband = 1;
        Utils::readImageinout1(background.ladfile,surfio->lads,width, height,nband);
    }else{
        surfio->lads.emplace_back(0);
    }

    /// ------------------------------------
    ///  Divided  Background, Need to be provided
    ///-------------------------------------

    /// ------------------------------------
    ///  Divided  Background, Need to be provided
    ///-------------------------------------

    return true;
}



PrimMesh Scene::XYZ2XZY(PrimMesh model,int mark){
    PrimMesh temp;
    temp.meshId = model.meshId;
    temp.nIndices = model.nIndices;
    temp.nVertices = model.nVertices;
    temp.indices = std::move(model.indices);
    temp.vertices.reserve(model.vertices.size());
    for (const VertexAttribute& source : model.vertices)
    {
        VertexAttribute vertex{};
        vertex.color = source.color;
        vertex.nrm = source.nrm;
        vertex.texCoord = source.texCoord;
        vertex.pos = {source.pos.x, source.pos.z, source.pos.y};
        temp.vertices.emplace_back(vertex);
    }
    temp.nVertices = static_cast<uint32_t>(temp.vertices.size());
    for (int i = 0; i < model.voxelIds.size(); i++)
    {
        glm::vec3 center = {
                model.voxelIds[i].x,
                model.voxelIds[i].z,
                model.voxelIds[i].y};
        temp.voxelIds.emplace_back(center);
    }

    if (mark == 1) {
        temp.isValids.reserve(model.voxelIds.size());
        temp.faceIds.reserve(model.voxelIds.size());
        for (size_t i = 0; i < model.voxelIds.size(); ++i) {
            temp.isValids.emplace_back(i < model.isValids.size() ? model.isValids[i] : int5{});
            temp.faceIds.emplace_back(i < model.faceIds.size() ? model.faceIds[i] : 0);
        }
    }

    return temp;
}


ObjMesh Scene::XYZ2XZY(ObjMesh model){
    ObjMesh temp;
    temp.meshId = model.meshId;
    temp.nIndices = model.nIndices;
    temp.nVertices = model.nVertices;
    temp.indices = model.indices;
    for (int i = 0; i< model.nVertices;i++)
    {
        VertexAttribute vertex = {};
        vertex.color = model.vertices[i].color;
        vertex.nrm = model.vertices[i].nrm;
        vertex.texCoord = model.vertices[i].texCoord;
        vertex.pos = {model.vertices[i].pos.x,
                      model.vertices[i].pos.z,
                      model.vertices[i].pos.y};
        temp.vertices.emplace_back(vertex);
    }


    return temp;
}

void Scene::outputObjMesh(ObjMesh model, std::string &fileName) {
    std::ofstream outfile(fileName);

    if (outfile.is_open()){
        for (auto & vertice : model.vertices){
            outfile << "v " << vertice.pos.x << " "
            << vertice.pos.y << " "
            << vertice.pos.z << " " << std::endl;
        }
        for (int k = 0; k < model.indices.size(); k = k+3){
            outfile << "f " << model.indices[k] + 1 << " "
            << model.indices[k + 1] + 1 << " "
            << model.indices[k + 2] + 1 << " " << std::endl;
        }
        outfile.close();
    }

}


