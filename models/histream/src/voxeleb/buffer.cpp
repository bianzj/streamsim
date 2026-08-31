//
// Created by admin on 2024/1/28.
//

#include "buffer.h"



bool Buffer::createBuffer(std::shared_ptr<VoxelebIO> &voxellstio){

    VkDevice & m_device = voxellstio->m_device;
    nvvk::Queue &m_queue  = voxellstio->m_queues[eGCT];
    auto & meshio = voxellstio->m_meshio;
    auto & instanceio = voxellstio->m_instanceio;
    auto & virtualio = voxellstio->m_virtualio;
    auto & m_pAlloc = voxellstio->m_pAlloc;
    auto & voxelio = voxellstio->m_voxelio;
    auto & surfio = voxellstio->m_surfio;
    auto n_voxel = voxellstio->n_voxel;
    auto & m_pAccelStruct = voxellstio->m_pAccelStruct;
    auto & defined = voxellstio->m_defined;

    nvvk::CommandPool cmdGen(m_device, m_queue.familyIndex);

    // spectral
    if (meshio->spectrals.size() > 0)
    {
        VkCommandBuffer cmdBufSpectral = cmdGen.createCommandBuffer();
        VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        meshio->m_pBufferSpectral = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufSpectral, meshio->spectrals, usage_));
        cmdGen.submitAndWait(cmdBufSpectral);
    }


    // fixedSpectral
    if (meshio->fixedSpectrals.size() > 0)
    {
        VkCommandBuffer cmdBufSpectral = cmdGen.createCommandBuffer();
        VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        meshio->m_pFixedSpectralBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufSpectral, meshio->fixedSpectrals, usage_));
        cmdGen.submitAndWait(cmdBufSpectral);
    }

    if (meshio->thermals.size() > 0)
    {
        VkCommandBuffer cmdBufSpectral = cmdGen.createCommandBuffer();
        VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        meshio->m_pBufferThermal = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufSpectral, meshio->thermals, usage_));
        cmdGen.submitAndWait(cmdBufSpectral);
    }




    // canopy
    if (meshio->canopies.size() > 0)
    {
        VkCommandBuffer cmdBufCanopy = cmdGen.createCommandBuffer();
        meshio->m_pBufferCanopy = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufCanopy, meshio->canopies, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
        cmdGen.submitAndWait(cmdBufCanopy);
    }

    // tempe
    VkCommandBuffer cmdBufTempe = cmdGen.createCommandBuffer();
    std::vector<VoxelTempe> voxelTempes(n_voxel, VoxelTempe{305, 295});
    for (int i = 0; i < n_voxel; ++i) {
        const int instanceId = voxelio->voxellinks[i].instanceId;
        const int meshId = instanceio->instanceLinks[instanceId].meshId;
        if (meshio->meshLinks[meshId].type == static_cast<int>(Type::WATER)) {
            voxelTempes[i] = VoxelTempe{298.15f, 298.15f};
        }
    }
    voxelio->m_pTempeBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufTempe, voxelTempes,
                                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufTempe);


    ///--------------------------------------------------------------------
    ///  Scene properties
    ///--------------------------------------------------------------------
    // obj models buffer
    for (int kmodel = 0; kmodel < meshio->primMeshes.size(); kmodel++)
    {
        nvvk::CommandPool cmdBufGet(m_device, m_queue.familyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queue.queue);
        VkCommandBuffer cmdBufModel = cmdBufGet.createCommandBuffer();
        MeshBuffer model;
        model.nbVertices = static_cast<uint32_t>(meshio->primMeshes[kmodel].nVertices);
        model.nbIndices = static_cast<uint32_t>(meshio->primMeshes[kmodel].nIndices);

        VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        model.vertexBuffer =
                m_pAlloc->createBuffer(cmdBufModel, meshio->primMeshes[kmodel].vertices, usage_);

        usage_ = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        model.indexBuffer =
                m_pAlloc->createBuffer(cmdBufModel, meshio->primMeshes[kmodel].indices, usage_);
        cmdBufGet.submitAndWait(cmdBufModel);
        m_pAlloc->finalizeAndReleaseStaging();
        meshio->m_bufferMeshes.emplace_back(model);
    }

    // modelLink
    VkCommandBuffer cmdBufModelLink = cmdGen.createCommandBuffer();
    for (int kmodel = 0; kmodel < meshio->primMeshes.size(); kmodel++)
    {
        meshio->meshLinks[kmodel].vertexAddress = nvvk::getBufferDeviceAddress(m_device, meshio->m_bufferMeshes[kmodel].vertexBuffer.buffer);
        meshio->meshLinks[kmodel].indexAddress = nvvk::getBufferDeviceAddress(m_device, meshio->m_bufferMeshes[kmodel].indexBuffer.buffer);
    }
    meshio->m_pBufferMeshLink = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufModelLink, meshio->meshLinks, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    cmdGen.submitAndWait(cmdBufModelLink);


    // instance link
    if (instanceio->instanceLinks.size() > 0)
    {
        VkCommandBuffer cmdBufInstanceLink = cmdGen.createCommandBuffer();
        VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        instanceio->m_pBufferInstanceLink = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufInstanceLink, instanceio->instanceLinks, usage_));
        cmdGen.submitAndWait(cmdBufInstanceLink);
    }

    // voxelLink and nano
    if (voxelio->nanoHandle.size() > 0)
    {
        // voxel link; ps: this is not voxel intance link, but the voxel link
        VkCommandBuffer cmdBufVoxelLink = cmdGen.createCommandBuffer();
        VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        voxelio->m_pVoxelLinkBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufVoxelLink, voxelio->voxellinks, usage_));
        cmdGen.submitAndWait(cmdBufVoxelLink);

        // voxel Nano
        VkCommandBuffer cmdBufVoxelNano = cmdGen.createCommandBuffer();
        voxelio->m_pVoxelNanoBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufVoxelNano, voxelio->nanoHandle.size(), voxelio->nanoHandle.data(),
                                                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        cmdGen.submitAndWait(cmdBufVoxelNano);
    }


    ///--------------------------------------------------------------------
    ///  Accelerate properties
    ///--------------------------------------------------------------------
    m_pAccelStruct->createAccelStruct(m_device,meshio,instanceio);

    ///--------------------------------------------------------------------
    ///  geometry properties
    ///--------------------------------------------------------------------
    VkCommandBuffer cmdBufSensor = cmdGen.createCommandBuffer();
    voxellstio->m_pBufferSensor = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufSensor, sizeof(SensorMatrix), &voxellstio->sensor,
                                                                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    cmdGen.submitAndWait(cmdBufSensor);

