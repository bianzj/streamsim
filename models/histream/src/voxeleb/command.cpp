//
// Created by admin on 2024/1/26.
//

#include "command.h"
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

struct ScatteringAggregate {
    uint64_t maxStepTruncations{0}, transmittanceStops{0}, tracedRays{0}, reserved{0};
    double absorbedShortwave{0}, absorbedPar{0}, transmittanceResidual{0}, truncatedResidual{0};
};

void clearEnergyState(const std::shared_ptr<VoxelebIO>& modelio)
{
    nvvk::CommandPool commandPool(modelio->m_device, modelio->m_queueIndex);
    vk::CommandBuffer command = commandPool.createCommandBuffer();
    vkCmdFillBuffer(command, modelio->m_voxelio->m_pStateBuffer->buffer,
                    0, sizeof(EBState), 0);
    commandPool.submitAndWait(command);
}

uint32_t readEnergyState(const std::shared_ptr<VoxelebIO>& modelio,
                         const nvvk::Buffer& readback)
{
    nvvk::CommandPool commandPool(modelio->m_device, modelio->m_queueIndex);
    vk::CommandBuffer command = commandPool.createCommandBuffer();
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    VkBufferCopy copy{};
    copy.size = sizeof(EBState);
    vkCmdCopyBuffer(command, modelio->m_voxelio->m_pStateBuffer->buffer,
                    readback.buffer, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    commandPool.submitAndWait(command);

    const void* mapped = modelio->m_pAlloc->map(readback);
    const uint32_t count = *static_cast<const uint32_t*>(mapped);
    modelio->m_pAlloc->unmap(readback);
    return count;
}

void resetScatteringStats(const std::shared_ptr<VoxelebIO>& modelio)
{
    nvvk::CommandPool pool(modelio->m_device, modelio->m_queueIndex);
    VkCommandBuffer command = pool.createCommandBuffer();
    vkCmdFillBuffer(command, modelio->m_pScatteringStats->buffer, 0,
                    4 * sizeof(ScatteringOrderStats), 0);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    pool.submitAndWait(command);
}

std::array<ScatteringOrderStats, 4> readScatteringStats(
    const std::shared_ptr<VoxelebIO>& modelio)
{
    std::array<ScatteringOrderStats, 4> stats{};
    const VkDeviceSize bytes = sizeof(stats);
    nvvk::Buffer staging = modelio->m_pAlloc->createBuffer(bytes,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    nvvk::CommandPool pool(modelio->m_device, modelio->m_queueIndex);
    VkCommandBuffer command = pool.createCommandBuffer();
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    VkBufferCopy copy{};
    copy.size = bytes;
    vkCmdCopyBuffer(command, modelio->m_pScatteringStats->buffer, staging.buffer, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    pool.submitAndWait(command);
    const void* mapped = modelio->m_pAlloc->map(staging);
    std::memcpy(stats.data(), mapped, static_cast<size_t>(bytes));
    modelio->m_pAlloc->unmap(staging);
    modelio->m_pAlloc->destroy(staging);
    return stats;
}

void writeScatteringStats(const std::shared_ptr<VoxelebIO>& modelio,
    const std::array<ScatteringAggregate, 4>& stats, double milliseconds,
    const std::array<double, 3>& orderMilliseconds, bool legacy)
{
    const auto directory = std::filesystem::path(modelio->projectDir) / "diagnostics";
    std::filesystem::create_directories(directory);
    const auto file = directory / ("scattering_node=" + std::to_string(modelio->k_node) + ".json");
    std::ofstream output(file);
    if (!output) throw std::runtime_error("Cannot write scattering diagnostics: " + file.string());
    const auto jsonNumber = [](double value) {
        if (!std::isfinite(value)) return std::string("null");
        std::ostringstream text;
        text << std::setprecision(10) << value;
        return text.str();
    };
    output << std::setprecision(10)
        << "{\n  \"format\": \"streamsim-shortwave-scattering-v1\",\n"
        << "  \"node\": " << modelio->k_node << ",\n"
        << "  \"legacy\": " << (legacy ? "true" : "false") << ",\n"
        << "  \"requestedOrders\": " << modelio->shortwaveScatteringOrders << ",\n"
        << "  \"completedOrders\": " << (legacy ? 1 : modelio->shortwaveScatteringOrders) << ",\n"
        << "  \"orderContributionEarlyStop\": false,\n"
        << "  \"orderDefinition\": \"" << (legacy
            ? "legacy sky order 0 plus solar order 1"
            : "sky orders 0..K plus solar orders 1..K; direct solar absorption is separate") << "\",\n"
        << "  \"voxelCount\": " << modelio->n_voxel << ",\n"
        << "  \"directions\": 64,\n"
        << "  \"maxRaySteps\": " << modelio->setting.maxStep << ",\n"
        << "  \"transmittanceCutoff\": 0.01,\n"
        << "  \"elapsedMs\": " << milliseconds << ",\n"
        << "  \"statsAvailable\": " << (legacy ? "false" : "true") << ",\n"
        << "  \"statsScope\": \"diffuse shortwave paths only; solar visibility is not counted\",\n"
        << "  \"counterAggregation\": \"GPU counters reset per spectral group; CPU uint64 totals\",\n"
        << "  \"residualAggregation\": \"sum of dimensionless ray throughputs at cutoff; not omitted energy\",\n"
        << "  \"fluxAggregation\": \"sum of per-voxel flux densities; not total domain power\",\n"
        << "  \"perOrder\": [\n";
    bool valid = true;
    for (int index = 0; index < 4; ++index) {
        const auto& value = stats[index];
        valid = valid && value.reserved == 0 && std::isfinite(value.absorbedShortwave) &&
            std::isfinite(value.absorbedPar) && std::isfinite(value.transmittanceResidual) &&
            std::isfinite(value.truncatedResidual);
        output << "    {\"order\": " << (index == 3 ? 0 : index + 1)
            << ", \"elapsedMs\": " << (index == 3 ? 0.0 : orderMilliseconds[index])
            << ", \"maxStepTruncations\": " << value.maxStepTruncations
            << ", \"transmittanceStops\": " << value.transmittanceStops
            << ", \"tracedRays\": " << value.tracedRays
            << ", \"invalidValues\": " << value.reserved
            << ", \"absorbedShortwave\": " << jsonNumber(value.absorbedShortwave)
            << ", \"absorbedPar\": " << jsonNumber(value.absorbedPar)
            << ", \"transmittanceResidual\": " << jsonNumber(value.transmittanceResidual)
            << ", \"truncatedResidual\": " << jsonNumber(value.truncatedResidual) << "}"
            << (index == 3 ? "\n" : ",\n");
    }
    output << "  ],\n  \"valid\": " << (valid ? "true" : "false") << "\n}\n";
    output.close();
    std::cout << "SHORTWAVE_SCATTERING\tnode=" << modelio->k_node
        << "\trequested=" << modelio->shortwaveScatteringOrders
        << "\tcompleted=" << (legacy ? 1 : modelio->shortwaveScatteringOrders)
        << "\tlegacy=" << (legacy ? 1 : 0) << "\telapsedMs=" << milliseconds
        << "\tdiagnostics=" << file.string() << std::endl;
    if (!valid) throw std::runtime_error("Invalid shortwave scattering state: " + file.string());
}

} // namespace




bool Command::create(std::shared_ptr<VoxelebIO> &modelio){
    // for the firest init
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(modelio->m_device, &fci, nullptr, &modelio->m_fence);
//    vkWaitForFences(modelio->m_device, 1, &(modelio->m_fence), VK_TRUE, UINT64_MAX);
    vkResetFences(modelio->m_device, 1,  &(modelio->m_fence));

    // Here 4 means number of pipelines + 1
    modelio->m_semaphores.resize(modelio->n_pipeline+1);
    std::generate_n( modelio->m_semaphores.begin(), modelio->n_pipeline+1,
                     [&]
                     {
                         VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                         VkSemaphore semaphore;
                         vkCreateSemaphore( modelio->m_device, &sci, nullptr, &semaphore);
                         return semaphore;
                     });


    // if (vkCreateFence(modelio->m_device, &fci, nullptr, &modelio->m_fence) != VK_SUCCESS)
    //     return false;

    return true;
}

bool Command::runEB(std::shared_ptr<VoxelebIO> &modelio){

    modelio->m_currentSemaphore = 1;

    glm::ivec3 voxelSize1D = glm::ivec3((modelio->n_voxel + (GROUP_SIZEX - 1)) / GROUP_SIZEX, 1, 1);
    auto & descSet = modelio->m_descSet;
    auto & setting = modelio->setting;


    //--------------------------------------------------

    int stageInt = 0;

    std::vector<VkSemaphore> beginSemaph;
    beginSemaph.emplace_back(modelio->m_semaphores[0]);

    submit(modelio, VoxelEBStage::gap, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);

    submit(modelio, VoxelEBStage::directVNIR, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);

    const auto shortwaveBegin = std::chrono::steady_clock::now();
    std::array<double, 3> orderMilliseconds{};
    std::array<ScatteringAggregate, 4> scatteringStats{};
    if (modelio->newShortwaveScattering) {
        if (uint64_t(modelio->n_voxel) * 64 > UINT32_MAX) {
            throw std::runtime_error("One spectral group exceeds the scattering diagnostic counter capacity");
        }
        setting.scatteringStage = 0;
        submit(modelio, VoxelEBStage::scatteringVNIR, voxelSize1D, std::nullopt, std::nullopt);
        waitFence(modelio);
        // Each spectral group owns the entire directional field until all its
        // orders finish. Reusing two fields keeps memory independent of K.
        const int width = std::max(setting.n_jump, 1);
        for (int first = 0; first < 2001;) {
            resetScatteringStats(modelio);
            int end = std::min(first + width, 2001);
            // PAR includes 700 nm: indices 0..300 form one integration domain.
            if (first < 301 && end > 301) end = 301;
            setting.scatteringBandStart = first;
            setting.scatteringBandEnd = end;
            setting.scatteringStage = 1;
            setting.scatteringOrder = 0;
            submit(modelio, VoxelEBStage::scatteringVNIR, voxelSize1D, std::nullopt, std::nullopt);
            waitFence(modelio);
            for (int order = 1; order <= modelio->shortwaveScatteringOrders; ++order) {
                const auto orderBegin = std::chrono::steady_clock::now();
                setting.scatteringOrder = order;
                setting.scatteringStage = 2;
                submit(modelio, VoxelEBStage::scatteringVNIR, voxelSize1D, std::nullopt, std::nullopt);
                waitFence(modelio);
                setting.scatteringStage = 3;
                submit(modelio, VoxelEBStage::scatteringVNIR, voxelSize1D, std::nullopt, std::nullopt);
                waitFence(modelio);
                orderMilliseconds[order - 1] += std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - orderBegin).count();
            }
            // Reset/read per group so 2001-band jobs cannot wrap uint32 GPU
            // counters. The inexpensive 128-byte transfer feeds uint64 totals.
            const auto groupStats = readScatteringStats(modelio);
            for (size_t index = 0; index < scatteringStats.size(); ++index) {
                auto& total = scatteringStats[index];
                const auto& value = groupStats[index];
                total.maxStepTruncations += value.maxStepTruncations;
                total.transmittanceStops += value.transmittanceStops;
                total.tracedRays += value.tracedRays;
                total.reserved += value.reserved;
                total.absorbedShortwave += value.absorbedShortwave;
                total.absorbedPar += value.absorbedPar;
                total.transmittanceResidual += value.transmittanceResidual;
                total.truncatedResidual += value.truncatedResidual;
            }
            first = end;
        }
    } else {
        submit(modelio, VoxelEBStage::diffuseVNIR, voxelSize1D, std::nullopt, std::nullopt);
        waitFence(modelio);
    }
    const double shortwaveMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - shortwaveBegin).count();
    writeScatteringStats(modelio, scatteringStats, shortwaveMs, orderMilliseconds,
        !modelio->newShortwaveScattering);

    submit(modelio, VoxelEBStage::directTIR, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);

    nvvk::Buffer stateReadback = modelio->m_pAlloc->createBuffer(
        sizeof(EBState), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);


    setting.energyFinalize = 0;
    modelio->energyIterations = 0;
    modelio->energyFinalUnconvergedStateCount = 0;
    modelio->energyFinalized = false;
    for(int kiter = 0;kiter < 50; kiter++) {


        submit(modelio, VoxelEBStage::diffuseTIR, voxelSize1D, std::nullopt, std::nullopt);
        waitFence(modelio);

        //--------------------------------------------------


        submit(modelio, VoxelEBStage::aero, voxelSize1D, std::nullopt, std::nullopt);
        waitFence(modelio);

        //--------------------------------------------------

        submit(modelio, VoxelEBStage::bio, voxelSize1D, std::nullopt, std::nullopt);
        waitFence(modelio);


        //--------------------------------------------------


        submit(modelio, VoxelEBStage::evapo, voxelSize1D, std::nullopt, std::nullopt);
        waitFence(modelio);

        //--------------------------------------------------
        clearEnergyState(modelio);

        submit(modelio, VoxelEBStage::budget, voxelSize1D, std::nullopt, std::nullopt);
        waitFence(modelio);
        modelio->energyIterations = kiter + 1;
        if(kiter >= 2 && readEnergyState(modelio, stateReadback) == 0U) {
            break;
        }



        //--------------------------------------------------

    }
    // budget updated temperature last. Refresh radiation, physiology and
    // exchange fluxes at that final temperature before advancing history.
    // The final budget dispatch only writes G/checks residuals; it cannot
    // make another temperature update or advance the physical time node.
    submit(modelio, VoxelEBStage::diffuseTIR, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);
    submit(modelio, VoxelEBStage::aero, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);
    submit(modelio, VoxelEBStage::bio, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);
    submit(modelio, VoxelEBStage::evapo, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);
    clearEnergyState(modelio);
    setting.energyFinalize = 1;
    submit(modelio, VoxelEBStage::budget, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);
    modelio->energyFinalUnconvergedStateCount = readEnergyState(modelio, stateReadback);
    modelio->energyFinalized = true;
    setting.energyFinalize = 0;
    modelio->m_pAlloc->destroy(stateReadback);

    submit(modelio, VoxelEBStage::updateTp, voxelSize1D, std::nullopt, std::nullopt);
    waitFence(modelio);

    return true;
}


