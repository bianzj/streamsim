#include "facetrt_vulkan.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace facetvk {
namespace {

constexpr uint64_t kEmptyEdgeKey = std::numeric_limits<uint64_t>::max();

constexpr VkShaderStageFlags kVisibilityPushStages = VK_SHADER_STAGE_VERTEX_BIT |
                                                     VK_SHADER_STAGE_FRAGMENT_BIT |
                                                     VK_SHADER_STAGE_COMPUTE_BIT;
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                               VkDebugUtilsMessageTypeFlagsEXT,
                                               const VkDebugUtilsMessengerCallbackDataEXT* data,
                                               void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        // Validation output is intentionally concise; fatal API errors throw separately.
        std::fprintf(stderr, "[Vulkan] %s\n", data->pMessage);
    }
    return VK_FALSE;
}

bool hasLayer(const char* name)
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    return std::any_of(layers.begin(), layers.end(), [name](const VkLayerProperties& layer) {
        return std::strcmp(layer.layerName, name) == 0;
    });
}

float dot3(const std::array<float, 3>& a, const std::array<float, 3>& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

std::array<float, 3> cross3(const std::array<float, 3>& a, const std::array<float, 3>& b)
{
    return {a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

std::array<float, 3> normalize3(std::array<float, 3> value)
{
    const float length = std::sqrt(dot3(value, value));
    if (!(length > 1.0e-8f)) {
        throw std::invalid_argument("A visibility direction has zero length");
    }
    for (float& component : value) {
        component /= length;
    }
    return value;
}

} // namespace

FacetrtVulkan::~FacetrtVulkan()
{
    destroy();
}

void FacetrtVulkan::require(VkResult result, const char* operation) const
{
    if (result != VK_SUCCESS) {
        std::ostringstream stream;
        stream << operation << " failed with VkResult " << static_cast<int>(result);
        throw std::runtime_error(stream.str());
    }
}

void FacetrtVulkan::initialize(const Config& config, const std::string& shaderDirectory)
{
    destroy();
    if (config.rasterWidth == 0 || config.rasterHeight == 0) {
        throw std::invalid_argument("Raster dimensions must be positive");
    }
    if (config.maxFragmentsPerPixel == 0 || config.maxFragmentsPerPixel > 64) {
        throw std::invalid_argument("maxFragmentsPerPixel must be in [1, 64]");
    }
    m_config = config;
    m_shaderDirectory = shaderDirectory;
    createContext();
    createCommandPool();
}

void FacetrtVulkan::createContext()
{
    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    const bool useValidation = m_config.enableValidation && hasLayer(validationLayer);

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "facet-vulkan-radiosity";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "radiosity";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> layers;
    std::vector<const char*> extensions;
    if (useValidation) {
        layers.push_back(validationLayer);
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instanceInfo.ppEnabledLayerNames = layers.data();
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    require(vkCreateInstance(&instanceInfo, nullptr, &m_instance), "vkCreateInstance");

    if (useValidation) {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = debugCallback;
        const auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (createMessenger != nullptr) {
            require(createMessenger(m_instance, &debugInfo, nullptr, &m_debugMessenger),
                    "vkCreateDebugUtilsMessengerEXT");
        }
    }

    uint32_t deviceCount = 0;
    require(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr),
            "vkEnumeratePhysicalDevices");
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan physical device is available");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    require(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()),
            "vkEnumeratePhysicalDevices");

    struct Candidate {
        VkPhysicalDevice device{VK_NULL_HANDLE};
        uint32_t queueFamily{};
        uint32_t score{};
    };
    std::vector<Candidate> candidates;
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceShaderAtomicInt64Features atomic64{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features.pNext = &atomic64;
        vkGetPhysicalDeviceFeatures2(device, &features);
        if (!features.features.shaderInt64 || !atomic64.shaderBufferInt64Atomics) {
            continue;
        }

        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queues.data());
        for (uint32_t family = 0; family < queueCount; ++family) {
            const VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if ((queues[family].queueFlags & required) != required) {
                continue;
            }
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);
            uint32_t score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 1000U : 100U;
            score += properties.limits.maxImageDimension2D / 1024U;
            candidates.push_back({device, family, score});
            break;
        }
    }
    if (candidates.empty()) {
        throw std::runtime_error(
            "The GPU must provide a graphics+compute queue and shaderBufferInt64Atomics");
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });
    const size_t selected = std::min<size_t>(m_config.gpuIndex, candidates.size() - 1U);
    m_physicalDevice = candidates[selected].device;
    m_queueFamily = candidates[selected].queueFamily;

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = m_queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceShaderAtomicInt64Features atomic64{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES};
    atomic64.shaderBufferInt64Atomics = VK_TRUE;
    VkPhysicalDeviceFeatures features{};
    features.shaderInt64 = VK_TRUE;

    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.pNext = &atomic64;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.pEnabledFeatures = &features;
    require(vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device), "vkCreateDevice");
    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
}

void FacetrtVulkan::createCommandPool()
{
    VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = m_queueFamily;
    require(vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool),
            "vkCreateCommandPool");
}

uint32_t FacetrtVulkan::findMemoryType(uint32_t typeBits,
                                         VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);
    for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        if ((typeBits & (1U << index)) != 0U &&
            (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }
    throw std::runtime_error("No compatible Vulkan memory type was found");
}

