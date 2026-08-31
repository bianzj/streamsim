//
// Created by admin on 2024/1/26.
//

#include "voxelrt.h"



bool Voxelrt::setup(AppSetting &appsetting, std::shared_ptr<VoxelrtIO> &modelio){


    modelio->m_device = appsetting.m_context.m_device;
    modelio->m_physicalDevice = appsetting.m_context.m_physicalDevice;
    modelio->m_instance = appsetting.m_context.m_instance;
    modelio->m_queues = appsetting.m_queues;
    modelio->m_queue =  modelio->m_queues[eGCT].queue;
    modelio->m_queueIndex = modelio->m_queues[eGCT].familyIndex;
    //    m_instance = appSetting.m_context.m_instance;
//    m_device = appSetting.m_context.m_device;
//    m_physicalDevice = appSetting.m_context.m_physicalDevice;
//    m_queues = appSetting.m_queues;


//    VkCommandPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
//    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
//    vkCreateCommandPool(modelio->m_device, &poolCreateInfo, nullptr, &modelio->m_cmdPool);

    modelio->m_genCmdBuf.init(modelio->m_device,modelio->m_queueIndex);


//    VkPipelineCacheCreateInfo pipelineCacheInfo{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
//    vkCreatePipelineCache(modelio->m_device, &pipelineCacheInfo, nullptr, &modelio->m_pipelineCache);

    modelio->m_pAlloc  = std::make_shared<Allocator>();
    modelio->m_pAlloc->init(modelio->m_instance, modelio->m_device, modelio->m_physicalDevice);
    modelio->m_debug.setup(modelio->m_device);

    VkPhysicalDeviceProperties2 rayTracingProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    rayTracingProperties.pNext = &(modelio->m_rtProperties);
    vkGetPhysicalDeviceProperties2(modelio->m_physicalDevice, &rayTracingProperties);

    if (modelio->useSBTWrapper)
    {
        modelio->m_sbtWrapper.setup(modelio->m_device, modelio->m_queueIndex, modelio->m_pAlloc.get(), modelio->m_rtProperties);
    }

    modelio->m_pAccelStruct->m_rtBuilder.setup(modelio->m_device, modelio->m_pAlloc.get(),modelio->m_queueIndex);



    return true;

}




bool Voxelrt::upload(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelrtIO> &modelio){

    // auto & fileio = modelio->m_fileio;
    // auto & meshio = modelio->m_meshio;

    //uploadDefined(fileio,modelio);
    m_pCompo->createCompOptical(fileio, modelio);
    m_pScene->createPrimObjScene(fileio, modelio);
    m_pGeometry->createGeometry(fileio,modelio);
//    defineOPO(modelio);
   // uploadMeteo(fileio,modelio);
    uploadSetting(fileio, modelio);
  //  uploadAero(fileio,modelio);

    return true;
}

bool Voxelrt::uploadSetting(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelrtIO> &modelio) {

    modelio->n_wave = fileio->m_pVoxelrtXml->sensorxml.waves.size();
    modelio->n_angle = fileio->m_pVoxelrtXml->sensorxml.viewAngles.size();
    modelio->isTemperature =  fileio->m_pVoxelrtXml->sensorxml.isTemperature;
    modelio->isDisplay = fileio->m_pVoxelrtXml->sensorxml.isDisplay;
    modelio->isAlbedo = fileio->m_pVoxelrtXml->sensorxml.isAlbedo;
    modelio->isImage = fileio->m_pVoxelrtXml->sensorxml.isImage;
    modelio->imageSize = fileio->m_pVoxelrtXml->sensorxml.resolution;
    modelio->maxDepth = fileio->m_pVoxelrtXml->settingxml.maxDepth;
    modelio->n_sample = fileio->m_pVoxelrtXml->settingxml.n_sample;

    return true;
}

bool Voxelrt::updateSetting(std::shared_ptr<VoxelrtIO> &modelio){

    // auto &opo = modelio->m_opo;

    // auto &sceneio = modelio->m_sceneio;

    modelio->setting.imageSize = modelio->imageSize;
    modelio->setting.n_wave = modelio->n_wave;
    modelio->setting.scale = modelio->stepsize_surface;
   // modelio->setting.isTemperature = modelio->isTemperature;
    modelio->setting.isDisplay = modelio->isDisplay;
    modelio->setting.maxDepth = modelio->maxDepth;
    modelio->setting.n_sample = modelio->n_sample;
    modelio->setting.voxelSize = modelio->voxelSize_XZY;


    return true;
    //modelio->setting.maxDepth = fileio->m_pXmlInput.
}



bool Voxelrt::uploadDefined(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelrtIO> &modelio)
{

    fileio->readDefined(modelio->m_defined);

    return false;
}

