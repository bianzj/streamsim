#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace facetvk {

// Geometry is uploaded as a non-indexed triangle list. The three vertices of
// one facet must be consecutive and carry the same unit normal.
struct Vertex {
    float position[3]{};
    float normal[3]{};
};

// Direction points from the scene towards the virtual orthographic camera.
// Visibility directions must have equal solid-angle weight.
struct Direction {
    float x{};
    float y{};
    float z{1.0f};
};

// One entry per oriented side: side 2*f is the geometric-normal side and
// side 2*f+1 is the opposite side.
struct SurfaceOptics {
    float reflectance{};
    float transmittance{};
    float emission{};
    float directIrradiance{};
};

struct Config {
    uint32_t rasterWidth{512};
    uint32_t rasterHeight{512};
    uint32_t maxFragmentsPerPixel{32};
    uint32_t edgeHashCapacity{0}; // 0 selects a power-of-two capacity automatically.
    bool enableValidation{false};
    uint32_t gpuIndex{0};
};

struct GraphDiagnostics {
    uint32_t facetCount{};
    uint32_t surfaceCount{};
    uint32_t directedEdgeCount{};
    uint32_t fragmentOverflow{};
    uint32_t hashOverflow{};
    float maxClosureError{};
};

struct SolveResult {
    std::vector<float> radiosity;
    uint32_t iterations{};
    float maxDelta{};
};

// Raster-only Vulkan radiosity model:
// 1. Equal-solid-angle directions rasterize all facets into an A-buffer.
// 2. A compute pass depth-sorts each pixel and records adjacent oriented sides.
// 3. The visibility graph is compacted once to CSR.
// 4. Two-sided reflection/transmission is solved by GPU Jacobi iterations.
// No acceleration structure, ray query, or ray-tracing pipeline is created.
class FacetrtVulkan {
public:
    FacetrtVulkan() = default;
    ~FacetrtVulkan();

    FacetrtVulkan(const FacetrtVulkan&) = delete;
    FacetrtVulkan& operator=(const FacetrtVulkan&) = delete;

    void initialize(const Config& config, const std::string& shaderDirectory);
    void setGeometry(const std::vector<Vertex>& vertices);
    GraphDiagnostics buildVisibilityGraph(const std::vector<Direction>& directions);

    SolveResult solve(const std::vector<SurfaceOptics>& optics,
                      float skyRadiosity,
                      uint32_t iterations = 64,
                      float relaxation = 1.0f);
    std::vector<float> computeSunlitFraction(const Direction& sunDirection);

    void destroy();

    [[nodiscard]] bool initialized() const noexcept { return m_device != VK_NULL_HANDLE; }
    [[nodiscard]] uint32_t facetCount() const noexcept { return m_facetCount; }
    [[nodiscard]] uint32_t surfaceCount() const noexcept { return m_facetCount * 2U; }

private:
    struct Buffer {
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDeviceSize size{};
    };

    struct CsrEdge {
        uint32_t neighbor{};
        uint32_t count{};
    };

    struct VisibilityPush {
        float viewProjection[16]{};
        float viewDirection[4]{};
        uint32_t raster[4]{}; // width, height, max layers, hash capacity
    };

    struct SolvePush {
        uint32_t facetCount{};
        uint32_t ping{};
        float skyRadiosity{};
        float relaxation{1.0f};
    };

    void createContext();
    void createCommandPool();
    void createVisibilityResources();
    void createVisibilityDescriptors();
    void createVisibilityPipelines();
    void createSolveResources(uint32_t directedEdgeCount);
    void createSolveDescriptors();
    void createSolvePipeline();

    void destroyVisibilityResources();
    void destroySolveResources();
    void destroyBuffer(Buffer& buffer) const;

    [[nodiscard]] Buffer createBuffer(VkDeviceSize size,
                                      VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags properties) const;
    void uploadBuffer(Buffer& destination, const void* data, VkDeviceSize size) const;
    void downloadBuffer(const Buffer& source, void* data, VkDeviceSize size) const;
    void copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size) const;

    [[nodiscard]] uint32_t findMemoryType(uint32_t typeBits,
                                          VkMemoryPropertyFlags properties) const;
    [[nodiscard]] VkCommandBuffer beginCommands() const;
    void endCommands(VkCommandBuffer commandBuffer) const;
    [[nodiscard]] VkShaderModule loadShader(const std::string& fileName) const;
    [[nodiscard]] std::array<float, 16> makeProjection(const Direction& direction) const;
    [[nodiscard]] uint32_t chooseHashCapacity() const;
    [[nodiscard]] uint32_t nextPowerOfTwo(uint32_t value) const;

    void require(VkResult result, const char* operation) const;
    void validateGeometry(const std::vector<Vertex>& vertices) const;
    void validateOptics(const std::vector<SurfaceOptics>& optics) const;

    Config m_config{};
    std::string m_shaderDirectory;

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_queue{VK_NULL_HANDLE};
    uint32_t m_queueFamily{};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};

    uint32_t m_facetCount{};
    uint32_t m_hashCapacity{};
    std::array<float, 3> m_boundsMin{};
    std::array<float, 3> m_boundsMax{};

    Buffer m_vertices;
    Buffer m_fragments;
    Buffer m_pixelCounts;
    Buffer m_edgeKeys;
    Buffer m_edgeCounts;
    Buffer m_denominator;
    Buffer m_skyCounts;
    Buffer m_visibilityStats;

    VkDescriptorSetLayout m_visibilitySetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_visibilityDescriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet m_visibilityDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout m_visibilityPipelineLayout{VK_NULL_HANDLE};
    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    Buffer m_directTotal;
    Buffer m_directVisible;
    VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
    VkPipeline m_rasterPipeline{VK_NULL_HANDLE};
    VkPipeline m_resolvePipeline{VK_NULL_HANDLE};

    std::vector<uint32_t> m_rowOffsetsCpu;
    std::vector<CsrEdge> m_edgesCpu;
    VkDescriptorSet m_directDescriptorSet{VK_NULL_HANDLE};
    VkPipeline m_directResolvePipeline{VK_NULL_HANDLE};
    std::vector<uint32_t> m_denominatorCpu;
    std::vector<uint32_t> m_skyCountsCpu;

    Buffer m_rowOffsets;
    Buffer m_edges;
    Buffer m_optics;
    Buffer m_radiosityA;
    Buffer m_radiosityB;
    Buffer m_residual;

    VkDescriptorSetLayout m_solveSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_solveDescriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet m_solveDescriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout m_solvePipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_solvePipeline{VK_NULL_HANDLE};
};

} // namespace facetvk
