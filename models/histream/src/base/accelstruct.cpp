//
// Created by admin on 2024/1/24.
//

#include "accelstruct.h"

nvvk::RaytracingBuilderKHR::BlasInput AccelStruct::objectToVkGeometryKHR(const VkDevice device,
                                                                         const MeshBuffer& model)
{
    // Building part
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 att;
        glm::vec3 color;
        glm::vec2 texCoord;
    };

    // Building part
    VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = model.vertexBuffer.buffer;
    VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(device, &info);
    info.buffer = model.indexBuffer.buffer;
    VkDeviceAddress indexAddress = vkGetBufferDeviceAddress(device, &info);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = sizeof(Vertex);
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;
    triangles.maxVertex = model.nbVertices;

    // Setting up the build info of the acceleration
    VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeom.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;  // For AnyHit
    asGeom.geometry.triangles = triangles;

    VkAccelerationStructureBuildRangeInfoKHR offset{};
    offset.firstVertex = 0;
    offset.primitiveCount = model.nbIndices / 3;
    offset.primitiveOffset = 0;
    offset.transformOffset = 0;

    nvvk::RaytracingBuilderKHR::BlasInput input;
    input.asGeometry.emplace_back(asGeom);
    input.asBuildOffsetInfo.emplace_back(offset);

    return input;
}


bool AccelStruct::createAccelStruct(VkDevice device, std::shared_ptr<MeshIO> &meshio,
                                    std::shared_ptr<InstanceIO> &instanceio) {


//    auto & meshio = raytracingio->m_meshio;
//    auto & instanceio = raytracingio->m_instanceio;
//    auto & device = raytracingio->m_device;

    // Bottom level acceleration structure.
    std::vector<nvvk::RaytracingBuilderKHR::BlasInput> allBlas;
    allBlas.reserve(meshio->m_bufferMeshes.size());
    for (const auto& model : meshio->m_bufferMeshes)
    {
        auto blas = objectToVkGeometryKHR(device, model);
        allBlas.emplace_back(blas);
    }

    m_rtBuilder.buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                                   | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);

    // Top level acceleration structure.
    std::vector<VkAccelerationStructureInstanceKHR> allTlas;
    allTlas.reserve(instanceio->instances.size());
    for (int i = 0; i < static_cast<int>(instanceio->instances.size()); i++)
    {
        VkGeometryInstanceFlagsKHR flags{};
        flags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
        VkAccelerationStructureInstanceKHR rayInst{};
        rayInst.transform = nvvk::toTransformMatrixKHR(instanceio->instances[i].object2worldMatrix);  // Position of the instance
        rayInst.instanceCustomIndex = i;                       // gl_InstanceCustomIndexEXT
        rayInst.accelerationStructureReference = m_rtBuilder.getBlasDeviceAddress(instanceio->instances[i].meshId);
        rayInst.flags = flags;
        rayInst.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
        rayInst.mask = 0xFF;
        allTlas.emplace_back(rayInst);
    }
    m_rtBuilder.buildTlas(allTlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    return true;

}




