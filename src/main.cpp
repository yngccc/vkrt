#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <stack>
#include <string>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <tuple>
#include <numbers>
using namespace std::literals;

#include <shellscalingapi.h>
#define _XM_SSE4_INTRINSICS_
#include <directxmath.h>
#include <directxcolors.h>
using namespace DirectX;

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>

#define module shaderModule
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#undef module

#include <fbx/fbxsdk.h>
#undef snprintf

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
//#include <stb/stb_ds.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.cpp>
#include <imgui/imgui_draw.cpp>
#include <imgui/imgui_widgets.cpp>
#include <imgui/imgui_tables.cpp>

#include <nlohmann/json.hpp>

typedef unsigned int uint;
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

#define countof(array) (sizeof(array) / sizeof(array[0]))

size_t operator""_kb(size_t n) { return 1024 * n; }
size_t operator""_mb(size_t n) { return 1024 * 1024 * n; }
size_t operator""_gb(size_t n) { return 1024 * 1024 * 1024 * n; }
long double operator""_deg(long double deg) { return deg / 180.0 * std::numbers::pi; }

size_t align(size_t x, size_t n) { size_t remainder = x % n; return remainder == 0 ? x : x + (n - remainder); }
uint8* align(uint8* ptr, size_t n) { return (uint8*)align((intptr_t)ptr, n); }

void setCurrentDirToExeDir() {
	wchar_t buf[512];
	DWORD n = GetModuleFileNameW(nullptr, buf, countof(buf));
	assert(n > 0 && n < countof(buf));
	std::filesystem::path path(buf);
	std::filesystem::path parentPath = path.parent_path();
	assert(SetCurrentDirectoryW(parentPath.c_str()));
}

std::vector<char> readFile(const std::filesystem::path& path) {
	std::fstream file(path, std::ios::in | std::ios::binary);
	assert(file.is_open());
	std::vector<char> data(std::istreambuf_iterator<char>{file}, {});
	return data;
}

VkBool32 vulkanDebugCallBack(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* cbData, void* userData) {
	const char* msg = cbData->pMessage;
	__debugbreak();
	return false;
}

VkDebugUtilsMessengerEXT vkDebugUtilsMessenger = nullptr;
void* vulkanDebugCallBackUserData = nullptr;

PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger = nullptr;
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName = nullptr;

void loadVkInstanceProcs(VkInstance instance) {
#define getInstanceProcAddrEXT(name) name = (PFN_##name##EXT)vkGetInstanceProcAddr(instance, #name "EXT"); assert(name);
	getInstanceProcAddrEXT(vkCreateDebugUtilsMessenger);
	getInstanceProcAddrEXT(vkDebugMarkerSetObjectName);
#undef getInstanceProcAddrKHR
	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	debugUtilsMessengerCreateInfo.pfnUserCallback = vulkanDebugCallBack;
	debugUtilsMessengerCreateInfo.pUserData = &vulkanDebugCallBackUserData;
	vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, NULL, &vkDebugUtilsMessenger);
}

PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelines = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandles = nullptr;
PFN_vkCmdTraceRaysKHR vkCmdTraceRays = nullptr;
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructure = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizes = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddress = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructures = nullptr;

void loadVkDeviceProcs(VkDevice device) {
#define getDeviceProcAddrKHR(name) name = (PFN_##name##KHR)vkGetDeviceProcAddr(device, #name "KHR"); assert(name);
	getDeviceProcAddrKHR(vkCreateRayTracingPipelines);
	getDeviceProcAddrKHR(vkGetRayTracingShaderGroupHandles);
	getDeviceProcAddrKHR(vkCmdTraceRays);
	getDeviceProcAddrKHR(vkCreateAccelerationStructure);
	getDeviceProcAddrKHR(vkGetAccelerationStructureBuildSizes);
	getDeviceProcAddrKHR(vkGetAccelerationStructureDeviceAddress);
	getDeviceProcAddrKHR(vkCmdBuildAccelerationStructures);
#undef getDeviceProcAddr
}

const int vkSwapChainImageCount = 2;
const int vkMaxFrameInFlight = 2;

struct Vulkan {
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;

	uint32 graphicsComputeQueueFamilyIndex;
	uint32 transferQueueFamilyIndex;
	VkQueue graphicsQueue;
	VkQueue computeQueue;
	VkQueue transferQueue;
	VkCommandPool graphicsComputeCmdPool;
	VkCommandPool transferCmdPool;

	VkSwapchainKHR swapChain;
	VkRenderPass swapChainRenderPass;
	struct {
		VkImage image;
		VkImageView imageView;
		VkFramebuffer frameBuffer;
	} swapChainImageFramebuffers[vkSwapChainImageCount];

	uint32 hostVisibleCoherentMemoryType;
	uint32 deviceLocalMemoryType;

	struct Memory {
		VkDeviceMemory memory;
		uint64 capacity;
		uint64 offset;
	};
	Memory stagingBuffersMemory;
	Memory gpuColorBuffersMemory;
	Memory gpuTexturesMemory;
	Memory gpuBuffersMemory;

	VkBuffer stagingBuffer;

	std::pair<VkImage, VkImageView> blankTexture;
	std::pair<VkImage, VkImageView> imguiTexture;
	std::pair<VkImage, VkImageView> accumulationColorBuffer;
	std::pair<VkImage, VkImageView> colorBuffer;

	VkSampler trilinearSampler;

	struct {
		VkCommandBuffer graphicsCmdBuf;
		VkCommandBuffer computeCmdBuf;
		VkCommandBuffer transferCmdBuf;
		VkSemaphore swapChainImageSemaphore;
		VkSemaphore queueSemaphore;
		VkFence queueFence;
		VkDescriptorPool descriptorPool;
		Memory uniformBuffersMemory;
		VkBuffer rayTracingConstantBuffer;
		uint64 rayTracingConstantBufferMemoryOffset;
		uint64 rayTracingConstantBufferMemorySize;
		VkBuffer imguiVertBuffer;
		uint64 imguiVertBufferMemoryOffset;
		uint64 imguiVertBufferMemorySize;
		VkBuffer imguiIndexBuffer;
		uint64 imguiIndexBufferMemoryOffset;
		uint64 imguiIndexBufferMemorySize;
	} frames[vkMaxFrameInFlight];
	uint64 frameCount;
	uint64 frameIndex;
	uint32 accumulatedFrameCount;

	VkDescriptorSetLayout swapChainDescriptorSet0Layout;
	VkPipelineLayout swapChainPipelineLayout;
	VkPipeline swapChainPipeline;

	VkDescriptorSetLayout imguiDescriptorSet0Layout;
	VkPipelineLayout imguiPipelineLayout;
	VkPipeline imguiPipeline;

	VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR pathTracePipelineProps;
	VkDescriptorSetLayout pathTraceDescriptorSet0Layout;
	uint32 pathTraceDescriptorSet0TextureCount;
	VkPipelineLayout pathTracePipelineLayout;
	VkPipeline pathTracePipeline;
	VkBuffer pathTraceSBTBuffer;
	VkStridedDeviceAddressRegionKHR pathTraceSBTBufferRayGenDeviceAddress;
	VkStridedDeviceAddressRegionKHR pathTraceSBTBufferMissDeviceAddress;
	VkStridedDeviceAddressRegionKHR pathTraceSBTBufferHitGroupDeviceAddress;
	VkStridedDeviceAddressRegionKHR pathTraceSBTBufferCallableDeviceAddress;

