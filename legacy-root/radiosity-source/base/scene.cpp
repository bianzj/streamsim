//
// Created by bianzunjian on 2024/3/24.
//

#include "scene.h"

bool Scene::createObjScene(std::shared_ptr<FileIO> & fileio, std::shared_ptr<RadiosityIO> & modelio)
{
    bool isInterp = false;
    auto & scenexml = fileio->m_pRadiosityXml->scenexml;
    auto & meshio = modelio->m_meshio;
    auto & instanceio = modelio->m_instanceio;

    ObjLoader loader;
    //-------------------------
    //-- Background Mesh
    //-------------------------
    std::cout << "create background" << std::endl;
    if(scenexml.background.isDEM) {
        return false;
    }else{
        // loader.createBackground(scenexml.sceneSize,0.1);

        loader.createBackground(scenexml.sceneSize,50000000000);
    }


    int & n_modelmesh = modelio->n_modelmesh;
    loader.m_objmesh.meshId = n_modelmesh;
    meshio->objMeshes.emplace_back(loader.m_objmesh);
    MeshLink bgMeshLink{};
    bgMeshLink.type = int(Type::SOIL);
    std::string bgSpectralName = scenexml.background.spectralName;
    bgMeshLink.spectralId =  meshio->spectralNames.find(bgSpectralName)->second;
    std::string bgThermalName;
    int bgThermalIndex = 0;
    if (fileio->m_pRadiosityXml->sensorxml.isTemperature)
    {
        bgThermalName =  fileio->m_pRadiosityXml->scenexml.background.thermalName;
        bgThermalIndex = meshio->thermalNames.find(bgThermalName)->second;
    }
    bgMeshLink.thermalId = bgThermalIndex;
    meshio->meshLinks.emplace_back(bgMeshLink);


    Instance bgInstance{};
    bgInstance.meshId = static_cast<uint32_t>(n_modelmesh);
    n_modelmesh++;
    glm::mat4 bgunit = glm::mat4(1.0f);
    glm::vec3 bgShift = glm::vec3{0, 0 - loader.minElevation, 0};
    glm::vec3 bgScale = glm::vec3{1.0, 1.0, 1.0};
    glm::mat4 bgMat = glm::scale(bgunit, bgScale) * glm::translate(bgunit,bgShift);
    bgInstance.object2worldMatrix = bgMat;
    bgInstance.world2objectMatrix = glm::transpose(glm::inverse(bgMat));
    instanceio->instances.emplace_back(bgInstance);


    InstanceLink bgInstanceLink{};
    bgInstanceLink.meshId = bgInstance.meshId;
    instanceio->instanceLinks.emplace_back(bgInstanceLink);


    float x = fileio->m_pRadiosityXml->scenexml.sceneSize.x; // lenght
    float y = fileio->m_pRadiosityXml->scenexml.sceneSize.y; // width
    float z = fileio->m_pRadiosityXml->scenexml.sceneSize.z; // height
    modelio->sMin = glm::vec3(-x / 2.0, 0, -y / 2.0);
    modelio->sMax = glm::vec3(x / 2.0, z, y / 2.0);
    modelio->sceneSize = glm::vec3(x,y,z);
    modelio->sceneOrigin = scenexml.sceneOrigin;

    //-------------------------
    //-- Obj Models
    //-------------------------
    int n_obj = fileio->m_pRadiosityXml->scenexml.objEntities.size();
    for (int kobj = 0; kobj < n_obj; kobj++)
    {
        auto &objEntity = fileio->m_pRadiosityXml->scenexml.objEntities[kobj];
        std::string fileName = objEntity.filePath;
        std::string objName = objEntity.objName;



        ///////////
        /// import huge obj
        loader.loadModel(fileName);
        for (int kmesh = 0; kmesh < loader.m_objMeshs.size(); kmesh++)
        {
            /// ------------------------------------
            /// model/mesh
            ///-------------------------------------
            std::string meshName = loader.m_meshNames[kmesh];
            std::string spectralName = "leaf";
            std::string thermalName;

            meshio->objMeshes.emplace_back(loader.m_objMeshs[kmesh]);

            /// ------------------------------------
            /// Mesh Link
            ///-------------------------------------

            int spectralIndex = meshio->spectralNames.find(spectralName)->second;
            int thermalIndex = 0;
            if (fileio->m_pRadiosityXml->sensorxml.isTemperature == true)
            {
                thermalName = "K300";
                thermalIndex = meshio->thermalNames.find(thermalName)->second;
            }
            MeshLink meshLink{};
            meshLink.type = int(Type::VEGETATION);
            meshLink.spectralId = spectralIndex;
            meshLink.thermalId = thermalIndex;
            if(thermalIndex>1){
                int a = 10;}
            meshio->meshLinks.emplace_back(meshLink);

            int n_instancet = objEntity.objDistributions.size();
            for (int kinstance = 0; kinstance < n_instancet; kinstance++)
            {
                /// ------------------------------------
                /// Instance
                ///-------------------------------------
                Instance instance{};
                instance.meshId = static_cast<uint32_t>(n_modelmesh);
                glm::vec3 shift0 = {0,0,0};
                float scale0 = 1;
                float angle0 = 0;

                //nvmath::vec3f shift = nvmath::vec3f{ shift0.x - x/2.0,shift0.z,shift0.y - z/2.0 };
                glm::vec3 shift;
                if (isInterp == true)
                {
                    // double shiftInterp = loader.getShiftInterp(interp, shift0);
                    // shift = nvmath::vec3f{shift0.x - x / 2.0, shift0.z + shiftInterp - loader.minElevation, shift0.y - y / 2.0};
                }
                else
                {
                    shift = glm::vec3{shift0.x - x / 2.0, shift0.z, shift0.y - y / 2.0};
                }


                glm::mat4 unit = glm::mat4(1.0f);
                glm::vec3 scale = glm::vec3(scale0);
                glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0,1.0,0.0));
                glm::mat4 mat = glm::scale(unit,scale) * glm::translate(unit, shift) * angle;
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





        ////////////
        /// import obj through meshName
        // int n_mesh = objEntity.meshNames.size();
        // std::cout << "Process obj: " << objName << std::endl;
        // for (int kmesh = 0; kmesh < n_mesh; kmesh++)
        // {
        //     /// ------------------------------------
        //     /// model/mesh
        //     ///-------------------------------------
        //     std::string meshName = objEntity.meshNames[kmesh];
        //     std::string spectralName = objEntity.spectralNames[kmesh];
        //     std::string thermalName;
        //
        //     std::cout << "Loading obj: " << objName << "." << meshName << std::endl;
        //     loader.loadMesh(fileName, meshName);
        //     loader.m_objmesh.meshId = n_modelmesh;
        //     meshio->objMeshes.emplace_back(loader.m_objmesh);
        //
        //     /// ------------------------------------
        //     /// Mesh Link
        //     ///-------------------------------------
        //
        //     int spectralIndex = meshio->spectralNames.find(spectralName)->second;
        //     int thermalIndex = 0;
        //     if (fileio->m_pRadiosityXml->sensorxml.isTemperature == true)
        //     {
        //         thermalName = objEntity.thermalNames[kmesh];
        //         thermalIndex = meshio->thermalNames.find(thermalName)->second;
        //     }
        //     MeshLink meshLink{};
        //     meshLink.type = int(Type::VEGETATION);
        //     meshLink.spectralId = spectralIndex;
        //     meshLink.thermalId = thermalIndex;
        //     if(thermalIndex>1){
        //         int a = 10;}
        //     meshio->meshLinks.emplace_back(meshLink);
        //
        //     int n_instancet = objEntity.objDistributions.size();
        //     for (int kinstance = 0; kinstance < n_instancet; kinstance++)
        //     {
        //         /// ------------------------------------
        //         /// Instance
        //         ///-------------------------------------
        //         Instance instance{};
        //         instance.meshId = static_cast<uint32_t>(n_modelmesh);
        //         glm::vec3 shift0 = objEntity.objDistributions[kinstance];
        //         float scale0 = objEntity.scales[kinstance];
        //         float angle0 = objEntity.rotations[kinstance];
        //
        //         //nvmath::vec3f shift = nvmath::vec3f{ shift0.x - x/2.0,shift0.z,shift0.y - z/2.0 };
        //         glm::vec3 shift;
        //         if (isInterp == true)
        //         {
        //             // double shiftInterp = loader.getShiftInterp(interp, shift0);
        //             // shift = nvmath::vec3f{shift0.x - x / 2.0, shift0.z + shiftInterp - loader.minElevation, shift0.y - y / 2.0};
        //         }
        //         else
        //         {
        //             shift = glm::vec3{shift0.x - x / 2.0, shift0.z, shift0.y - y / 2.0};
        //         }
        //
        //
        //         glm::mat4 unit = glm::mat4(1.0f);
        //         glm::vec3 scale = glm::vec3(scale0);
        //         glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0,1.0,0.0));
        //         glm::mat4 mat = glm::scale(unit,scale) * glm::translate(unit, shift) * angle;
        //         instance.object2worldMatrix = mat;
        //         instance.world2objectMatrix = glm::transpose(glm::inverse(mat));
        //         instanceio->instances.emplace_back(instance);
        //
        //         /// ------------------------------------
        //         /// InstanceLink
        //         ///-------------------------------------
        //         InstanceLink instanceLink{};
        //         instanceLink.meshId = instance.meshId;
        //         instanceio->instanceLinks.emplace_back(instanceLink);
        //     }
        //     n_modelmesh++;
        // }
    }

    return false;
}