FacetrtVulkan::Buffer FacetrtVulkan::createBuffer(VkDeviceSize size,
                                                       VkBufferUsageFlags usage,
                                                       VkMemoryPropertyFlags properties) const
{
    Buffer result{};
    result.size = std::max<VkDeviceSize>(size, 4);
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = result.size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    require(vkCreateBuffer(m_device, &bufferInfo, nullptr, &result.buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(m_device, result.buffer, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);
    require(vkAllocateMemory(m_device, &allocation, nullptr, &result.memory), "vkAllocateMemory");
    require(vkBindBufferMemory(m_device, result.buffer, result.memory, 0), "vkBindBufferMemory");
    return result;
}

VkCommandBuffer FacetrtVulkan::beginCommands() const
{
    VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocation.commandPool = m_commandPool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    require(vkAllocateCommandBuffers(m_device, &allocation, &commandBuffer),
            "vkAllocateCommandBuffers");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    require(vkBeginCommandBuffer(commandBuffer, &begin), "vkBeginCommandBuffer");
    return commandBuffer;
}

void FacetrtVulkan::endCommands(VkCommandBuffer commandBuffer) const
{
    require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;
    require(vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit");
    require(vkQueueWaitIdle(m_queue), "vkQueueWaitIdle");
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

void FacetrtVulkan::copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size) const
{
    VkCommandBuffer commandBuffer = beginCommands();
    const VkBufferCopy copy{0, 0, size};
    vkCmdCopyBuffer(commandBuffer, source, destination, 1, &copy);
    endCommands(commandBuffer);
}

void FacetrtVulkan::uploadBuffer(Buffer& destination,
                                    const void* data,
                                    VkDeviceSize size) const
{
    if (size > destination.size) {
        throw std::out_of_range("Upload exceeds Vulkan buffer size");
    }
    Buffer staging = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mapped = nullptr;
    require(vkMapMemory(m_device, staging.memory, 0, size, 0, &mapped), "vkMapMemory");
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(m_device, staging.memory);
    copyBuffer(staging.buffer, destination.buffer, size);
    destroyBuffer(staging);
}

void FacetrtVulkan::downloadBuffer(const Buffer& source,
                                      void* data,
                                      VkDeviceSize size) const
{
    if (size > source.size) {
        throw std::out_of_range("Download exceeds Vulkan buffer size");
    }
    Buffer staging = createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    copyBuffer(source.buffer, staging.buffer, size);
    void* mapped = nullptr;
    require(vkMapMemory(m_device, staging.memory, 0, size, 0, &mapped), "vkMapMemory");
    std::memcpy(data, mapped, static_cast<size_t>(size));
    vkUnmapMemory(m_device, staging.memory);
    destroyBuffer(staging);
}

void FacetrtVulkan::destroyBuffer(Buffer& buffer) const
{
    if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, buffer.buffer, nullptr);
    }
    if (buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, buffer.memory, nullptr);
    }
    buffer = {};
}

VkShaderModule FacetrtVulkan::loadShader(const std::string& fileName) const
{
    const std::string path = m_shaderDirectory + "/" + fileName;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open SPIR-V shader: " + path);
    }
    const std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0) {
        throw std::runtime_error("Invalid SPIR-V shader size: " + path);
    }
    file.seekg(0);
    std::vector<uint32_t> words(static_cast<size_t>(size) / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(words.data()), size);

    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = static_cast<size_t>(size);
    info.pCode = words.data();
    VkShaderModule module = VK_NULL_HANDLE;
    require(vkCreateShaderModule(m_device, &info, nullptr, &module), "vkCreateShaderModule");
    return module;
}

uint32_t FacetrtVulkan::nextPowerOfTwo(uint32_t value) const
{
    if (value <= 1U) {
        return 1U;
    }
    --value;
    value |= value >> 1U;
    value |= value >> 2U;
    value |= value >> 4U;
    value |= value >> 8U;
    value |= value >> 16U;
    return value + 1U;
}

uint32_t FacetrtVulkan::chooseHashCapacity() const
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    const uint64_t descriptorEntries =
        uint64_t(properties.limits.maxStorageBufferRange) / sizeof(uint64_t);
    if (descriptorEntries < 65536ULL) {
        throw std::runtime_error("GPU maxStorageBufferRange is too small for FacetRT");
    }
    uint32_t maximumCapacity = 1U;
    while (maximumCapacity <= (std::numeric_limits<uint32_t>::max() >> 1U) &&
           uint64_t(maximumCapacity << 1U) <= descriptorEntries) {
        maximumCapacity <<= 1U;
    }
    if (m_config.edgeHashCapacity != 0U) {
        const uint32_t capacity = nextPowerOfTwo(m_config.edgeHashCapacity);
        if (capacity < m_config.edgeHashCapacity || capacity > maximumCapacity) {
            throw std::overflow_error(
                "edgeHashCapacity exceeds GPU maxStorageBufferRange");
        }
        return capacity;
    }
    const uint64_t requested = std::max<uint64_t>(65536ULL,
                                                   uint64_t(surfaceCount()) * 64ULL);
    if (requested >= uint64_t(maximumCapacity)) {
        return maximumCapacity;
    }
    return nextPowerOfTwo(static_cast<uint32_t>(requested));
}

void FacetrtVulkan::validateGeometry(const std::vector<Vertex>& vertices) const
{
    if (vertices.empty() || (vertices.size() % 3U) != 0U) {
        throw std::invalid_argument("Geometry must be a non-empty triangle list");
    }
    for (const Vertex& vertex : vertices) {
        for (float coordinate : vertex.position) {
            if (!std::isfinite(coordinate)) {
                throw std::invalid_argument("Geometry contains a non-finite position");
            }
        }
        const float normalLength = std::sqrt(vertex.normal[0] * vertex.normal[0] +
                                             vertex.normal[1] * vertex.normal[1] +
                                             vertex.normal[2] * vertex.normal[2]);
        if (!(normalLength > 0.99f && normalLength < 1.01f)) {
            throw std::invalid_argument("Facet normals must be normalized");
        }
    }
}

