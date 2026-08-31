// Inital the information about Vulkan(extensions), device and surface window.
#pragma once

#ifdef USE_NEW_NVPRO_CORE
//#include "nvvkhl/appbase_vk.hpp"
//#include "nvvkhl/appbase_vkpp.hpp"
#else
//#include "appbase_vk.hpp"
//#include "appbase_vkpp.hpp"
#include <nvvk/appwindowprofiler_vk.hpp>
#endif

#include <vulkan/vulkan.hpp>
#include "nvvk/context_vk.hpp"
#include "nvh/gltfscene.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/profiler_vk.hpp"
#include "nvvk/raytraceKHR_vk.hpp"
#include "nvvk/raypicker_vk.hpp"
#include "nvvk/structs_vk.hpp"

#include "queue.h"
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
/// <summary>
///
/// </summary>
//



// GLFW Callback functions
static void onErrorCallback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

class AppSetting
{
public:

	void init();
	void destroy();

	nvvk::Context   m_context{};
	std::vector<nvvk::Queue> m_queues{};

	bool m_ifDisplay{ false };
	int SAMPLE_WIDTH = 960;
	int SAMPLE_HEIGHT = 720;
private:
	int initWindow();
	int initVulkan();
	int initSurface();
	nvvk::ContextCreateInfo m_contextInfo = nvvk::ContextCreateInfo(true);
	int             gpuDeviceIndex{ 0 };
	GLFWwindow* m_window;
	VkSurfaceKHR    m_surface{};
};