bool Scene::createObjScene(std::shared_ptr<FileIO> & fileio, std::shared_ptr<RadiosityEBIO> & modelio)
{
    bool isInterp = false;
    auto & scenexml = fileio->m_pRadiosityebXml->scenexml;
    auto & meshio = modelio->m_meshio;
    auto & instanceio = modelio->m_instanceio;

    ObjLoader loader;
    //-------------------------
    //-- Background Mesh
    //-------------------------
    // _2D::BilinearInterpolator<double> interp;
    if(scenexml.background.isDEM) {
        loader.createBackgroundFromDEM(scenexml.background.DEMPath, scenexml.sceneSize, scenexml.background.demResolution);
        isInterp = true;
    }else{
        // loader.createBackground(scenexml.sceneSize);
        loader.createBackground(scenexml.sceneSize, 0.2); /// 0.5, 2, 4 的结果保持的一致，因此采用2的分辨率进行调试
    }

    float x = fileio->m_pRadiosityebXml->scenexml.sceneSize.x; // lenght
    float y = fileio->m_pRadiosityebXml->scenexml.sceneSize.y; // width
    float z = fileio->m_pRadiosityebXml->scenexml.sceneSize.z; // height
    modelio->sMin = glm::vec3(-x / 2.0, 0, -y / 2.0);
    modelio->sMax = glm::vec3(x / 2.0, z, y / 2.0);
    modelio->sceneSize = glm::vec3(x,y,z);
    modelio->sceneOrigin = scenexml.sceneOrigin;

    int & n_modelmesh = modelio->n_modelmesh;
    loader.m_objmesh.meshId = n_modelmesh;
    meshio->objMeshes.emplace_back(loader.m_objmesh);
    MeshLink bgMeshLink{};
    bgMeshLink.type = int(Type::SOIL);
    std::string bgSpectralName = scenexml.background.spectralName;
    bgMeshLink.spectralId =  meshio->spectralNames.find(bgSpectralName)->second;
    std::string bgThermalName;
    int bgThermalIndex = 0;
//    if (fileio->m_pRadiosityebXml->sensorxml.isTemperature)
//    {
//        bgThermalName =  fileio->m_pRadiosityebXml->scenexml.background.thermalName;
//        bgThermalIndex = meshio->thermalNames.find(bgThermalName)->second;
//    }
    bgMeshLink.thermalId = bgThermalIndex;
    int bgPropIndex = 0;
    std::string bgPropName = scenexml.background.bgPropName;
    bgPropIndex = meshio->soilsetNames.find(bgPropName)->second;
    bgMeshLink.bioId = bgPropIndex;
    meshio->meshLinks.emplace_back(bgMeshLink);


    Instance bgInstance{};
    bgInstance.meshId = static_cast<uint32_t>(n_modelmesh);
    n_modelmesh++;
    glm::mat4 bgunit = glm::mat4(1.0f);
    glm::vec3 bgShift = glm::vec3{- x / 2.0, - loader.minElevation, - y / 2.0};
    glm::vec3 bgScale = glm::vec3{1.0, 1.0, 1.0};
    glm::mat4 bgMat = glm::scale(bgunit, bgScale) * glm::translate(bgunit,bgShift);
    bgInstance.object2worldMatrix = bgMat;
    bgInstance.world2objectMatrix = glm::transpose(glm::inverse(bgMat));
    instanceio->instances.emplace_back(bgInstance);


    InstanceLink bgInstanceLink{};
    bgInstanceLink.meshId = bgInstance.meshId;
    instanceio->instanceLinks.emplace_back(bgInstanceLink);




    //-------------------------
    //-- Obj Models
    //-------------------------
    int n_obj = fileio->m_pRadiosityebXml->scenexml.objEntityplus.size();
    for (int kobj = 0; kobj < n_obj; kobj++)
    {
        auto &objEntity = fileio->m_pRadiosityebXml->scenexml.objEntityplus[kobj];
        std::string fileName = objEntity.filePath;
        std::string objName = objEntity.objName;
        int n_mesh = objEntity.meshNames.size();


        if(objEntity.isdisfromFile == true){
            int n_dis = 0;
            float *tempx, *tempy,*tempz;
            tempx = Utils::readascfile(objEntity.distributefile,0,0,n_dis);
            tempy = Utils::readascfile(objEntity.distributefile,0,1,n_dis);
            tempz = Utils::readascfile(objEntity.distributefile,0,2,n_dis);
            objEntity.objDistributions.resize(n_dis);
            objEntity.scales.resize(n_dis);
            objEntity.rotations.resize(n_dis);
            for(int kin = 0;kin<n_dis;kin++)
            {
                objEntity.objDistributions[kin]=(glm::vec3(tempx[kin],tempy[kin],tempz[kin]));
                objEntity.scales[kin] = 1.0;
                objEntity.rotations[kin] = 0.0;
            }
        }

        for (int kmesh = 0; kmesh < n_mesh; kmesh++)
        {
            /// ------------------------------------
            /// model/mesh
            ///-------------------------------------
            std::string meshName = objEntity.meshNames[kmesh];
            std::string spectralName = objEntity.spectralNames[kmesh];
            std::string propName = objEntity.propNames[kmesh];
            std::string thermalName;
            Type type = objEntity.types[kmesh];

            loader.loadMesh(fileName, meshName);
            loader.m_objmesh.meshId = n_modelmesh;
            meshio->objMeshes.emplace_back(loader.m_objmesh);

            /// ------------------------------------
            /// Mesh Link
            ///-------------------------------------

            int spectralIndex = meshio->spectralNames.find(spectralName)->second;
            int thermalIndex = 0;
//            if (fileio->m_pRadiosityebXml->sensorxml.isTemperature == true)
//            {
//                thermalName = objEntity.thermalNames[kmesh];
//                thermalIndex = meshio->thermalNames.find(thermalName)->second;
//            }

            int canopyId = 0;
            if (type == Type::VEGETATION) {
                const std::string &canopyName = objEntity.canopyNames.at(kmesh);
                canopyId = meshio->canopyNames.at(canopyName);
            }

            int bioId;
            if(type == Type::VEGETATION) {
                // meshlink.leafbioId = meshio->leafbioNames.find(propName)->second;
                bioId = meshio->leafbioNames.find(propName)->second;
            }else if(type ==Type::SOIL){
                //  meshlink.soilsetId = meshio->soilsetNames.find(propName)->second;
                bioId = meshio->soilsetNames.find(propName)->second;
            }

            MeshLink meshLink{};
            meshLink.type = int(type);
            meshLink.spectralId = spectralIndex;
            meshLink.thermalId = thermalIndex;
            meshLink.bioId = bioId;
            meshLink.canopyId = canopyId;
            meshio->meshLinks.emplace_back(meshLink);





            int n_instancet = objEntity.objDistributions.size();
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
                    double shiftInterp = loader.getShiftInterp(shift0, scenexml.background.demResolution);

                    shift = glm::vec3{shift0.x - x / 2.0, shift0.z + shiftInterp - loader.minElevation, shift0.y - y / 2.0};
                }
                else
                {
                    shift = glm::vec3{shift0.x - x / 2.0, shift0.z, shift0.y - y / 2.0};
                }


                glm::mat4 unit = glm::mat4(1.0f);
                glm::vec3 scale = glm::vec3(scale0);
                glm::mat4 angle = glm::rotate(unit, glm::radians(angle0), glm::vec3(0.0,1.0,0.0));
                glm::mat4 mat = glm::scale(unit,scale) * glm::translate(unit, shift) * angle;
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
    }

    return false;
}