bool Command::runRT(std::shared_ptr<VoxelebIO> &modelio){

    modelio->m_currentSemaphore = 1;

    glm::ivec3 voxelSize2D = glm::ivec3((modelio->setting.imageSize.x + (GROUP_SIZEXY - 1)) / GROUP_SIZEXY,
                                        (modelio->setting.imageSize.y + (GROUP_SIZEXY - 1)) / GROUP_SIZEXY, 1);
    auto & descSet = modelio->m_descSet;
    auto & setting = modelio->setting;


    //--------------------------------------------------

//    nvmath::vec3i size = nvmath::vec3i((m_setting.size.x + (GROUP_SIZEXY - 1)) / GROUP_SIZEXY,
//                                       (m_setting.size.y + (GROUP_SIZEXY - 1)) / GROUP_SIZEXY, 1);

    submit(modelio, VoxelEBStage::out, voxelSize2D, std::nullopt, std::nullopt);
    waitFence(modelio);



    return true;
}

bool Command::runFluid(std::shared_ptr<VoxelebIO> &modelio, int iterations)
{
    if (!modelio->fluid.enabled || modelio->fluidCellCount == 0 || iterations <= 0) return true;
    const glm::ivec3 dispatch(
        static_cast<int>((modelio->fluidCellCount + GROUP_SIZEX - 1) / GROUP_SIZEX), 1, 1);

    // Keep command buffers short enough for Windows' GPU watchdog while
    // avoiding a queue submission and fence wait for every LBM pass.
    constexpr int iterationsPerBatch = 32;
    nvvk::CommandPool commandPool(modelio->m_device, modelio->m_queueIndex);
    for (int first = 0; first < iterations; first += iterationsPerBatch) {
        const int batchSize = std::min(iterationsPerBatch, iterations - first);
        VkCommandBuffer command = commandPool.createCommandBuffer();
        VkMemoryBarrier memoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        for (int iteration = 0; iteration < batchSize; ++iteration) {
            recordCommandBuffer(command, modelio->m_descSet, modelio->m_pipelineLayout,
                                modelio->m_pipelines[VoxelEBStage::fluidLbm], modelio->setting);
            vkCmdDispatch(command, dispatch.x, dispatch.y, dispatch.z);
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                                 1, &memoryBarrier, 0, nullptr, 0, nullptr);

            recordCommandBuffer(command, modelio->m_descSet, modelio->m_pipelineLayout,
                                modelio->m_pipelines[VoxelEBStage::fluidCommit], modelio->setting);
            vkCmdDispatch(command, dispatch.x, dispatch.y, dispatch.z);
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                                 1, &memoryBarrier, 0, nullptr, 0, nullptr);
        }

        vkEndCommandBuffer(command);
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &command;
        vkQueueSubmit(modelio->m_queue, 1, &submitInfo, modelio->m_fence);
        waitFence(modelio);
    }
    return true;
}