bool Voxelrt::create(std::shared_ptr<VoxelrtIO> &modelio) {


    m_pBuffer->createBuffer(modelio);
    m_pDescriptor->createDescriptor(modelio);
    if (!m_pPipeline->createPipeline(modelio)) {
        return false;
    }
    m_pCommand->create(modelio);
    updateSetting(modelio);
    return true;
}

bool Voxelrt::run(std::shared_ptr<VoxelrtIO> &modelio, std::shared_ptr<FileIO> &fileio) {



    modelio->k_node = -1;

    std::cout << "Time Info:" << "    t_"  << std::endl;


    if(modelio->isUAVTrave == true) {

        for (int kpos = 0; kpos < modelio->n_pos; kpos++) {
            modelio->k_pos = kpos;
            m_pGeometry->updateSensorPos(modelio, kpos);
//            Angle angle = modelio->angles[kangle];
//            std::cout << "Angle Info:"
//                      << "    vza_" << std::to_string(angle.vza) << "    vaa_" << std::to_string(angle.vaa)
//                      << "    sza_" << std::to_string(angle.sza) << "    saa_" << std::to_string(angle.saa)
//                      << std::endl;
            m_pCommand->runRT(modelio);
            outputPos(modelio, fileio, -1, kpos);
        }
    }else {

        for (int kangle = 0; kangle < modelio->n_angle; kangle++) {
            modelio->k_angle = kangle;
            m_pGeometry->updateAngle(modelio, kangle);
            Angle angle = modelio->angles[kangle];
            std::cout << "Angle Info:"
                      << "    vza_" << std::to_string(angle.vza) << "    vaa_" << std::to_string(angle.vaa)
                      << "    sza_" << std::to_string(angle.sza) << "    saa_" << std::to_string(angle.saa)
                      << std::endl;
            m_pCommand->runRT(modelio);
            output(modelio, fileio, -1, kangle);
        }
    }


    return true;
}



bool Voxelrt::destroy(std::shared_ptr<VoxelrtIO> & modelio){

    m_pBuffer->destroy(modelio);
    modelio->m_sbtWrapper.destroy();
    modelio->m_pAccelStruct->m_rtBuilder.destroy();

    m_pDescriptor->destroy(modelio);
    m_pPipeline->destroy(modelio);
    m_pCommand->destroy(modelio);



    vkDeviceWaitIdle(modelio->m_device);
    modelio->m_genCmdBuf.deinit();
    modelio->m_pAlloc->deinit();
    
    return false;

}



void Voxelrt::output(std::shared_ptr<VoxelrtIO> &modelio, std::shared_ptr<FileIO> &fileio, int knode, int kangle) {
    VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const int width = modelio->imageSize.x;
    const int height = modelio->imageSize.y;
    const int nWave = modelio->n_wave;
    if (width <= 0 || height <= 0 || nWave <= 0
        || kangle < 0 || static_cast<size_t>(kangle) >= modelio->angles.size()) {
        return;
    }

    const size_t imageElements = static_cast<size_t>(width) * height;
    const size_t totalElements = imageElements * nWave;
    const VkDeviceSize bufferSize = totalElements * sizeof(float);
    nvvk::Buffer pixelBuffer = modelio->m_pAlloc->createBuffer(
        bufferSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_pVirtual->bufferToBuffer(
        modelio, *(modelio->m_virtualio->m_pBufferStorage), bufferSize, pixelBuffer);

    void* mappedData = modelio->m_pAlloc->map(pixelBuffer);
    std::vector<float> sourceData(totalElements);
    std::memcpy(sourceData.data(), mappedData, static_cast<size_t>(bufferSize));
    modelio->m_pAlloc->unmap(pixelBuffer);
    modelio->m_pAlloc->destroy(pixelBuffer);

    Angle angle = modelio->angles[kangle];
    Eigen::VectorXd cx;
    Eigen::VectorXd cy;
    m_pGeometry->orthcorrect(modelio, angle.vza, angle.vaa, cx, cy);

    std::vector<float> orthData(totalElements, 0.0f);
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            const int ii = static_cast<int>(i * cx[0] + j * cx[1] + i * j * cx[2] + cx[3]);
            const int jj = static_cast<int>(i * cy[0] + j * cy[1] + i * j * cy[2] + cy[3]);
            if (ii < 0 || ii >= width || jj < 0 || jj >= height) {
                continue;
            }

            const size_t destination = static_cast<size_t>(j) * width + i;
            const size_t source = static_cast<size_t>(jj) * width + ii;
            for (int band = 0; band < nWave; ++band) {
                const size_t bandOffset = static_cast<size_t>(band) * imageElements;
                const float value = sourceData[bandOffset + source];
                if (value != 0.0f) {
                    orthData[bandOffset + destination] = value;
                }
            }
        }
    }

    const float time = knode == -1 ? -1.0f : modelio->meteo.t;
    fileio->writeENVIdata(
        modelio->projectDir, orthData.data(), width, height, nWave, angle, time, -1);
}

