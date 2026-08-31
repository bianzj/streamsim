//
// Created by admin on 2024/1/24.
//

#include <iomanip>
#include "raytracing.h"


bool Raytracing::setup( AppSetting &appsetting, std::shared_ptr<RaytracingIO> &raytracingio){


    raytracingio->m_device = appsetting.m_context.m_device;
    raytracingio->m_physicalDevice = appsetting.m_context.m_physicalDevice;
    raytracingio->m_instance = appsetting.m_context.m_instance;
    raytracingio->m_queues = appsetting.m_queues;
    raytracingio->m_queue =  raytracingio->m_queues[eGCT].queue;
    raytracingio->m_queueIndex = raytracingio->m_queues[eGCT].familyIndex;
    //    m_instance = appSetting.m_context.m_instance;
//    m_device = appSetting.m_context.m_device;
//    m_physicalDevice = appSetting.m_context.m_physicalDevice;
//    m_queues = appSetting.m_queues;


//    VkCommandPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
//    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
//    vkCreateCommandPool(raytracingio->m_device, &poolCreateInfo, nullptr, &raytracingio->m_cmdPool);

    raytracingio->m_genCmdBuf.init(raytracingio->m_device,raytracingio->m_queueIndex);


//    VkPipelineCacheCreateInfo pipelineCacheInfo{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
//    vkCreatePipelineCache(raytracingio->m_device, &pipelineCacheInfo, nullptr, &raytracingio->m_pipelineCache);

    raytracingio->m_pAlloc  = std::make_shared<Allocator>();
    raytracingio->m_pAlloc->init(raytracingio->m_instance, raytracingio->m_device, raytracingio->m_physicalDevice);
    raytracingio->m_debug.setup(raytracingio->m_device);

    /// requesting ray tracing properties. from physical device.
    VkPhysicalDeviceProperties2 rayTracingProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    rayTracingProperties.pNext = &(raytracingio->m_rtProperties);
    vkGetPhysicalDeviceProperties2(raytracingio->m_physicalDevice, &rayTracingProperties);

    if (raytracingio->useSBTWrapper)
    {
        raytracingio->m_sbtWrapper.setup(raytracingio->m_device, raytracingio->m_queueIndex, raytracingio->m_pAlloc.get(), raytracingio->m_rtProperties);
    }

    raytracingio->m_pAccelStruct->m_rtBuilder.setup(raytracingio->m_device, raytracingio->m_pAlloc.get(),raytracingio->m_queueIndex);

    return true;

}




bool Raytracing::upload(std::shared_ptr<FileIO> &fileio,std::shared_ptr<RaytracingIO> &raytracingio){

   // auto & fileio = raytracingio->m_fileio;
   // auto & meshio = raytracingio->m_meshio;


    m_pCompo->createCompOptical(fileio, raytracingio);
    m_pScene->createObjScene(fileio,raytracingio);
    m_pGeometry->createGeometry(fileio,raytracingio);
//    defineOPO(raytracingio);
    uploadSetting(fileio, raytracingio);

    return true;
}

bool Raytracing::create(std::shared_ptr<RaytracingIO> &raytracingio) {


    m_pBuffer->createBuffer(raytracingio);
    m_pDescriptor->createDescriptor(raytracingio);
    m_pPipeline->createPipeline(raytracingio);
    m_pCommand->create(raytracingio);
    updateSetting(raytracingio);
    return true;
}

//bool Raytracing::defineOPO(std::shared_ptr<RaytracingIO> &raytracingio){
//
//    //--------------------------------------------------------
//    //-- BRDF/Temperature
//    //-------------------------------------------------------
////    auto &opo = raytracingio->m_opo;
////    auto &fileio = raytracingio->m_fileio;
//    opo->isTemperature =  fileio->m_pXmlInput->sensor.isTemperature;
//
//    opo->isImage = fileio->m_pXmlInput->sensor.isImageSave;
//    opo->n_sample = fileio->m_pXmlInput->setting.n_sample;
//    opo->maxDepth = fileio->m_pXmlInput->setting.maxDepth;
//    return true;
//
//}