//bool Command::run(std::shared_ptr<VoxelebIO> &modelio){
//
//    modelio->m_currentSemaphore = 1;
//
//    glm::ivec3 voxelSize1D = glm::ivec3((modelio->n_voxel + (GROUP_SIZEX - 1)) / GROUP_SIZEX, 1, 1);
//    auto & descSet = modelio->m_descSet;
//    auto & setting = modelio->setting;
//
//
//    auto & piplineLayout_rt = modelio->m_pipelineLayout_rt;
//    auto & piplines_rt = modelio->m_pipelines_rt;
//    auto & piplineLayout_eb = modelio->m_pipelineLayout_eb;
//    auto & piplines_eb = modelio->m_pipelines_eb;
//    auto & piplineLayout_et = modelio->m_pipelineLayout_et;
//    auto & piplines_et = modelio->m_pipelines_et;
//    auto & piplineLayout_bio = modelio->m_pipelineLayout_bio;
//    auto & pipline_bio = modelio->m_pipeline_bio;
//    auto & piplineLayout_aero = modelio->m_pipelineLayout_aero;
//    auto & pipline_aero = modelio->m_pipeline_aero;
//    //--------------------------------------------------
//
//    int stageInt = 0;
//
//    stageInt = (int)VoxelRadStage::gap;
//    submit(modelio, voxelSize1D,descSet,piplineLayout_rt,piplines_rt[stageInt],setting,nullptr, nullptr);
//    waitFence(modelio);
//
//    stageInt = (int)VoxelRadStage::directVNIR;
//    submit(modelio, voxelSize1D,descSet,piplineLayout_rt,piplines_rt[stageInt],setting,nullptr, nullptr);
//    waitFence(modelio);
//
//    stageInt = (int)VoxelRadStage::diffuseVNIR;
//    submit(modelio, voxelSize1D,descSet,piplineLayout_rt,piplines_rt[stageInt],setting,nullptr, nullptr);
//    waitFence(modelio);
//
//
//
//
//    //--------------------------------------------------
//
//
//    submit(modelio, voxelSize1D,descSet,piplineLayout_bio,pipline_bio,setting,nullptr, nullptr);
//    waitFence(modelio);
//
//    //--------------------------------------------------
//
//    submit(modelio, voxelSize1D,descSet,piplineLayout_aero,pipline_aero,setting,nullptr, nullptr);
//    waitFence(modelio);
//
//
//    //--------------------------------------------------
//
//
//    stageInt = (int)ETStage::evapo;
//    submit(modelio, voxelSize1D,descSet,piplineLayout_et,piplines_et[stageInt],setting,nullptr, nullptr);
//    waitFence(modelio);
//
//    //--------------------------------------------------
//
//    stageInt = (int)EBStage::budget;
//    submit(modelio, voxelSize1D,descSet,piplineLayout_eb,piplines_eb[stageInt],setting,nullptr, nullptr);
//    waitFence(modelio);
//
//
//    //--------------------------------------------------
//
//
//
//
//
//    return true;
//}