	static Vulkan* create(SDL_Window* sdlWindow, bool validation) {
		Vulkan* vk = new Vulkan();

		SDL_SysWMinfo sdlWindowInfo;
		SDL_VERSION(&sdlWindowInfo.version);
		assert(SDL_GetWindowWMInfo(sdlWindow, &sdlWindowInfo) == SDL_TRUE);
		int windowWidth, windowHeight;
		SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
		{
			VkApplicationInfo appInfo = {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.apiVersion = VK_MAKE_VERSION(1, 2, 0)
			};
			const char* instanceExtensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_utils", "VK_EXT_debug_report" };
			const char* validationLayer = "VK_LAYER_KHRONOS_validation";
			VkValidationFeatureEnableEXT validationFeaturesEnabled[] = {
				// VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
				// VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
				VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
			};
			VkValidationFeaturesEXT validationFeatures = {
				.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
				.enabledValidationFeatureCount = countof(validationFeaturesEnabled),
				.pEnabledValidationFeatures = validationFeaturesEnabled
			};
			VkInstanceCreateInfo instanceCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pNext = &validationFeatures,
				.pApplicationInfo = &appInfo,
				.enabledLayerCount = validation ? 1u : 0u,
				.ppEnabledLayerNames = &validationLayer,
				.enabledExtensionCount = countof(instanceExtensions),
				.ppEnabledExtensionNames = instanceExtensions
			};
			vkCreateInstance(&instanceCreateInfo, nullptr, &vk->instance);
			loadVkInstanceProcs(vk->instance);
		}
		{
			VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
			surfaceCreateInfo.hinstance = sdlWindowInfo.info.win.hinstance;
			surfaceCreateInfo.hwnd = sdlWindowInfo.info.win.window;
			vkCreateWin32SurfaceKHR(vk->instance, &surfaceCreateInfo, nullptr, &vk->surface);
		}
		{
			uint32 physicalDeviceCount;
			vkEnumeratePhysicalDevices(vk->instance, &physicalDeviceCount, nullptr);
			std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
			vkEnumeratePhysicalDevices(vk->instance, &physicalDeviceCount, physicalDevices.data());

			for (auto& pDevice : physicalDevices) {
				VkPhysicalDeviceProperties props;
				vkGetPhysicalDeviceProperties(pDevice, &props);
				if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
					vk->physicalDevice = pDevice;
					break;
				}
			}
			assert(vk->physicalDevice);
		}
		{
			uint32 queueFamilyPropCount;
			vkGetPhysicalDeviceQueueFamilyProperties(vk->physicalDevice, &queueFamilyPropCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
			vkGetPhysicalDeviceQueueFamilyProperties(vk->physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());
			vk->graphicsComputeQueueFamilyIndex = UINT_MAX;
			vk->transferQueueFamilyIndex = UINT_MAX;
			for (uint32 i = 0; i < queueFamilyPropCount; i++) {
				auto& props = queueFamilyProps[i];
				if ((props.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
					(props.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
					(props.queueCount >= 2)) {
					VkBool32 presentSupport = false;
					vkGetPhysicalDeviceSurfaceSupportKHR(vk->physicalDevice, i, vk->surface, &presentSupport);
					if (presentSupport) {
						vk->graphicsComputeQueueFamilyIndex = i;
						props.queueCount -= 2;
						break;
					}
				}
			}
			for (uint32 i = 0; i < queueFamilyProps.size(); i++) {
				auto& props = queueFamilyProps[i];
				if (!(props.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
					!(props.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
					(props.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
					(props.queueCount > 0)) {
					vk->transferQueueFamilyIndex = i;
					props.queueCount -= 1;
					break;
				}
			}
			if (vk->transferQueueFamilyIndex == UINT_MAX) {
				for (uint32 i = 0; i < queueFamilyProps.size(); i++) {
					auto& props = queueFamilyProps[i];
					if ((props.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
						(props.queueCount > 0)) {
						vk->transferQueueFamilyIndex = i;
						props.queueCount -= 1;
						break;
					}
				}
			}
			assert(vk->graphicsComputeQueueFamilyIndex != UINT_MAX);
			assert(vk->transferQueueFamilyIndex != UINT_MAX);
		}
		{
			uint32 deviceExtensionPropCount;
			vkEnumerateDeviceExtensionProperties(vk->physicalDevice, nullptr, &deviceExtensionPropCount, nullptr);
			std::vector<VkExtensionProperties> deviceExtensionProps(deviceExtensionPropCount);
			vkEnumerateDeviceExtensionProperties(vk->physicalDevice, nullptr, &deviceExtensionPropCount, deviceExtensionProps.data());

			const char* enabledDeviceExtensions[] = {
				"VK_KHR_swapchain", "VK_KHR_acceleration_structure", "VK_KHR_ray_tracing_pipeline", "VK_KHR_deferred_host_operations"
			};
			VkPhysicalDevice16BitStorageFeatures Storage16BitsFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
				.pNext = nullptr
			};
			VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
				.pNext = &Storage16BitsFeatures
			};
			VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
				.pNext = &descriptorIndexFeatures,
			};
			VkPhysicalDeviceBufferDeviceAddressFeaturesEXT bufferDeviceAddressFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
				.pNext = &scalarBlockLayoutFeatures
			};
			VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
				.pNext = &bufferDeviceAddressFeatures
			};
			VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
				.pNext = &accelerationStructureFeatures
			};
			VkPhysicalDeviceFeatures2 features = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &rayTracingPipelineFeatures
			};
			vkGetPhysicalDeviceFeatures2(vk->physicalDevice, &features);
			assert(features.features.shaderSampledImageArrayDynamicIndexing);

			vk->accelerationStructureProperties = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
				.pNext = nullptr
			};
			vk->pathTracePipelineProps = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
				.pNext = &vk->accelerationStructureProperties
			};
			VkPhysicalDeviceProperties2 properties = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
				.pNext = &vk->pathTracePipelineProps
			};
			vkGetPhysicalDeviceProperties2(vk->physicalDevice, &properties);
			assert(properties.properties.limits.maxDescriptorSetSampledImages > 10000);

			float queuePriorities[3] = { 1.0f, 0.5f, 0.5f };
			VkDeviceQueueCreateInfo queueCreateInfos[2] = {
				{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.queueCount = 2,
					.pQueuePriorities = queuePriorities
				},
				{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
				}
			};
			uint32 queueCreateInfoCount = 1;
			if (vk->transferQueueFamilyIndex == vk->graphicsComputeQueueFamilyIndex) {
				queueCreateInfos[0].queueCount = 3;
			}
			else {
				queueCreateInfoCount = 2;
				queueCreateInfos[1].queueFamilyIndex = vk->transferQueueFamilyIndex;
				queueCreateInfos[1].queueCount = 1;
				queueCreateInfos[1].pQueuePriorities = &queuePriorities[2];
			}
			VkDeviceCreateInfo deviceCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &rayTracingPipelineFeatures,
				.queueCreateInfoCount = queueCreateInfoCount,
				.pQueueCreateInfos = queueCreateInfos,
				.enabledExtensionCount = countof(enabledDeviceExtensions),
				.ppEnabledExtensionNames = enabledDeviceExtensions,
				.pEnabledFeatures = &features.features
			};
			vkCreateDevice(vk->physicalDevice, &deviceCreateInfo, nullptr, &vk->device);
			loadVkDeviceProcs(vk->device);
		}
		{
			vkGetDeviceQueue(vk->device, vk->graphicsComputeQueueFamilyIndex, 0, &vk->graphicsQueue);
			vkGetDeviceQueue(vk->device, vk->graphicsComputeQueueFamilyIndex, 1, &vk->computeQueue);
			if (vk->transferQueueFamilyIndex == vk->graphicsComputeQueueFamilyIndex) {
				vkGetDeviceQueue(vk->device, vk->transferQueueFamilyIndex, 2, &vk->transferQueue);
			}
			else {
				vkGetDeviceQueue(vk->device, vk->transferQueueFamilyIndex, 0, &vk->transferQueue);
			}

			VkCommandPoolCreateInfo commandPoolCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = vk->graphicsComputeQueueFamilyIndex
			};
			vkCreateCommandPool(vk->device, &commandPoolCreateInfo, nullptr, &vk->graphicsComputeCmdPool);

			commandPoolCreateInfo.queueFamilyIndex = vk->transferQueueFamilyIndex;
			vkCreateCommandPool(vk->device, &commandPoolCreateInfo, nullptr, &vk->transferCmdPool);
		}
		{
			uint32 presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physicalDevice, vk->surface, &presentModeCount, nullptr);
			std::vector<VkPresentModeKHR> presentModes(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physicalDevice, vk->surface, &presentModeCount, presentModes.data());

			VkSurfaceCapabilitiesKHR surfaceCapabilities;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physicalDevice, vk->surface, &surfaceCapabilities);
			assert(surfaceCapabilities.currentExtent.width == windowWidth && surfaceCapabilities.currentExtent.height == windowHeight);

			uint surfaceFormatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physicalDevice, vk->surface, &surfaceFormatCount, nullptr);
			std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physicalDevice, vk->surface, &surfaceFormatCount, surfaceFormats.data());
			assert(std::any_of(surfaceFormats.begin(), surfaceFormats.end(),
				[](VkSurfaceFormatKHR format) {
					return format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
				}
			) && "device surface does not support VK_FORMAT_B8G8R8A8_UNORM and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR");

			VkSwapchainCreateInfoKHR swapChainCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.surface = vk->surface,
				.minImageCount = vkSwapChainImageCount,
				.imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
				.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
				.imageExtent = { (uint32)windowWidth, (uint32)windowHeight },
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
				.clipped = true
			};
			vkCreateSwapchainKHR(vk->device, &swapChainCreateInfo, nullptr, &vk->swapChain);
		}
		{
			VkAttachmentDescription attachment = {
				.format = VK_FORMAT_B8G8R8A8_UNORM,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			};
			VkAttachmentReference colorAttachmentRef = {
				.attachment = 0,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};
			VkSubpassDescription subpassDescription = {
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 1,
				.pColorAttachments = &colorAttachmentRef
			};
			VkSubpassDependency subpassDependencies[2] = {
				{
					.srcSubpass = VK_SUBPASS_EXTERNAL,
					.dstSubpass = 0,
					.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
				},
				{
					.srcSubpass = 0,
					.dstSubpass = VK_SUBPASS_EXTERNAL,
					.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = 0,
					.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
				}
			};
			VkRenderPassCreateInfo renderPassCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 1,
				.pAttachments = &attachment,
				.subpassCount = 1,
				.pSubpasses = &subpassDescription,
				.dependencyCount = countof(subpassDependencies),
				.pDependencies = subpassDependencies
			};
			vkCreateRenderPass(vk->device, &renderPassCreateInfo, nullptr, &vk->swapChainRenderPass);
		}
		{
			uint32 swapChainImageCount = 0;
			vkGetSwapchainImagesKHR(vk->device, vk->swapChain, &swapChainImageCount, nullptr);
			assert(swapChainImageCount == vkSwapChainImageCount);
			VkImage swapChainImages[vkSwapChainImageCount];
			vkGetSwapchainImagesKHR(vk->device, vk->swapChain, &swapChainImageCount, swapChainImages);
			for (size_t i = 0; i < countof(vk->swapChainImageFramebuffers); i++) {
				auto& imageFramebuffer = vk->swapChainImageFramebuffers[i];
				imageFramebuffer.image = swapChainImages[i];

				VkImageViewCreateInfo imageCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = imageFramebuffer.image,
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = VK_FORMAT_B8G8R8A8_UNORM,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				};
				vkCreateImageView(vk->device, &imageCreateInfo, nullptr, &imageFramebuffer.imageView);

				VkFramebufferCreateInfo framebufferCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					.renderPass = vk->swapChainRenderPass,
					.attachmentCount = 1,
					.pAttachments = &imageFramebuffer.imageView,
					.width = (uint32)windowWidth,
					.height = (uint32)windowHeight,
					.layers = 1
				};
				vkCreateFramebuffer(vk->device, &framebufferCreateInfo, nullptr, &imageFramebuffer.frameBuffer);
			}
		}
		{
			VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
			vkGetPhysicalDeviceMemoryProperties(vk->physicalDevice, &physicalDeviceMemoryProperties);

			vk->hostVisibleCoherentMemoryType = UINT32_MAX;
			vk->deviceLocalMemoryType = UINT32_MAX;
			for (uint32 i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
				VkMemoryType& mtype = physicalDeviceMemoryProperties.memoryTypes[i];
				if (mtype.propertyFlags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
					vk->deviceLocalMemoryType = i;
				}
				else if (mtype.propertyFlags == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					vk->hostVisibleCoherentMemoryType = i;
				}
				else if (mtype.propertyFlags == (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
				}
			}
			assert(vk->hostVisibleCoherentMemoryType != UINT32_MAX);
			assert(vk->deviceLocalMemoryType != UINT32_MAX);
			{
				VkBufferCreateInfo bufferCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					.size = 512_mb,
					.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
				};
				vkCreateBuffer(vk->device, &bufferCreateInfo, nullptr, &vk->stagingBuffer);
				VkMemoryRequirements memoryRequirements;
				vkGetBufferMemoryRequirements(vk->device, vk->stagingBuffer, &memoryRequirements);
				VkMemoryAllocateInfo memoryAllocateInfo = {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.allocationSize = memoryRequirements.size,
					.memoryTypeIndex = vk->hostVisibleCoherentMemoryType
				};
				vkAllocateMemory(vk->device, &memoryAllocateInfo, nullptr, &vk->stagingBuffersMemory.memory);
				vkBindBufferMemory(vk->device, vk->stagingBuffer, vk->stagingBuffersMemory.memory, 0);
				vk->stagingBuffersMemory.capacity = memoryAllocateInfo.allocationSize;
				vk->stagingBuffersMemory.offset = memoryAllocateInfo.allocationSize;
			}
			{
				vk->createColorBuffers(windowWidth, windowHeight);
			}
			{
				VkMemoryAllocateInfo memoryAllocateInfo = {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.allocationSize = 4_gb,
					.memoryTypeIndex = vk->deviceLocalMemoryType
				};
				vkAllocateMemory(vk->device, &memoryAllocateInfo, nullptr, &vk->gpuTexturesMemory.memory);
				vk->gpuTexturesMemory.capacity = memoryAllocateInfo.allocationSize;
				vk->gpuTexturesMemory.offset = 0;
			}
			{
				VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
					.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
				};
				VkMemoryAllocateInfo memoryAllocateInfo = {
						.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
						.pNext = &memoryAllocateFlagsInfo,
						.allocationSize = 1_gb,
						.memoryTypeIndex = vk->deviceLocalMemoryType
				};
				vkAllocateMemory(vk->device, &memoryAllocateInfo, nullptr, &vk->gpuBuffersMemory.memory);
				vk->gpuBuffersMemory.capacity = memoryAllocateInfo.allocationSize;
				vk->gpuBuffersMemory.offset = 0;
			}
		}
		{
			VkSamplerCreateInfo samplerCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR
			};
			vkCreateSampler(vk->device, &samplerCreateInfo, nullptr, &vk->trilinearSampler);
		}
		{
			for (auto& frame : vk->frames) {
				VkCommandBufferAllocateInfo commandBufferAllocInfo = {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = vk->graphicsComputeCmdPool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1
				};
				vkAllocateCommandBuffers(vk->device, &commandBufferAllocInfo, &frame.graphicsCmdBuf);
				vkAllocateCommandBuffers(vk->device, &commandBufferAllocInfo, &frame.computeCmdBuf);
				commandBufferAllocInfo.commandPool = vk->transferCmdPool;
				vkAllocateCommandBuffers(vk->device, &commandBufferAllocInfo, &frame.transferCmdBuf);

				VkSemaphoreCreateInfo semaphoreCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
				};
				vkCreateSemaphore(vk->device, &semaphoreCreateInfo, nullptr, &frame.swapChainImageSemaphore);
				vkCreateSemaphore(vk->device, &semaphoreCreateInfo, nullptr, &frame.queueSemaphore);

				VkFenceCreateInfo fenceCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
					.flags = VK_FENCE_CREATE_SIGNALED_BIT
				};
				vkCreateFence(vk->device, &fenceCreateInfo, nullptr, &frame.queueFence);

				VkDescriptorPoolSize poolSizes[] = {
					{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1000 },
					{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000 },
					{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1000 }
				};
				VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
					.maxSets = 1000,
					.poolSizeCount = countof(poolSizes),
					.pPoolSizes = poolSizes
				};
				vkCreateDescriptorPool(vk->device, &descriptorPoolCreateInfo, nullptr, &frame.descriptorPool);

				VkMemoryAllocateInfo memoryAllocateInfo = {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.allocationSize = 64_mb,
					.memoryTypeIndex = vk->hostVisibleCoherentMemoryType
				};
				vkAllocateMemory(vk->device, &memoryAllocateInfo, nullptr, &frame.uniformBuffersMemory.memory);
				frame.uniformBuffersMemory.capacity = memoryAllocateInfo.allocationSize;
				frame.uniformBuffersMemory.offset = 0;

				frame.rayTracingConstantBufferMemorySize = 1_kb;
				std::tie(frame.rayTracingConstantBuffer, frame.rayTracingConstantBufferMemoryOffset) =
					vk->createBuffer(&frame.uniformBuffersMemory, frame.rayTracingConstantBufferMemorySize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

				frame.imguiVertBufferMemorySize = 2_mb;
				std::tie(frame.imguiVertBuffer, frame.imguiVertBufferMemoryOffset) =
					vk->createBuffer(&frame.uniformBuffersMemory, frame.imguiVertBufferMemorySize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

				frame.imguiIndexBufferMemorySize = 1_mb;
				std::tie(frame.imguiIndexBuffer, frame.imguiIndexBufferMemoryOffset) =
					vk->createBuffer(&frame.uniformBuffersMemory, frame.imguiIndexBufferMemorySize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
			}
		}
		{
			vk->blankTexture = vk->createImage2DAndView(&vk->gpuTexturesMemory,
				4, 4, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
			);

			ImFont* imFont = ImGui::GetIO().Fonts->AddFontDefault();
			assert(imFont);
			uint8* imguiTextureData;
			int imguiTextureWidth, imguiTextureHeight;
			ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&imguiTextureData, &imguiTextureWidth, &imguiTextureHeight);

			vk->imguiTexture = vk->createImage2DAndView(&vk->gpuTexturesMemory, imguiTextureWidth, imguiTextureHeight,
				VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			);

			uint64 stagingBufferSize = 4 * 16 + 4 * imguiTextureWidth * imguiTextureHeight;
			uint8* stagingBufferPtr = nullptr;
			vkMapMemory(vk->device, vk->stagingBuffersMemory.memory, 0, stagingBufferSize, 0, (void**)&stagingBufferPtr);
			memset(stagingBufferPtr, UINT8_MAX, 4 * 16);
			memcpy(stagingBufferPtr + 4 * 16, imguiTextureData, imguiTextureWidth * imguiTextureHeight * 4);
			vkUnmapMemory(vk->device, vk->stagingBuffersMemory.memory);

			auto& cmdBuf = vk->frames[vk->frameIndex].graphicsCmdBuf;
			VkCommandBufferBeginInfo cmdBufBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkResetCommandBuffer(cmdBuf, 0);
			vkBeginCommandBuffer(cmdBuf, &cmdBufBeginInfo);

			VkImageMemoryBarrier imageBarriers0[] = {
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = vk->blankTexture.first,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = vk->imguiTexture.first,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = 0,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_GENERAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = vk->accumulationColorBuffer.first,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = 0,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = vk->colorBuffer.first,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				},
			};
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, countof(imageBarriers0), imageBarriers0);

			VkBufferImageCopy bufferImageCopys[] = {
				{
					.bufferOffset = 0,
					.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
					.imageExtent = { 4, 4, 1 }
				},
				{
					.bufferOffset = 4 * 16,
					.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
					.imageExtent = { (uint32)imguiTextureWidth, (uint32)imguiTextureHeight, 1 }
				},
			};
			vkCmdCopyBufferToImage(cmdBuf, vk->stagingBuffer, vk->blankTexture.first, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopys[0]);
			vkCmdCopyBufferToImage(cmdBuf, vk->stagingBuffer, vk->imguiTexture.first, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopys[1]);

			VkImageMemoryBarrier ImageBarriers1[] = {
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = vk->blankTexture.first,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = vk->imguiTexture.first,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				},
			};
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, countof(ImageBarriers1), ImageBarriers1);

			vkEndCommandBuffer(cmdBuf);
			VkSubmitInfo queueSubmitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &cmdBuf
			};
			vkQueueSubmit(vk->graphicsQueue, 1, &queueSubmitInfo, nullptr);
			vkQueueWaitIdle(vk->graphicsQueue);
		}
		{
			VkShaderModuleCreateInfo shaderModuleCreateInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

			std::vector<char> vertexShaderCode;
			std::vector<char> fragmentShaderCode;
			VkPipelineShaderStageCreateInfo vertFragStages[] = {
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_VERTEX_BIT,
					.pName = "main"
				},
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.pName = "main"
				}
			};

			VkPipelineVertexInputStateCreateInfo vertexInputState{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
			VkPipelineViewportStateCreateInfo viewPortState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
			VkPipelineRasterizationStateCreateInfo rasterStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
			VkPipelineMultisampleStateCreateInfo multisampleState{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
			VkPipelineDepthStencilStateCreateInfo depthStencilState{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
			VkPipelineColorBlendStateCreateInfo colorBlendState{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
			VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

			VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.stageCount = countof(vertFragStages),
				.pStages = vertFragStages,
				.pVertexInputState = &vertexInputState,
				.pInputAssemblyState = &inputAssemblyState,
				.pViewportState = &viewPortState,
				.pRasterizationState = &rasterStateCreateInfo,
				.pMultisampleState = &multisampleState,
				.pDepthStencilState = &depthStencilState,
				.pColorBlendState = &colorBlendState,
				.pDynamicState = &dynamicState
			};

			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

			vertexShaderCode = readFile("swapChain_vert.spv");
			fragmentShaderCode = readFile("swapChain_frag.spv");
			shaderModuleCreateInfo.codeSize = vertexShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)vertexShaderCode.data();
			vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &vertFragStages[0].shaderModule);
			shaderModuleCreateInfo.codeSize = fragmentShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)fragmentShaderCode.data();
			vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &vertFragStages[1].shaderModule);

			inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			VkViewport viewport = { 0, 0, float(windowWidth), float(windowHeight), 0, 1 };
			VkRect2D scissor = { {0, 0}, {(uint32)windowWidth, (uint32)windowHeight} };
			viewPortState.viewportCount = 1;
			viewPortState.scissorCount = 1;
			viewPortState.pViewports = &viewport;
			viewPortState.pScissors = &scissor;

			multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
			};
			colorBlendState.attachmentCount = 1;
			colorBlendState.pAttachments = &colorBlendAttachmentState;

			VkDynamicState swapChainDynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			dynamicState.dynamicStateCount = countof(swapChainDynamicStates);
			dynamicState.pDynamicStates = swapChainDynamicStates;

			VkDescriptorSetLayoutBinding swapChainDescriptorSetBindings[] = {
				{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
				{1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
			};
			descriptorSetLayoutCreateInfo.bindingCount = countof(swapChainDescriptorSetBindings);
			descriptorSetLayoutCreateInfo.pBindings = swapChainDescriptorSetBindings;
			vkCreateDescriptorSetLayout(vk->device, &descriptorSetLayoutCreateInfo, nullptr, &vk->swapChainDescriptorSet0Layout);

			pipelineLayoutCreateInfo.setLayoutCount = 1;
			pipelineLayoutCreateInfo.pSetLayouts = &vk->swapChainDescriptorSet0Layout;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
			vkCreatePipelineLayout(vk->device, &pipelineLayoutCreateInfo, nullptr, &vk->swapChainPipelineLayout);

			graphicsPipelineCreateInfo.layout = vk->swapChainPipelineLayout;
			graphicsPipelineCreateInfo.renderPass = vk->swapChainRenderPass;
			graphicsPipelineCreateInfo.subpass = 0;
			vkCreateGraphicsPipelines(vk->device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &vk->swapChainPipeline);

			vertexShaderCode = readFile("imgui_vert.spv");
			fragmentShaderCode = readFile("imgui_frag.spv");
			shaderModuleCreateInfo.codeSize = vertexShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)vertexShaderCode.data();
			vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &vertFragStages[0].shaderModule);
			shaderModuleCreateInfo.codeSize = fragmentShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)fragmentShaderCode.data();
			vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &vertFragStages[1].shaderModule);

			VkVertexInputBindingDescription imguiVertexBindingDescription = {
				.binding = 0, .stride = sizeof(ImDrawVert), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			};
			VkVertexInputAttributeDescription imguiVertexAttributeDescriptions[] = {
				{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0 },
				{.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 8 },
				{.location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = 16 }
			};
			vertexInputState.vertexBindingDescriptionCount = 1;
			vertexInputState.pVertexBindingDescriptions = &imguiVertexBindingDescription;
			vertexInputState.vertexAttributeDescriptionCount = countof(imguiVertexAttributeDescriptions);
			vertexInputState.pVertexAttributeDescriptions = imguiVertexAttributeDescriptions;

			colorBlendAttachmentState.blendEnable = true;
			colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VkDynamicState imguiDynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			dynamicState.dynamicStateCount = countof(imguiDynamicStates);
			dynamicState.pDynamicStates = imguiDynamicStates;

			VkDescriptorSetLayoutBinding imguiDescriptorSetBindings[] = {
				{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
			};
			descriptorSetLayoutCreateInfo.bindingCount = countof(imguiDescriptorSetBindings);
			descriptorSetLayoutCreateInfo.pBindings = imguiDescriptorSetBindings;
			vkCreateDescriptorSetLayout(vk->device, &descriptorSetLayoutCreateInfo, nullptr, &vk->imguiDescriptorSet0Layout);

			VkPushConstantRange imguiPushConstantRange = {
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = 16
			};
			pipelineLayoutCreateInfo.setLayoutCount = 1;
			pipelineLayoutCreateInfo.pSetLayouts = &vk->imguiDescriptorSet0Layout;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pipelineLayoutCreateInfo.pPushConstantRanges = &imguiPushConstantRange;
			vkCreatePipelineLayout(vk->device, &pipelineLayoutCreateInfo, nullptr, &vk->imguiPipelineLayout);

			graphicsPipelineCreateInfo.layout = vk->imguiPipelineLayout;
			graphicsPipelineCreateInfo.renderPass = vk->swapChainRenderPass;
			graphicsPipelineCreateInfo.subpass = 0;
			vkCreateGraphicsPipelines(vk->device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &vk->imguiPipeline);
		}
		{
			vk->pathTraceDescriptorSet0TextureCount = 1024;
			VkDescriptorSetLayoutBinding descriptorSetBindings[] = {
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = vk->pathTraceDescriptorSet0TextureCount, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
			};
			for (uint32 i = 0; i < countof(descriptorSetBindings); i++) {
				descriptorSetBindings[i].binding = (uint32)i;
				if (descriptorSetBindings[i].descriptorCount == 0) {
					descriptorSetBindings[i].descriptorCount = 1;
				}
			}

			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = countof(descriptorSetBindings),
				.pBindings = descriptorSetBindings,
			};
			vkCreateDescriptorSetLayout(vk->device, &descriptorSetLayoutCreateInfo, nullptr, &vk->pathTraceDescriptorSet0Layout);

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.setLayoutCount = 1,
				.pSetLayouts = &vk->pathTraceDescriptorSet0Layout,
			};
			vkCreatePipelineLayout(vk->device, &pipelineLayoutCreateInfo, nullptr, &vk->pathTracePipelineLayout);

			VkShaderModuleCreateInfo shaderModuleCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			};
			VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
				{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .pName = "main" },
				{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .pName = "main" },
				{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .pName = "main" }
			};
			std::vector<char> rayGenShaderCode = readFile("pathTrace_rgen.spv");
			std::vector<char> rayMissShaderCode = readFile("pathTrace_rmiss.spv");
			std::vector<char> rayChitShaderCode = readFile("pathTrace_primary_rchit.spv");
			//std::vector<char> rayChitShaderCode = readFile("pathTrace_rchit.spv");

			shaderModuleCreateInfo.codeSize = rayGenShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)rayGenShaderCode.data();
			vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &shaderStageCreateInfos[0].shaderModule);
			shaderModuleCreateInfo.codeSize = rayMissShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)rayMissShaderCode.data();
			vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &shaderStageCreateInfos[1].shaderModule);
			shaderModuleCreateInfo.codeSize = rayChitShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)rayChitShaderCode.data();
			vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &shaderStageCreateInfos[2].shaderModule);

			VkRayTracingShaderGroupCreateInfoKHR shaderGroups[] = {
				{
					.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
					.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					.generalShader = 0,
					.closestHitShader = VK_SHADER_UNUSED_KHR,
					.anyHitShader = VK_SHADER_UNUSED_KHR,
					.intersectionShader = VK_SHADER_UNUSED_KHR
				},
				{
					.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
					.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
					.generalShader = 1,
					.closestHitShader = VK_SHADER_UNUSED_KHR,
					.anyHitShader = VK_SHADER_UNUSED_KHR,
					.intersectionShader = VK_SHADER_UNUSED_KHR
				},
				{
					.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
					.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
					.generalShader = VK_SHADER_UNUSED_KHR,
					.closestHitShader = 2,
					.anyHitShader = VK_SHADER_UNUSED_KHR,
					.intersectionShader = VK_SHADER_UNUSED_KHR
				}
			};

			VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
				.stageCount = countof(shaderStageCreateInfos),
				.pStages = shaderStageCreateInfos,
				.groupCount = countof(shaderGroups),
				.pGroups = shaderGroups,
				.maxPipelineRayRecursionDepth = 3,
				.layout = vk->pathTracePipelineLayout
			};
			vkCreateRayTracingPipelines(vk->device, nullptr, nullptr, 1, &pipelineCreateInfo, nullptr, &vk->pathTracePipeline);

			std::vector<char> shaderGroupHandles(vk->pathTracePipelineProps.shaderGroupHandleSize* countof(shaderGroups));
			vkGetRayTracingShaderGroupHandles(vk->device, vk->pathTracePipeline, 0, countof(shaderGroups), shaderGroupHandles.size(), shaderGroupHandles.data());

			uint64 alignedGroupSize = align(vk->pathTracePipelineProps.shaderGroupHandleSize, vk->pathTracePipelineProps.shaderGroupBaseAlignment);
			uint64 sbtBufferSize = alignedGroupSize * countof(shaderGroups);
			vk->pathTraceSBTBuffer = vk->createBuffer(&vk->gpuBuffersMemory, sbtBufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			).first;

			uint8* stagingBufferMappedMemory = nullptr;
			vkMapMemory(vk->device, vk->stagingBuffersMemory.memory, 0, sbtBufferSize, 0, (void**)&stagingBufferMappedMemory);
			for (size_t i = 0; i < countof(shaderGroups); i++) {
				memcpy(
					stagingBufferMappedMemory + i * alignedGroupSize,
					shaderGroupHandles.data() + i * vk->pathTracePipelineProps.shaderGroupHandleSize,
					vk->pathTracePipelineProps.shaderGroupHandleSize
				);
			}
			vkUnmapMemory(vk->device, vk->stagingBuffersMemory.memory);
			VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};

			auto& cmdBuf = vk->frames[vk->frameIndex].graphicsCmdBuf;
			vkResetCommandBuffer(cmdBuf, 0);
			vkBeginCommandBuffer(cmdBuf, &beginInfo);
			VkBufferCopy bufferCopy = {
				.srcOffset = 0, .dstOffset = 0, .size = sbtBufferSize
			};
			vkCmdCopyBuffer(cmdBuf, vk->stagingBuffer, vk->pathTraceSBTBuffer, 1, &bufferCopy);
			vkEndCommandBuffer(cmdBuf);
			VkSubmitInfo submitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmdBuf
			};
			vkQueueSubmit(vk->graphicsQueue, 1, &submitInfo, nullptr);
			vkQueueWaitIdle(vk->graphicsQueue);

			VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = vk->pathTraceSBTBuffer
			};
			VkDeviceAddress sbtBasedAddress = vkGetBufferDeviceAddress(vk->device, &bufferDeviceAddressInfo);
			vk->pathTraceSBTBufferRayGenDeviceAddress = { sbtBasedAddress, 0, 0 };
			vk->pathTraceSBTBufferMissDeviceAddress = { sbtBasedAddress + alignedGroupSize, 0, 0 };
			vk->pathTraceSBTBufferHitGroupDeviceAddress = { sbtBasedAddress + alignedGroupSize * 2, 0, 0 };
			vk->pathTraceSBTBufferCallableDeviceAddress = { 0, 0, 0 };
		}

		return vk;
	}

	void createColorBuffers(uint32 width, uint32 height) {
		VkImageCreateInfo imageCreateInfos[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.extent = { width, height, 1 },
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_STORAGE_BIT,
			},
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R16G16B16A16_SFLOAT,
				.extent = { width, height, 1 },
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			},
		};
		vkCreateImage(device, &imageCreateInfos[0], nullptr, &accumulationColorBuffer.first);
		vkCreateImage(device, &imageCreateInfos[1], nullptr, &colorBuffer.first);

		VkMemoryRequirements MemoryRequirements[2];
		vkGetImageMemoryRequirements(device, accumulationColorBuffer.first, &MemoryRequirements[0]);
		vkGetImageMemoryRequirements(device, colorBuffer.first, &MemoryRequirements[1]);

		VkMemoryAllocateInfo memoryAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = align(MemoryRequirements[0].size, MemoryRequirements[1].alignment) + MemoryRequirements[1].size,
			.memoryTypeIndex = deviceLocalMemoryType
		};
		vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &gpuColorBuffersMemory.memory);
		gpuColorBuffersMemory.capacity = memoryAllocateInfo.allocationSize;

		vkBindImageMemory(device, accumulationColorBuffer.first, gpuColorBuffersMemory.memory, 0);
		gpuColorBuffersMemory.offset = align(MemoryRequirements[0].size, MemoryRequirements[1].alignment);
		vkBindImageMemory(device, colorBuffer.first, gpuColorBuffersMemory.memory, gpuColorBuffersMemory.offset);
		gpuColorBuffersMemory.offset += MemoryRequirements[1].size;
		assert(gpuColorBuffersMemory.offset == gpuColorBuffersMemory.capacity);

		VkImageViewCreateInfo imageViewCreateInfos[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = accumulationColorBuffer.first,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = imageCreateInfos[0].format,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			},
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = colorBuffer.first,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = imageCreateInfos[1].format,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			},
		};
		vkCreateImageView(device, &imageViewCreateInfos[0], nullptr, &accumulationColorBuffer.second);
		vkCreateImageView(device, &imageViewCreateInfos[1], nullptr, &colorBuffer.second);
	}

	std::pair<VkBuffer, uint64> createBuffer(Vulkan::Memory* memory, uint64 size, VkBufferUsageFlags usageFlags) {
		VkBufferCreateInfo bufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = usageFlags
		};
		VkBuffer buffer;
		vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer);
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
		memory->offset = align(memory->offset, memoryRequirements.alignment);
		uint64 offset = memory->offset;
		vkBindBufferMemory(device, buffer, memory->memory, memory->offset);
		memory->offset += memoryRequirements.size;
		return std::make_pair(buffer, offset);
	}

	VkImage createImage2D(Vulkan::Memory* memory, uint32 width, uint32 height, VkFormat format, VkImageUsageFlags usageFlags) {
		VkImageCreateInfo imageCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = format,
			.extent = { width, height, 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = usageFlags,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};
		VkImage image;
		vkCreateImage(device, &imageCreateInfo, nullptr, &image);
		VkMemoryRequirements memoryRequirement;
		vkGetImageMemoryRequirements(device, image, &memoryRequirement);
		memory->offset = align(memory->offset, memoryRequirement.alignment);
		vkBindImageMemory(device, image, memory->memory, memory->offset);
		memory->offset += memoryRequirement.size;

		return image;
	}

	std::pair<VkImage, VkImageView> createImage2DAndView(Vulkan::Memory* memory, uint32 width, uint32 height, VkFormat format, VkImageAspectFlags aspectFlags, VkImageUsageFlags usageFlags) {
		VkImage image = createImage2D(memory, width, height, format, usageFlags);
		VkImageViewCreateInfo imageViewCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = aspectFlags,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		VkImageView imageView;
		vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView);
		return std::make_pair(image, imageView);
	}

	void handleWindowResize(uint windowWidth, uint windowHeight) {
		vkQueueWaitIdle(graphicsQueue);

		vkDestroySwapchainKHR(device, swapChain, nullptr);
		for (size_t i = 0; i < countof(swapChainImageFramebuffers); i++) {
			auto& imageFramebuffer = swapChainImageFramebuffers[i];
			vkDestroyFramebuffer(device, imageFramebuffer.frameBuffer, nullptr);
		}

		VkSwapchainCreateInfoKHR swapChainCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surface,
			.minImageCount = vkSwapChainImageCount,
			.imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
			.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
			.imageExtent = { (uint32)windowWidth, (uint32)windowHeight },
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
			.clipped = true
		};
		vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapChain);

		uint swapChainImageCount = 0;
		vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, nullptr);
		assert(swapChainImageCount == vkSwapChainImageCount);
		VkImage swapChainImages[vkSwapChainImageCount];
		vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages);
		for (size_t i = 0; i < countof(swapChainImageFramebuffers); i++) {
			auto& imageFramebuffer = swapChainImageFramebuffers[i];
			imageFramebuffer.image = swapChainImages[i];

			VkImageViewCreateInfo imageCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = imageFramebuffer.image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_B8G8R8A8_UNORM,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			vkCreateImageView(device, &imageCreateInfo, nullptr, &imageFramebuffer.imageView);

			VkFramebufferCreateInfo framebufferCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = swapChainRenderPass,
				.attachmentCount = 1,
				.pAttachments = &imageFramebuffer.imageView,
				.width = (uint32)windowWidth,
				.height = (uint32)windowHeight,
				.layers = 1
			};
			vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &imageFramebuffer.frameBuffer);
		}

		vkDestroyImageView(device, accumulationColorBuffer.second, nullptr);
		vkDestroyImage(device, accumulationColorBuffer.first, nullptr);
		vkDestroyImageView(device, colorBuffer.second, nullptr);
		vkDestroyImage(device, colorBuffer.first, nullptr);
		vkFreeMemory(device, gpuColorBuffersMemory.memory, nullptr);
		createColorBuffers(windowWidth, windowHeight);
		accumulatedFrameCount = 0;

		auto& cmdBuf = frames[frameIndex].graphicsCmdBuf;
		VkCommandBufferBeginInfo cmdBufBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		vkResetCommandBuffer(cmdBuf, 0);
		vkBeginCommandBuffer(cmdBuf, &cmdBufBeginInfo);
		VkImageMemoryBarrier imageBarriers[] = {
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = 0,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_GENERAL,
				.srcQueueFamilyIndex = graphicsComputeQueueFamilyIndex,
				.dstQueueFamilyIndex = graphicsComputeQueueFamilyIndex,
				.image = accumulationColorBuffer.first,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			},
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = 0,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = graphicsComputeQueueFamilyIndex,
				.dstQueueFamilyIndex = graphicsComputeQueueFamilyIndex,
				.image = colorBuffer.first,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			},
		};
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, countof(imageBarriers), imageBarriers);
		vkEndCommandBuffer(cmdBuf);
		VkSubmitInfo queueSubmitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmdBuf
		};
		vkQueueSubmit(graphicsQueue, 1, &queueSubmitInfo, nullptr);
		vkQueueWaitIdle(graphicsQueue);
	}
};