bool Raytracing::uploadSetting(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RaytracingIO> &raytracingio) {

    raytracingio->n_wave = fileio->m_pRaytracingXml->sensorxml.waves.size();
    raytracingio->n_angle = fileio->m_pRaytracingXml->sensorxml.viewAngles.size();
    raytracingio->isTemperature =  fileio->m_pRaytracingXml->sensorxml.isTemperature;
    raytracingio->isDisplay = fileio->m_pRaytracingXml->sensorxml.isDisplay;
    raytracingio->isAlbedo = fileio->m_pRaytracingXml->sensorxml.isAlbedo;
    raytracingio->isImage = fileio->m_pRaytracingXml->sensorxml.isImage;
    raytracingio->isOrth = fileio->m_pRaytracingXml->sensorxml.isOrth;
    raytracingio->imageSize = fileio->m_pRaytracingXml->sensorxml.resolution;
    raytracingio->maxDepth = fileio->m_pRaytracingXml->settingxml.maxDepth;
    raytracingio->n_sample = fileio->m_pRaytracingXml->settingxml.n_sample;

    return true;
}

bool Raytracing::updateSetting(std::shared_ptr<RaytracingIO> &raytracingio){

   // auto &opo = raytracingio->m_opo;

   // auto &sceneio = raytracingio->m_sceneio;
    raytracingio->setting.imageSize = raytracingio->imageSize;
    raytracingio->setting.n_wave = raytracingio->n_wave;
    raytracingio->setting.isTemperature = raytracingio->isTemperature;
    raytracingio->setting.isDisplay = raytracingio->isDisplay;
    raytracingio->setting.maxDepth = raytracingio->maxDepth;
    raytracingio->setting.n_sample = raytracingio->n_sample;

    return true;
    //raytracingio->setting.maxDepth = fileio->m_pXmlInput.
}


bool Raytracing::run(std::shared_ptr<RaytracingIO> &raytracingio, std::shared_ptr<FileIO> &fileio) {

    /// 清除txt文件信息
    if (raytracingio->isAlbedo)
    {
        // 以写入模式打开文件（std::ios::trunc 会清空文件）
        std::ofstream file(raytracingio->projectDir + "\\albedo.txt", std::ios::trunc);
        // 检查是否成功打开
        if (!file.is_open()) {
            std::cerr << "Error: Could not clear file " << raytracingio->projectDir + "\\albedo.txt" << std::endl;
        }
        // 文件内容已被清空，无需额外操作
        file.close(); // 显式关闭（可选）
    }


    for(int kangle = 0; kangle < raytracingio->n_angle; kangle++)
    {
        raytracingio->kangle = kangle;
        m_pGeometry->updateAngle(raytracingio,kangle);
       // updateSetting(raytracingio);

        m_pCommand->run(raytracingio);

        if (raytracingio->isImage)
        {
            // output
            //output(raytracingio,fileio,kangle);
            outputOrth(raytracingio,fileio,kangle);
        }

        if (raytracingio->isOrth)
        {
            outputOrth(raytracingio,fileio,kangle);
        }

        if (raytracingio->isAlbedo)
        {
            outputAlbedo(raytracingio,fileio,kangle);
        }

        std::cout << "Success: " << kangle << std::endl;
    }


    return true;
}

//bool Raytracing::createGPUBuffer(std::shared_ptr<RaytracingIO> &raytracingio){
//
//return false;
//}