void Command::submit(std::shared_ptr<VoxelebIO> &modelio, VoxelEBStage stage, glm::ivec3 dispatchSize,
                     const std::optional<std::vector<VkSemaphore>> &inSemaphores, const std::optional<VkSemaphore> &outSemaphore)
{

    auto & descSet = modelio->m_descSet;
    auto & setting = modelio->setting;
    auto & pipelineLayout = modelio->m_pipelineLayout;
    auto & pipeline = modelio->m_pipelines[stage];
    if (std::getenv("STREAMSIM_GPU_DIAGNOSTICS")) std::cout << "GPU_STAGE\t" << static_cast<int>(stage) << std::endl;


    auto & m_currentSemaphore = modelio->m_currentSemaphore;
    // Preparing for the compute shader
    VkCommandBuffer cmdBuf = modelio->m_genCmdBuf.createCommandBuffer();

//    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
//    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//    beginInfo.pNext = nullptr;
//    beginInfo.pInheritanceInfo = nullptr;
//    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // Queue/fence completion alone does not make previous SSBO writes visible
    // to this dispatch. This also orders the previous scattering pass globally.
    VkMemoryBarrier memoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    // Dispatching the shader only for the other;
    recordCommandBuffer(cmdBuf, descSet, pipelineLayout, pipeline, setting );
    vkCmdDispatch(cmdBuf, dispatchSize.x, dispatchSize.y, dispatchSize.z);

    auto & semaphores = modelio->m_semaphores;
    // new semaphores
    std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
    VkSubmitInfo submitInfoCompute{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfoCompute.commandBufferCount = 1;
    submitInfoCompute.pCommandBuffers = &cmdBuf;
//    submitInfoCompute.waitSemaphoreCount =  static_cast<uint32_t>(1);
//    submitInfoCompute.pWaitSemaphores =  inSemaphores.has_value() ? inSemaphores->data() : &semaphores[m_currentSemaphore-1];
//    submitInfoCompute.pWaitDstStageMask = waitStages.data();
//    submitInfoCompute.signalSemaphoreCount = 1;
//    submitInfoCompute.pSignalSemaphores = outSemaphore.has_value() ? &outSemaphore.value() : & semaphores[m_currentSemaphore];


    vkEndCommandBuffer(cmdBuf);
    vkQueueSubmit( modelio->m_queue, 1, &submitInfoCompute,  modelio->m_fence);
    m_currentSemaphore++;
}


//void Command::submit(std::shared_ptr<VoxelebIO> &modelio,glm::ivec3 dispatchSize,
//                     VkDescriptorSet descSet, VkPipelineLayout pipelineLayout,
//                     VkPipeline pipeline, VoxelLstSetting setting,
//                     const std::optional<VkSemaphore> &inSemaphore, const std::optional<VkSemaphore> &outSemaphore)
//{
//    auto & m_currentSemaphore = modelio->m_currentSemaphore;
//    // Preparing for the compute shader
//    VkCommandBuffer cmdBuf = modelio->m_genCmdBuf.createCommandBuffer();
//
////    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
////    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
////    vkBeginCommandBuffer(cmdBuf, &beginInfo);
//
//    // Dispatching the shader only for the other;
//    recordCommandBuffer(cmdBuf, descSet, pipelineLayout, pipeline, setting );
//    vkCmdDispatch(cmdBuf, dispatchSize.x, dispatchSize.y, dispatchSize.z);
//
//    // new semaphores
//    std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
//    VkSubmitInfo submitInfoCompute{VK_STRUCTURE_TYPE_SUBMIT_INFO};
//    submitInfoCompute.waitSemaphoreCount =  static_cast<uint32_t>(1);
//    submitInfoCompute.pWaitSemaphores =  inSemaphore.has_value() ? &inSemaphore.value() : & modelio->m_semaphores[m_currentSemaphore - 1];
//    submitInfoCompute.pWaitDstStageMask = waitStages.data();
//    submitInfoCompute.commandBufferCount = 1;
//    submitInfoCompute.pCommandBuffers = &cmdBuf;
//    submitInfoCompute.signalSemaphoreCount = 1;
//    submitInfoCompute.pSignalSemaphores = outSemaphore.has_value() ? &outSemaphore.value() : & modelio->m_semaphores[m_currentSemaphore];
//
//
//
//    // old using only fense
////    const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
////    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
////    submitInfo.pWaitDstStageMask = &waitStageMask;
////    submitInfo.pCommandBuffers = &cmdBuf;
////    submitInfo.commandBufferCount = 1;
//
//    vkEndCommandBuffer(cmdBuf);
//    vkQueueSubmit( modelio->m_queue, 1, &submitInfoCompute,  modelio->m_fence);
//    m_currentSemaphore++;
//}

//void Command::recordCommandBuffer(VkCommandBuffer cmdBuf, VkDescriptorSet descSet, VkPipelineLayout pipelineLayout,
//                     std::map<VoxelEBStage, VkPipeline> pipelines, VoxelEBStage stage, VoxelLstSetting setting)
//{
//    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[stage]);
//    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0,
//                            static_cast<uint32_t>(1), &descSet, 0, nullptr);
//    // Sending the push constant information
//    vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VoxelLstSetting), &setting);
//}

void Command::recordCommandBuffer(VkCommandBuffer cmdBuf, VkDescriptorSet descSet, VkPipelineLayout pipelineLayout,
                                  VkPipeline pipeline,  VoxelLstSetting setting)
{
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0,
                            static_cast<uint32_t>(1), &descSet, 0, nullptr);
    // Sending the push constant information
    vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VoxelLstSetting), &setting);
}



void Command::waitFence(std::shared_ptr<VoxelebIO> &modelio)
{
    const VkResult result = vkWaitForFences(modelio->m_device, 1, &(modelio->m_fence), VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) throw std::runtime_error("VoxelEB GPU execution failed: " + std::to_string(result));
    vkResetFences(modelio->m_device, 1,  &(modelio->m_fence));
}


void Command::destroy(std::shared_ptr<VoxelebIO> &modelio) {


    vkDestroyFence(modelio->m_device, modelio->m_fence, nullptr);
       // vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_commandBuffers[i]);

    for (auto& semaphore : modelio->m_semaphores)
    {
        vkDestroySemaphore(modelio->m_device, semaphore, nullptr);
    }

}
