#include "appsetting.h"

void AppSetting::init()
{
	initWindow();
	initVulkan();
	initSurface();
}

void AppSetting::destroy()
{
	if (!m_ifDisplay)
	{
		vkDestroySurfaceKHR(m_context.m_instance, m_surface, nullptr);
	}

	m_context.deinit();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

int AppSetting::initWindow()
{
	// Setup GLFW window
	glfwSetErrorCallback(onErrorCallback);
	if (glfwInit() == GLFW_FALSE)
	{
		return 0;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_window = glfwCreateWindow(SAMPLE_WIDTH, SAMPLE_HEIGHT, PROJECT_NAME, nullptr, nullptr);

	// Setup Vulkan
	if (glfwVulkanSupported() == GLFW_FALSE)
	{
		printf("GLFW: Vulkan Not Supported\n");
		return 0;
	}

	return 0;
}

int AppSetting::initVulkan()
{
	// Vulkan required extensions
	if (m_ifDisplay) { assert(glfwVulkanSupported() == 1); }
	uint32_t count{ 0 };
	auto     reqExtensions = glfwGetRequiredInstanceExtensions(&count);

	// Requesting Vulkan extensions and layers
	m_contextInfo.setVersion(1, 3);                       // Using Vulkan 1.3
	for (uint32_t ext_id = 0; ext_id < count; ext_id++)  // Adding required extensions (surface, win32, linux, ..)
		m_contextInfo.addInstanceExtension(reqExtensions[ext_id]);
	m_contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);  // Allow debug names
	m_contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);            // Enabling ability to present rendering

	VkPhysicalDeviceShaderClockFeaturesKHR clockFeature{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR };
	m_contextInfo.addDeviceExtension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME, false, &clockFeature);
	// #VKRay: Activate the ray tracing extension
	VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = nvvk::make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>();
	m_contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &asFeatures);
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
	m_contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);
	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = nvvk::make<VkPhysicalDeviceRayQueryFeaturesKHR>();
	m_contextInfo.addDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME, false, &rayQueryFeatures);
	m_contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	m_contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	m_contextInfo.addDeviceExtension(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME, true);
	// Extra queues for parallel load/build
	m_contextInfo.addRequestedQueue(m_contextInfo.defaultQueueGCT, 1, 1.0f);  // Loading scene - mipmap generation

	m_contextInfo.addDeviceExtension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);
	m_contextInfo.addDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	m_contextInfo.addDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
	VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR physicalDevicePipelineExecutableProprtiesFeaturesKHR = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR, nullptr, VK_TRUE };
	m_contextInfo.addDeviceExtension(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME, true,
		&physicalDevicePipelineExecutableProprtiesFeaturesKHR);

	m_contextInfo.addDeviceExtension("VK_KHR_spirv_1_4");
	m_contextInfo.addDeviceExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
	m_contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT m_fragmentShaderInterlockFeatures{
	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT,  // sType
	nullptr,                                                                   // pNext
	VK_TRUE,                                                                   // fragmentShaderSampleInterlock
	VK_TRUE,                                                                   // fragmentShaderPixelInterlock
	VK_FALSE };
	m_contextInfo.addDeviceExtension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME, true, &m_fragmentShaderInterlockFeatures);

	// VkPhysicalDeviceScalarBlockLayoutFeatures scalarLayout{
	// 	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES
	// };
	// scalarLayout.scalarBlockLayout = VK_TRUE;
	// m_contextInfo.addDeviceExtension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, true, &scalarLayout);

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
	rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;
	accelerationStructureFeatures.accelerationStructure = VK_TRUE;
	VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDevicesAddressFeatures{};
	enabledBufferDevicesAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
		enabledBufferDevicesAddressFeatures.pNext = &accelerationStructureFeatures;
	enabledBufferDevicesAddressFeatures.bufferDeviceAddress = VK_TRUE;
	VkPhysicalDeviceShaderAtomicFloatFeaturesEXT enabledAtomicsFeatures{};
	enabledAtomicsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
	enabledAtomicsFeatures.pNext = &enabledBufferDevicesAddressFeatures;
	enabledAtomicsFeatures.shaderBufferFloat32AtomicAdd = VK_TRUE;
	m_contextInfo.addDeviceExtension(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME, true, &enabledAtomicsFeatures);

	#define ENABLE_GPU_PRINTF //   Enabling printf in shaders
	// #extension GL_EXT_debug_printf
	// debugPrintfEXT("");