bool Scene::fromobj2facet(std::shared_ptr<RadiosityIO> &modelio) {
    auto & meshio = modelio->m_meshio;
    auto & instanceio = modelio->m_instanceio;
    auto &facetio = modelio->m_facetio;

    auto & npoly= modelio->m_npoly;
    npoly = 0;
    
    auto & sMin = modelio->sMin;
    auto & sMax = modelio->sMax;
    sMin = glm::vec3(1000,1000,1000);
    sMax = glm::vec3(0,0,0);
    for(int kinstance = 0; kinstance < instanceio->instances.size();kinstance++)
    {
        auto &instance = instanceio->instances[kinstance];

        int meshId = instance.meshId;
        auto &objmesh = meshio->objMeshes[meshId];

        uint32_t kpos=0;
        

        
        for(int k=0; k< objmesh.indices.size();k=k+3)
        {
            Facet facet{};
            for(int kk=0;kk<3;kk++) {
                kpos = objmesh.indices[k+kk];
                glm::vec3 point = instance.object2worldMatrix * glm::vec4(objmesh.vertices[kpos].pos, 1.0);
//                glm::vec3 point = objmesh.vertices[kpos].pos;
                facet.points[kk] = glm::vec3{point.x,point.z,point.y};
                if (facet.points[kk].x < sMin[0]) sMin[0] = facet.points[kk].x;
                if (facet.points[kk].y < sMin[1]) sMin[1] = facet.points[kk].y;
                if (facet.points[kk].z < sMin[2]) sMin[2] = facet.points[kk].z;
                if (facet.points[kk].x > sMax[0]) sMax[0] = facet.points[kk].x;
                if (facet.points[kk].y > sMax[1]) sMax[1] = facet.points[kk].y;
                if (facet.points[kk].z > sMax[2]) sMax[2] = facet.points[kk].z;
            }

            facet.fsign = instance.meshId;


            
            facet.pcenter = glm::vec3((facet.points[0].x+facet.points[1].x+facet.points[2].x)*0.333,
                                      (facet.points[0].y+facet.points[1].y+facet.points[2].y)*0.333,
                                      (facet.points[0].z+facet.points[1].z+facet.points[2].z)*0.333);

            glm::vec3 vec1,vec2;
            vec1.x = facet.points[1].x - facet.points[0].x;
            vec1.y = facet.points[1].y - facet.points[0].y;
            vec1.z = facet.points[1].z - facet.points[0].z;
            vec2.x = facet.points[2].x - facet.points[0].x;
            vec2.y = facet.points[2].y - facet.points[0].y;
            vec2.z = facet.points[2].z - facet.points[0].z;

            float norm[3];
            SCI::cross(vec1,vec2,norm);
            facet.pnorm.x = norm[0];
            facet.pnorm.y = norm[1];
            facet.pnorm.z = norm[2];


            facet.psize = SCI::trianglearea(facet.points[0],facet.points[1],facet.points[2]);

            // std::cout << facet.points[0].x << " " << facet.points[0].y << " " << facet.points[0].z << std::endl;

            if(facet.psize ==0)
            {
                int a = 10;
            }

            facetio->facets.emplace_back(facet);
            
            npoly = npoly + 1;

        }
    }

    for(int j=0;j<3;j++) modelio->m_xvw[j] = 0.5*(sMin[j]+sMax[j]);

    modelio->m_facetio->facetVFs.resize(npoly);
    modelio->m_facetio->facetRTs.resize(npoly);
    scale2(modelio);
    return false;
}