void FacetrtVulkan::setGeometry(const std::vector<Vertex>& vertices)
{
    if (!initialized()) {
        throw std::logic_error("initialize() must be called before setGeometry()");
    }
    validateGeometry(vertices);
    require(vkDeviceWaitIdle(m_device), "vkDeviceWaitIdle");
    destroySolveResources();
    destroyVisibilityResources();

    m_facetCount = static_cast<uint32_t>(vertices.size() / 3U);
    m_boundsMin = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max()};
    m_boundsMax = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                   std::numeric_limits<float>::lowest()};
    for (const Vertex& vertex : vertices) {
        for (size_t axis = 0; axis < 3; ++axis) {
            m_boundsMin[axis] = std::min(m_boundsMin[axis], vertex.position[axis]);
            m_boundsMax[axis] = std::max(m_boundsMax[axis], vertex.position[axis]);
        }
    }
    m_hashCapacity = chooseHashCapacity();
    createVisibilityResources();
    uploadBuffer(m_vertices, vertices.data(), vertices.size() * sizeof(Vertex));
    createVisibilityDescriptors();
    createVisibilityPipelines();
}

std::array<float, 16> FacetrtVulkan::makeProjection(const Direction& direction) const
{
    const std::array<float, 3> view = normalize3({direction.x, direction.y, direction.z});
    const std::array<float, 3> reference = std::abs(view[2]) < 0.9f
                                               ? std::array<float, 3>{0.0f, 0.0f, 1.0f}
                                               : std::array<float, 3>{0.0f, 1.0f, 0.0f};
    const std::array<float, 3> right = normalize3(cross3(reference, view));
    const std::array<float, 3> up = cross3(view, right);
    const std::array<float, 3> center{(m_boundsMin[0] + m_boundsMax[0]) * 0.5f,
                                      (m_boundsMin[1] + m_boundsMax[1]) * 0.5f,
                                      (m_boundsMin[2] + m_boundsMax[2]) * 0.5f};
    const std::array<float, 3> diagonal{m_boundsMax[0] - m_boundsMin[0],
                                        m_boundsMax[1] - m_boundsMin[1],
                                        m_boundsMax[2] - m_boundsMin[2]};
    const float radius = std::max(0.5f * std::sqrt(dot3(diagonal, diagonal)) * 1.001f,
                                  1.0e-4f);

    std::array<float, 16> matrix{};
    matrix[0] = right[0] / radius;
    matrix[1] = up[0] / radius;
    matrix[2] = -view[0] / (2.0f * radius);
    matrix[4] = right[1] / radius;
    matrix[5] = up[1] / radius;
    matrix[6] = -view[1] / (2.0f * radius);
    matrix[8] = right[2] / radius;
    matrix[9] = up[2] / radius;
    matrix[10] = -view[2] / (2.0f * radius);
    matrix[12] = -dot3(center, right) / radius;
    matrix[13] = -dot3(center, up) / radius;
    matrix[14] = 0.5f + dot3(center, view) / (2.0f * radius);
    matrix[15] = 1.0f;
    return matrix;
}