struct Camera {
	XMVECTOR position = XMVectorSet(0, 0, 10, 0);
	XMVECTOR view = XMVectorSet(0, 0, -1, 0);
	XMMATRIX viewMat;
	XMMATRIX projMat;
	XMMATRIX viewProjMat;
};

struct Image {
	uint32 width;
	uint32 height;
	VkFormat format;
	uint32 size;
	uint8* data;
};

struct Model {
	std::string name;
	std::filesystem::path filePath;
	cgltf_data* gltfData;
	std::vector<Image> images;
};

struct Light {
	enum Type {
		Point
	};
	Type type;
	float position[3];
	float color[3];
};

XMMATRIX getNodeTransformMat(cgltf_node* node) {
	if (node->has_matrix) {
		return XMMATRIX(node->matrix);
	}
	else {
		XMMATRIX mat = XMMatrixIdentity();
		if (node->has_scale) {
			mat = XMMatrixScaling(node->scale[0], node->scale[1], node->scale[2]) * mat;
		}
		if (node->has_rotation) {
			mat = XMMatrixRotationQuaternion(XMVectorSet(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3])) * mat;
		}
		if (node->has_translation) {
			mat = XMMatrixTranslation(node->translation[0], node->translation[1], node->translation[2]) * mat;
		}
		return mat;
	}
}