bool Scene::fromobj2facet(std::shared_ptr<RadiosityEBIO> &modelio) {
    auto & meshio = modelio->m_meshio;
    auto & instanceio = modelio->m_instanceio;
    auto &facetio = modelio->m_facetio;

    auto & npoly= modelio->m_npoly;
    npoly = 0;

    auto & sMin = modelio->sMin;
    auto & sMax = modelio->sMax;
    sMin = glm::vec3(1000,1000,1000);
    sMax = glm::vec3(0,0,0);
    for(int kinstance = 0; kinstance < instanceio->instances.size();kinstance++)
    {
        auto &instance = instanceio->instances[kinstance];

        int meshId = instance.meshId;
        auto &objmesh = meshio->objMeshes[meshId];

        uint32_t kpos=0;



        for(int k=0; k< objmesh.indices.size();k=k+3)
        {
            Facet facet{};
            for(int kk=0;kk<3;kk++) {
                kpos = objmesh.indices[k+kk];
                glm::vec3 point = instance.object2worldMatrix * glm::vec4(objmesh.vertices[kpos].pos, 1.0);
//                glm::vec3 point = objmesh.vertices[kpos].pos;
                facet.points[kk] = glm::vec3{point.x,point.z,point.y};
                if (facet.points[kk].x < sMin[0]) sMin[0] = facet.points[kk].x;
                if (facet.points[kk].y < sMin[1]) sMin[1] = facet.points[kk].y;
                if (facet.points[kk].z < sMin[2]) sMin[2] = facet.points[kk].z;
                if (facet.points[kk].x > sMax[0]) sMax[0] = facet.points[kk].x;
                if (facet.points[kk].y > sMax[1]) sMax[1] = facet.points[kk].y;
                if (facet.points[kk].z > sMax[2]) sMax[2] = facet.points[kk].z;
            }

            facet.fsign = instance.meshId;



            facet.pcenter = glm::vec3((facet.points[0].x+facet.points[1].x+facet.points[2].x)*0.333,
                                      (facet.points[0].y+facet.points[1].y+facet.points[2].y)*0.333,
                                      (facet.points[0].z+facet.points[1].z+facet.points[2].z)*0.333);

//            facet.pnorm = glm::normalize(glm::cross(facet.points[1],facet.points[0]));

            glm::vec3 vec1,vec2;
            vec1.x = facet.points[1].x - facet.points[0].x;
            vec1.y = facet.points[1].y - facet.points[0].y;
            vec1.z = facet.points[1].z - facet.points[0].z;
            vec2.x = facet.points[2].x - facet.points[0].x;
            vec2.y = facet.points[2].y - facet.points[0].y;
            vec2.z = facet.points[2].z - facet.points[0].z;

            //SCI::cross(vec1,vec2,facet.pnorm);
            float norm[3];
            SCI::cross(vec1,vec2,norm);
            facet.pnorm.x = norm[0];
            facet.pnorm.y = norm[1];
            facet.pnorm.z = norm[2];

            facet.psize = SCI::trianglearea(facet.points[0],facet.points[1],facet.points[2]);

            if(facet.psize ==0)
            {
                int a = 10;
            }

            facetio->facets.emplace_back(facet);

            npoly = npoly + 1;

        }
    }

    for(int j=0;j<3;j++) modelio->m_xvw[j] = 0.5*(sMin[j]+sMax[j]);

    modelio->m_facetio->facetVFs.resize(npoly);
    modelio->m_facetio->facetRTs.resize(npoly);
    modelio->m_facetio->facetEBs.resize(npoly);
    scale2(modelio);
    return false;
}


