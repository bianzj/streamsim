//
// Created by admin on 2024/1/26.
//

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>
#include <gdal_priv.h>
#include "voxeleb.h"
#include "src/base/atmosphere_lut.h"

namespace {

bool isOpticalWavelength(float wavelength)
{
    const float wavelengthNanometers = wavelength <= 2.5f
        ? wavelength * 1000.0f : wavelength;
    return wavelengthNanometers <= 2500.0f;
}

float inversePlanckTemperature(float wavelengthNanometers, float radiance)
{
    if (!std::isfinite(radiance) || radiance <= 0.0f) {
        return 0.0f;
    }
    constexpr double c1 = 1.191043934e8;
    constexpr double c2 = 14388.291040;
    const double wavelengthMicrometers = wavelengthNanometers > 50.0f
        ? static_cast<double>(wavelengthNanometers) / 1000.0
        : static_cast<double>(wavelengthNanometers);
    const double denominator = wavelengthMicrometers *
        std::log(c1 /
            (static_cast<double>(radiance) *
             std::pow(wavelengthMicrometers, 5.0)) + 1.0);
    return denominator > 0.0
        ? static_cast<float>(c2 / denominator)
        : 0.0f;
}

std::vector<std::string> voxelebBandNames(const std::vector<float>& waves,
                                          bool temperatureOutput)
{
    std::vector<std::string> names;
    names.reserve(waves.size());
    for (const float wave : waves) {
        std::ostringstream label;
        if (isOpticalWavelength(wave)) {
            label << "Reflectance [-] @ " << wave << " nm";
        } else if (temperatureOutput) {
            label << "Brightness temperature [K] @ " << wave << " nm";
        } else {
            label << "Spectral radiance [W m-2 sr-1 um-1] @ " << wave << " nm";
        }
        names.push_back(label.str());
    }
    return names;
}

} // namespace

bool Voxeleb::setup(AppSetting &appsetting, std::shared_ptr<VoxelebIO> &modelio){


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




bool Voxeleb::upload(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &modelio){

    // auto & fileio = modelio->m_fileio;
    // auto & meshio = modelio->m_meshio;

    uploadDefined(fileio,modelio);
    m_pCompo->createCompProperty(fileio, modelio);
    // Hex extraction is part of scene construction, therefore its control
    // flag must be loaded before createPrimObjScene.
    uploadSetting(fileio, modelio);
    m_pScene->createPrimObjScene(fileio, modelio);
    m_pGeometry->createGeometry(fileio,modelio);
//    defineOPO(modelio);
    uploadMeteo(fileio,modelio);
    uploadAero(fileio, modelio);

    return true;
}

bool Voxeleb::uploadSetting(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &modelio) {

    modelio->n_wave = fileio->m_pVoxelebXml->sensorxml.waves.size();
    modelio->n_angle = fileio->m_pVoxelebXml->sensorxml.viewAngles.size();
    modelio->isTemperature =  fileio->m_pVoxelebXml->sensorxml.isTemperature;
    modelio->isDisplay = fileio->m_pVoxelebXml->sensorxml.isDisplay;
    modelio->isAlbedo = fileio->m_pVoxelebXml->sensorxml.isAlbedo;
    modelio->isImage = fileio->m_pVoxelebXml->sensorxml.isImage;
    modelio->isRadiationProcess = fileio->m_pVoxelebXml->sensorxml.isRadiationProcess;
    modelio->isEnergyProcess = fileio->m_pVoxelebXml->sensorxml.isEnergyProcess;
    if (!modelio->isRadiationProcess && !modelio->isEnergyProcess &&
        fileio->m_pVoxelebXml->sensorxml.isProcess) {
        modelio->isEnergyProcess = true;
    }
    modelio->isProcess = modelio->isRadiationProcess || modelio->isEnergyProcess ||
                         fileio->m_pVoxelebXml->sensorxml.isProcess;
    modelio->imageSize = fileio->m_pVoxelebXml->sensorxml.resolution;
    modelio->maxDepth = fileio->m_pVoxelebXml->settingxml.maxDepth;
    modelio->n_sample = fileio->m_pVoxelebXml->settingxml.n_sample;
    modelio->heterogeneousVoxel = fileio->m_pVoxelebXml->settingxml.heterogeneousVoxel;
    modelio->periodicNeighborCount =
        fileio->m_pVoxelebXml->settingxml.periodicNeighborCount;
    modelio->skyboxEnabled = fileio->m_pVoxelebXml->settingxml.skyboxEnabled;
    modelio->setting.n_jump = fileio->m_pVoxelebXml->settingxml.spectralAccelerationWidth;
    modelio->vegetationTemperatureMethod =
        fileio->m_pVoxelebXml->settingxml.vegetationTemperatureMethod;
    modelio->fluid = fileio->m_pVoxelebXml->fluidxml;

    return true;
}

bool Voxeleb::updateSetting(std::shared_ptr<VoxelebIO> &modelio){

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
    modelio->setting.voxelCount = modelio->n_voxel;
    modelio->setting.periodicNeighborCount = modelio->periodicNeighborCount;
    modelio->setting.skyboxEnabled = modelio->skyboxEnabled ? 1 : 0;


    return true;
    //modelio->setting.maxDepth = fileio->m_pXmlInput.
}


bool  Voxeleb::uploadMeteo(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &modelio){


   // Utils::readascfileinout(meteofile,0,1,)
    modelio->startTimeNode = fileio->m_pVoxelebXml->meteoxml.startTimeNode;
    modelio->endTimeNode = fileio->m_pVoxelebXml->meteoxml.endTimeNode;
   fileio->readMeteo(modelio->m_defined,modelio->n_node,modelio->meteos,modelio->atomconds);
   return true;
}

bool  Voxeleb::updateMeteo(std::shared_ptr<VoxelebIO> &modelio, int knode){

    modelio->meteo = modelio->meteos[knode];

    nvvk::CommandPool cmdBufGet(modelio->m_device, modelio->m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*modelio->m_pMeteoBuffer).buffer, 0, sizeof(Meteo), &modelio->meteo);
    if (modelio->m_pFluidParameters) {
        const float interval = std::max(0.1f, modelio->meteo.dTime);
        const float windSpeed = std::max(0.0f, modelio->meteo.u);
        const int scalarSubsteps = std::max(1, static_cast<int>(std::ceil(
            interval / std::max(0.001f, modelio->fluid.timeStep))));
        const float velocityScale = std::max(1.0f, modelio->fluidParameters.sources.w);
        const float latticeWindSpeed = windSpeed / velocityScale;
        const int domainCells = std::max(modelio->fluidGridSize.x, modelio->fluidGridSize.z);
        const int flowSubsteps = windSpeed > 0.001f
            ? static_cast<int>(std::min(4096.0,
                std::ceil(static_cast<double>(domainCells)
                    / std::max(0.01, static_cast<double>(latticeWindSpeed)))))
            : 1;
        // One domain crossing is normally sufficient for this simple steady
        // urban-flow update. Cap only this convergence estimate; an explicitly
        // requested scalar time step is still honored.
        modelio->fluidSubsteps = std::max(scalarSubsteps, flowSubsteps);
        modelio->fluidParameters.spacingTime.w = interval / modelio->fluidSubsteps;
        const float direction = glm::radians(modelio->fluid.windDirection);
        modelio->fluidParameters.ambientWind = glm::vec4(
            windSpeed * std::sin(direction), 0.0f,
            windSpeed * std::cos(direction), modelio->meteo.Ta + 273.15f);
        vkCmdUpdateBuffer(cmdBuf, modelio->m_pFluidParameters->buffer, 0,
                          sizeof(FluidParameters), &modelio->fluidParameters);
    }
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = (*modelio->m_pMeteoBuffer).buffer;
    barrier.offset = 0;
    barrier.size = sizeof(Meteo);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 1, &barrier, 0, nullptr);
    if (modelio->m_pFluidParameters) {
        barrier.buffer = modelio->m_pFluidParameters->buffer;
        barrier.size = sizeof(FluidParameters);
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 1, &barrier, 0, nullptr);
    }
    cmdBufGet.submitAndWait(cmdBuf);

    return true;
}