void FacetrtVulkan::createVisibilityResources()
{
    const VkBufferUsageFlags deviceStorage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkMemoryPropertyFlags deviceLocal = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const uint64_t pixelCount = uint64_t(m_config.rasterWidth) * m_config.rasterHeight;
    const uint64_t fragmentCount = pixelCount * m_config.maxFragmentsPerPixel;
    if (fragmentCount > std::numeric_limits<VkDeviceSize>::max() / 8ULL) {
        throw std::overflow_error("Visibility A-buffer size overflow");
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    const auto requireStorageRange = [&](VkDeviceSize size, const char* name) {
        if (size > properties.limits.maxStorageBufferRange) {
            std::ostringstream stream;
            stream << name << " requires " << size
                   << " bytes, exceeding GPU maxStorageBufferRange "
                   << properties.limits.maxStorageBufferRange;
            throw std::runtime_error(stream.str());
        }
    };
    requireStorageRange(fragmentCount * 8ULL, "Visibility fragment buffer");
    requireStorageRange(VkDeviceSize(m_hashCapacity) * sizeof(uint64_t),
                        "Visibility edge-key buffer");
    requireStorageRange(VkDeviceSize(m_hashCapacity) * sizeof(uint32_t),
                        "Visibility edge-count buffer");

    m_vertices = createBuffer(VkDeviceSize(m_facetCount) * 3U * sizeof(Vertex),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              deviceLocal);
    m_fragments = createBuffer(fragmentCount * 8ULL, deviceStorage, deviceLocal);
    m_pixelCounts = createBuffer(pixelCount * sizeof(uint32_t), deviceStorage, deviceLocal);
    m_edgeKeys = createBuffer(VkDeviceSize(m_hashCapacity) * sizeof(uint64_t),
                              deviceStorage, deviceLocal);
    m_edgeCounts = createBuffer(VkDeviceSize(m_hashCapacity) * sizeof(uint32_t),
                                deviceStorage, deviceLocal);
    m_denominator = createBuffer(VkDeviceSize(surfaceCount()) * sizeof(uint32_t),
                                 deviceStorage, deviceLocal);
    m_skyCounts = createBuffer(VkDeviceSize(surfaceCount()) * sizeof(uint32_t),
                               deviceStorage, deviceLocal);
    m_visibilityStats = createBuffer(2U * sizeof(uint32_t), deviceStorage, deviceLocal);
    m_directTotal = createBuffer(VkDeviceSize(surfaceCount()) * sizeof(uint32_t),
                                 deviceStorage, deviceLocal);
    m_directVisible = createBuffer(VkDeviceSize(surfaceCount()) * sizeof(uint32_t),
                                   deviceStorage, deviceLocal);
}

void FacetrtVulkan::createVisibilityDescriptors()
{
    std::array<VkDescriptorSetLayoutBinding, 7> bindings{};
    for (uint32_t index = 0; index < bindings.size(); ++index) {
        bindings[index].binding = index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    require(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
                                        &m_visibilitySetLayout),
            "vkCreateDescriptorSetLayout(visibility)");

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                  static_cast<uint32_t>(bindings.size() * 2U)};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    require(vkCreateDescriptorPool(m_device, &poolInfo, nullptr,
                                   &m_visibilityDescriptorPool),
            "vkCreateDescriptorPool(visibility)");

    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    const std::array<VkDescriptorSetLayout, 2> layouts{m_visibilitySetLayout,
                                                       m_visibilitySetLayout};
    allocation.descriptorPool = m_visibilityDescriptorPool;
    allocation.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocation.pSetLayouts = layouts.data();
    std::array<VkDescriptorSet, 2> sets{};
    require(vkAllocateDescriptorSets(m_device, &allocation, sets.data()),
            "vkAllocateDescriptorSets(visibility)");
    m_visibilityDescriptorSet = sets[0];
    m_directDescriptorSet = sets[1];

    const std::array<Buffer*, 7> buffers{&m_fragments, &m_pixelCounts, &m_edgeKeys,
                                         &m_edgeCounts, &m_denominator, &m_skyCounts,
                                         &m_visibilityStats};
    std::array<VkDescriptorBufferInfo, 7> bufferInfos{};
    std::array<VkWriteDescriptorSet, 7> writes{};
    for (uint32_t index = 0; index < buffers.size(); ++index) {
        bufferInfos[index] = {buffers[index]->buffer, 0, buffers[index]->size};
        writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[index].dstSet = m_visibilityDescriptorSet;
        writes[index].dstBinding = index;
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[index].pBufferInfo = &bufferInfos[index];
    }
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(),
                           0, nullptr);

    const std::array<Buffer*, 7> directBuffers{&m_fragments, &m_pixelCounts, &m_edgeKeys,
                                               &m_edgeCounts, &m_directTotal,
                                               &m_directVisible, &m_visibilityStats};
    std::array<VkDescriptorBufferInfo, 7> directInfos{};
    std::array<VkWriteDescriptorSet, 7> directWrites{};
    for (uint32_t index = 0; index < directBuffers.size(); ++index) {
        directInfos[index] = {directBuffers[index]->buffer, 0, directBuffers[index]->size};
        directWrites[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        directWrites[index].dstSet = m_directDescriptorSet;
        directWrites[index].dstBinding = index;
        directWrites[index].descriptorCount = 1;
        directWrites[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        directWrites[index].pBufferInfo = &directInfos[index];
    }
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(directWrites.size()),
                           directWrites.data(), 0, nullptr);
}

void FacetrtVulkan::createVisibilityPipelines()
{
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                           VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(VisibilityPush);
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_visibilitySetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    require(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr,
                                   &m_visibilityPipelineLayout),
            "vkCreatePipelineLayout(visibility)");

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    require(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass),
            "vkCreateRenderPass");

    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.width = m_config.rasterWidth;
    framebufferInfo.height = m_config.rasterHeight;
    framebufferInfo.layers = 1;
    require(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffer),
            "vkCreateFramebuffer");

    VkShaderModule vertexModule = loadShader("visibility.vert.spv");
    VkShaderModule fragmentModule = loadShader("visibility.frag.spv");
    const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_VERTEX_BIT, vertexModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_FRAGMENT_BIT, fragmentModule, "main", nullptr}}};

    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(Vertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::array<VkVertexInputAttributeDescription, 2> attributes{{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)}}};
    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexBinding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkPipelineDepthStencilStateCreateInfo depthStencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_visibilityPipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    require(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                      nullptr, &m_rasterPipeline),
            "vkCreateGraphicsPipelines(visibility)");
    vkDestroyShaderModule(m_device, vertexModule, nullptr);
    vkDestroyShaderModule(m_device, fragmentModule, nullptr);

    VkShaderModule resolveModule = loadShader("visibility_resolve.comp.spv");
    VkComputePipelineCreateInfo computeInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    computeInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    computeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeInfo.stage.module = resolveModule;
    computeInfo.stage.pName = "main";
    computeInfo.layout = m_visibilityPipelineLayout;
    require(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computeInfo,
                                     nullptr, &m_resolvePipeline),
            "vkCreateComputePipelines(visibility resolve)");
    vkDestroyShaderModule(m_device, resolveModule, nullptr);

    VkShaderModule directModule = loadShader("direct_resolve.comp.spv");
    computeInfo.stage.module = directModule;
    require(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computeInfo,
                                     nullptr, &m_directResolvePipeline),
            "vkCreateComputePipelines(direct resolve)");
    vkDestroyShaderModule(m_device, directModule, nullptr);
}

