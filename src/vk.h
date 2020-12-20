#include "common.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.cpp>
#include <imgui_widgets.cpp>
#include <imgui_draw.cpp>
//#include <ImGuizmo.h>

#define vkCheck(VkAPICall) \
	{ \
		VkResult vkResult = VkAPICall; \
		if (vkResult != VK_SUCCESS) { \
			DebugBreak(); \
			fatal("vulkan error: "s + #VkAPICall); \
		} \
	}

void* vulkanDebugCallBackUserData = nullptr;
PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger = nullptr;
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName = nullptr;
PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelines = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandles = nullptr;
PFN_vkCmdTraceRaysKHR vkCmdTraceRays = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructures = nullptr;
PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructures = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizes = nullptr;
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructure = nullptr;

VkBool32 vulkanDebugCallBack(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT types,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData) {
	DebugBreak();
	return VK_FALSE;
}

const int vkSwapChainImageCount = 3;
const int vkMaxFrameInFlight = 3;

struct Vulkan {
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;

	uint32 graphicsComputeQueueFamilyIndex;
	VkQueue graphicsQueue;
	VkQueue computeQueue;
	uint32 transferQueueFamilyIndex;
	VkQueue transferQueue;

	VkCommandPool graphicsComputeCommandPool;
	VkCommandPool transferCommandPool;
	VkCommandBuffer computeCommandBuffer;
	VkCommandBuffer transferCommandBuffer;

	VkSwapchainKHR swapChain;
	VkRenderPass swapChainRenderPass;
	struct {
		VkImage image;
		VkImageView imageView;
		VkExtent2D imageExtent;
		VkFramebuffer frameBuffer;
	} swapChainImageFramebuffers[vkSwapChainImageCount];

	uint32 stagingBuffersMemoryType;
	uint32 gpuTexturesMemoryType;
	uint32 gpuBuffersMemoryType;
	uint32 uniformBuffersMemoryType;

	struct Memory {
		VkDeviceMemory memory;
		uint64 capacity;
		uint64 offset;
	};

	Memory stagingBuffersMemory;
	Memory gpuTexturesMemory;
	Memory gpuBuffersMemory;

	VkBuffer stagingBuffer;

	VkImage colorBuffer;
	VkImageView colorBufferView;
	VkSampler colorBufferSampler;

	VkImage imguiTexture;
	VkImageView imguiTextureView;
	VkSampler imguiTextureSampler;

	struct {
		VkCommandBuffer cmdBuffer;
		VkSemaphore swapChainImageSemaphore;
		VkSemaphore queueSemaphore;
		VkFence queueFence;
		VkDescriptorPool descriptorPool;
		Memory uniformBuffersMemory;
		VkBuffer imguiVertBuffer;
		uint64 imguiVertBufferMemoryOffset;
		uint64 imguiVertBufferMemorySize;
		VkBuffer imguiIdxBuffer;
		uint64 imguiIdxBufferMemoryOffset;
		uint64 imguiIdxBufferMemorySize;
	} frames[vkMaxFrameInFlight];
	uint64 frameCount;
	uint64 frameIndex;

	VkDescriptorSetLayout swapChainDescriptorSet0Layout;
	VkPipelineLayout swapChainPipelineLayout;
	VkPipeline swapChainPipeline;

	VkDescriptorSetLayout imguiDescriptorSet0Layout;
	VkPipelineLayout imguiPipelineLayout;
	VkPipeline imguiPipeline;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
	VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;
	VkDescriptorSetLayout rayTracingDescriptorSet0Layout;
	VkPipelineLayout rayTracingPipelineLayout;
	VkPipeline rayTracingPipeline;
	VkBuffer rayTracingSBTBuffer;
	VkStridedDeviceAddressRegionKHR rayTracingSBTBufferRayGenDeviceAddress;
	VkStridedDeviceAddressRegionKHR rayTracingSBTBufferMissDeviceAddress;
	VkStridedDeviceAddressRegionKHR rayTracingSBTBufferHitGroupDeviceAddress;

	static Vulkan* create(SDL_Window* sdlWindow) {
		auto vk = new Vulkan();
		int windowW, windowH;
		SDL_GetWindowSize(sdlWindow, &windowW, &windowH);
		{
			uint32 instanceExtensionCount;
			vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));
			std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
			vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data()));

			VkApplicationInfo appInfo = {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.apiVersion = VK_MAKE_VERSION(1, 2, 0)
			};
			const char* enabledInstanceLayers[] = { "VK_LAYER_KHRONOS_validation" };
			const char* enabledInstanceExtensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface", "VK_EXT_debug_utils", "VK_EXT_debug_report" };
			VkInstanceCreateInfo instanceCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo = &appInfo,
				.enabledLayerCount = countof(enabledInstanceLayers),
				.ppEnabledLayerNames = enabledInstanceLayers,
				.enabledExtensionCount = countof(enabledInstanceExtensions),
				.ppEnabledExtensionNames = enabledInstanceExtensions
			};
			vkCheck(vkCreateInstance(&instanceCreateInfo, nullptr, &vk->instance));
			{
				vkCreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk->instance, "vkCreateDebugUtilsMessengerEXT");
				vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetInstanceProcAddr(vk->instance, "vkDebugMarkerSetObjectNameEXT");
				vkCreateRayTracingPipelines = (PFN_vkCreateRayTracingPipelinesKHR)vkGetInstanceProcAddr(vk->instance, "vkCreateRayTracingPipelinesKHR");
				vkGetRayTracingShaderGroupHandles = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetInstanceProcAddr(vk->instance, "vkGetRayTracingShaderGroupHandlesKHR");
				vkCmdTraceRays = (PFN_vkCmdTraceRaysKHR)vkGetInstanceProcAddr(vk->instance, "vkCmdTraceRaysKHR");
				vkCmdBuildAccelerationStructures = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(vk->instance, "vkCmdBuildAccelerationStructuresKHR");
				vkBuildAccelerationStructures = (PFN_vkBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(vk->instance, "vkBuildAccelerationStructuresKHR");
				vkGetAccelerationStructureBuildSizes = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetInstanceProcAddr(vk->instance, "vkGetAccelerationStructureBuildSizesKHR");
				vkCreateAccelerationStructure = (PFN_vkCreateAccelerationStructureKHR)vkGetInstanceProcAddr(vk->instance, "vkCreateAccelerationStructureKHR");
				assert(vkCreateDebugUtilsMessenger);
				assert(vkDebugMarkerSetObjectName);
				assert(vkCreateRayTracingPipelines);
				assert(vkGetRayTracingShaderGroupHandles);
				assert(vkCmdTraceRays);
				assert(vkCmdBuildAccelerationStructures);
				assert(vkBuildAccelerationStructures);
				assert(vkGetAccelerationStructureBuildSizes);
				assert(vkCreateAccelerationStructure);
			}
			VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
			debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
			debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			debugUtilsMessengerCreateInfo.pfnUserCallback = vulkanDebugCallBack;
			debugUtilsMessengerCreateInfo.pUserData = &vulkanDebugCallBackUserData;
			VkDebugUtilsMessengerEXT messenger;
			vkCheck(vkCreateDebugUtilsMessenger(vk->instance, &debugUtilsMessengerCreateInfo, NULL, &messenger));
		}
		{
			SDL_SysWMinfo sdlWindowInfo;
			SDL_VERSION(&sdlWindowInfo.version);
			if (SDL_GetWindowWMInfo(sdlWindow, &sdlWindowInfo) != SDL_TRUE) {
				fatal("SDL_GetWindowWMInfo failed");
			}
			VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
			surfaceCreateInfo.hinstance = sdlWindowInfo.info.win.hinstance;
			surfaceCreateInfo.hwnd = sdlWindowInfo.info.win.window;
			vkCheck(vkCreateWin32SurfaceKHR(vk->instance, &surfaceCreateInfo, nullptr, &vk->surface));
		}
		{
			uint32 physicalDeviceCount;
			vkCheck(vkEnumeratePhysicalDevices(vk->instance, &physicalDeviceCount, nullptr));
			std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
			vkCheck(vkEnumeratePhysicalDevices(vk->instance, &physicalDeviceCount, physicalDevices.data()));

			for (auto& pDevice : physicalDevices) {
				VkPhysicalDeviceProperties props;
				vkGetPhysicalDeviceProperties(pDevice, &props);
				if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
					vk->physicalDevice = pDevice;
					break;
				}
			}
			if (!vk->physicalDevice) {
				fatal("No discrete GPU found");
			}
		}
		{
			uint32 queueFamilyPropCount;
			vkGetPhysicalDeviceQueueFamilyProperties(vk->physicalDevice, &queueFamilyPropCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
			vkGetPhysicalDeviceQueueFamilyProperties(vk->physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());
			vk->graphicsComputeQueueFamilyIndex = UINT_MAX;
			vk->transferQueueFamilyIndex = UINT_MAX;
			for (uint32 i = 0; i < queueFamilyProps.size(); i++) {
				if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
					queueFamilyProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
					queueFamilyProps[i].queueCount >= 2) {
					VkBool32 presentSupport = false;
					vkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(vk->physicalDevice, i, vk->surface, &presentSupport));
					if (presentSupport) {
						vk->graphicsComputeQueueFamilyIndex = i;
						queueFamilyProps[i].queueCount -= 2;
						break;
					}
				}
			}
			for (uint32 i = 0; i < queueFamilyProps.size(); i++) {
				if (queueFamilyProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT && queueFamilyProps[i].queueCount > 0) {
					vk->transferQueueFamilyIndex = i;
					queueFamilyProps[i].queueCount -= 1;
					break;
				}
			}
			if (vk->graphicsComputeQueueFamilyIndex == UINT_MAX) {
				fatal("No graphics/compute queues found");
			}
			if (vk->transferQueueFamilyIndex == UINT_MAX) {
				fatal("No transfer queue found");
			}
		}
		{
			uint32 deviceExtensionPropCount;
			vkCheck(vkEnumerateDeviceExtensionProperties(vk->physicalDevice, nullptr, &deviceExtensionPropCount, nullptr));
			std::vector<VkExtensionProperties> deviceExtensionProps(deviceExtensionPropCount);
			vkCheck(vkEnumerateDeviceExtensionProperties(vk->physicalDevice, nullptr, &deviceExtensionPropCount, deviceExtensionProps.data()));

			const char* enabledDeviceExtensions[] = {
				"VK_KHR_swapchain", "VK_EXT_debug_marker",
				"VK_EXT_descriptor_indexing", "VK_KHR_buffer_device_address",
				"VK_KHR_acceleration_structure", "VK_KHR_ray_tracing_pipeline", "VK_KHR_deferred_host_operations" 
			};
			VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
			};
			VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
				.pNext = &accelerationStructureFeatures
			};
			VkPhysicalDeviceBufferDeviceAddressFeaturesEXT bufferDeviceAddressFeatures = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
				.pNext = &rayTracingPipelineFeatures
			};
			VkPhysicalDeviceFeatures2 features = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &bufferDeviceAddressFeatures
			};
			vkGetPhysicalDeviceFeatures2(vk->physicalDevice, &features);
			if (!features.features.shaderSampledImageArrayDynamicIndexing) {
				fatal("GPU does not support vulkan shaderSampledImageArrayDynamicIndexing");
			}

			vk->accelerationStructureProperties = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
			};
			vk->rayTracingProperties = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
				.pNext = &vk->accelerationStructureProperties
			};
			VkPhysicalDeviceProperties2 properties = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
				.pNext = &vk->rayTracingProperties
			};
			vkGetPhysicalDeviceProperties2(vk->physicalDevice, &properties);
			if (properties.properties.limits.maxDescriptorSetSampledImages < 10000) {
				fatal("GPU maxDescriptorSetSampledImages potentially too small");
			}

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
				.pNext = &bufferDeviceAddressFeatures,
				.queueCreateInfoCount = queueCreateInfoCount,
				.pQueueCreateInfos = queueCreateInfos,
				.enabledExtensionCount = countof(enabledDeviceExtensions),
				.ppEnabledExtensionNames = enabledDeviceExtensions,
				.pEnabledFeatures = &features.features
			};
			vkCheck(vkCreateDevice(vk->physicalDevice, &deviceCreateInfo, nullptr, &vk->device));
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
			vkCheck(vkCreateCommandPool(vk->device, &commandPoolCreateInfo, nullptr, &vk->graphicsComputeCommandPool));

			commandPoolCreateInfo.queueFamilyIndex = vk->transferQueueFamilyIndex;
			vkCheck(vkCreateCommandPool(vk->device, &commandPoolCreateInfo, nullptr, &vk->transferCommandPool));

			VkCommandBufferAllocateInfo commandBufferAllocInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = vk->graphicsComputeCommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};
			vkCheck(vkAllocateCommandBuffers(vk->device, &commandBufferAllocInfo, &vk->computeCommandBuffer));

			commandBufferAllocInfo.commandPool = vk->transferCommandPool;
			vkCheck(vkAllocateCommandBuffers(vk->device, &commandBufferAllocInfo, &vk->transferCommandBuffer));
		}
		{
			uint32 presentModeCount;
			vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physicalDevice, vk->surface, &presentModeCount, nullptr));
			std::vector<VkPresentModeKHR> presentModes(presentModeCount);
			vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physicalDevice, vk->surface, &presentModeCount, presentModes.data()));

			VkSurfaceCapabilitiesKHR surfaceCapabilities;
			vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physicalDevice, vk->surface, &surfaceCapabilities));
			if (windowW != surfaceCapabilities.currentExtent.width or windowH != surfaceCapabilities.currentExtent.height) {
				fatal("mismatch size between sdl_window and vulkan surface");
			}

			uint32 surfaceFormatCount;
			vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physicalDevice, vk->surface, &surfaceFormatCount, nullptr));
			std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
			vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physicalDevice, vk->surface, &surfaceFormatCount, surfaceFormats.data()));
			bool formatSupport = false;
			for (auto& format : surfaceFormats) {
				if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					formatSupport = true;
					break;
				}
			}
			if (!formatSupport) {
				fatal("No VK_FORMAT_B8G8R8A8_UNORM + VK_COLOR_SPACE_SRGB_NONLINEAR_KHR surface format found");
			}

			VkSwapchainCreateInfoKHR swapChainCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.surface = vk->surface,
				.minImageCount = vkSwapChainImageCount,
				.imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
				.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
				.imageExtent = surfaceCapabilities.currentExtent,
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = &vk->graphicsComputeQueueFamilyIndex,
				.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
				.clipped = true
			};
			vkCheck(vkCreateSwapchainKHR(vk->device, &swapChainCreateInfo, nullptr, &vk->swapChain));
		}
		{
			VkAttachmentDescription attachment = {
				.format = VK_FORMAT_B8G8R8A8_UNORM,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
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
			vkCheck(vkCreateRenderPass(vk->device, &renderPassCreateInfo, nullptr, &vk->swapChainRenderPass));
		}
		{
			uint32 swapChainImageCount;
			vkCheck(vkGetSwapchainImagesKHR(vk->device, vk->swapChain, &swapChainImageCount, nullptr));
			if (swapChainImageCount != vkSwapChainImageCount) {
				fatal("vkGetSwapchainImagesKHR returned different image count than vkSwapChainImageCount");
			}
			VkImage swapChainImages[vkSwapChainImageCount];
			vkCheck(vkGetSwapchainImagesKHR(vk->device, vk->swapChain, &swapChainImageCount, swapChainImages));
			for (uint i = 0; i < countof(vk->swapChainImageFramebuffers); i++) {
				auto& imageFramebuffer = vk->swapChainImageFramebuffers[i];
				imageFramebuffer.image = swapChainImages[i];

				VkImageViewCreateInfo imageCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = imageFramebuffer.image,
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = VK_FORMAT_B8G8R8A8_UNORM,
					.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				};
				vkCheck(vkCreateImageView(vk->device, &imageCreateInfo, nullptr, &imageFramebuffer.imageView));

				int windowW, windowH;
				SDL_GetWindowSize(sdlWindow, &windowW, &windowH);
				imageFramebuffer.imageExtent = { uint32(windowW), uint32(windowH) };

				VkFramebufferCreateInfo framebufferCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					.renderPass = vk->swapChainRenderPass,
					.attachmentCount = 1,
					.pAttachments = &imageFramebuffer.imageView,
					.width = uint32(windowW),
					.height = uint32(windowH),
					.layers = 1
				};
				vkCheck(vkCreateFramebuffer(vk->device, &framebufferCreateInfo, nullptr, &imageFramebuffer.frameBuffer));
			}
		}
		{
			VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
			vkGetPhysicalDeviceMemoryProperties(vk->physicalDevice, &physicalDeviceMemoryProperties);

			bool stagingBuffersMemoryTypeFound = false;
			bool gpuTexturesMemoryTypeFound = false;
			bool gpuBuffersMemoryTypeFound = false;
			bool uniformBuffersMemoryTypeFound = false;
			for (uint32 i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
				VkMemoryType& mtype = physicalDeviceMemoryProperties.memoryTypes[i];
				if (mtype.propertyFlags == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					vk->stagingBuffersMemoryType = i;
					stagingBuffersMemoryTypeFound = true;
				}
				else if (mtype.propertyFlags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
					vk->gpuTexturesMemoryType = i;
					gpuTexturesMemoryTypeFound = true;
					vk->gpuBuffersMemoryType = i;
					gpuBuffersMemoryTypeFound = true;
				}
				else if (mtype.propertyFlags == (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					vk->uniformBuffersMemoryType = i;
					uniformBuffersMemoryTypeFound = true;
				}
			}
			if (!stagingBuffersMemoryTypeFound) fatal("stagingBuffersMemoryType not found");
			if (!gpuTexturesMemoryTypeFound) fatal("gpuTexturesMemoryType not found");
			if (!gpuBuffersMemoryTypeFound) fatal("gpuBuffersMemoryType not found");
			if (!uniformBuffersMemoryTypeFound) fatal("uniformBuffersMemoryType not found");

			VkMemoryAllocateInfo memoryAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = 512_mb,
				.memoryTypeIndex = vk->stagingBuffersMemoryType
			};
			vkCheck(vkAllocateMemory(vk->device, &memoryAllocateInfo, nullptr, &vk->stagingBuffersMemory.memory));
			vk->stagingBuffersMemory.capacity = memoryAllocateInfo.allocationSize;
			vk->stagingBuffersMemory.offset = 0;

			memoryAllocateInfo.allocationSize = 4_gb;
			memoryAllocateInfo.memoryTypeIndex = vk->gpuTexturesMemoryType;
			vkCheck(vkAllocateMemory(vk->device, &memoryAllocateInfo, nullptr, &vk->gpuTexturesMemory.memory));
			vk->gpuTexturesMemory.capacity = memoryAllocateInfo.allocationSize;
			vk->gpuTexturesMemory.offset = 0;

			VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
				.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
			};
			memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
			memoryAllocateInfo.allocationSize = 1_gb;
			memoryAllocateInfo.memoryTypeIndex = vk->gpuBuffersMemoryType;
			vkCheck(vkAllocateMemory(vk->device, &memoryAllocateInfo, nullptr, &vk->gpuBuffersMemory.memory));
			vk->gpuBuffersMemory.capacity = memoryAllocateInfo.allocationSize;
			vk->gpuBuffersMemory.offset = 0;
		}
		{
			VkBufferCreateInfo bufferCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = vk->stagingBuffersMemory.capacity,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			};
			vkCheck(vkCreateBuffer(vk->device, &bufferCreateInfo, nullptr, &vk->stagingBuffer));
			VkMemoryRequirements stagingBufferMemoryRequirements;
			vkGetBufferMemoryRequirements(vk->device, vk->stagingBuffer, &stagingBufferMemoryRequirements);
			vkCheck(vkBindBufferMemory(vk->device, vk->stagingBuffer, vk->stagingBuffersMemory.memory, 0));
			vk->stagingBuffersMemory.offset += stagingBufferMemoryRequirements.size;
		}
		{
			VkImageCreateInfo imageCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.extent = { uint32(windowW), uint32(windowH), 1 },
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			};
			vkCheck(vkCreateImage(vk->device, &imageCreateInfo, nullptr, &vk->colorBuffer));
			VkMemoryRequirements memoryRequirement;
			vkGetImageMemoryRequirements(vk->device, vk->colorBuffer, &memoryRequirement);
			vk->gpuTexturesMemory.offset = align(vk->gpuTexturesMemory.offset, memoryRequirement.alignment);
			vkCheck(vkBindImageMemory(vk->device, vk->colorBuffer, vk->gpuTexturesMemory.memory, vk->gpuTexturesMemory.offset));
			vk->gpuTexturesMemory.offset += memoryRequirement.size;
			VkImageViewCreateInfo imageViewCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = vk->colorBuffer,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = imageCreateInfo.format,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			vkCheck(vkCreateImageView(vk->device, &imageViewCreateInfo, nullptr, &vk->colorBufferView));
			VkSamplerCreateInfo samplerCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR
			};
			vkCheck(vkCreateSampler(vk->device, &samplerCreateInfo, nullptr, &vk->colorBufferSampler));
		}
		{
			ImFont* imFont = ImGui::GetIO().Fonts->AddFontDefault();
			assert(imFont);
			uint8* imguiTextureData;
			int imguiTextureWidth;
			int imguiTextureHeight;
			ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&imguiTextureData, &imguiTextureWidth, &imguiTextureHeight);
			VkImageCreateInfo imageCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R8G8B8A8_UNORM,
				.extent = { uint32(imguiTextureWidth), uint32(imguiTextureHeight), 1 },
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			};
			vkCheck(vkCreateImage(vk->device, &imageCreateInfo, nullptr, &vk->imguiTexture));
			VkMemoryRequirements memoryRequirement;
			vkGetImageMemoryRequirements(vk->device, vk->imguiTexture, &memoryRequirement);
			vk->gpuTexturesMemory.offset = align(vk->gpuTexturesMemory.offset, memoryRequirement.alignment);
			vkCheck(vkBindImageMemory(vk->device, vk->imguiTexture, vk->gpuTexturesMemory.memory, vk->gpuTexturesMemory.offset));
			vk->gpuTexturesMemory.offset += memoryRequirement.size;
			VkImageViewCreateInfo imageViewCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = vk->imguiTexture,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = imageCreateInfo.format,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			vkCheck(vkCreateImageView(vk->device, &imageViewCreateInfo, nullptr, &vk->imguiTextureView));
			VkSamplerCreateInfo samplerCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR
			};
			vkCheck(vkCreateSampler(vk->device, &samplerCreateInfo, nullptr, &vk->imguiTextureSampler));

			void* mappedStagingMemory;
			vkCheck(vkMapMemory(vk->device, vk->stagingBuffersMemory.memory, 0, imguiTextureWidth * imguiTextureHeight * 4, 0, &mappedStagingMemory));
			memcpy(mappedStagingMemory, imguiTextureData, imguiTextureWidth * imguiTextureHeight * 4);
			vkUnmapMemory(vk->device, vk->stagingBuffersMemory.memory);

			VkCommandBufferBeginInfo cmdBufferBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkCheck(vkResetCommandBuffer(vk->transferCommandBuffer, 0));
			vkCheck(vkBeginCommandBuffer(vk->transferCommandBuffer, &cmdBufferBeginInfo));
			VkImageMemoryBarrier imageBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
				.dstQueueFamilyIndex = vk->transferQueueFamilyIndex,
				.image = vk->imguiTexture,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			vkCmdPipelineBarrier(vk->transferCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
			VkBufferImageCopy bufferImageCopy = {
				.bufferOffset = 0,
				.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				.imageExtent = { uint32(imguiTextureWidth), uint32(imguiTextureHeight), 1 }
			};
			vkCmdCopyBufferToImage(vk->transferCommandBuffer, vk->stagingBuffer, vk->imguiTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);
			imageBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = vk->transferQueueFamilyIndex,
				.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
				.image = vk->imguiTexture,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			vkCmdPipelineBarrier(vk->transferCommandBuffer, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
			vkCheck(vkEndCommandBuffer(vk->transferCommandBuffer));
			VkSubmitInfo queueSubmitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &vk->transferCommandBuffer
			};
			vkCheck(vkQueueSubmit(vk->transferQueue, 1, &queueSubmitInfo, nullptr));
			vkCheck(vkQueueWaitIdle(vk->transferQueue));
		}
		{
			for (auto& frame : vk->frames) {
				VkCommandBufferAllocateInfo commandBufferAllocInfo = {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = vk->graphicsComputeCommandPool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1
				};
				vkCheck(vkAllocateCommandBuffers(vk->device, &commandBufferAllocInfo, &frame.cmdBuffer));

				VkSemaphoreCreateInfo semaphoreCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
				};
				vkCheck(vkCreateSemaphore(vk->device, &semaphoreCreateInfo, nullptr, &frame.swapChainImageSemaphore));
				vkCheck(vkCreateSemaphore(vk->device, &semaphoreCreateInfo, nullptr, &frame.queueSemaphore));

				VkFenceCreateInfo fenceCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
					.flags = VK_FENCE_CREATE_SIGNALED_BIT
				};
				vkCheck(vkCreateFence(vk->device, &fenceCreateInfo, nullptr, &frame.queueFence));

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
				vkCheck(vkCreateDescriptorPool(vk->device, &descriptorPoolCreateInfo, nullptr, &frame.descriptorPool));

				VkMemoryAllocateInfo memoryAllocateInfo = {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.allocationSize = 64_mb,
					.memoryTypeIndex = vk->uniformBuffersMemoryType
				};
				vkCheck(vkAllocateMemory(vk->device, &memoryAllocateInfo, nullptr, &frame.uniformBuffersMemory.memory));
				frame.uniformBuffersMemory.capacity = memoryAllocateInfo.allocationSize;
				frame.uniformBuffersMemory.offset = 0;

				VkBufferCreateInfo bufferCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					.size = 2_mb,
					.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
				};
				vkCheck(vkCreateBuffer(vk->device, &bufferCreateInfo, nullptr, &frame.imguiVertBuffer));

				VkMemoryRequirements bufferMemoryRequirements;
				vkGetBufferMemoryRequirements(vk->device, frame.imguiVertBuffer, &bufferMemoryRequirements);
				frame.uniformBuffersMemory.offset = align(frame.uniformBuffersMemory.offset, bufferMemoryRequirements.alignment);
				vkCheck(vkBindBufferMemory(vk->device, frame.imguiVertBuffer, frame.uniformBuffersMemory.memory, frame.uniformBuffersMemory.offset));
				frame.imguiVertBufferMemoryOffset = frame.uniformBuffersMemory.offset;
				frame.imguiVertBufferMemorySize = bufferMemoryRequirements.size;
				frame.uniformBuffersMemory.offset += bufferMemoryRequirements.size;

				bufferCreateInfo.size = 1_mb;
				bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
				vkCheck(vkCreateBuffer(vk->device, &bufferCreateInfo, nullptr, &frame.imguiIdxBuffer));

				vkGetBufferMemoryRequirements(vk->device, frame.imguiIdxBuffer, &bufferMemoryRequirements);
				frame.uniformBuffersMemory.offset = align(frame.uniformBuffersMemory.offset, bufferMemoryRequirements.alignment);
				vkCheck(vkBindBufferMemory(vk->device, frame.imguiIdxBuffer, frame.uniformBuffersMemory.memory, frame.uniformBuffersMemory.offset));
				frame.imguiIdxBufferMemoryOffset = frame.uniformBuffersMemory.offset;
				frame.imguiIdxBufferMemorySize = bufferMemoryRequirements.size;
				frame.uniformBuffersMemory.offset += bufferMemoryRequirements.size;
			}
		}
		{
			std::vector<char> vertexShaderCode;
			std::vector<char> fragmentShaderCode;
			VkShaderModuleCreateInfo shaderModuleCreateInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			VkPipelineShaderStageCreateInfo vertFragStages[2] = {
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

			vertexShaderCode = readFile("swapChain.vert.spv");
			fragmentShaderCode = readFile("swapChain.frag.spv");
			shaderModuleCreateInfo.codeSize = vertexShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)vertexShaderCode.data();
			vkCheck(vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &vertFragStages[0].module));
			shaderModuleCreateInfo.codeSize = fragmentShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)fragmentShaderCode.data();
			vkCheck(vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &vertFragStages[1].module));

			inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			VkViewport viewport = { 0, 0, float(windowW), float(windowH), 0, 1 };
			VkRect2D scissor = { {0, 0}, {uint32(windowW), uint32(windowH)} };
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

			VkDescriptorSetLayoutBinding swapChainDescriptorSetBindings[1] = {
				{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
			};
			descriptorSetLayoutCreateInfo.bindingCount = countof(swapChainDescriptorSetBindings);
			descriptorSetLayoutCreateInfo.pBindings = swapChainDescriptorSetBindings;
			vkCheck(vkCreateDescriptorSetLayout(vk->device, &descriptorSetLayoutCreateInfo, nullptr, &vk->swapChainDescriptorSet0Layout));

			pipelineLayoutCreateInfo.setLayoutCount = 1;
			pipelineLayoutCreateInfo.pSetLayouts = &vk->swapChainDescriptorSet0Layout;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
			vkCheck(vkCreatePipelineLayout(vk->device, &pipelineLayoutCreateInfo, nullptr, &vk->swapChainPipelineLayout));

			graphicsPipelineCreateInfo.layout = vk->swapChainPipelineLayout;
			graphicsPipelineCreateInfo.renderPass = vk->swapChainRenderPass;
			graphicsPipelineCreateInfo.subpass = 0;
			vkCheck(vkCreateGraphicsPipelines(vk->device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &vk->swapChainPipeline));

			vertexShaderCode = readFile("imgui.vert.spv");
			fragmentShaderCode = readFile("imgui.frag.spv");
			shaderModuleCreateInfo.codeSize = vertexShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)vertexShaderCode.data();
			vkCheck(vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &vertFragStages[0].module));
			shaderModuleCreateInfo.codeSize = fragmentShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)fragmentShaderCode.data();
			vkCheck(vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &vertFragStages[1].module));

			VkVertexInputBindingDescription imguiVertexBindingDescription = {
				0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX
			};
			VkVertexInputAttributeDescription imguiVertexAttributeDescriptions[] = {
				{ 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
				{ 1, 0, VK_FORMAT_R32G32_SFLOAT, 8 },
				{ 2, 0, VK_FORMAT_R8G8B8A8_UNORM, 16 }
			};
			vertexInputState.vertexBindingDescriptionCount = 1;
			vertexInputState.pVertexBindingDescriptions = &imguiVertexBindingDescription;
			vertexInputState.vertexAttributeDescriptionCount = countof(imguiVertexAttributeDescriptions);
			vertexInputState.pVertexAttributeDescriptions = imguiVertexAttributeDescriptions;

			colorBlendAttachmentState.blendEnable = VK_TRUE;
			colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VkDynamicState dynamicScissor = VK_DYNAMIC_STATE_SCISSOR;
			dynamicState.dynamicStateCount = 1;
			dynamicState.pDynamicStates = &dynamicScissor;

			VkDescriptorSetLayoutBinding imguiDescriptorSetBindings[1] = {
				{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
			};
			descriptorSetLayoutCreateInfo.bindingCount = countof(imguiDescriptorSetBindings);
			descriptorSetLayoutCreateInfo.pBindings = imguiDescriptorSetBindings;
			vkCheck(vkCreateDescriptorSetLayout(vk->device, &descriptorSetLayoutCreateInfo, nullptr, &vk->imguiDescriptorSet0Layout));

			VkPushConstantRange imguiPushConstantRange = { VK_SHADER_STAGE_VERTEX_BIT, 0, 16 };
			pipelineLayoutCreateInfo.setLayoutCount = 1;
			pipelineLayoutCreateInfo.pSetLayouts = &vk->imguiDescriptorSet0Layout;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pipelineLayoutCreateInfo.pPushConstantRanges = &imguiPushConstantRange;
			vkCheck(vkCreatePipelineLayout(vk->device, &pipelineLayoutCreateInfo, nullptr, &vk->imguiPipelineLayout));

			graphicsPipelineCreateInfo.layout = vk->imguiPipelineLayout;
			graphicsPipelineCreateInfo.renderPass = vk->swapChainRenderPass;
			graphicsPipelineCreateInfo.subpass = 0;
			vkCheck(vkCreateGraphicsPipelines(vk->device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &vk->imguiPipeline));
		}
		{
			VkDescriptorSetLayoutBinding descriptorSetBindings[1] = {
				{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}
			};
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = countof(descriptorSetBindings),
				.pBindings = descriptorSetBindings
			};
			vkCheck(vkCreateDescriptorSetLayout(vk->device, &descriptorSetLayoutCreateInfo, nullptr, &vk->rayTracingDescriptorSet0Layout));

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.setLayoutCount = 1,
				.pSetLayouts = &vk->rayTracingDescriptorSet0Layout
			};
			vkCheck(vkCreatePipelineLayout(vk->device, &pipelineLayoutCreateInfo, nullptr, &vk->rayTracingPipelineLayout));

			VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
					.pName = "main"
				},
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_MISS_BIT_KHR,
					.pName = "main"
				}
			};

			std::vector<char> rayGenShaderCode = readFile("rayTrace.rgen.spv");
			std::vector<char> rayMissShaderCode = readFile("rayTrace.rmiss.spv");
			VkShaderModuleCreateInfo shaderModuleCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = rayGenShaderCode.size(),
				.pCode = (uint32*)rayGenShaderCode.data()
			};
			vkCheck(vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &shaderStageCreateInfos[0].module));
			shaderModuleCreateInfo.codeSize = rayMissShaderCode.size();
			shaderModuleCreateInfo.pCode = (uint32*)rayMissShaderCode.data();
			vkCheck(vkCreateShaderModule(vk->device, &shaderModuleCreateInfo, nullptr, &shaderStageCreateInfos[1].module));

			VkRayTracingShaderGroupCreateInfoKHR shaderGroups[2] = {
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
				}
			};

			VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
				.stageCount = countof(shaderStageCreateInfos),
				.pStages = shaderStageCreateInfos,
				.groupCount = countof(shaderGroups),
				.pGroups = shaderGroups,
				.layout = vk->rayTracingPipelineLayout
			};
			vkCheck(vkCreateRayTracingPipelines(vk->device, nullptr, nullptr, 1, &pipelineCreateInfo, nullptr, &vk->rayTracingPipeline));

			std::vector<char> shaderGroupHandles(vk->rayTracingProperties.shaderGroupHandleSize* countof(shaderGroups));
			vkCheck(vkGetRayTracingShaderGroupHandles(vk->device, vk->rayTracingPipeline, 0, countof(shaderGroups), shaderGroupHandles.size(), shaderGroupHandles.data()));

			uint64 alignedGroupSize = align(vk->rayTracingProperties.shaderGroupHandleSize, vk->rayTracingProperties.shaderGroupBaseAlignment);
			uint64 sbtBufferSize = alignedGroupSize * countof(shaderGroups);
			VkBufferCreateInfo bufferCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = sbtBufferSize,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			};
			vkCheck(vkCreateBuffer(vk->device, &bufferCreateInfo, nullptr, &vk->rayTracingSBTBuffer));
			VkMemoryRequirements bufferMemoryRequirements;
			vkGetBufferMemoryRequirements(vk->device, vk->rayTracingSBTBuffer, &bufferMemoryRequirements);
			vk->gpuBuffersMemory.offset = align(vk->gpuBuffersMemory.offset, bufferMemoryRequirements.alignment);
			vkCheck(vkBindBufferMemory(vk->device, vk->rayTracingSBTBuffer, vk->gpuBuffersMemory.memory, 0));
			vk->gpuBuffersMemory.offset += bufferMemoryRequirements.size;

			uint8* stagingBufferMappedMemory;
			vkCheck(vkMapMemory(vk->device, vk->stagingBuffersMemory.memory, 0, sbtBufferSize, 0, (void**)&stagingBufferMappedMemory));
			for (uint i = 0; i < countof(shaderGroups); i += 1) {
				memcpy(stagingBufferMappedMemory + i * alignedGroupSize,
					shaderGroupHandles.data() + i * vk->rayTracingProperties.shaderGroupHandleSize,
					vk->rayTracingProperties.shaderGroupHandleSize);
			}
			vkUnmapMemory(vk->device, vk->stagingBuffersMemory.memory);
			VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkCheck(vkResetCommandBuffer(vk->transferCommandBuffer, 0));
			vkCheck(vkBeginCommandBuffer(vk->transferCommandBuffer, &beginInfo));
			VkBufferCopy bufferCopy = { 0, 0, sbtBufferSize };
			vkCmdCopyBuffer(vk->transferCommandBuffer, vk->stagingBuffer, vk->rayTracingSBTBuffer, 1, &bufferCopy);
			vkCheck(vkEndCommandBuffer(vk->transferCommandBuffer));
			VkSubmitInfo submitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &vk->transferCommandBuffer
			};
			vkCheck(vkQueueSubmit(vk->transferQueue, 1, &submitInfo, nullptr));
			vkCheck(vkQueueWaitIdle(vk->transferQueue));

			VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
				.buffer = vk->rayTracingSBTBuffer
			};
			VkDeviceAddress sbtBasedAddress = vkGetBufferDeviceAddress(vk->device, &bufferDeviceAddressInfo);
			vk->rayTracingSBTBufferRayGenDeviceAddress = { sbtBasedAddress, 0, 0 };
			vk->rayTracingSBTBufferMissDeviceAddress = { sbtBasedAddress + alignedGroupSize, 0, 0 };
		}

		return vk;
	}
};