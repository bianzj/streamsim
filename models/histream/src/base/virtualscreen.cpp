//
// Created by admin on 2024/1/24.
//

#include "virtualscreen.h"


bool VirtualScreen::bufferToBuffer(std::shared_ptr<RaytracingIO> & raytracingio,
                                   const nvvk::Buffer& bufferIn, VkDeviceSize size, const nvvk::Buffer& bufferOut)
{
    VkDevice &m_device = raytracingio->m_device;
    int m_queueFamilyIndex = raytracingio->m_queueIndex;

    nvvk::CommandPool genCmdBuf((vk::Device)m_device, m_queueFamilyIndex);
    vk::CommandBuffer cmdBuff = genCmdBuf.createCommandBuffer();
    //// Copy the image to the buffer
    vk::BufferCopy copyRegion;
    copyRegion.setSrcOffset(0);
    copyRegion.setDstOffset(0);
    copyRegion.setSize(size);
    cmdBuff.copyBuffer(bufferIn.buffer, bufferOut.buffer, copyRegion);
    genCmdBuf.submitAndWait(cmdBuff);

    return true;
}

bool VirtualScreen::bufferToBuffer(std::shared_ptr<VoxelebIO> & modelio,
                                   const nvvk::Buffer& bufferIn, VkDeviceSize size, const nvvk::Buffer& bufferOut)
{
    VkDevice &m_device = modelio->m_device;
    int m_queueFamilyIndex = modelio->m_queueIndex;

    nvvk::CommandPool genCmdBuf((vk::Device)m_device, m_queueFamilyIndex);
    vk::CommandBuffer cmdBuff = genCmdBuf.createCommandBuffer();
    // The producer and readback use the same VoxelEB queue family. A fence
    // completes execution, but resource accesses still need memory dependencies.
    VkBufferMemoryBarrier sourceBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    sourceBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    sourceBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    sourceBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    sourceBarrier.buffer = bufferIn.buffer;
    sourceBarrier.offset = 0;
    sourceBarrier.size = size;
    vkCmdPipelineBarrier(cmdBuff,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &sourceBarrier, 0, nullptr);

    vk::BufferCopy copyRegion;
    copyRegion.setSrcOffset(0);
    copyRegion.setDstOffset(0);
    copyRegion.setSize(size);
    cmdBuff.copyBuffer(bufferIn.buffer, bufferOut.buffer, copyRegion);

    VkBufferMemoryBarrier hostBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostBarrier.buffer = bufferOut.buffer;
    hostBarrier.offset = 0;
    hostBarrier.size = size;
    vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &hostBarrier, 0, nullptr);
    // Explicitly submit on the producer queue, including a nonzero queue index.
    genCmdBuf.submitAndWait(cmdBuff, modelio->m_queue);

    return true;
}

bool VirtualScreen::bufferToBuffer(std::shared_ptr<VoxelrtIO> & modelio,
                                   const nvvk::Buffer& bufferIn, VkDeviceSize size, const nvvk::Buffer& bufferOut)
{
    VkDevice &m_device = modelio->m_device;
    int m_queueFamilyIndex = modelio->m_queueIndex;

    nvvk::CommandPool genCmdBuf((vk::Device)m_device, m_queueFamilyIndex);
    vk::CommandBuffer cmdBuff = genCmdBuf.createCommandBuffer();
    //// Copy the image to the buffer
    vk::BufferCopy copyRegion;
    copyRegion.setSrcOffset(0);
    copyRegion.setDstOffset(0);
    copyRegion.setSize(size);
    cmdBuff.copyBuffer(bufferIn.buffer, bufferOut.buffer, copyRegion);
    genCmdBuf.submitAndWait(cmdBuff);

    return true;
}