void FacetrtVulkan::createSolveResources(uint32_t directedEdgeCount)
{
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkMemoryPropertyFlags memory = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    m_rowOffsets = createBuffer(m_rowOffsetsCpu.size() * sizeof(uint32_t), usage, memory);
    m_edges = createBuffer(std::max<size_t>(directedEdgeCount, 1U) * sizeof(CsrEdge),
                           usage, memory);
    m_optics = createBuffer(VkDeviceSize(surfaceCount()) * sizeof(SurfaceOptics), usage, memory);
    m_radiosityA = createBuffer(VkDeviceSize(surfaceCount()) * sizeof(float), usage, memory);
    m_radiosityB = createBuffer(VkDeviceSize(surfaceCount()) * sizeof(float), usage, memory);
    m_residual = createBuffer(sizeof(uint32_t), usage, memory);

    uploadBuffer(m_rowOffsets, m_rowOffsetsCpu.data(),
                 m_rowOffsetsCpu.size() * sizeof(uint32_t));
    if (!m_edgesCpu.empty()) {
        uploadBuffer(m_edges, m_edgesCpu.data(), m_edgesCpu.size() * sizeof(CsrEdge));
    } else {
        const CsrEdge empty{};
        uploadBuffer(m_edges, &empty, sizeof(empty));
    }
}

void FacetrtVulkan::createSolveDescriptors()
{
    std::array<VkDescriptorSetLayoutBinding, 8> bindings{};
    for (uint32_t index = 0; index < bindings.size(); ++index) {
        bindings[index].binding = index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    require(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_solveSetLayout),
            "vkCreateDescriptorSetLayout(solve)");

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                  static_cast<uint32_t>(bindings.size())};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    require(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_solveDescriptorPool),
            "vkCreateDescriptorPool(solve)");
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = m_solveDescriptorPool;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &m_solveSetLayout;
    require(vkAllocateDescriptorSets(m_device, &allocation, &m_solveDescriptorSet),
            "vkAllocateDescriptorSets(solve)");

    const std::array<Buffer*, 8> buffers{&m_rowOffsets, &m_edges, &m_denominator,
                                         &m_skyCounts, &m_optics, &m_radiosityA,
                                         &m_radiosityB, &m_residual};
    std::array<VkDescriptorBufferInfo, 8> infos{};
    std::array<VkWriteDescriptorSet, 8> writes{};
    for (uint32_t index = 0; index < buffers.size(); ++index) {
        infos[index] = {buffers[index]->buffer, 0, buffers[index]->size};
        writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[index].dstSet = m_solveDescriptorSet;
        writes[index].dstBinding = index;
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[index].pBufferInfo = &infos[index];
    }
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(),
                           0, nullptr);
}

void FacetrtVulkan::createSolvePipeline()
{
    VkPushConstantRange pushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SolvePush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_solveSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    require(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_solvePipelineLayout),
            "vkCreatePipelineLayout(solve)");

    VkShaderModule module = loadShader("radiosity.comp.spv");
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = module;
    info.stage.pName = "main";
    info.layout = m_solvePipelineLayout;
    require(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &info, nullptr,
                                     &m_solvePipeline),
            "vkCreateComputePipelines(radiosity)");
    vkDestroyShaderModule(m_device, module, nullptr);
}