struct PrimitiveVerticesData {
	cgltf_accessor* indices;
	cgltf_accessor* positions;
	cgltf_accessor* normals;
	cgltf_accessor* uvs;
};

struct Vertex {
	float position[4];
	float normal[4];
	float uv[2];
	float pad0[2];
};

struct Geometry {
	uint32 vertexOffset;
	uint32 indexOffset;
	uint32 materialIndex;
	uint32 pad0;
};

struct Instance {
	float transform[4][4];
	float transformIT[4][4];
	uint32 geometryOffset;
	uint32 pad0[3];
};

struct Material {
	float baseColorFactor[4];
	float emissiveFactor[4];
	uint32 baseColorTextureIndex;
	uint32 emissiveTextureIndex;
	uint32 pad0[2];
};

struct Scene {
	std::filesystem::path filePath;
	Camera camera;
	std::vector<Model> models;
	std::vector<Light> lights;

	VkBuffer verticesBuffer;
	VkBuffer indicesBuffer;
	VkBuffer geometriesBuffer;
	VkBuffer materialsBuffer;
	VkBuffer instancesBuffer;
	std::vector<std::pair<VkImage, VkImageView>> textures;

	VkBuffer blasBuffer;
	std::vector<VkAccelerationStructureKHR> blas;
	VkBuffer tlasBuffer;
	VkAccelerationStructureKHR tlas;