void Scene::scale2(std::shared_ptr<RadiosityIO> modelio) {
    float rr = 0;
    float radi = 0;
    auto &xvw = modelio->m_xvw;
    auto &vfacetio = modelio->m_facetio->facets;
    auto &transfer = modelio->m_scenescale;
    for(int i=0;i<modelio->m_npoly;i++)
    {
        for(int ii =0;ii<3;ii++) {
            rr = sqrt((xvw[0] - vfacetio[i].points[ii].x) * (xvw[0] - vfacetio[i].points[ii].x) +
                      (xvw[1] - vfacetio[i].points[ii].y) * (xvw[1] - vfacetio[i].points[ii].y) +
                      (xvw[2] - vfacetio[i].points[ii].z) * (xvw[2] - vfacetio[i].points[ii].z));
            if (rr > radi) radi = rr;
        }
    }
    float delx = 2* radi;
    float dely = 2* radi;
    int mxpic = MXPIC;
    int mypic = MYPIC;
    float a = std::min(0.5*(mxpic - 2) / radi, 0.5*(mypic - 2) / radi);
    float c=a;
    float b = mxpic / 2.0;
    float d = mypic / 2.0;
    transfer = {a,b,c,d,delx,dely};
    int test = 10;
}


void Scene::scale2(std::shared_ptr<RadiosityEBIO> modelio) {
    float rr = 0;
    float radi = 0;
    auto &xvw = modelio->m_xvw;
    auto &vfacetio = modelio->m_facetio->facets;
    auto &transfer = modelio->m_scenescale;
    for(int i=0;i<modelio->m_npoly;i++)
    {
        for(int ii =0;ii<3;ii++) {
            rr = sqrt((xvw[0] - vfacetio[i].points[ii].x) * (xvw[0] - vfacetio[i].points[ii].x) +
                      (xvw[1] - vfacetio[i].points[ii].y) * (xvw[1] - vfacetio[i].points[ii].y) +
                      (xvw[2] - vfacetio[i].points[ii].z) * (xvw[2] - vfacetio[i].points[ii].z));
            if (rr > radi) radi = rr;
        }
    }
    float delx = 2* radi;
    float dely = 2* radi;
    int mxpic = MXPIC;
    int mypic = MYPIC;
    float a = std::min(0.5*(mxpic - 2) / radi, 0.5*(mypic - 2) / radi);
    float c=a;
    float b = mxpic / 2.0;
    float d = mypic / 2.0;
    transfer = {a,b,c,d,delx,dely};
    int test = 10;
}