void Voxelrt::outputPos(std::shared_ptr<VoxelrtIO> &modelio, std::shared_ptr<FileIO> &fileio, int knode, int kpos) {
    VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const int width = modelio->imageSize.x;
    const int height = modelio->imageSize.y;
    const int nWave = modelio->n_wave;
    if (width <= 0 || height <= 0 || nWave <= 0 || modelio->angles.empty()) {
        return;
    }

    const size_t totalElements = static_cast<size_t>(width) * height * nWave;
    const VkDeviceSize bufferSize = totalElements * sizeof(float);
    nvvk::Buffer pixelBuffer = modelio->m_pAlloc->createBuffer(
        bufferSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_pVirtual->bufferToBuffer(
        modelio, *(modelio->m_virtualio->m_pBufferStorage), bufferSize, pixelBuffer);

    void* mappedData = modelio->m_pAlloc->map(pixelBuffer);
    std::vector<float> outputData(totalElements);
    std::memcpy(outputData.data(), mappedData, static_cast<size_t>(bufferSize));
    modelio->m_pAlloc->unmap(pixelBuffer);
    modelio->m_pAlloc->destroy(pixelBuffer);

    Angle angle = modelio->angles[0];
    const float time = knode == -1 ? -1.0f : modelio->meteo.t;
    fileio->writeENVIdata(
        modelio->projectDir, outputData.data(), width, height, nWave, angle, time, kpos);
}


void Voxelrt::outputVoxel(std::shared_ptr<VoxelrtIO> &modelio, std::shared_ptr<FileIO> &fileio) {


//    VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
//    int num = modelio->n_voxel;
//    VkDeviceSize bufferSize = num * sizeof(VoxelHeatflux);
//    nvvk::Buffer pixelBuffer = modelio->m_pAlloc->createBuffer(bufferSize, usage,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
//    m_pVirtual->bufferToBuffer(modelio, *(modelio->m_voxelio->m_pFluxBuffer), bufferSize, pixelBuffer);
//    // write the buffer to disk
//    void *data = modelio->m_pAlloc->map(pixelBuffer);
//    VoxelHeatflux *pData = reinterpret_cast<VoxelHeatflux *>(data);
//    VoxelHeatflux test0 = pData[0];
//    modelio->m_pAlloc->unmap(pixelBuffer);
//    modelio->m_pAlloc->destroy(pixelBuffer);


//    VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
//    int num = modelio->n_voxel;
//    VkDeviceSize bufferSize = num * sizeof(VoxelRss);
//    nvvk::Buffer pixelBuffer = modelio->m_pAlloc->createBuffer(bufferSize, usage,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
//    m_pVirtual->bufferToBuffer(modelio, *(modelio->m_voxelio->m_pRssBuffer), bufferSize, pixelBuffer);
//    // write the buffer to disk
//    void *data = modelio->m_pAlloc->map(pixelBuffer);
//    VoxelRss *pData = reinterpret_cast<VoxelRss *>(data);
//    VoxelRss test0 = pData[0];
//    modelio->m_pAlloc->unmap(pixelBuffer);
//    modelio->m_pAlloc->destroy(pixelBuffer);


//    VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
//    int num = modelio->n_voxel;
//    VkDeviceSize bufferSize = num * sizeof(VoxelTempe);
//    nvvk::Buffer pixelBuffer = modelio->m_pAlloc->createBuffer(bufferSize, usage,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
//    m_pVirtual->bufferToBuffer(modelio, *(modelio->m_voxelio->m_pTempeBuffer), bufferSize, pixelBuffer);
//    // write the buffer to disk
//    void *data = modelio->m_pAlloc->map(pixelBuffer);
//    VoxelTempe *pData = reinterpret_cast<VoxelTempe *>(data);
//    VoxelTempe test0 = pData[0];
//    modelio->m_pAlloc->unmap(pixelBuffer);
//    modelio->m_pAlloc->destroy(pixelBuffer);

}


//bool Voxelrt::uploadAero(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelrtIO> &modelio) {
//
//
//    if(fileio->m_pVoxelrtXml->aerocondxml.aerotype == AeroType::ONE) {
//        modelio->aeroconds.emplace_back(fileio->m_pVoxelrtXml->aerocondxml.aerocond);
//    }else if (fileio->m_pVoxelrtXml->aerocondxml.aerotype == AeroType::image)
//    {
//        int a = 10;
//    }else if(fileio->m_pVoxelrtXml->aerocondxml.aerotype == AeroType::gridCal){
//        int b = 10;
//    }
//
//    return false;
//
//
//}