//    VkCommandBuffer cmdBufWaveInds = cmdGen.createCommandBuffer();
//    raytracingio->m_pBufferWaveInds = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufWaveInds, raytracingio->sensorWavelengthInds, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
//    cmdGen.submitAndWait(cmdBufWaveInds);

    VkCommandBuffer cmdBufLight = cmdGen.createCommandBuffer();
    voxellstio->m_pBufferLight = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufLight, sizeof(LightSet), &voxellstio->light,
                                                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    cmdGen.submitAndWait(cmdBufLight);

    // sensor wave
    if (voxellstio->waves.size() > 0)
    {
        VkCommandBuffer cmdBufWave = cmdGen.createCommandBuffer();
        voxellstio->m_pBufferWave = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufWave, voxellstio->waves, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
        cmdGen.submitAndWait(cmdBufWave);
    }

    // meteo and aero
    if (voxellstio->atomconds.size() > 0)
    {
        VkCommandBuffer cmdBufWave = cmdGen.createCommandBuffer();
        voxellstio->m_pBufferAtomcond = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufWave, voxellstio->atomconds, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
        cmdGen.submitAndWait(cmdBufWave);
    }

    // dir
    VkCommandBuffer cmdBufDir = cmdGen.createCommandBuffer();
    std::vector<VoxelDir> voxelDirs(n_voxel, VoxelDir{0, -1});
    voxelio->m_pDirBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufDir, voxelDirs,
                                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufDir);

    // rads
    VkCommandBuffer cmdBufRads = cmdGen.createCommandBuffer();
    std::vector<VoxelRad> voxelRads(n_voxel * DIFFUSENUM, VoxelRad{0, 0});
    voxelio->m_pRadsBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufRads, voxelRads,
                                                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufRads);

    // netRad
    VkCommandBuffer cmdBufNetRad = cmdGen.createCommandBuffer();
    std::vector<VoxelNetRad> voxelNetRads(n_voxel, VoxelNetRad{0, 0, 0, 0});
    voxelio->m_pNetRadBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufNetRad, voxelNetRads,
                                                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufNetRad);

    // pnet
    VkCommandBuffer cmdBufPnet = cmdGen.createCommandBuffer();
    std::vector<VoxelPnet> voxelPnets(n_voxel, VoxelPnet{0, 0});
    voxelio->m_pPnetBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufPnet, voxelPnets,
                                                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufPnet);