	static Scene* create(const std::filesystem::path& filePath, Vulkan* vk) {
		Scene* scene = new Scene();
		scene->filePath = filePath;
		scene->loadJson();
		scene->loadModelsData();
		scene->buildVkResources(vk);
		return scene;
	}

	void loadJson() {
		std::ifstream file(filePath);
		nlohmann::json json;
		file >> json;
		auto jCamera = json["camera"];
		if (!jCamera.is_null()) {
			auto position = jCamera["position"];
			auto view = jCamera["view"];
			camera.position = XMVectorSet(position[0], position[1], position[2], 0);
			camera.view = XMVector3Normalize(XMVectorSet(view[0], view[1], view[2], 0));
			camera.viewMat = XMMatrixLookAtRH(camera.position, camera.position + camera.view, XMVectorSet(0, 1, 0, 0));
			camera.projMat = XMMatrixPerspectiveFovRH((float)80.0_deg, 1920.0f / 1080.0f, 0.1f, 1000.0f);
			camera.viewProjMat = camera.viewMat * camera.projMat;
		}
		auto jModels = json["models"];
		if (!jModels.is_null()) {
			for (auto& jModel : jModels) {
				Model m = {
					.name = jModel["name"],
					.filePath = std::string(jModel["path"])
				};
				models.push_back(m);
			}
		}
		auto jLights = json["lights"];
		if (!jLights.is_null()) {
			for (auto& light : jLights) {
				auto type = light["type"];
				if (type.is_string() && type == "point") {
					auto position = light["position"];
					auto color = light["color"];
					Light l = {
						.position = { position[0], position[1], position[2] },
						.color = { color[0], color[1], color[2] }
					};
					lights.push_back(l);
				}
			}
		}
	}

	void loadModelsData() {
		for (auto& model : models) {
			std::filesystem::path modelFilePath = filePath.parent_path() / model.filePath;
			std::string modelFilePathStr = modelFilePath.generic_string();
			cgltf_options gltfOption = {};
			cgltf_result parseFileResult = cgltf_parse_file(&gltfOption, modelFilePathStr.c_str(), &model.gltfData);
			assert(parseFileResult == cgltf_result_success);
			assert(model.gltfData->scene);
			cgltf_result loadBuffersResult = cgltf_load_buffers(&gltfOption, model.gltfData, modelFilePathStr.c_str());
			assert(loadBuffersResult == cgltf_result_success);
			for (size_t i = 0; i < model.gltfData->images_count; i++) {
				auto& gltfImage = model.gltfData->images[i];
				int width, height, comp;
				std::filesystem::path imagePath = modelFilePath.parent_path() / gltfImage.uri;
				stbi_uc* data = stbi_load(imagePath.generic_string().c_str(), &width, &height, &comp, 0);
				assert(data);
				VkFormat format =
					comp == 1 ? VK_FORMAT_R8_UNORM :
					comp == 2 ? VK_FORMAT_R8G8_UNORM :
					comp == 3 ? VK_FORMAT_R8G8B8A8_SRGB :
					comp == 4 ? VK_FORMAT_R8G8B8A8_SRGB :
					VK_FORMAT_UNDEFINED;
				assert(format != VK_FORMAT_UNDEFINED);
				if (comp == 3) {
					comp = 4;
					uint8* newData = new uint8[width * height * 4]();
					for (int i = 0; i < width * height; i++) {
						newData[i * 4 + 0] = data[i * 3 + 0];
						newData[i * 4 + 1] = data[i * 3 + 1];
						newData[i * 4 + 2] = data[i * 3 + 2];
					}
					stbi_image_free(data);
					data = newData;
				}
				Image image = {
					.width = (uint32)width,
					.height = (uint32)height,
					.format = format,
					.size = (uint32)(width * height * comp),
					.data = data
				};
				model.images.push_back(image);
			}
		}
	}

