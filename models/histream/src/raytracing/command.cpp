//
// Created by admin on 2024/1/26.
//

#include "command.h"


Command::Command()
{

}


bool Command::create(std::shared_ptr<RaytracingIO> &raytracingio){
    // for the firest init
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(raytracingio->m_device, &fci, nullptr, &raytracingio->m_fence);

    // Here 4 means number of pipelines + 1
    raytracingio->m_semaphores.resize(2);
    std::generate_n( raytracingio->m_semaphores.begin(), 2,
                     [&]
                     {
                         VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                         VkSemaphore semaphore;
                         vkCreateSemaphore( raytracingio->m_device, &sci, nullptr, &semaphore);
                         return semaphore;
                     });

    return false;
}

bool Command::run(std::shared_ptr<RaytracingIO> &raytracingio){

    raytracingio->m_currentSemaphore = 1;

    submit(raytracingio,nullptr, nullptr);
    waitFence(raytracingio);

    return true;
}



//void Command::submit(std::shared_ptr<RaytracingIO> &raytracingio, const std::optional<std::vector<VkSemaphore>> &inSemaphore, const std::optional<VkSemaphore> &outSemaphore)
//{
//    auto & m_currentSemaphore = raytracingio->m_currentSemaphore;
//    // Preparing for the compute shader
//    VkCommandBuffer cmdBuf = raytracingio->m_genCmdBuf.createCommandBuffer();
//    recordCommandBuffer(cmdBuf,raytracingio);
//    std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
//    VkSubmitInfo submitInfoCompute{VK_STRUCTURE_TYPE_SUBMIT_INFO};
//
//    submitInfoCompute.waitSemaphoreCount = static_cast<uint32_t>(inSemaphore.has_value() ? inSemaphore.value().size() : 1);
//    submitInfoCompute.pWaitSemaphores = inSemaphore.has_value() ? inSemaphore.value().data() : & raytracingio->m_semaphores[m_currentSemaphore - 1];
//    submitInfoCompute.pWaitDstStageMask = waitStages.data();
//    submitInfoCompute.commandBufferCount = 1;
//    submitInfoCompute.pCommandBuffers = &cmdBuf;
//    submitInfoCompute.signalSemaphoreCount = 1;
//    submitInfoCompute.pSignalSemaphores = outSemaphore.has_value() ? &outSemaphore.value() : & raytracingio->m_semaphores[m_currentSemaphore];
//    // Dispatching the shader
//   // vkCmdDispatch(cmdBuf, dispatchSize.x, dispatchSize.y, dispatchSize.z);
//
//
//    vkEndCommandBuffer(cmdBuf);
//    vkQueueSubmit( raytracingio->m_queue, 1, &submitInfoCompute,  raytracingio->m_fence);
//    m_currentSemaphore++;
//}

void Command::submit(std::shared_ptr<RaytracingIO> &raytracingio,
                     const std::optional<VkSemaphore> &inSemaphore, const std::optional<VkSemaphore> &outSemaphore)
{
    auto & m_currentSemaphore = raytracingio->m_currentSemaphore;
    // Preparing for the compute shader
    VkCommandBuffer cmdBuf = raytracingio->m_genCmdBuf.createCommandBuffer();

//    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
//    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//    vkBeginCommandBuffer(cmdBuf, &beginInfo);


    recordCommandBuffer(cmdBuf,raytracingio);

    // new semaphores
//    std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
//    VkSubmitInfo submitInfoCompute{VK_STRUCTURE_TYPE_SUBMIT_INFO};
//    submitInfoCompute.waitSemaphoreCount =  static_cast<uint32_t>(1);
//    submitInfoCompute.pWaitSemaphores =  inSemaphore.has_value() ? &inSemaphore.value() : & raytracingio->m_semaphores[m_currentSemaphore - 1];
//    submitInfoCompute.pWaitDstStageMask = waitStages.data();
//    submitInfoCompute.commandBufferCount = 1;
//    submitInfoCompute.pCommandBuffers = &cmdBuf;
//    submitInfoCompute.signalSemaphoreCount = 1;
//    submitInfoCompute.pSignalSemaphores = outSemaphore.has_value() ? &outSemaphore.value() : & raytracingio->m_semaphores[m_currentSemaphore];

     // Dispatching the shader only for the other;
    // vkCmdDispatch(cmdBuf, dispatchSize.x, dispatchSize.y, dispatchSize.z);

    // old using only fense
    const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.pCommandBuffers = &cmdBuf;
    submitInfo.commandBufferCount = 1;

    vkEndCommandBuffer(cmdBuf);
    vkQueueSubmit( raytracingio->m_queue, 1, &submitInfo,  raytracingio->m_fence);
    m_currentSemaphore++;
}

void Command::recordCommandBuffer(const VkCommandBuffer& cmdBuf, std::shared_ptr<RaytracingIO> &raytracingio)
{
    auto &m_pipeline = raytracingio->m_pipeline;
    auto &m_pipelineLayout = raytracingio->m_pipelineLayout;
    auto &m_descSet = raytracingio->m_descSet;
    auto &m_setting = raytracingio->setting;
    auto & useSBTWrapper = raytracingio->useSBTWrapper;
    auto & m_rgenRegion = raytracingio->m_rgenRegion;
    auto & m_missRegion = raytracingio->m_missRegion;
    auto & m_hitRegion = raytracingio->m_hitRegion;
    auto & m_callRegion = raytracingio->m_callRegion;
//    auto & m_sceneio = raytracingio->m_sceneio;
    auto & m_sbtWrapper= raytracingio->m_sbtWrapper;
    VkExtent2D size;
    size.width = raytracingio->imageSize.x;
    size.height = raytracingio->imageSize.y;

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineLayout, 0,
                            static_cast<uint32_t>(1), &m_descSet, 0, nullptr);
    vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(RayRTSetting), &(raytracingio->setting));

    if (useSBTWrapper)
    {
        auto& regions = m_sbtWrapper.getRegions();
        vkCmdTraceRaysKHR(cmdBuf, &regions[0], &regions[1], &regions[2], &regions[3], size.width, size.height, 1);
    }
    else
    {
        vkCmdTraceRaysKHR(cmdBuf, &m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion, size.width, size.height, 1);
    }

//
//    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
//    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0,
//                            static_cast<uint32_t>(1), &m_descSet, 0, nullptr);
//    // Sending the push constant information
//    vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VoxelLstSetting), &m_setting);
}

void Command::waitFence(std::shared_ptr<RaytracingIO> &raytracingio)
{
    vkWaitForFences(raytracingio->m_device, 1, &(raytracingio->m_fence), VK_TRUE, UINT64_MAX);
    vkResetFences(raytracingio->m_device, 1,  &(raytracingio->m_fence));
}


void Command::destroy(std::shared_ptr<RaytracingIO> &raytracingio) {


    vkDestroyFence(raytracingio->m_device, raytracingio->m_fence, nullptr);
       // vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_commandBuffers[i]);

    for (auto& semaphore : raytracingio->m_semaphores)
    {
        vkDestroySemaphore(raytracingio->m_device, semaphore, nullptr);
    }

}