GraphDiagnostics FacetrtVulkan::buildVisibilityGraph(
    const std::vector<Direction>& directions)
{
    if (m_rasterPipeline == VK_NULL_HANDLE || m_facetCount == 0U) {
        throw std::logic_error("setGeometry() must be called before buildVisibilityGraph()");
    }
    if (directions.empty()) {
        throw std::invalid_argument("At least one visibility direction is required");
    }
    destroySolveResources();

    VisibilityPush push{};
    push.raster[0] = m_config.rasterWidth;
    push.raster[1] = m_config.rasterHeight;
    push.raster[2] = m_config.maxFragmentsPerPixel;
    push.raster[3] = m_hashCapacity;

    VkCommandBuffer commandBuffer = beginCommands();
    vkCmdFillBuffer(commandBuffer, m_edgeKeys.buffer, 0, m_edgeKeys.size, 0xffffffffU);
    vkCmdFillBuffer(commandBuffer, m_edgeCounts.buffer, 0, m_edgeCounts.size, 0U);
    vkCmdFillBuffer(commandBuffer, m_denominator.buffer, 0, m_denominator.size, 0U);
    vkCmdFillBuffer(commandBuffer, m_skyCounts.buffer, 0, m_skyCounts.size, 0U);
    vkCmdFillBuffer(commandBuffer, m_visibilityStats.buffer, 0, m_visibilityStats.size, 0U);

    VkMemoryBarrier transferToShader{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    transferToShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    transferToShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &transferToShader, 0, nullptr, 0, nullptr);
    endCommands(commandBuffer);

    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertices.buffer, &vertexOffset);
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(m_config.rasterWidth),
                              static_cast<float>(m_config.rasterHeight), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, {m_config.rasterWidth, m_config.rasterHeight}};
    const VkRenderPassBeginInfo renderBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                             nullptr,
                                             m_renderPass,
                                             m_framebuffer,
                                             scissor,
                                             0,
                                             nullptr};

    for (const Direction& inputDirection : directions) {
        commandBuffer = beginCommands();
        const std::array<float, 3> direction =
            normalize3({inputDirection.x, inputDirection.y, inputDirection.z});
        const std::array<float, 16> projection = makeProjection(inputDirection);
        std::copy(projection.begin(), projection.end(), push.viewProjection);
        push.viewDirection[0] = direction[0];
        push.viewDirection[1] = direction[1];
        push.viewDirection[2] = direction[2];
        push.viewDirection[3] = 0.0f;

        vkCmdFillBuffer(commandBuffer, m_pixelCounts.buffer, 0, m_pixelCounts.size, 0U);
        VkMemoryBarrier countClear{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        countClear.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        countClear.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             1, &countClear, 0, nullptr, 0, nullptr);

        vkCmdBeginRenderPass(commandBuffer, &renderBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rasterPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_visibilityPipelineLayout, 0, 1,
                                &m_visibilityDescriptorSet, 0, nullptr);
        vkCmdPushConstants(commandBuffer, m_visibilityPipelineLayout,
                           kVisibilityPushStages,
                           0, sizeof(push), &push);
        vkCmdDraw(commandBuffer, m_facetCount * 3U, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        VkMemoryBarrier rasterToResolve{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        rasterToResolve.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        rasterToResolve.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             1, &rasterToResolve, 0, nullptr, 0, nullptr);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_resolvePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_visibilityPipelineLayout, 0, 1,
                                &m_visibilityDescriptorSet, 0, nullptr);
        vkCmdPushConstants(commandBuffer, m_visibilityPipelineLayout,
                           kVisibilityPushStages, 0, sizeof(push), &push);
        const uint64_t pixels = uint64_t(m_config.rasterWidth) * m_config.rasterHeight;
        vkCmdDispatch(commandBuffer, static_cast<uint32_t>((pixels + 63ULL) / 64ULL), 1, 1);

        VkMemoryBarrier resolveToNext{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        resolveToNext.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        resolveToNext.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT |
                                      VK_ACCESS_SHADER_READ_BIT |
                                      VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT |
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 1, &resolveToNext, 0, nullptr, 0, nullptr);
        endCommands(commandBuffer);
    }

    std::vector<uint64_t> edgeKeys(m_hashCapacity);
    std::vector<uint32_t> edgeCounts(m_hashCapacity);
    std::array<uint32_t, 2> statistics{};
    m_denominatorCpu.resize(surfaceCount());
    m_skyCountsCpu.resize(surfaceCount());
    downloadBuffer(m_edgeKeys, edgeKeys.data(), edgeKeys.size() * sizeof(uint64_t));
    downloadBuffer(m_edgeCounts, edgeCounts.data(), edgeCounts.size() * sizeof(uint32_t));
    downloadBuffer(m_denominator, m_denominatorCpu.data(),
                   m_denominatorCpu.size() * sizeof(uint32_t));
    downloadBuffer(m_skyCounts, m_skyCountsCpu.data(),
                   m_skyCountsCpu.size() * sizeof(uint32_t));
    downloadBuffer(m_visibilityStats, statistics.data(), sizeof(statistics));

    GraphDiagnostics diagnostics{};
    diagnostics.facetCount = m_facetCount;
    diagnostics.surfaceCount = surfaceCount();
    diagnostics.fragmentOverflow = statistics[0];
    diagnostics.hashOverflow = statistics[1];
    if (diagnostics.fragmentOverflow != 0U) {
        throw std::runtime_error(
            "Visibility A-buffer overflow; increase Config::maxFragmentsPerPixel");
    }
    if (diagnostics.hashOverflow != 0U) {
        throw std::runtime_error(
            "Visibility edge hash overflow; increase Config::edgeHashCapacity");
    }

    std::vector<std::pair<uint64_t, uint32_t>> undirectedEdges;
    undirectedEdges.reserve(std::min<uint32_t>(m_hashCapacity / 16U, 1U << 20U));
    for (uint32_t slot = 0; slot < m_hashCapacity; ++slot) {
        if (edgeKeys[slot] == kEmptyEdgeKey || edgeCounts[slot] == 0U) {
            continue;
        }
        const uint32_t sideA = static_cast<uint32_t>(edgeKeys[slot] >> 32U);
        const uint32_t sideB = static_cast<uint32_t>(edgeKeys[slot]);
        if (sideA >= surfaceCount() || sideB >= surfaceCount()) {
            throw std::runtime_error("GPU visibility hash contains an invalid surface index");
        }
        undirectedEdges.emplace_back(edgeKeys[slot], edgeCounts[slot]);
    }

    m_rowOffsetsCpu.assign(surfaceCount() + 1U, 0U);
    for (const auto& entry : undirectedEdges) {
        const uint32_t sideA = static_cast<uint32_t>(entry.first >> 32U);
        const uint32_t sideB = static_cast<uint32_t>(entry.first);
        ++m_rowOffsetsCpu[sideA + 1U];
        ++m_rowOffsetsCpu[sideB + 1U];
    }
    std::partial_sum(m_rowOffsetsCpu.begin(), m_rowOffsetsCpu.end(),
                     m_rowOffsetsCpu.begin());
    m_edgesCpu.assign(m_rowOffsetsCpu.back(), {});
    std::vector<uint32_t> cursor = m_rowOffsetsCpu;
    for (const auto& entry : undirectedEdges) {
        const uint32_t sideA = static_cast<uint32_t>(entry.first >> 32U);
        const uint32_t sideB = static_cast<uint32_t>(entry.first);
        const uint32_t count = entry.second;
        m_edgesCpu[cursor[sideA]++] = {sideB, count};
        m_edgesCpu[cursor[sideB]++] = {sideA, count};
    }
    diagnostics.directedEdgeCount = static_cast<uint32_t>(m_edgesCpu.size());

    float maxClosureError = 0.0f;
    for (uint32_t side = 0; side < surfaceCount(); ++side) {
        uint64_t visibleCount = m_skyCountsCpu[side];
        for (uint32_t index = m_rowOffsetsCpu[side];
             index < m_rowOffsetsCpu[side + 1U]; ++index) {
            visibleCount += m_edgesCpu[index].count;
        }
        if (m_denominatorCpu[side] == 0U) {
            continue;
        }
        const float closure = static_cast<float>(visibleCount) /
                              static_cast<float>(m_denominatorCpu[side]);
        maxClosureError = std::max(maxClosureError, std::abs(closure - 1.0f));
    }
    diagnostics.maxClosureError = maxClosureError;

    createSolveResources(diagnostics.directedEdgeCount);
    createSolveDescriptors();
    createSolvePipeline();
    return diagnostics;
}