#ifdef ENABLE_GPU_PRINTF
	m_contextInfo.addDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
	std::vector<VkValidationFeatureEnableEXT>  enables{ VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
	std::vector<VkValidationFeatureDisableEXT> disables{};
	VkValidationFeaturesEXT                    features{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
	features.enabledValidationFeatureCount = static_cast<uint32_t>(enables.size());
	features.pEnabledValidationFeatures = enables.data();
	features.disabledValidationFeatureCount = static_cast<uint32_t>(disables.size());
	features.pDisabledValidationFeatures = disables.data();
	m_contextInfo.instanceCreateInfoExt = &features;
#endif  // ENABLE_GPU_PRINTF

	// Creating Vulkan base application
	//nvvk::Context m_context{};
	//m_context.init(m_contextInfo);


	m_contextInfo.verboseCompatibleDevices = false; // 关闭“兼容设备”列表打印
	m_contextInfo.verboseUsed              = false; // 关闭“当前使用的设备/扩展”打印
	m_contextInfo.verboseAvailable         = false; // 关闭“所有可用扩展”列表打印

	m_context.initInstance(m_contextInfo);

	//assert(asFeatures.accelerationStructure == VK_TRUE && rayQueryFeatures.rayQuery == VK_TRUE);
	// inital device
	auto compatibleDevices = m_context.getCompatibleDevices(m_contextInfo);  // Find all compatible devices
	assert(!compatibleDevices.empty());
	m_context.initDevice(compatibleDevices[0], m_contextInfo);  // Use first compatible device
	m_context.ignoreDebugMessage(1303270965);  // Bogus "general layout" perf warning.

	//auto  qGCT1 = m_context.createQueue(m_contextInfo.defaultQueueGCT, "GCT1", 1.0f);
	m_queues.push_back({ m_context.m_queueGCT.queue, m_context.m_queueGCT.familyIndex, m_context.m_queueGCT.queueIndex });
	//m_queues.push_back({ qGCT1.queue, qGCT1.familyIndex, qGCT1.queueIndex });
	m_queues.push_back({ m_context.m_queueC.queue, m_context.m_queueC.familyIndex, m_context.m_queueC.queueIndex });
	m_queues.push_back({ m_context.m_queueT.queue, m_context.m_queueT.familyIndex, m_context.m_queueT.queueIndex });

	// Initialize Vulkan function pointers
	vk::DynamicLoader dl;
	auto              vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(static_cast<vk::Instance>(m_context.m_instance));
	VULKAN_HPP_DEFAULT_DISPATCHER.init(static_cast<vk::Device>(m_context.m_device));

	//VULKAN_HPP_DEFAULT_DISPATCHER.init(m_context.m_instance, vkGetInstanceProcAddr, m_context.m_device);

	return 1;
}

int AppSetting::initSurface()
{
	VkResult err = glfwCreateWindowSurface(m_context.m_instance, m_window, nullptr, &m_surface);

	if (err != VK_SUCCESS)
	{
		assert(!"Failed to create a Window surface");
	}
	VkBool32 supportsPresent;
	vkGetPhysicalDeviceSurfaceSupportKHR(m_context.m_physicalDevice, m_context.m_queueGCT.familyIndex, m_surface, &supportsPresent);
	return supportsPresent == VK_TRUE;

	return 1;
}
