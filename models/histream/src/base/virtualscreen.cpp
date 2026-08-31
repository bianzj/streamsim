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
    //// Copy the image to the buffer
    vk::BufferCopy copyRegion;
    copyRegion.setSrcOffset(0);
    copyRegion.setDstOffset(0);
    copyRegion.setSize(size);
    cmdBuff.copyBuffer(bufferIn.buffer, bufferOut.buffer, copyRegion);
    genCmdBuf.submitAndWait(cmdBuff);

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