bool Voxeleb::uploadDefined(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &modelio)
{

    fileio->readDefined(modelio->m_defined);

    return true;
}

bool Voxeleb::create(std::shared_ptr<VoxelebIO> &modelio) {


    m_pBuffer->createBuffer(modelio);
    m_pDescriptor->createDescriptor(modelio);
    if (!m_pPipeline->createPipeline(modelio)) {
        return false;
    }
    m_pCommand->create(modelio);
    updateSetting(modelio);
    return true;
}

bool Voxeleb::run(std::shared_ptr<VoxelebIO> &modelio, std::shared_ptr<FileIO> &fileio) {


// for(int knode = 72; knode < 75;knode = knode + 1) {
    /// 清除txt文件信息
    // 以写入模式打开文件（std::ios::trunc 会清空文件）
    std::ofstream file(modelio->projectDir + "\\result_statistics.txt", std::ios::trunc);
    // 检查是否成功打开
    if (!file.is_open()) {
        std::cerr << "Error: Could not clear file " << modelio->projectDir + "\\result_statistics.txt" << std::endl;
    }
    file << "t SZA SAA VZA VAA wavelength "
         << (modelio->isTemperature ? "Temperature_K" : "Radiance")
         << "\n";
    // 文件内容已被清空，无需额外操作
    file.close(); // 显式关闭（可选）


    std::cout << "PROGRESS\t5\t加载体元场景与气象驱动" << std::endl;
    std::cout << "PROGRESS\t10\t温度方法：土壤按材质 0/1/2，植被="
              << (modelio->vegetationTemperatureMethod == 0
                      ? "Ball-Berry 经验法" : "Farquhar 机制法")
              << std::endl;

    //modelio->startTimeNode = 63;
    //modelio->endTimeNode = 74;
    for(int knode = modelio->startTimeNode; knode < modelio->endTimeNode;knode = knode +1) {
    // for(int knode = 0; knode < 144;knode = knode + 1) {
        modelio->k_node = knode;

        updateMeteo(modelio,knode);

        // Energy balance must use the solar position of the current meteo node.
        if (!modelio->angles.empty()) {
            m_pGeometry->updateAngle(modelio, 0);
        }

        // Explicit staggered coupling: advance the atmospheric field with the
        // latest surface temperatures, then use that wind/air state in the
        // current energy-balance solve. Updated surfaces feed the next node.
        if (modelio->fluid.enabled) {
            m_pCommand->runFluid(modelio, modelio->fluidSubsteps);
        }

        m_pCommand->runEB(modelio);

        if (std::getenv("STREAMSIM_TEMPERATURE_DIAGNOSTICS") != nullptr &&
            modelio->m_voxelio->m_pTempeBuffer) {
            const size_t count = static_cast<size_t>(modelio->n_voxel);
            std::vector<VoxelTempe> temperatures(count);
            const VkDeviceSize bytes = count * sizeof(VoxelTempe);
            nvvk::Buffer staging = modelio->m_pAlloc->createBuffer(
                bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            m_pVirtual->bufferToBuffer(modelio, *modelio->m_voxelio->m_pTempeBuffer,
                                       bytes, staging);
            void* mapped = modelio->m_pAlloc->map(staging);
            std::memcpy(temperatures.data(), mapped, static_cast<size_t>(bytes));
            modelio->m_pAlloc->unmap(staging);
            modelio->m_pAlloc->destroy(staging);

            float minima[6], maxima[6];
            size_t typeCounts[6]{};
            std::fill_n(minima, 6, std::numeric_limits<float>::max());
            std::fill_n(maxima, 6, std::numeric_limits<float>::lowest());
            for (size_t index = 0; index < count && index < modelio->m_voxelio->voxellinks.size(); ++index) {
                const VoxelLink& voxel = modelio->m_voxelio->voxellinks[index];
                if (voxel.instanceId < 0 || voxel.instanceId >= static_cast<int>(modelio->m_instanceio->instanceLinks.size())) continue;
                const uint32_t meshId = modelio->m_instanceio->instanceLinks[voxel.instanceId].meshId;
                if (meshId >= modelio->m_meshio->meshLinks.size()) continue;
                const int type = modelio->m_meshio->meshLinks[meshId].type;
                if (type < 0 || type >= 6) continue;
                minima[type] = std::min(minima[type], std::min(temperatures[index].sunlit, temperatures[index].shaded));
                maxima[type] = std::max(maxima[type], std::max(temperatures[index].sunlit, temperatures[index].shaded));
                ++typeCounts[type];
            }
            for (int type = 0; type < 6; ++type) {
                if (typeCounts[type] == 0) continue;
                std::cout << "TEMPERATURE_RANGE\ttype=" << type << "\tcount="
                          << typeCounts[type] << "\tmin=" << minima[type]
                          << "\tmax=" << maxima[type] << std::endl;
            }
        }

        if (modelio->fluid.enabled) {
            outputFluidSlices(modelio);
        }

        const int completed = knode - modelio->startTimeNode + 1;
        const int total = std::max(1, modelio->endTimeNode - modelio->startTimeNode);
        const int progress = 10 + 85 * completed / total;
        std::cout << "PROGRESS\t" << progress << "\t能量平衡时间节点 "
                  << completed << "/" << total << " T="
                  << std::to_string(modelio->meteo.t) << std::endl;
        if (modelio->isRadiationProcess || modelio->isEnergyProcess) {
            outputVoxel(modelio,fileio);
        }
    //
    //    if (knode == 73)
        // if (1)
        {
        const int observationCount = modelio->isUAVTrave ? 1 : modelio->n_angle;
        const int cruisePosition = modelio->isUAVTrave && modelio->n_pos > 0
            ? (knode - modelio->startTimeNode) % modelio->n_pos : -1;
        for (int kangle = 0; kangle < observationCount; kangle++) {
            modelio->k_angle = kangle;
            if (cruisePosition >= 0) {
                modelio->k_pos = cruisePosition;
                m_pGeometry->updateSensorPos(modelio, cruisePosition);
            } else {
                m_pGeometry->updateAngle(modelio,kangle);
            }

            Angle angle = modelio->angles[kangle];

            ////updateSetting(modelio);
            m_pCommand->runRT(modelio);

    //        std::cout << "Success: " << kangle << std::endl;


                std::cout << "OBSERVATION\t"
                      << "    vza_" << std::to_string(angle.vza) << "    vaa_" << std::to_string(angle.vaa)
                      << "    sza_" << std::to_string(angle.sza) << "    saa_" << std::to_string(angle.saa) << std::endl;
                if (cruisePosition >= 0) {
                    const glm::vec3 position = modelio->uavposes[cruisePosition];
                    std::cout << "OBSERVATION\t航点 " << (cruisePosition + 1) << "/" << modelio->n_pos
                              << " X=" << position.x << " Y=" << position.y
                              << " H=" << position.z << std::endl;
                }

                if (modelio->isImage)
                {
                    output(modelio,fileio,knode, kangle, cruisePosition);
                }

                outputTxt(modelio, fileio, kangle);
            }
            // output(modelio,fileio,knode, kangle);
        }

    }
    std::cout << "PROGRESS\t100\t体元能量平衡计算完成" << std::endl;
    return true;
}



bool Voxeleb::destroy(std::shared_ptr<VoxelebIO> & modelio){

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



void Voxeleb::output(std::shared_ptr<VoxelebIO> &modelio, std::shared_ptr<FileIO> &fileio, int knode, int kangle, int kpos) {
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
    const bool perspective = fileio->m_pVoxelebXml->sensorxml.projection == Projection::PERSPECTIVE;
    const float sensorHeight = kpos >= 0 && kpos < static_cast<int>(modelio->uavposes.size())
        ? modelio->uavposes[kpos].z : fileio->m_pVoxelebXml->sensorxml.position.z;
    std::vector<float> outputData(totalElements, 0.0f);
    Eigen::VectorXd cx;
    Eigen::VectorXd cy;
    if (!perspective) m_pGeometry->orthcorrect(modelio, angle.vza, angle.vaa, cx, cy);

    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            const size_t destination = static_cast<size_t>(j) * width + i;
            size_t source = destination;
            if (!perspective) {
                const int ii = static_cast<int>(i * cx[0] + j * cx[1] + i * j * cx[2] + cx[3]);
                const int jj = static_cast<int>(i * cy[0] + j * cy[1] + i * j * cy[2] + cy[3]);
                if (ii < 0 || ii >= width || jj < 0 || jj >= height) continue;
                source = static_cast<size_t>(jj) * width + ii;
            }
            float skyZenith = 0.0f;
            const bool isSkyPixel = perspective && atmosphereSkyPixelZenith(
                i, j, width, height,
                fileio->m_pVoxelebXml->sensorxml.sensorFov,
                angle.vza, angle.vaa, skyZenith);
            for (int band = 0; band < nWave; ++band) {
                const size_t bandOffset = static_cast<size_t>(band) * imageElements;
                const float sourceValue = sourceData[bandOffset + source];
                float value = sourceValue;
                if ((!std::isfinite(sourceValue) || sourceValue == 0.0f) && isSkyPixel) {
                    value = atmosphereSkyOutputValue(
                        fileio->m_pVoxelebXml->atmospherexml,
                        modelio->waves[band], sensorHeight, skyZenith,
                        modelio->isTemperature,
                        fileio->m_pVoxelebXml->lightxml.skyTemperature);
                } else {
                    value = atmosphereCorrectRadiance(
                        sourceValue, fileio->m_pVoxelebXml->atmospherexml,
                        modelio->waves[band], sensorHeight, angle.vza);
                    if (modelio->isTemperature && !isOpticalWavelength(modelio->waves[band]) && value > 0.0f)
                        value = inversePlanckTemperature(modelio->waves[band], value);
                }
                if (value != 0.0f) outputData[bandOffset + destination] = value;
            }
        }
    }

    const float time = modelio->meteo.t;
    fileio->writeTIFData(
        modelio->projectDir, outputData.data(), width, height, nWave, angle, time,
        fileio->m_pVoxelebXml->atmospherexml.enabled ? "_v_a" : "_v", kpos, false,
        voxelebBandNames(modelio->waves, modelio->isTemperature));
}
void Voxeleb::outputTxt(std::shared_ptr<VoxelebIO>& modelio, std::shared_ptr<FileIO>& fileio, int kangle)
{
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
                float value = sourceData[bandOffset + source];
                if (modelio->isTemperature && !isOpticalWavelength(modelio->waves[band])
                    && value > 0.0f) {
                    value = inversePlanckTemperature(
                        modelio->waves[band], value);
                }
                if (value != 0.0f) {
                    orthData[bandOffset + destination] = value;
                }
            }
        }
    }

    const float time = modelio->meteo.t;
    std::vector<float> meanValues(static_cast<size_t>(nWave), 0.0f);
    for (int band = 0; band < nWave; ++band) {
        const float* bandBegin = orthData.data() + static_cast<size_t>(band) * imageElements;
        double sum = 0.0;
        size_t count = 0;
        for (size_t pixel = 0; pixel < imageElements; ++pixel) {
            const float value = bandBegin[pixel];
            if (value > 0.0f) {
                sum += value;
                ++count;
            }
        }
        meanValues[static_cast<size_t>(band)] = count == 0
            ? 0.0f : static_cast<float>(sum / count);
    }

    std::ofstream outfile(modelio->projectDir + "\\result_statistics.txt", std::ios::app);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file "
                  << modelio->projectDir + "\\result_statistics.txt" << std::endl;
        return;
    }

    for (int band = 0; band < nWave; ++band) {
        outfile << time << " " << angle.sza << " " << angle.saa << " "
                << angle.vza << " " << angle.vaa << " " << modelio->waves[band] << " "
                << meanValues[static_cast<size_t>(band)]
                << "\n";
    }
}
void Voxeleb::outputVoxel(std::shared_ptr<VoxelebIO> &modelio, std::shared_ptr<FileIO> &fileio) {
    (void)fileio;
    if (!modelio || modelio->n_voxel <= 0 || !modelio->m_voxelio ||
        !modelio->m_voxelio->m_pDirBuffer || !modelio->m_voxelio->m_pNetRadBuffer ||
        !modelio->m_voxelio->m_pFluxBuffer ||
        (!modelio->isRadiationProcess && !modelio->isEnergyProcess)) {
        return;
    }

    const size_t voxelCount = static_cast<size_t>(modelio->n_voxel);
    std::vector<VoxelDir> directions(voxelCount);
    std::vector<VoxelNetRad> netRadiation(voxelCount);
    std::vector<VoxelHeatflux> heatFlux(voxelCount);
    const auto download = [&](const nvvk::Buffer& source, void* destination, VkDeviceSize size) {
        nvvk::Buffer staging = modelio->m_pAlloc->createBuffer(
            size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_pVirtual->bufferToBuffer(modelio, source, size, staging);
        void* mapped = modelio->m_pAlloc->map(staging);
        std::memcpy(destination, mapped, static_cast<size_t>(size));
        modelio->m_pAlloc->unmap(staging);
        modelio->m_pAlloc->destroy(staging);
    };
    download(*modelio->m_voxelio->m_pDirBuffer, directions.data(), voxelCount * sizeof(VoxelDir));
    download(*modelio->m_voxelio->m_pNetRadBuffer, netRadiation.data(), voxelCount * sizeof(VoxelNetRad));
    download(*modelio->m_voxelio->m_pFluxBuffer, heatFlux.data(), voxelCount * sizeof(VoxelHeatflux));

    const auto isProfileSoilVoxel = [&](size_t voxel) {
        if (!modelio->m_instanceio || !modelio->m_meshio ||
            voxel >= modelio->m_voxelio->voxellinks.size()) {
            return false;
        }
        const int instanceId = modelio->m_voxelio->voxellinks[voxel].instanceId;
        if (instanceId < 0 ||
            instanceId >= static_cast<int>(modelio->m_instanceio->instanceLinks.size())) {
            return false;
        }
        const int meshId = modelio->m_instanceio->instanceLinks[instanceId].meshId;
        if (meshId < 0 || meshId >= static_cast<int>(modelio->m_meshio->meshLinks.size())) {
            return false;
        }
        const MeshLink& meshLink = modelio->m_meshio->meshLinks[meshId];
        return meshLink.type == static_cast<int>(Type::SOIL) &&
            meshLink.bioId >= 0 &&
            meshLink.bioId < static_cast<int>(modelio->m_meshio->soilsets.size()) &&
            modelio->m_meshio->soilsets[meshLink.bioId].method == 2;
    };
    bool hasSoilProfile = false;
    if (modelio->isEnergyProcess && modelio->m_voxelio->m_pTLASTBuffer) {
        for (size_t voxel = 0; voxel < voxelCount && !hasSoilProfile; ++voxel) {
            hasSoilProfile = isProfileSoilVoxel(voxel);
        }
    }
    std::vector<TLAST> soilProfile;
    if (hasSoilProfile) {
        soilProfile.resize(voxelCount * TLASTNUM);
        download(*modelio->m_voxelio->m_pTLASTBuffer, soilProfile.data(),
                 voxelCount * TLASTNUM * sizeof(TLAST));
    }

    const double julianTime = static_cast<double>(modelio->meteo.t);
    int day = static_cast<int>(std::floor(julianTime));
    int totalMinutes = static_cast<int>(std::llround((julianTime - day) * 1440.0));
    if (totalMinutes >= 1440) {
        day += totalMinutes / 1440;
        totalMinutes %= 1440;
    }
    std::ostringstream time;
    time << "DOY" << day << '_' << std::setw(2) << std::setfill('0') << totalMinutes / 60
         << '-' << std::setw(2) << std::setfill('0') << totalMinutes % 60;

    const std::filesystem::path directory = std::filesystem::path(modelio->projectDir) / "process";
    std::filesystem::create_directories(directory);
    const float scale = std::max(0.0001f, modelio->stepsize_surface);
    const auto writeProcess = [&](const std::string& type) {
        // Radiation in VoxelEB is a time sequence of VoxelRT fields;
        // energy fluxes retain the VoxelEB model name.
        const std::string processModel =
            type == "radiation" ? "voxelrt" : "voxeleb";
        const std::string stem = processModel + "_T=" + time.str();
        const std::filesystem::path binaryPath = directory / (stem + ".bin");
        const std::filesystem::path metadataPath = directory / (stem + ".json");
        const bool writeSoilProfile = type == "energy" && hasSoilProfile;
        std::ofstream binary(binaryPath, std::ios::binary | std::ios::trunc);
        if (!binary) {
            throw std::runtime_error("Cannot write voxel process file: " + binaryPath.string());
        }
        for (size_t voxel = 0; voxel < voxelCount; ++voxel) {
            const VoxelLink& link = modelio->m_voxelio->voxellinks[voxel];
            const float sunlit = std::clamp(directions[voxel].solar, 0.0f, 1.0f);
            const float shaded = 1.0f - sunlit;
            const float position[3] = {
                (static_cast<float>(link.voxelId.x) + 0.5f) * scale,
                (static_cast<float>(link.voxelId.y) + 0.5f) * scale,
                (static_cast<float>(link.voxelId.z) + 0.5f) * scale
            };
            binary.write(reinterpret_cast<const char*>(position), sizeof(position));
            float values[3];
            if (type == "radiation") {
                values[0] = netRadiation[voxel].diffuseVrad +
                            sunlit * netRadiation[voxel].directVrad;
                values[1] = netRadiation[voxel].diffuseTrad +
                            sunlit * netRadiation[voxel].directTrad;
                values[2] = sunlit * (heatFlux[voxel].Hsunlit + heatFlux[voxel].LEsunlit + heatFlux[voxel].Gsunlit) +
                            shaded * (heatFlux[voxel].Hshaded + heatFlux[voxel].LEshaded + heatFlux[voxel].Gshaded);
            } else {
                values[0] = sunlit * heatFlux[voxel].LEsunlit + shaded * heatFlux[voxel].LEshaded;
                values[1] = sunlit * heatFlux[voxel].Hsunlit + shaded * heatFlux[voxel].Hshaded;
                values[2] = sunlit * heatFlux[voxel].Gsunlit + shaded * heatFlux[voxel].Gshaded;
            }
            binary.write(reinterpret_cast<const char*>(values), sizeof(values));
            if (writeSoilProfile) {
                float layerTemperatures[TLASTNUM];
                if (isProfileSoilVoxel(voxel)) {
                    for (int layer = 0; layer < TLASTNUM; ++layer) {
                        const TLAST& temperature = soilProfile[voxel * TLASTNUM + layer];
                        layerTemperatures[layer] =
                            sunlit * temperature.sunlit + shaded * temperature.shaded;
                    }
                } else {
                    std::fill_n(layerTemperatures, TLASTNUM,
                                std::numeric_limits<float>::quiet_NaN());
                }
                binary.write(reinterpret_cast<const char*>(layerTemperatures),
                             sizeof(layerTemperatures));
            }
        }
        binary.close();

        std::ofstream metadata(metadataPath, std::ios::trunc);
        if (!metadata) {
            throw std::runtime_error("Cannot write voxel process metadata: " + metadataPath.string());
        }
        metadata << std::setprecision(9)
                 << "{\n  \"kind\": \"voxel-" << type << "-process\",\n"
                 << "  \"model\": \"" << processModel << "\",\n"
                 << "  \"processType\": \"" << type << "\",\n"
                 << "  \"geometry\": \"voxel\",\n"
                 << "  \"node\": " << modelio->k_node << ",\n"
                 << "  \"julianTime\": " << modelio->meteo.t << ",\n"
                 << "  \"time\": \"" << time.str() << "\",\n"
                 << "  \"voxelCount\": " << voxelCount << ",\n"
                 << "  \"voxelSize\": " << scale << ",\n"
                 << "  \"dataFile\": \"" << binaryPath.filename().string() << "\",\n"
                 << "  \"dataType\": \"float32-little-endian\",\n"
                 << "  \"layout\": \"voxel-interleaved\",\n"
                 << "  \"recordFloats\": " << (writeSoilProfile ? 6 + TLASTNUM : 6) << ",\n"
                 << "  \"positionOffsets\": [0,1,2],\n"
                 << "  \"fields\": ";
        if (type == "radiation") {
            metadata << "[{\"id\":\"shortwaveRadiation\",\"label\":\"短波辐射 [W m⁻²]\",\"offset\":3},"
                        "{\"id\":\"longwaveRadiation\",\"label\":\"长波辐射 [W m⁻²]\",\"offset\":4},"
                        "{\"id\":\"netRadiation\",\"label\":\"净辐射 [W m⁻²]\",\"offset\":5}]\n}\n";
        } else {
            metadata << "[{\"id\":\"latentHeat\",\"label\":\"潜热 [W m⁻²]\",\"offset\":3},"
                        "{\"id\":\"sensibleHeat\",\"label\":\"显热 [W m⁻²]\",\"offset\":4},"
                        "{\"id\":\"surfaceHeatFlux\",\"label\":\"表面热通量 [W m⁻²]\",\"offset\":5}]";
            if (writeSoilProfile) {
                metadata << ",\n  \"soilProfile\": {\n"
                         << "    \"layerCount\": " << TLASTNUM << ",\n"
                         << "    \"temperatureUnit\": \"degC\",\n"
                         << "    \"depthUnit\": \"m\",\n"
                         << "    \"depths\": [0,0.02,0.04,0.10,0.20,0.40,0.60,1.00],\n"
                         << "    \"offsets\": [6,7,8,9,10,11,12,13],\n"
                         << "    \"aggregation\": \"actual-soil-column\",\n"
                         << "    \"lowerBoundary\": \"material-Tsoil-at-1m\"\n"
                         << "  }";
            }
            metadata << "\n}\n";
        }
        std::cout << "PROCESS\t" << metadataPath.string() << std::endl;
    };
    if (modelio->isRadiationProcess) writeProcess("radiation");
    if (modelio->isEnergyProcess) writeProcess("energy");
    if (modelio->isEnergyProcess && modelio->fluid.enabled &&
        modelio->m_pFluidVelocityA && modelio->m_pFluidScalarA &&
        modelio->fluidCellCount > 0) {
        const size_t cellCount = static_cast<size_t>(modelio->fluidCellCount);
        std::vector<glm::vec4> velocity(cellCount);
        std::vector<glm::vec4> scalar(cellCount);
        download(*modelio->m_pFluidVelocityA, velocity.data(), cellCount * sizeof(glm::vec4));
        download(*modelio->m_pFluidScalarA, scalar.data(), cellCount * sizeof(glm::vec4));

        const std::string stem = "voxelfluid_T=" + time.str();
        const std::filesystem::path binaryPath = directory / (stem + ".bin");
        const std::filesystem::path metadataPath = directory / (stem + ".json");
        std::ofstream binary(binaryPath, std::ios::binary | std::ios::trunc);
        if (!binary) throw std::runtime_error("Cannot write fluid process file: " + binaryPath.string());
        const int nx = modelio->fluidGridSize.x;
        const int nz = modelio->fluidGridSize.z;
        const int layer = nx * nz;
        for (size_t index = 0; index < cellCount; ++index) {
            const int y = static_cast<int>(index) / layer;
            const int remainder = static_cast<int>(index) - y * layer;
            const int z = remainder / nx;
            const int x = remainder - z * nx;
            const float record[8] = {
                (x + 0.5f) * modelio->fluidParameters.spacingTime.x,
                (y + 0.5f) * modelio->fluidParameters.spacingTime.y,
                (z + 0.5f) * modelio->fluidParameters.spacingTime.z,
                velocity[index].x, velocity[index].y, velocity[index].z,
                scalar[index].x, scalar[index].y
            };
            binary.write(reinterpret_cast<const char*>(record), sizeof(record));
        }
        binary.close();

        std::ofstream metadata(metadataPath, std::ios::trunc);
        if (!metadata) throw std::runtime_error("Cannot write fluid process metadata: " + metadataPath.string());
        metadata << std::setprecision(9)
                 << "{\n  \"kind\": \"voxel-fluid-process\",\n"
                 << "  \"model\": \"voxelfluid\",\n"
                 << "  \"processType\": \"fluid\",\n"
                 << "  \"geometry\": \"voxel\",\n"
                 << "  \"node\": " << modelio->k_node << ",\n"
                 << "  \"julianTime\": " << modelio->meteo.t << ",\n"
                 << "  \"time\": \"" << time.str() << "\",\n"
                 << "  \"voxelCount\": " << cellCount << ",\n"
                 << "  \"voxelSize\": " << modelio->fluidParameters.spacingTime.x << ",\n"
                 << "  \"gridSize\": [" << modelio->fluidGridSize.x << ','
                 << modelio->fluidGridSize.y << ',' << modelio->fluidGridSize.z << "],\n"
                 << "  \"dataFile\": \"" << binaryPath.filename().string() << "\",\n"
                 << "  \"dataType\": \"float32-little-endian\",\n"
                 << "  \"layout\": \"voxel-interleaved\",\n"
                 << "  \"recordFloats\": 8,\n"
                 << "  \"positionOffsets\": [0,1,2],\n"
                 << "  \"fields\": [{\"id\":\"windX\",\"label\":\"X 风速 [m s⁻¹]\",\"offset\":3},"
                    "{\"id\":\"windVertical\",\"label\":\"垂直风速 [m s⁻¹]\",\"offset\":4},"
                    "{\"id\":\"windY\",\"label\":\"Y 风速 [m s⁻¹]\",\"offset\":5},"
                    "{\"id\":\"airTemperature\",\"label\":\"空气温度 [K]\",\"offset\":6},"
                    "{\"id\":\"smoke\",\"label\":\"烟雾浓度 [0–1]\",\"offset\":7}]\n}\n";
        std::cout << "PROCESS\t" << metadataPath.string() << std::endl;
    }
}

void Voxeleb::outputFluidSlices(std::shared_ptr<VoxelebIO>& modelio)
{
    if (!modelio || !modelio->fluid.enabled
        || (!modelio->fluid.outputWind && !modelio->fluid.outputAirTemperature)
        || modelio->fluidCellCount == 0 || !modelio->m_pFluidMeta
        || (modelio->fluid.outputWind && !modelio->m_pFluidVelocityA)
        || (modelio->fluid.outputAirTemperature && !modelio->m_pFluidScalarA)) {
        return;
    }

    const int nx = modelio->fluidGridSize.x;
    const int ny = modelio->fluidGridSize.y;
    const int nz = modelio->fluidGridSize.z;
    if (nx <= 0 || ny <= 0 || nz <= 0) return;

    const size_t cellCount = static_cast<size_t>(modelio->fluidCellCount);
    std::vector<FluidCellMeta> meta(cellCount);
    std::vector<glm::vec4> velocity;
    std::vector<glm::vec4> scalar;
    if (modelio->fluid.outputWind) velocity.resize(cellCount);
    if (modelio->fluid.outputAirTemperature) scalar.resize(cellCount);

    const auto download = [&](const nvvk::Buffer& source, void* destination, VkDeviceSize size) {
        nvvk::Buffer staging = modelio->m_pAlloc->createBuffer(
            size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_pVirtual->bufferToBuffer(modelio, source, size, staging);
        void* mapped = modelio->m_pAlloc->map(staging);
        std::memcpy(destination, mapped, static_cast<size_t>(size));
        modelio->m_pAlloc->unmap(staging);
        modelio->m_pAlloc->destroy(staging);
    };
    download(*modelio->m_pFluidMeta, meta.data(), cellCount * sizeof(FluidCellMeta));
    if (modelio->fluid.outputWind) {
        download(*modelio->m_pFluidVelocityA, velocity.data(), cellCount * sizeof(glm::vec4));
    }
    if (modelio->fluid.outputAirTemperature) {
        download(*modelio->m_pFluidScalarA, scalar.data(), cellCount * sizeof(glm::vec4));
    }

    const float spacingX = std::max(0.0001f, modelio->fluidParameters.spacingTime.x);
    const float spacingY = std::max(0.0001f, modelio->fluidParameters.spacingTime.y);
    const float spacingZ = std::max(0.0001f, modelio->fluidParameters.spacingTime.z);
    const int layerY = std::clamp(
        static_cast<int>(std::floor(modelio->fluid.outputHeight / spacingY)), 0, ny - 1);
    const float actualHeight = (static_cast<float>(layerY) + 0.5f) * spacingY;
    const float noData = std::numeric_limits<float>::quiet_NaN();
    const size_t pixelCount = static_cast<size_t>(nx) * static_cast<size_t>(nz);

    std::vector<std::vector<float>> windBands;
    std::vector<float> temperature;
    if (modelio->fluid.outputWind) {
        windBands.assign(4, std::vector<float>(pixelCount, noData));
    }
    if (modelio->fluid.outputAirTemperature) {
        temperature.assign(pixelCount, noData);
    }
    for (int z = 0; z < nz; ++z) {
        for (int x = 0; x < nx; ++x) {
            const size_t source = static_cast<size_t>(x)
                + static_cast<size_t>(nx) * (static_cast<size_t>(z)
                + static_cast<size_t>(nz) * static_cast<size_t>(layerY));
            const size_t destination = static_cast<size_t>(nz - 1 - z)
                * static_cast<size_t>(nx) + static_cast<size_t>(x);
            if (meta[source].kind == 1) continue;
            if (modelio->fluid.outputWind) {
                const glm::vec3 value = glm::vec3(velocity[source]);
                windBands[0][destination] = glm::length(glm::vec2(value.x, value.z));
                windBands[1][destination] = value.x;
                windBands[2][destination] = value.y;
                windBands[3][destination] = value.z;
            }
            if (modelio->fluid.outputAirTemperature) {
                temperature[destination] = scalar[source].x - 273.15f;
            }
        }
    }

    const double julianTime = static_cast<double>(modelio->meteo.t);
    int day = static_cast<int>(std::floor(julianTime));
    int totalMinutes = static_cast<int>(std::llround((julianTime - day) * 1440.0));
    if (totalMinutes >= 1440) {
        day += totalMinutes / 1440;
        totalMinutes %= 1440;
    }
    std::ostringstream time;
    time << "DOY" << day << '_' << std::setw(2) << std::setfill('0')
         << totalMinutes / 60 << '-' << std::setw(2) << std::setfill('0')
         << totalMinutes % 60;
    std::ostringstream height;
    height << std::fixed << std::setprecision(2) << modelio->fluid.outputHeight;

    const std::filesystem::path directory(modelio->projectDir);
    std::filesystem::create_directories(directory);
    GDALAllRegister();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!driver) throw std::runtime_error("GDAL GTiff driver is unavailable");
    const double geoTransform[6] = {
        static_cast<double>(modelio->sceneOrigin_XYZ.x), static_cast<double>(spacingX), 0.0,
        static_cast<double>(modelio->sceneOrigin_XYZ.y + modelio->sceneSize_XYZ.y),
        0.0, -static_cast<double>(spacingZ)
    };
    const std::string requestedHeight = std::to_string(modelio->fluid.outputHeight);
    const std::string resolvedHeight = std::to_string(actualHeight);

    const auto createDataset = [&](const std::filesystem::path& path, int bandCount) {
        GDALDataset* dataset = driver->Create(path.string().c_str(), nx, nz, bandCount,
                                              GDT_Float32, nullptr);
        if (!dataset) throw std::runtime_error("Cannot create fluid GeoTIFF: " + path.string());
        double transform[6];
        std::copy(std::begin(geoTransform), std::end(geoTransform), transform);
        dataset->SetGeoTransform(transform);
        dataset->SetMetadataItem("MODEL", "VoxelEB D3Q19 BGK LBM");
        dataset->SetMetadataItem("TIME", time.str().c_str());
        dataset->SetMetadataItem("REQUESTED_HEIGHT_M", requestedHeight.c_str());
        dataset->SetMetadataItem("ACTUAL_HEIGHT_M", resolvedHeight.c_str());
        dataset->SetMetadataItem("COORDINATE_SYSTEM", "Local scene coordinates in metres");
        return dataset;
    };
    const auto writeBand = [&](GDALDataset* dataset, int index,
                               const std::vector<float>& values,
                               const char* description, const char* unit) {
        GDALRasterBand* band = dataset->GetRasterBand(index);
        band->SetDescription(description);
        band->SetUnitType(unit);
        band->SetNoDataValue(static_cast<double>(noData));
        if (band->RasterIO(GF_Write, 0, 0, nx, nz,
                           const_cast<float*>(values.data()), nx, nz,
                           GDT_Float32, 0, 0) != CE_None) {
            throw std::runtime_error("Cannot write fluid GeoTIFF raster band");
        }
    };

    if (modelio->fluid.outputWind) {
        const std::filesystem::path path = directory /
            ("fluid_wind_H=" + height.str() + "m_T=" + time.str() + ".tif");
        GDALDataset* dataset = createDataset(path, 4);
        try {
            writeBand(dataset, 1, windBands[0], "Horizontal wind speed", "m/s");
            writeBand(dataset, 2, windBands[1], "Wind X component", "m/s");
            writeBand(dataset, 3, windBands[2], "Wind vertical component", "m/s");
            writeBand(dataset, 4, windBands[3], "Wind Y component", "m/s");
        } catch (...) {
            GDALClose(dataset);
            throw;
        }
        GDALClose(dataset);
        std::cout << "FLUID_OUTPUT\t" << path.string() << std::endl;
    }
    if (modelio->fluid.outputAirTemperature) {
        const std::filesystem::path path = directory /
            ("fluid_air_temperature_H=" + height.str() + "m_T=" + time.str() + ".tif");
        GDALDataset* dataset = createDataset(path, 1);
        try {
            writeBand(dataset, 1, temperature, "Air temperature", "degree Celsius");
        } catch (...) {
            GDALClose(dataset);
            throw;
        }
        GDALClose(dataset);
        std::cout << "FLUID_OUTPUT\t" << path.string() << std::endl;
    }
}


bool Voxeleb::uploadAero(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &modelio) {


    if(fileio->m_pVoxelebXml->aerocondxml.aerotype == AeroType::ONE) {
        modelio->aeroconds.emplace_back(fileio->m_pVoxelebXml->aerocondxml.aerocond);
    }else if (fileio->m_pVoxelebXml->aerocondxml.aerotype == AeroType::image)
    {
        int a = 10;
    }else if(fileio->m_pVoxelebXml->aerocondxml.aerotype == AeroType::gridCal){
        int b = 10;
    }

    return false;


}