///--------------------------------------------------------------------
    ///  aero properties
    ///--------------------------------------------------------------------
    // aero
    auto cmdBufAero = cmdGen.createCommandBuffer();
    voxellstio->m_pBufferAero = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufAero, voxellstio->aeroconds,
                                                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufAero);

    ///--------------------------------------------------------------------
    ///  meteo properties
    ///--------------------------------------------------------------------
    // meteo
    auto cmdBufMeteo = cmdGen.createCommandBuffer();
    uint32_t size = sizeof(Meteo);
    // voxellstio->m_pMeteoBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufMeteo,sizeof(Meteo), &voxellstio->meteos[voxellstio->k_node],
    //                                                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    //                                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    voxellstio->m_pMeteoBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufMeteo,sizeof(Meteo), &voxellstio->meteos[voxellstio->k_node],
                                                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    cmdGen.submitAndWait(cmdBufMeteo);



    // surfL
//    VkCommandBuffer cmdBufSurfL = cmdGen.createCommandBuffer();
//    VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
//    voxelio->m_pSurfLBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufSurfL, voxelio->surfLs, usage_));
//    cmdGen.submitAndWait(cmdBufSurfL);

    // raa
    VkCommandBuffer cmdBufRaa = cmdGen.createCommandBuffer();
    std::vector<VoxelRaa> voxelRaas(n_voxel, VoxelRaa{0});
    voxelio->m_pRaaBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufRaa, voxelRaas,
                                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufRaa);

    ///--------------------------------------------------------------------
    ///  bio properties
    ///--------------------------------------------------------------------
    auto cmdBufBio = cmdGen.createCommandBuffer();
    // leafBio
    meshio->m_pLeafBioBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufBio, meshio->leafbios, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    // soilSet
    meshio->m_pSoilSetBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufBio, meshio->soilsets, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    // waterSet
    meshio->m_pWaterSetBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufBio, meshio->watersets, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    // air
    std::vector<VoxelAir> voxelair(n_voxel, VoxelAir{voxellstio->meteos[0].Ca, voxellstio->meteos[0].Oa, voxellstio->meteos[0].ea});
    // voxelio->m_pAirBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufBio, voxelair,
    //                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    //                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    voxelio->m_pAirBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufBio, voxelair,
                                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufBio);

    // rss
    voxelio->m_pRssBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(n_voxel * sizeof(VoxelRss),
                                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    ///--------------------------------------------------------------------
    ///  Evapo properties
    ///--------------------------------------------------------------------
    auto cmdBufEvapo = cmdGen.createCommandBuffer();
    // flux
    voxelio->m_pFluxBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(n_voxel * sizeof(VoxelHeatflux),
                                                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    // tLast
    std::vector<TLAST> tlast(n_voxel * TLASTNUM, TLAST{23, 23});
    for (int i = 0; i < n_voxel; ++i) {
        const float sunlit = voxelTempes[i].sunlit - 273.15f;
        const float shaded = voxelTempes[i].shaded - 273.15f;
        for (int k = 0; k < TLASTNUM; ++k) {
            tlast[i * TLASTNUM + k] = TLAST{sunlit, shaded};
        }
    }
    voxelio->m_pTLASTBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufEvapo, tlast,
                                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufEvapo);

    // state
    auto cmdBufState = cmdGen.createCommandBuffer();
    size = sizeof(EBState);
    // voxelio->m_pStateBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufState,
    //                                                          size, &voxelio->m_state,
    //                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    //                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    voxelio->m_pStateBuffer = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufState,
                                                             size, &voxelio->m_state,
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

    cmdGen.submitAndWait(cmdBufState);


    ///--------------------------------------------------------------------
    ///  Virtual Sceen properties
    ///--------------------------------------------------------------------
    /// storage
    VkCommandBuffer cmdBufStorage = cmdGen.createCommandBuffer();
    int outputSize = voxellstio->n_wave * voxellstio->imageSize.x * voxellstio->imageSize.y;
    std::vector<float> outputImage(outputSize, 0.0);
    virtualio->m_pBufferStorage = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufStorage, outputImage,
                                                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufStorage);


    VkCommandBuffer cmdBufLad = cmdGen.createCommandBuffer();
    surfio->m_pBufferLad = std::make_shared<nvvk::Buffer>(m_pAlloc->createBuffer(cmdBufLad, surfio->lads, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    cmdGen.submitAndWait(cmdBufLad);


    m_pAlloc->finalizeAndReleaseStaging();

    return false;

}



void Buffer::destroy(std::shared_ptr<VoxelebIO> &voxellstio){

    VkDevice & m_device = voxellstio->m_device;
    nvvk::Queue &m_queue  = voxellstio->m_queues[eGCT];
    auto & meshio = voxellstio->m_meshio;
    auto & instanceio = voxellstio->m_instanceio;
    auto & virtualio = voxellstio->m_virtualio;
    auto & m_pAlloc = voxellstio->m_pAlloc;
    auto & voxelio = voxellstio->m_voxelio;
    auto n_voxel = voxellstio->n_voxel;
    auto & m_pAccelStruct = voxellstio->m_pAccelStruct;
    auto & defined = voxellstio->m_defined;
    auto & surfio = voxellstio->m_surfio;


    if (meshio->spectrals.size() > 0)
    {
        m_pAlloc->destroy(*(meshio->m_pBufferSpectral));
    }

    m_pAlloc->destroy(*(meshio->m_pFixedSpectralBuffer));
//    m_pAlloc->destroy(*(voxelio->m_pSurfLBuffer));


    if (meshio->thermals.size() > 0) // no utilise the isTemperature
    {
        m_pAlloc->destroy(*(meshio->m_pBufferThermal));
    }
    m_pAlloc->destroy(*(voxelio->m_pTempeBuffer));

    if (meshio->canopies.size() > 0)
    {
        m_pAlloc->destroy(*(meshio->m_pBufferCanopy));
    }
    m_pAlloc->destroy(*(meshio->m_pBufferMeshLink));

    m_pAlloc->destroy(*(instanceio->m_pBufferInstanceLink));
    m_pAlloc->destroy(*(voxelio->m_pVoxelLinkBuffer));
    m_pAlloc->destroy(*(voxelio->m_pVoxelNanoBuffer));

    m_pAlloc->destroy(*(surfio->m_pBufferLad));

    m_pAlloc->destroy(*(voxellstio->m_pBufferSensor));
    m_pAlloc->destroy(*(voxellstio->m_pBufferWave));
   // m_pAlloc->destroy(*(voxellstio->m_pBufferWaveset));
    m_pAlloc->destroy(*(voxellstio->m_pBufferLight));

    for (int i = 0; i < meshio->m_bufferMeshes.size(); i++)
    {
        m_pAlloc->destroy((meshio->m_bufferMeshes[i].vertexBuffer));
        m_pAlloc->destroy((meshio->m_bufferMeshes[i].indexBuffer));
    }

    if (voxellstio->atomconds.size() > 0)
    {
        m_pAlloc->destroy(*(voxellstio->m_pBufferAtomcond));
    }

    m_pAlloc->destroy(*(voxelio->m_pDirBuffer));
    m_pAlloc->destroy(*(voxelio->m_pRadsBuffer));
    m_pAlloc->destroy(*(voxelio->m_pNetRadBuffer));
    m_pAlloc->destroy(*(voxelio->m_pPnetBuffer));
    m_pAlloc->destroy(*(virtualio->m_pBufferStorage));

    m_pAlloc->destroy(*(voxellstio->m_pMeteoBuffer));
    m_pAlloc->destroy(*(voxellstio->m_pBufferAero));

    m_pAlloc->destroy(*(voxelio->m_pRaaBuffer));
    m_pAlloc->destroy(*(meshio->m_pLeafBioBuffer));
    m_pAlloc->destroy(*(meshio->m_pSoilSetBuffer));
    m_pAlloc->destroy(*(meshio->m_pWaterSetBuffer));

    m_pAlloc->destroy(*(voxelio->m_pRssBuffer));
    m_pAlloc->destroy(*(voxelio->m_pAirBuffer));

    m_pAlloc->destroy(*(voxelio->m_pFluxBuffer));
    m_pAlloc->destroy(*(voxelio->m_pTLASTBuffer));
    m_pAlloc->destroy(*(voxelio->m_pStateBuffer));

    //m_rtBuilder.destroy();
}