	void buildVkResources(Vulkan* vk) {
		std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasInfos;
		std::vector<VkAccelerationStructureBuildSizesInfoKHR> blasSizes;
		std::vector<std::vector<VkAccelerationStructureGeometryKHR>> blasGeometries;
		std::vector<std::vector<VkAccelerationStructureBuildRangeInfoKHR>> blasRanges;
		std::vector<std::vector<PrimitiveVerticesData>> primitiveVerticesData;
		uint64 verticesBufferSize = 0;
		uint64 indicesBufferSize = 0;
		uint64 geometriesBufferSize = 0;
		{
			uint64 blasBufferSize = 0;
			for (auto& model : models) {
				for (size_t meshIndex = 0; meshIndex < model.gltfData->meshes_count; meshIndex++) {
					auto& mesh = model.gltfData->meshes[meshIndex];
					std::vector<VkAccelerationStructureGeometryKHR> geometries(mesh.primitives_count);
					std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges(mesh.primitives_count);
					std::vector<PrimitiveVerticesData> verticesData(mesh.primitives_count);
					for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; primitiveIndex++) {
						auto& primitive = mesh.primitives[primitiveIndex];
						cgltf_accessor* indices = primitive.indices;
						cgltf_accessor* positions = nullptr;
						cgltf_accessor* normals = nullptr;
						cgltf_accessor* uvs = nullptr;
						assert(primitive.type == cgltf_primitive_type_triangles);
						assert(indices->component_type == cgltf_component_type_r_16u);
						assert(indices->type == cgltf_type_scalar);
						assert(indices->count % 3 == 0);
						assert(indices->stride == 2);
						assert(indices->buffer_view->buffer->data);
						for (size_t attribIndex = 0; attribIndex < primitive.attributes_count; attribIndex++) {
							auto& attribute = primitive.attributes[attribIndex];
							if (attribute.type == cgltf_attribute_type_position) {
								assert(attribute.data->component_type == cgltf_component_type_r_32f);
								assert(attribute.data->type == cgltf_type_vec3);
								assert(attribute.data->buffer_view->buffer->data);
								positions = attribute.data;
							}
							if (attribute.type == cgltf_attribute_type_normal) {
								assert(attribute.data->component_type == cgltf_component_type_r_32f);
								assert(attribute.data->type == cgltf_type_vec3);
								assert(attribute.data->buffer_view->buffer->data);
								normals = attribute.data;
							}
							if (attribute.type == cgltf_attribute_type_texcoord) {
								assert(attribute.data->component_type == cgltf_component_type_r_32f);
								assert(attribute.data->type == cgltf_type_vec2);
								assert(attribute.data->buffer_view->buffer->data);
								uvs = attribute.data;
							}
						}
						assert(positions);
						assert(normals);
						//assert(uvs);

						geometries[primitiveIndex] = {
							.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
							.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
							.geometry = {
								.triangles = {
									.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
									.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
									.maxVertex = (uint32)(positions->count),
									.indexType = VK_INDEX_TYPE_UINT16,
								}
							}
						};

						ranges[primitiveIndex] = {
							.primitiveCount = (uint32)(indices->count / 3)
						};

						verticesData[primitiveIndex] = PrimitiveVerticesData{ .indices = indices, .positions = positions, .normals = normals, .uvs = uvs };

						verticesBufferSize += positions->count * sizeof(struct Vertex);
						indicesBufferSize += indices->count * sizeof(uint16);
						geometriesBufferSize += sizeof(struct Geometry);
					}

					VkAccelerationStructureBuildGeometryInfoKHR blasInfo = {
						.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
						.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
						.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
						.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
						.geometryCount = (uint32)mesh.primitives_count,
						.pGeometries = geometries.data()
					};
					std::vector<uint32> maxPrimitiveCounts(mesh.primitives_count);
					for (size_t i = 0; i < maxPrimitiveCounts.size(); i++) {
						maxPrimitiveCounts[i] = ranges[i].primitiveCount;
					}
					VkAccelerationStructureBuildSizesInfoKHR size = {
						.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
					};
					vkGetAccelerationStructureBuildSizes(vk->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &blasInfo, maxPrimitiveCounts.data(), &size);

					blasBufferSize += size.accelerationStructureSize;

					blasInfos.push_back(blasInfo);
					blasSizes.push_back(size);
					blasGeometries.push_back(std::move(geometries));
					blasRanges.push_back(std::move(ranges));
					primitiveVerticesData.push_back(std::move(verticesData));
				}
			}

			VkBufferUsageFlags bufferUsageFlags =
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			verticesBuffer = vk->createBuffer(&vk->gpuBuffersMemory, verticesBufferSize, bufferUsageFlags).first;
			indicesBuffer = vk->createBuffer(&vk->gpuBuffersMemory, indicesBufferSize, bufferUsageFlags).first;
			geometriesBuffer = vk->createBuffer(&vk->gpuBuffersMemory, geometriesBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).first;
			blasBuffer = vk->createBuffer(&vk->gpuBuffersMemory, blasBufferSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR).first;

			uint64 blasBufferOffset = 0;
			for (size_t i = 0; i < blasInfos.size(); i++) {
				auto& info = blasInfos[i];
				auto& size = blasSizes[i];
				VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
					.buffer = blasBuffer,
					.offset = blasBufferOffset,
					.size = size.accelerationStructureSize,
					.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
				};
				vkCreateAccelerationStructure(vk->device, &blasCreateInfo, nullptr, &info.dstAccelerationStructure);
				blas.push_back(info.dstAccelerationStructure);
				blasBufferOffset += size.accelerationStructureSize;
			}
		}

		std::vector<Material> materialsBufferData;
		uint64 materialsBufferSize = 0;
		std::vector<std::vector<uint32>> geometryMaterialIndices;
		{
			for (auto& model : models) {
				for (size_t meshIndex = 0; meshIndex < model.gltfData->meshes_count; meshIndex++) {
					auto& gltfMesh = model.gltfData->meshes[meshIndex];
					std::vector<uint32> materialIndices(gltfMesh.primitives_count);
					for (size_t indexIndex = 0; indexIndex < materialIndices.size(); indexIndex++) {
						auto& index = materialIndices[indexIndex];
						index = (uint32)(materialsBufferData.size() + std::distance(model.gltfData->materials, gltfMesh.primitives[indexIndex].material));
					}
					geometryMaterialIndices.push_back(std::move(materialIndices));
				}
				for (size_t materialIndex = 0; materialIndex < model.gltfData->materials_count; materialIndex++) {
					auto& gltfMaterial = model.gltfData->materials[materialIndex];
					Material material = {};
					memcpy(material.emissiveFactor, gltfMaterial.emissive_factor, 12);
					if (gltfMaterial.has_pbr_metallic_roughness) {
						memcpy(material.baseColorFactor, gltfMaterial.pbr_metallic_roughness.base_color_factor, 12);
						if (gltfMaterial.pbr_metallic_roughness.base_color_texture.texture) {
							uint64 textureIndex = std::distance(model.gltfData->images, gltfMaterial.pbr_metallic_roughness.base_color_texture.texture->image);
							material.baseColorTextureIndex = (uint32)(textures.size() + textureIndex);
						}
						else {
							material.baseColorTextureIndex = UINT32_MAX;
						}
					}
					materialsBufferData.push_back(material);
				}
				for (auto& image : model.images) {
					VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
					auto vkImageAndView = vk->createImage2DAndView(&vk->gpuTexturesMemory, image.width, image.height, image.format, VK_IMAGE_ASPECT_COLOR_BIT, flags);
					textures.push_back(vkImageAndView);
				}
			}
			materialsBufferSize = materialsBufferData.size() * sizeof(materialsBufferData[0]);
			materialsBuffer = vk->createBuffer(&vk->gpuBuffersMemory, materialsBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).first;
		}

		VkAccelerationStructureBuildGeometryInfoKHR tlasInfo;
		VkAccelerationStructureBuildSizesInfoKHR tlasSize;
		VkAccelerationStructureGeometryKHR tlasGeometry;
		VkAccelerationStructureBuildRangeInfoKHR tlasRange;
		std::vector<VkAccelerationStructureInstanceKHR> tlasBuildInstances;
		std::vector<Instance> instancesBufferData;
		uint64 instancesBufferSize = 0;
		{
			for (uint64 blasOffset = 0; auto & model : models) {
				std::stack<cgltf_node*> nodes;
				std::stack<XMMATRIX> transforms;
				for (size_t nodeIndex = 0; nodeIndex < model.gltfData->scene->nodes_count; nodeIndex++) {
					cgltf_node* node = model.gltfData->scene->nodes[nodeIndex];
					nodes.push(node);
					transforms.push(getNodeTransformMat(node));
				}
				while (!nodes.empty()) {
					cgltf_node* node = nodes.top();
					XMMATRIX transform = transforms.top();
					nodes.pop();
					transforms.pop();
					if (node->mesh) {
						uint64 blasIndex = blasOffset + std::distance(model.gltfData->meshes, node->mesh);
						VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo = {
							.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
							.accelerationStructure = blas[blasIndex]
						};
						VkAccelerationStructureInstanceKHR tlasInstance = {
							.mask = 0xff,
							.accelerationStructureReference = vkGetAccelerationStructureDeviceAddress(vk->device, &asDeviceAddressInfo)
						};
						XMMATRIX transformT = XMMatrixTranspose(transform);
						memcpy(tlasInstance.transform.matrix, transformT.r, 12 * sizeof(float));
						tlasBuildInstances.push_back(tlasInstance);

						Instance instance;
						memcpy(instance.transform, transform.r, sizeof(transform));
						XMMATRIX transformIT = XMMatrixTranspose(XMMatrixInverse(nullptr, transform));
						memcpy(instance.transformIT, transformIT.r, sizeof(transformIT));
						instance.geometryOffset = 0;
						for (uint64 i = 0; i < blasIndex; i++) {
							instance.geometryOffset += blasInfos[i].geometryCount;
						}
						instancesBufferData.push_back(instance);
					}
					for (size_t childIndex = 0; childIndex < node->children_count; childIndex++) {
						cgltf_node* childNode = node->children[childIndex];
						nodes.push(childNode);
						transforms.push(getNodeTransformMat(childNode) * transform);
					}
				}
				blasOffset += model.gltfData->meshes_count;
			}

			tlasGeometry = {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
				.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
				.geometry = {
					.instances = {
						.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
						.arrayOfPointers = VK_FALSE,
					}
				}
			};
			tlasInfo = {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
				.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
				.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
				.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
				.dstAccelerationStructure = nullptr,
				.geometryCount = 1,
				.pGeometries = &tlasGeometry,
			};
			tlasRange = { .primitiveCount = (uint32)tlasBuildInstances.size() };

			uint32 tlasBuildInstanceCount = (uint32)tlasBuildInstances.size();
			tlasSize = {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
			};
			vkGetAccelerationStructureBuildSizes(vk->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasInfo, &tlasBuildInstanceCount, &tlasSize);

			instancesBufferSize = instancesBufferData.size() * sizeof(instancesBufferData[0]);
			instancesBuffer = vk->createBuffer(&vk->gpuBuffersMemory, instancesBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).first;
			tlasBuffer = vk->createBuffer(&vk->gpuBuffersMemory, tlasSize.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR).first;
			VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
				.buffer = tlasBuffer,
				.size = tlasSize.accelerationStructureSize,
				.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
			};
			vkCreateAccelerationStructure(vk->device, &tlasCreateInfo, nullptr, &tlasInfo.dstAccelerationStructure);
			tlas = tlasInfo.dstAccelerationStructure;
		}

		VkBuffer tlasBuildInstancesBuffer;
		uint64 tlasBuildInstancesBufferSize = tlasBuildInstances.size() * sizeof(tlasBuildInstances[0]);
		{
			tlasBuildInstancesBuffer = vk->createBuffer(
				&vk->gpuBuffersMemory, tlasBuildInstancesBufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
			).first;

			VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
				.buffer = tlasBuildInstancesBuffer
			};
			tlasGeometry.geometry.instances.data.deviceAddress = vkGetBufferDeviceAddress(vk->device, &bufferDeviceAddressInfo);
		}

		VkBuffer scratchBuffer;
		{
			uint64 scratchBufferAlignment = vk->accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment;
			uint64 scratchBufferSize = 0;
			for (auto& size : blasSizes) {
				scratchBufferSize += align(size.buildScratchSize, scratchBufferAlignment);
			}
			scratchBufferSize += align(tlasSize.buildScratchSize, scratchBufferAlignment);
			scratchBuffer = vk->createBuffer(&vk->gpuBuffersMemory, scratchBufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT).first;

			VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
				.buffer = scratchBuffer
			};
			VkDeviceAddress	scratchBufferDeviceAddress = vkGetBufferDeviceAddress(vk->device, &bufferDeviceAddressInfo);
			scratchBufferDeviceAddress = align(scratchBufferDeviceAddress, scratchBufferAlignment);
			for (size_t infoIndex = 0; infoIndex < blasInfos.size(); infoIndex++) {
				auto& info = blasInfos[infoIndex];
				info.scratchData.deviceAddress = scratchBufferDeviceAddress;
				scratchBufferDeviceAddress = align(scratchBufferDeviceAddress + blasSizes[infoIndex].buildScratchSize, scratchBufferAlignment);
			}
			tlasInfo.scratchData.deviceAddress = scratchBufferDeviceAddress;
		}
		{
			uint64 stageBufferSize =
				verticesBufferSize + indicesBufferSize + geometriesBufferSize +
				materialsBufferSize + instancesBufferSize + tlasBuildInstancesBufferSize;
			for (auto& model : models) {
				for (auto& image : model.images) {
					stageBufferSize = align(stageBufferSize, 16);
					stageBufferSize += image.size;
				}
			}
			assert(stageBufferSize < vk->stagingBuffersMemory.capacity);
			uint8* stageBufferPtr = nullptr;
			vkMapMemory(vk->device, vk->stagingBuffersMemory.memory, 0, stageBufferSize, 0, (void**)&stageBufferPtr);
			uint8* verticesBufferPtr = stageBufferPtr;
			uint8* indicesBufferPtr = verticesBufferPtr + verticesBufferSize;
			uint8* geometriesBufferPtr = indicesBufferPtr + indicesBufferSize;
			uint8* materialsBufferPtr = geometriesBufferPtr + geometriesBufferSize;
			uint8* instancesBufferPtr = materialsBufferPtr + materialsBufferSize;
			uint8* tlasBuildInstancesBufferPtr = instancesBufferPtr + instancesBufferSize;
			uint8* texturesPtr = align(tlasBuildInstancesBufferPtr + tlasBuildInstancesBufferSize, 16);

			VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
				.buffer = verticesBuffer
			};
			VkDeviceAddress vertexBufferDeviceAddress = vkGetBufferDeviceAddress(vk->device, &bufferDeviceAddressInfo);
			bufferDeviceAddressInfo.buffer = indicesBuffer;
			VkDeviceAddress indexBufferDeviceAddress = vkGetBufferDeviceAddress(vk->device, &bufferDeviceAddressInfo);

			Geometry geometryData = { .vertexOffset = 0, .indexOffset = 0 };
			for (size_t blasIndex = 0; blasIndex < blasGeometries.size(); blasIndex++) {
				auto& geometries = blasGeometries[blasIndex];
				for (size_t geometryIndex = 0; geometryIndex < geometries.size(); geometryIndex++) {
					auto& geometry = geometries[geometryIndex];
					auto& verticesData = primitiveVerticesData[blasIndex][geometryIndex];
					uint64 verticesSize = verticesData.positions->count * sizeof(struct Vertex);
					uint64 indicesSize = verticesData.indices->count * sizeof(uint16);
					uint8* positions = (uint8*)(verticesData.positions->buffer_view->buffer->data) + verticesData.positions->offset + verticesData.positions->buffer_view->offset;
					uint8* normals = (uint8*)(verticesData.normals->buffer_view->buffer->data) + verticesData.normals->offset + verticesData.normals->buffer_view->offset;
					uint8* uvs = nullptr;
					if (verticesData.uvs) {
						uvs = (uint8*)(verticesData.uvs->buffer_view->buffer->data) + verticesData.uvs->offset + verticesData.uvs->buffer_view->offset;
					}
					for (uint64 i = 0; i < verticesData.positions->count; i++) {
						memcpy(verticesBufferPtr + i * sizeof(struct Vertex), positions + i * verticesData.positions->stride, 12);
						memcpy(verticesBufferPtr + i * sizeof(struct Vertex) + 16, normals + i * verticesData.normals->stride, 12);
						if (uvs) {
							memcpy(verticesBufferPtr + i * sizeof(struct Vertex) + 32, uvs + i * verticesData.uvs->stride, 8);
						}
					}
					uint8* indices = (uint8*)(verticesData.indices->buffer_view->buffer->data) + verticesData.indices->offset + verticesData.indices->buffer_view->offset;
					memcpy(indicesBufferPtr, indices, indicesSize);
					verticesBufferPtr += verticesSize;
					indicesBufferPtr += indicesSize;

					geometry.geometry.triangles.vertexData.deviceAddress = vertexBufferDeviceAddress;
					geometry.geometry.triangles.vertexStride = sizeof(struct Vertex);
					geometry.geometry.triangles.indexData.deviceAddress = indexBufferDeviceAddress;
					vertexBufferDeviceAddress += verticesSize;
					indexBufferDeviceAddress += indicesSize;

					geometryData.materialIndex = geometryMaterialIndices[blasIndex][geometryIndex];
					memcpy(geometriesBufferPtr, &geometryData, sizeof(geometryData));
					geometriesBufferPtr += sizeof(geometryData);
					geometryData.vertexOffset += (uint32)verticesData.positions->count;
					geometryData.indexOffset += (uint32)verticesData.indices->count;
				}
			}
			memcpy(materialsBufferPtr, materialsBufferData.data(), materialsBufferSize);
			memcpy(instancesBufferPtr, instancesBufferData.data(), instancesBufferData.size() * sizeof(instancesBufferData[0]));
			memcpy(tlasBuildInstancesBufferPtr, tlasBuildInstances.data(), tlasBuildInstancesBufferSize);
			for (auto& model : models) {
				for (auto& image : model.images) {
					memcpy(texturesPtr, image.data, image.size);
					texturesPtr = align(texturesPtr + image.size, 16);
				}
			}
			vkUnmapMemory(vk->device, vk->stagingBuffersMemory.memory);
		}
		{
			auto& cmdBuf = vk->frames[vk->frameIndex].graphicsCmdBuf;
			VkCommandBufferBeginInfo cmdBufferBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkResetCommandBuffer(cmdBuf, 0);
			vkBeginCommandBuffer(cmdBuf, &cmdBufferBeginInfo);

			std::pair<VkBuffer, uint64> bufferInfos[] = {
				{ verticesBuffer, verticesBufferSize },
				{ indicesBuffer, indicesBufferSize },
				{ geometriesBuffer, geometriesBufferSize },
				{ materialsBuffer, materialsBufferSize },
				{ instancesBuffer, instancesBufferSize },
				{ tlasBuildInstancesBuffer, tlasBuildInstancesBufferSize }
			};
			uint64 stagingBufferOffset = 0;
			for (auto& bufferInfo : bufferInfos) {
				VkBufferCopy bufferCopy = {
					.srcOffset = stagingBufferOffset,
					.size = bufferInfo.second
				};
				vkCmdCopyBuffer(cmdBuf, vk->stagingBuffer, bufferInfo.first, 1, &bufferCopy);
				stagingBufferOffset += bufferInfo.second;
			}

			std::vector<VkImageMemoryBarrier> imageBarriers(textures.size());
			for (size_t barrierIndex = 0; barrierIndex < imageBarriers.size(); barrierIndex++) {
				imageBarriers[barrierIndex] = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = textures[barrierIndex].first,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				};
			}
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, (uint32)imageBarriers.size(), imageBarriers.data());

			stagingBufferOffset = align(stagingBufferOffset, 16);
			uint64 textureIndex = 0;
			for (auto& model : models) {
				for (auto& image : model.images) {
					VkBufferImageCopy imageCopy = {
						.bufferOffset = stagingBufferOffset,
						.imageSubresource = {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.mipLevel = 0,
							.baseArrayLayer = 0,
							.layerCount = 1
						},
						.imageOffset = { 0, 0, 0 },
						.imageExtent = { image.width, image.height, 1 }
					};
					vkCmdCopyBufferToImage(cmdBuf, vk->stagingBuffer, textures[textureIndex].first, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
					stagingBufferOffset = align(stagingBufferOffset + image.size, 16);
					textureIndex++;
				}
			}

			VkMemoryBarrier memoryBarrier = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
			};
			for (auto& barrier : imageBarriers) {
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = 0;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, (uint32)imageBarriers.size(), imageBarriers.data());

			std::vector<VkAccelerationStructureBuildRangeInfoKHR*> blasRangesPtr(blasRanges.size());
			for (size_t i = 0; i < blasRangesPtr.size(); i++) {
				blasRangesPtr[i] = blasRanges[i].data();
			}
			vkCmdBuildAccelerationStructures(cmdBuf, (uint32)blasInfos.size(), blasInfos.data(), blasRangesPtr.data());

			memoryBarrier = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
				.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
			};
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr
			);
			VkAccelerationStructureBuildRangeInfoKHR* tlasRangePtr = &tlasRange;
			vkCmdBuildAccelerationStructures(cmdBuf, 1, &tlasInfo, &tlasRangePtr);

			vkEndCommandBuffer(cmdBuf);

			VkSubmitInfo queueSubmitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &cmdBuf
			};
			vkQueueSubmit(vk->graphicsQueue, 1, &queueSubmitInfo, nullptr);
			vkQueueWaitIdle(vk->graphicsQueue);
		}
	}

	void drawCommands(Vulkan* vk, uint8* uniformBufferPtr, uint windowWidth, uint windowHeight) {
		auto& vkFrame = vk->frames[vk->frameIndex];
		{
			struct {
				XMMATRIX screenToWorldMat;
				XMVECTOR eyePos;
				uint32 accumulatedFrameCount;
			} constantsBuffer = {
				XMMatrixInverse(nullptr, camera.viewProjMat),
				camera.position,
				vk->accumulatedFrameCount
			};
			memcpy(uniformBufferPtr + vkFrame.rayTracingConstantBufferMemoryOffset, &constantsBuffer, sizeof(constantsBuffer));

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = vkFrame.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &vk->pathTraceDescriptorSet0Layout
			};
			VkDescriptorSet descriptorSet;
			vkAllocateDescriptorSets(vk->device, &descriptorSetAllocateInfo, &descriptorSet);
			VkDescriptorImageInfo accumulationBufferImageInfo = {
				.imageView = vk->accumulationColorBuffer.second,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL
			};
			VkDescriptorImageInfo colorBufferImageInfo = {
				.imageView = vk->colorBuffer.second,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL
			};
			VkWriteDescriptorSetAccelerationStructureKHR accelerationStructInfo = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
				.accelerationStructureCount = 1,
				.pAccelerationStructures = &tlas
			};
			VkDescriptorBufferInfo verticesBufferInfo = { .buffer = verticesBuffer, .range = VK_WHOLE_SIZE };
			VkDescriptorBufferInfo indicesBufferInfo = { .buffer = indicesBuffer, .range = VK_WHOLE_SIZE };
			VkDescriptorBufferInfo geometriesBufferInfo = { .buffer = geometriesBuffer, .range = VK_WHOLE_SIZE };
			VkDescriptorBufferInfo materialsBufferInfo = { .buffer = materialsBuffer, .range = VK_WHOLE_SIZE };
			VkDescriptorBufferInfo instancesBufferInfo = { .buffer = instancesBuffer, .range = VK_WHOLE_SIZE };
			VkDescriptorBufferInfo constantsBufferInfo = { .buffer = vkFrame.rayTracingConstantBuffer, .range = VK_WHOLE_SIZE };
			VkDescriptorImageInfo textureSamplerInfo = { .sampler = vk->trilinearSampler };
			std::vector<VkDescriptorImageInfo> textureImageInfos(vk->pathTraceDescriptorSet0TextureCount);
			for (size_t i = 0; i < textureImageInfos.size(); i++) {
				auto& info = textureImageInfos[i];
				if (i < textures.size()) {
					info = {
						.imageView = textures[i].second,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
				}
				else {
					info = {
						.imageView = vk->blankTexture.second,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
				}
			}
			VkWriteDescriptorSet descriptorSetWrites[] = {
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumulationBufferImageInfo },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &colorBufferImageInfo },
				{.pNext = &accelerationStructInfo, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &verticesBufferInfo },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &indicesBufferInfo },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &geometriesBufferInfo },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &materialsBufferInfo },
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &instancesBufferInfo },
				{.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &constantsBufferInfo },
				{.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .pImageInfo = &textureSamplerInfo },
				{.descriptorCount = (uint32)textureImageInfos.size(), .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = textureImageInfos.data() }
			};
			for (size_t i = 0; i < countof(descriptorSetWrites); i++) {
				auto& setWrite = descriptorSetWrites[i];
				setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				setWrite.dstSet = descriptorSet;
				setWrite.dstBinding = (uint32)i;
				if (setWrite.descriptorCount == 0) setWrite.descriptorCount = 1;
			}
			vkUpdateDescriptorSets(vk->device, countof(descriptorSetWrites), descriptorSetWrites, 0, nullptr);

			vkCmdBindPipeline(vkFrame.graphicsCmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk->pathTracePipeline);
			vkCmdBindDescriptorSets(vkFrame.graphicsCmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk->pathTracePipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			VkImageMemoryBarrier imageMemoryBarriers[] = {
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_GENERAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = vk->colorBuffer.first,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				}
			};
			vkCmdPipelineBarrier(vkFrame.graphicsCmdBuf,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
				0, nullptr, 0, nullptr, countof(imageMemoryBarriers), imageMemoryBarriers
			);

			vkCmdTraceRays(vkFrame.graphicsCmdBuf,
				&vk->pathTraceSBTBufferRayGenDeviceAddress,
				&vk->pathTraceSBTBufferMissDeviceAddress,
				&vk->pathTraceSBTBufferHitGroupDeviceAddress,
				&vk->pathTraceSBTBufferCallableDeviceAddress,
				windowWidth, windowHeight, 1
			);

			imageMemoryBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemoryBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemoryBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(vkFrame.graphicsCmdBuf,
				VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
				0, nullptr, 0, nullptr, countof(imageMemoryBarriers), imageMemoryBarriers
			);

			vk->accumulatedFrameCount++;
		}
	}
};