std::vector<float> FacetrtVulkan::computeSunlitFraction(const Direction& sunDirection)
{
    if (m_directResolvePipeline == VK_NULL_HANDLE || m_facetCount == 0U) {
        throw std::logic_error("setGeometry() must be called before computeSunlitFraction()");
    }
    const std::array<float, 3> direction =
        normalize3({sunDirection.x, sunDirection.y, sunDirection.z});
    const std::array<float, 16> projection = makeProjection(sunDirection);
    VisibilityPush push{};
    std::copy(projection.begin(), projection.end(), push.viewProjection);
    push.viewDirection[0] = direction[0];
    push.viewDirection[1] = direction[1];
    push.viewDirection[2] = direction[2];
    push.raster[0] = m_config.rasterWidth;
    push.raster[1] = m_config.rasterHeight;
    push.raster[2] = m_config.maxFragmentsPerPixel;
    push.raster[3] = m_hashCapacity;

    VkCommandBuffer commandBuffer = beginCommands();
    vkCmdFillBuffer(commandBuffer, m_pixelCounts.buffer, 0, m_pixelCounts.size, 0U);
    vkCmdFillBuffer(commandBuffer, m_directTotal.buffer, 0, m_directTotal.size, 0U);
    vkCmdFillBuffer(commandBuffer, m_directVisible.buffer, 0, m_directVisible.size, 0U);
    vkCmdFillBuffer(commandBuffer, m_visibilityStats.buffer, 0, m_visibilityStats.size, 0U);
    VkMemoryBarrier clearBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    clearBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    clearBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &clearBarrier, 0, nullptr, 0, nullptr);

    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertices.buffer, &vertexOffset);
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(m_config.rasterWidth),
                              static_cast<float>(m_config.rasterHeight), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, {m_config.rasterWidth, m_config.rasterHeight}};
    VkRenderPassBeginInfo renderBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderBegin.renderPass = m_renderPass;
    renderBegin.framebuffer = m_framebuffer;
    renderBegin.renderArea = scissor;
    vkCmdBeginRenderPass(commandBuffer, &renderBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rasterPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_visibilityPipelineLayout, 0, 1,
                            &m_directDescriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, m_visibilityPipelineLayout,
                       kVisibilityPushStages,
                       0, sizeof(push), &push);
    vkCmdDraw(commandBuffer, m_facetCount * 3U, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    VkMemoryBarrier rasterToResolve{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    rasterToResolve.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    rasterToResolve.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         1, &rasterToResolve, 0, nullptr, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_directResolvePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_visibilityPipelineLayout, 0, 1,
                            &m_directDescriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, m_visibilityPipelineLayout,
                       kVisibilityPushStages, 0, sizeof(push), &push);
    const uint64_t pixels = uint64_t(m_config.rasterWidth) * m_config.rasterHeight;
    vkCmdDispatch(commandBuffer, static_cast<uint32_t>((pixels + 63ULL) / 64ULL), 1, 1);
    endCommands(commandBuffer);

    std::vector<uint32_t> total(surfaceCount());
    std::vector<uint32_t> visible(surfaceCount());
    std::array<uint32_t, 2> statistics{};
    downloadBuffer(m_directTotal, total.data(), total.size() * sizeof(uint32_t));
    downloadBuffer(m_directVisible, visible.data(), visible.size() * sizeof(uint32_t));
    downloadBuffer(m_visibilityStats, statistics.data(), sizeof(statistics));
    if (statistics[0] != 0U) {
        throw std::runtime_error(
            "Direct-light A-buffer overflow; increase Config::maxFragmentsPerPixel");
    }

    std::vector<float> fraction(surfaceCount(), 0.0f);
    for (uint32_t side = 0; side < surfaceCount(); ++side) {
        if (total[side] != 0U) {
            fraction[side] = static_cast<float>(visible[side]) /
                             static_cast<float>(total[side]);
        }
    }
    return fraction;
}

void FacetrtVulkan::validateOptics(const std::vector<SurfaceOptics>& optics) const
{
    if (optics.size() != surfaceCount()) {
        throw std::invalid_argument("Optics array must contain exactly two sides per facet");
    }
    for (const SurfaceOptics& value : optics) {
        if (!std::isfinite(value.reflectance) || !std::isfinite(value.transmittance) ||
            !std::isfinite(value.emission) || !std::isfinite(value.directIrradiance) ||
            value.reflectance < 0.0f || value.transmittance < 0.0f ||
            value.emission < 0.0f || value.directIrradiance < 0.0f ||
            value.reflectance + value.transmittance > 1.0001f) {
            throw std::invalid_argument(
                "Optical values must be finite, non-negative, and reflectance+transmittance<=1");
        }
    }
}