void Raytracing::outputOrth(std::shared_ptr<RaytracingIO> &modelio, std::shared_ptr<FileIO> &fileio, int kangle) {
VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    int width = modelio->imageSize.x;
    int height = modelio->imageSize.y;
    int n_wave = modelio->n_wave;

    // 【修正1】使用 size_t 防止整数溢出 (3000*3000*100 超过 int 上限)
    size_t total_elements = (size_t)width * height * n_wave;
    VkDeviceSize bufferSize = total_elements * sizeof(float);

    nvvk::Buffer pixelBuffer = modelio->m_pAlloc->createBuffer(bufferSize, usage,
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_pVirtual->bufferToBuffer(modelio, *(modelio->m_virtualio->m_pBufferStorage), bufferSize, pixelBuffer);

    // map memory
    void *data = modelio->m_pAlloc->map(pixelBuffer);

    // 【修正2】创建 CPU 端内存副本 (Deep Copy)
    // 这是解决 0xc0000005 崩溃的核心：将显存数据拷贝到 CPU，使其脱离 Vulkan 生命周期
    float* pCpuData = new float[total_elements];
    if (pCpuData) {
        // 使用 memcpy 高速拷贝
        std::memcpy(pCpuData, data, bufferSize);
    } else {
        // 内存分配失败处理 (通常是因为没有编译为 x64)
        std::cerr << "Error: Failed to allocate CPU memory. Make sure to compile in x64 mode!" << std::endl;
        modelio->m_pAlloc->unmap(pixelBuffer);
        modelio->m_pAlloc->destroy(pixelBuffer);
        return; // 或者适当的错误处理
    }

    // --- 此时数据已安全在 pCpuData 中，不再依赖 pData/data 指针 ---

    // 填充 outImage (使用 pCpuData)
    fileio->outImage.clear();
    float *walker = pCpuData;
    for (int kband = 0; kband < n_wave; kband++) {
        std::vector<float> outImage1;
        outImage1.assign(walker, walker + width * height);
        walker += width * height;
        fileio->outImage.push_back(outImage1);
    }

    // 【修正3】现在可以安全销毁 Vulkan 资源了
    modelio->m_pAlloc->unmap(pixelBuffer);
    modelio->m_pAlloc->destroy(pixelBuffer);

    Angle angle = modelio->angles[kangle];
    std::vector<float> waves = modelio->waves;
    glm::vec2 resolution = modelio->imageSize;

    // 计算校正参数
    Eigen::VectorXd cx;
    Eigen::VectorXd cy;
    m_pGeometry->orthcorrect(modelio, angle.vza, angle.vaa, cx, cy);
    std::cout << "x' = " << cx[0]<<std::endl;
    // 分配结果内存
    float *pData_orth = new float[total_elements];
    std::memset(pData_orth, 0, bufferSize);

    // --- 正射校正循环 (已优化健壮性) ---
    for(int i = 0; i < width; i++)
    {
        for(int j = 0; j < height; j++)
        {
            // 目标图像索引 (使用 long long 防止溢出)
            long long old = (long long)j * width + i;

            // 计算源图像坐标
            int ii = int(i * cx[0] + j * cx[1] + i * j * cx[2] + cx[3]);
            int jj = int(i * cy[0] + j * cy[1] + i * j * cy[2] + cy[3]);

            // 【修正4】严格的几何边界检查 (防止越界崩溃)
            // 必须先检查 ii, jj 是否在图像范围内，再进行后续计算
            if (ii < 0 || ii >= width || jj < 0 || jj >= height)
            {
                continue;
            }

            // 计算源图像索引 (标准 Row-Major: y * width + x)
            // 注意：如果你发现图像旋转了90度，请改回 ii * height + jj，但必须保留上面的 if 检查

            long long orth = (long long)jj * width + ii;

            // 双重保险：检查源索引是否越界
            if (orth >= (long long)width * height) continue;

            for(int k = 0; k < n_wave; k++)
            {
                // 【修正5】使用 long long 计算波段偏移，防止计算溢出
                long long band_offset = (long long)k * width * height;
                long long oldd = band_offset + old;
                long long orthh = band_offset + orth;
                // 读取数据 (使用 pCpuData)
                if (pCpuData[orthh] == 0) continue;
                pData_orth[oldd] = pCpuData[orthh];

            }
        }
    }


    if(modelio->istime == false) {
        fileio->writeENVIdata(modelio->projectDir, pData_orth, width, height, n_wave, angle, -1, -1);
    } else {
        fileio->writeENVIdata(modelio->projectDir, pData_orth, width, height, n_wave, angle, -1, -1);
    }

    // 释放内存
    delete[] pCpuData;    // 记得释放拷贝的源数据
    delete[] pData_orth;  // 释放结果数据
}

void Raytracing::output(std::shared_ptr<RaytracingIO>& modelio, std::shared_ptr<FileIO>& fileio, int kangle)
{
    VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    int width = modelio->imageSize.x;
    int height = modelio->imageSize.y;
    int n_wave = modelio->n_wave;

    // 【修正1】使用 size_t 防止整数溢出 (3000*3000*100 超过 int 上限)
    size_t total_elements = (size_t)width * height * n_wave;
    VkDeviceSize bufferSize = total_elements * sizeof(float);

    nvvk::Buffer pixelBuffer = modelio->m_pAlloc->createBuffer(bufferSize, usage,
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_pVirtual->bufferToBuffer(modelio, *(modelio->m_virtualio->m_pBufferStorage), bufferSize, pixelBuffer);

    // map memory
    void *data = modelio->m_pAlloc->map(pixelBuffer);

    // 【修正2】创建 CPU 端内存副本 (Deep Copy)
    // 这是解决 0xc0000005 崩溃的核心：将显存数据拷贝到 CPU，使其脱离 Vulkan 生命周期
    float* pCpuData = new float[total_elements];
    if (pCpuData) {
        // 使用 memcpy 高速拷贝
        std::memcpy(pCpuData, data, bufferSize);
    } else {
        // 内存分配失败处理 (通常是因为没有编译为 x64)
        std::cerr << "Error: Failed to allocate CPU memory. Make sure to compile in x64 mode!" << std::endl;
        modelio->m_pAlloc->unmap(pixelBuffer);
        modelio->m_pAlloc->destroy(pixelBuffer);
        return; // 或者适当的错误处理
    }

    // --- 此时数据已安全在 pCpuData 中，不再依赖 pData/data 指针 ---

    // 填充 outImage (使用 pCpuData)
    fileio->outImage.clear();
    float *walker = pCpuData;
    for (int kband = 0; kband < n_wave; kband++) {
        std::vector<float> outImage1;
        outImage1.assign(walker, walker + width * height);
        walker += width * height;
        fileio->outImage.push_back(outImage1);
    }

    // 【修正3】现在可以安全销毁 Vulkan 资源了
    modelio->m_pAlloc->unmap(pixelBuffer);
    modelio->m_pAlloc->destroy(pixelBuffer);

    Angle angle = modelio->angles[kangle];
    std::vector<float> waves = modelio->waves;
    glm::vec2 resolution = modelio->imageSize;

    fileio->writeENVIdata(modelio->projectDir, pCpuData, width, height, n_wave, angle,-1,-1);

    // 释放内存
    delete[] pCpuData;    // 记得释放拷贝的源数据
}

void Raytracing::outputAlbedo(std::shared_ptr<RaytracingIO>& modelio, std::shared_ptr<FileIO>& fileio, int kangle)
{
    VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    int width = modelio->imageSize.x;
    int height = modelio->imageSize.y;
    int n_wave = modelio->n_wave;
    VkDeviceSize bufferSize = width * height * n_wave * sizeof(float);
    nvvk::Buffer pixelBuffer = modelio->m_pAlloc->createBuffer(bufferSize, usage,
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_pVirtual->bufferToBuffer(modelio, *(modelio->m_virtualio->m_pBufferStorage), bufferSize, pixelBuffer);

    // write the buffer to disk
    void *data = modelio->m_pAlloc->map(pixelBuffer);
    float *pData = reinterpret_cast<float *>(data);

    fileio->outImage.clear();
    float *walker = pData;
    for (int kband = 0; kband < n_wave; kband++) {
        std::vector<float> outImage1;
        outImage1.assign(walker, walker + width * height);
        walker += width * height;
        fileio->outImage.push_back(outImage1);

        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < width * height; i++) {
            if (outImage1[i] > 0.0f) {
                sum += outImage1[i];
                count++;
            }
        }
        float mean = count == 0 ? 0.0f : sum / count;

        fileio->outImageMeanValue.push_back(mean);
    }

    modelio->m_pAlloc->unmap(pixelBuffer);
    modelio->m_pAlloc->destroy(pixelBuffer);

    Angle angle = modelio->angles[kangle];
    std::vector<float> waves = modelio->waves;

    // 1. 打开文件（追加模式：std::ios::app）
    std::ofstream outfile(modelio->projectDir + "\\albedo.txt", std::ios::app);  // 如果文件不存在会自动创建

    // 2. 检查文件是否成功打开
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file " << modelio->projectDir + "\\albedo.txt" << std::endl;
        return;
    }

    // 3. 写入数据（每行一个数）
    for (int num=0; num < n_wave; num++) {
        outfile << waves[num] << " " << angle.sza << " " << angle.saa << " " <<  angle.vza << " " << angle.vaa << " " << fileio->outImageMeanValue[fileio->outImageMeanValue.size() - n_wave + num] << "\n";  // 换行分隔
    }

    // 4. 关闭文件（析构函数会自动调用，但显式关闭更安全）
    outfile.close();


}


bool Raytracing::destroy( std::shared_ptr<RaytracingIO> &raytracingio)
{

    m_pBuffer->destroy(raytracingio);
    m_pDescriptor->destroy(raytracingio);
    m_pPipeline->destroy(raytracingio);
    m_pCommand->destroy(raytracingio);

    raytracingio->m_sbtWrapper.destroy();
    raytracingio->m_pAccelStruct->m_rtBuilder.destroy();

    vkDeviceWaitIdle(raytracingio->m_device);
    raytracingio->m_genCmdBuf.deinit();
//    vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
//    vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
    raytracingio->m_pAlloc->deinit();

    return true;
}