void imguiInit() {
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
	io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
	io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
	io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
	io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
	io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
	io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
	io.KeyMap[ImGuiKey_KeyPadEnter] = SDL_SCANCODE_KP_ENTER;
	io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
	io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
	io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
	io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
	io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
	io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;
	io.IniFilename = "imgui.ini";
	io.FontGlobalScale = 1.5f;
}

int main(int argc, char** argv) {
	setCurrentDirToExeDir();
	assert(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE) == S_OK);
	assert(SDL_Init(SDL_INIT_VIDEO) == 0);

	SDL_DisplayMode displayMode;
	assert(SDL_GetCurrentDisplayMode(0, &displayMode) == 0);

	SDL_Window* window = SDL_CreateWindow(
		"vkrt",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		int(displayMode.w * 0.75), int(displayMode.h * 0.75),
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
	);
	assert(window);
	int windowWidth, windowHeight;
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	int windowFullscreenState = 0;

	imguiInit();
	ImGuiIO& imguiIO = ImGui::GetIO();
	Vulkan* vk = Vulkan::create(window, std::any_of(argv, argv + argc, [](char* arg) { return !strcmp(arg, "-vkValidation"); }));
	Scene* scene = Scene::create("../../assets/cornell box.json", vk);

	SDL_Event event;
	bool running = true;

	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = false;
			}
			else if (event.type == SDL_WINDOWEVENT) {
				if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					int width = event.window.data1;
					int height = event.window.data2;
					if (width != windowWidth || height != windowHeight) {
						windowWidth = width;
						windowHeight = height;
						vk->handleWindowResize(width, height);
					}
				}
			}
			else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
				SDL_Scancode code = event.key.keysym.scancode;
				imguiIO.KeysDown[code] = (event.type == SDL_KEYDOWN);
				imguiIO.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
				imguiIO.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
				imguiIO.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
				imguiIO.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
			}
			else if (event.type == SDL_MOUSEMOTION) {
				imguiIO.MousePos = { (float)event.motion.x, (float)event.motion.y };
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
				int button = -1;
				if (event.button.button == SDL_BUTTON_LEFT) button = 0;
				if (event.button.button == SDL_BUTTON_RIGHT) button = 1;
				if (event.button.button == SDL_BUTTON_MIDDLE) button = 2;
				if (button != -1) imguiIO.MouseDown[button] = (event.type == SDL_MOUSEBUTTONDOWN);
			}
		}

		//ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
		ImGui::GetIO().DisplaySize = { (float)windowWidth, (float)windowHeight };
		ImGui::NewFrame();

		if (ImGui::IsKeyPressed(SDL_SCANCODE_F11)) {
			windowFullscreenState = windowFullscreenState ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
			int err = SDL_SetWindowFullscreen(window, windowFullscreenState);
			SDL_GetWindowSize(window, &windowWidth, &windowHeight);
			vk->handleWindowResize(windowWidth, windowHeight);
			assert(!err);
		}

		ImGui::Begin("test");
		ImGui::End();
		ImGui::Render();

		auto& vkFrame = vk->frames[vk->frameIndex];

		unsigned swapChainImageIndex;
		vkAcquireNextImageKHR(vk->device, vk->swapChain, UINT64_MAX, vkFrame.swapChainImageSemaphore, nullptr, &swapChainImageIndex);

		vkWaitForFences(vk->device, 1, &vkFrame.queueFence, true, UINT64_MAX);
		vkResetFences(vk->device, 1, &vkFrame.queueFence);
		vkResetDescriptorPool(vk->device, vkFrame.descriptorPool, 0);

		uint8* uniformBuffersMappedMemoryPtr;
		vkMapMemory(vk->device, vkFrame.uniformBuffersMemory.memory, 0, VK_WHOLE_SIZE, 0, (void**)&uniformBuffersMappedMemoryPtr);

		VkCommandBufferBeginInfo cmdBufBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		vkResetCommandBuffer(vkFrame.graphicsCmdBuf, 0);
		vkBeginCommandBuffer(vkFrame.graphicsCmdBuf, &cmdBufBeginInfo);

		scene->drawCommands(vk, uniformBuffersMappedMemoryPtr, windowWidth, windowHeight);

		VkClearValue clearValue = {
			.color = {0, 0, 0, 0}
		};
		VkRenderPassBeginInfo renderPassBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = vk->swapChainRenderPass,
			.framebuffer = vk->swapChainImageFramebuffers[swapChainImageIndex].frameBuffer,
			.renderArea = { {0, 0}, { (uint32)windowWidth, (uint32)windowHeight } },
			.clearValueCount = 1,
			.pClearValues = &clearValue
		};
		vkCmdBeginRenderPass(vkFrame.graphicsCmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			VkViewport viewport = {
				.x = 0, .y = 0,
				.width = (float)windowWidth, .height = (float)windowHeight,
				.minDepth = 0, .maxDepth = 1
			};
			VkRect2D scissor = { .offset = {0, 0}, .extent = {(uint32)windowWidth, (uint32)windowHeight} };
			vkCmdSetViewport(vkFrame.graphicsCmdBuf, 0, 1, &viewport);
			vkCmdSetScissor(vkFrame.graphicsCmdBuf, 0, 1, &scissor);

			vkCmdBindPipeline(vkFrame.graphicsCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->swapChainPipeline);

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = vkFrame.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &vk->swapChainDescriptorSet0Layout
			};
			VkDescriptorSet descriptorSet;
			vkAllocateDescriptorSets(vk->device, &descriptorSetAllocateInfo, &descriptorSet);
			VkDescriptorImageInfo descriptorImageInfo = {
				.sampler = vk->trilinearSampler,
				.imageView = vk->colorBuffer.second,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			VkWriteDescriptorSet writeDescriptorSets[] = {
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = descriptorSet,
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
					.pImageInfo = &descriptorImageInfo
				},
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = descriptorSet,
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &descriptorImageInfo
				}
			};
			vkUpdateDescriptorSets(vk->device, countof(writeDescriptorSets), writeDescriptorSets, 0, nullptr);
			vkCmdBindDescriptorSets(vkFrame.graphicsCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->swapChainPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			vkCmdDraw(vkFrame.graphicsCmdBuf, 3, 1, 0, 0);
		}
		{
			vkCmdBindPipeline(vkFrame.graphicsCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->imguiPipeline);

			struct { float windowSize[2]; } pushConsts = { {(float)windowWidth, (float)windowHeight} };
			vkCmdPushConstants(vkFrame.graphicsCmdBuf, vk->imguiPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = vkFrame.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &vk->imguiDescriptorSet0Layout
			};
			VkDescriptorSet descriptorSet;
			vkAllocateDescriptorSets(vk->device, &descriptorSetAllocateInfo, &descriptorSet);
			VkDescriptorImageInfo descriptorImageInfo = {
				.sampler = vk->trilinearSampler,
				.imageView = vk->imguiTexture.second,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			VkWriteDescriptorSet writeDescriptorSets[] = {
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = descriptorSet,
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
					.pImageInfo = &descriptorImageInfo
				},
				{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = descriptorSet,
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &descriptorImageInfo
				}
			};
			vkUpdateDescriptorSets(vk->device, countof(writeDescriptorSets), writeDescriptorSets, 0, nullptr);
			vkCmdBindDescriptorSets(vkFrame.graphicsCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->imguiPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(vkFrame.graphicsCmdBuf, 0, 1, &vkFrame.imguiVertBuffer, &offset);
			vkCmdBindIndexBuffer(vkFrame.graphicsCmdBuf, vkFrame.imguiIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

			uint64 vertBufferOffset = 0;
			uint64 indexBufferOffset = 0;
			const ImDrawData* drawData = ImGui::GetDrawData();
			for (int cmdListIndex = 0; cmdListIndex < drawData->CmdListsCount; cmdListIndex++) {
				const ImDrawList& dlist = *drawData->CmdLists[cmdListIndex];
				uint64 verticesSize = dlist.VtxBuffer.Size * sizeof(ImDrawVert);
				uint64 indicesSize = dlist.IdxBuffer.Size * sizeof(ImDrawIdx);
				memcpy(uniformBuffersMappedMemoryPtr + vkFrame.imguiVertBufferMemoryOffset + vertBufferOffset, dlist.VtxBuffer.Data, verticesSize);
				memcpy(uniformBuffersMappedMemoryPtr + vkFrame.imguiIndexBufferMemoryOffset + indexBufferOffset, dlist.IdxBuffer.Data, indicesSize);
				uint64 vertIndex = vertBufferOffset / sizeof(ImDrawVert);
				uint64 indexIndex = indexBufferOffset / sizeof(ImDrawIdx);
				for (int i = 0; i < dlist.CmdBuffer.Size; i++) {
					const ImDrawCmd& drawCmd = dlist.CmdBuffer[i];
					ImVec4 clipRect = drawCmd.ClipRect;
					if (clipRect.x < windowWidth && clipRect.y < windowHeight && clipRect.z >= 0.0f && clipRect.w >= 0.0f) {
						if (clipRect.x < 0) clipRect.x = 0;
						if (clipRect.y < 0) clipRect.y = 0;
						VkRect2D scissor = {
							{(int32)clipRect.x, (int32)clipRect.y}, {(uint32)(clipRect.z - clipRect.x), (uint32)(clipRect.w - clipRect.y)}
						};
						vkCmdSetScissor(vkFrame.graphicsCmdBuf, 0, 1, &scissor);
						vkCmdDrawIndexed(vkFrame.graphicsCmdBuf, drawCmd.ElemCount, 1, (uint32)indexIndex, (int32)vertIndex, 0);
					}
					indexIndex += drawCmd.ElemCount;
				}
				vertBufferOffset += align(verticesSize, sizeof(ImDrawVert));
				indexBufferOffset += align(indicesSize, sizeof(ImDrawIdx));
				assert(vertBufferOffset < vkFrame.imguiVertBufferMemorySize);
				assert(indexBufferOffset < vkFrame.imguiIndexBufferMemorySize);
			}
		}
		vkCmdEndRenderPass(vkFrame.graphicsCmdBuf);
		vkEndCommandBuffer(vkFrame.graphicsCmdBuf);
		vkUnmapMemory(vk->device, vkFrame.uniformBuffersMemory.memory);

		VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vkFrame.swapChainImageSemaphore,
			.pWaitDstStageMask = &pipelineStageFlags,
			.commandBufferCount = 1,
			.pCommandBuffers = &vkFrame.graphicsCmdBuf,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &vkFrame.queueSemaphore
		};
		vkQueueSubmit(vk->graphicsQueue, 1, &submitInfo, vkFrame.queueFence);

		VkPresentInfoKHR presentInfo = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vkFrame.queueSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &vk->swapChain,
			.pImageIndices = &swapChainImageIndex,
			.pResults = nullptr
		};
		VkResult presentResult = vkQueuePresentKHR(vk->graphicsQueue, &presentInfo);
		assert(presentResult == VK_SUCCESS);

		vk->frameCount++;
		vk->frameIndex = vk->frameCount % vkMaxFrameInFlight;
	}

	return 0;
}