SolveResult FacetrtVulkan::solve(const std::vector<SurfaceOptics>& optics,
                                   float skyRadiosity,
                                   uint32_t iterations,
                                   float relaxation)
{
    if (m_solvePipeline == VK_NULL_HANDLE) {
        throw std::logic_error("buildVisibilityGraph() must be called before solve()");
    }
    validateOptics(optics);
    if (!std::isfinite(skyRadiosity) || skyRadiosity < 0.0f) {
        throw std::invalid_argument("skyRadiosity must be finite and non-negative");
    }
    if (!(relaxation > 0.0f && relaxation <= 1.0f)) {
        throw std::invalid_argument("GPU Jacobi relaxation must be in (0,1]");
    }
    uploadBuffer(m_optics, optics.data(), optics.size() * sizeof(SurfaceOptics));

    VkCommandBuffer commandBuffer = beginCommands();
    vkCmdFillBuffer(commandBuffer, m_radiosityA.buffer, 0, m_radiosityA.size, 0U);
    vkCmdFillBuffer(commandBuffer, m_radiosityB.buffer, 0, m_radiosityB.size, 0U);
    vkCmdFillBuffer(commandBuffer, m_residual.buffer, 0, m_residual.size, 0U);
    VkMemoryBarrier clearBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    clearBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    clearBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         1, &clearBarrier, 0, nullptr, 0, nullptr);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_solvePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_solvePipelineLayout, 0, 1, &m_solveDescriptorSet,
                            0, nullptr);
    uint32_t ping = 0U;
    for (uint32_t iteration = 0; iteration < iterations; ++iteration) {
        vkCmdFillBuffer(commandBuffer, m_residual.buffer, 0, sizeof(uint32_t), 0U);
        VkMemoryBarrier residualClear{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        residualClear.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        residualClear.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             1, &residualClear, 0, nullptr, 0, nullptr);

        const SolvePush push{m_facetCount, ping, skyRadiosity, relaxation};
        vkCmdPushConstants(commandBuffer, m_solvePipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
        vkCmdDispatch(commandBuffer, (m_facetCount + 63U) / 64U, 1, 1);

        VkMemoryBarrier iterationBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        iterationBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        iterationBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                         VK_ACCESS_TRANSFER_WRITE_BIT |
                                         VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 1, &iterationBarrier, 0, nullptr, 0, nullptr);
        ping ^= 1U;
    }
    endCommands(commandBuffer);

    SolveResult result{};
    result.iterations = iterations;
    result.radiosity.resize(surfaceCount(), 0.0f);
    const Buffer& finalBuffer = ping == 0U ? m_radiosityA : m_radiosityB;
    downloadBuffer(finalBuffer, result.radiosity.data(),
                   result.radiosity.size() * sizeof(float));
    uint32_t deltaBits = 0U;
    downloadBuffer(m_residual, &deltaBits, sizeof(deltaBits));
    std::memcpy(&result.maxDelta, &deltaBits, sizeof(result.maxDelta));
    return result;
}

void FacetrtVulkan::destroySolveResources()
{
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    if (m_solvePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_solvePipeline, nullptr);
    }
    if (m_solvePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_solvePipelineLayout, nullptr);
    }
    if (m_solveDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_solveDescriptorPool, nullptr);
    }
    if (m_solveSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_solveSetLayout, nullptr);
    }
    m_solvePipeline = VK_NULL_HANDLE;
    m_solvePipelineLayout = VK_NULL_HANDLE;
    m_solveDescriptorPool = VK_NULL_HANDLE;
    m_solveDescriptorSet = VK_NULL_HANDLE;
    m_solveSetLayout = VK_NULL_HANDLE;
    destroyBuffer(m_rowOffsets);
    destroyBuffer(m_edges);
    destroyBuffer(m_optics);
    destroyBuffer(m_radiosityA);
    destroyBuffer(m_radiosityB);
    destroyBuffer(m_residual);
    m_rowOffsetsCpu.clear();
    m_edgesCpu.clear();
}

void FacetrtVulkan::destroyVisibilityResources()
{
    if (m_device == VK_NULL_HANDLE) {
        return;
    }
    if (m_resolvePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_resolvePipeline, nullptr);
    }
    if (m_directResolvePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_directResolvePipeline, nullptr);
    }
    if (m_rasterPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_rasterPipeline, nullptr);
    }
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
    }
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    }
    if (m_visibilityPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_visibilityPipelineLayout, nullptr);
    }
    if (m_visibilityDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_visibilityDescriptorPool, nullptr);
    }
    if (m_visibilitySetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_visibilitySetLayout, nullptr);
    }
    m_resolvePipeline = VK_NULL_HANDLE;
    m_directResolvePipeline = VK_NULL_HANDLE;
    m_rasterPipeline = VK_NULL_HANDLE;
    m_framebuffer = VK_NULL_HANDLE;
    m_renderPass = VK_NULL_HANDLE;
    m_visibilityPipelineLayout = VK_NULL_HANDLE;
    m_visibilityDescriptorPool = VK_NULL_HANDLE;
    m_visibilityDescriptorSet = VK_NULL_HANDLE;
    m_directDescriptorSet = VK_NULL_HANDLE;
    m_visibilitySetLayout = VK_NULL_HANDLE;
    destroyBuffer(m_vertices);
    destroyBuffer(m_fragments);
    destroyBuffer(m_pixelCounts);
    destroyBuffer(m_edgeKeys);
    destroyBuffer(m_edgeCounts);
    destroyBuffer(m_denominator);
    destroyBuffer(m_skyCounts);
    destroyBuffer(m_visibilityStats);
    destroyBuffer(m_directTotal);
    destroyBuffer(m_directVisible);
    m_denominatorCpu.clear();
    m_skyCountsCpu.clear();
}

void FacetrtVulkan::destroy()
{
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        destroySolveResources();
        destroyVisibilityResources();
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        }
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_debugMessenger != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        const auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyMessenger != nullptr) {
            destroyMessenger(m_instance, m_debugMessenger, nullptr);
        }
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
    m_instance = VK_NULL_HANDLE;
    m_debugMessenger = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_queue = VK_NULL_HANDLE;
    m_commandPool = VK_NULL_HANDLE;
    m_queueFamily = 0;
    m_facetCount = 0;
    m_hashCapacity = 0;
}

} // namespace facetvk
