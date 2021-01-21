#include "scene.h"

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
	if (SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE) != S_OK) {
		fatal("SetProcessDpiAwareness failed");
	}
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fatal("SDL_Init failed");
	}
	SDL_DisplayMode displayMode;
	if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
		fatal("SDL_GetCurrentDisplayMode failed");
	}
	SDL_Window* window = SDL_CreateWindow(
		"vkrt", 
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
		int(displayMode.w * 0.75), int(displayMode.h * 0.75), 
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
	);
	if (!window) {
		fatal("SDL_CreateWindow failed: "s + SDL_GetError());
	}

	imguiInit();
	ImGuiIO& imguiIO = ImGui::GetIO();
	Vulkan* vk = Vulkan::create(window);
	Scene* scene = Scene::create("../../assets/cornell box.json", vk);

	SDL_Event event;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = false;
			}
			else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
				bool isDown = event.type == SDL_KEYDOWN;
				SDL_Scancode code = event.key.keysym.scancode;
				imguiIO.KeysDown[code] = isDown;
				if (code == SDL_SCANCODE_LSHIFT || code == SDL_SCANCODE_RSHIFT) {
					imguiIO.KeyShift = isDown;
				}
				else if (code == SDL_SCANCODE_LCTRL || code == SDL_SCANCODE_RCTRL) {
					imguiIO.KeyCtrl = isDown;
				}
				else if (code == SDL_SCANCODE_LALT || code == SDL_SCANCODE_RALT) {
					imguiIO.KeyAlt = isDown;
				}
			}
			else if (event.type == SDL_MOUSEMOTION) {
				imguiIO.MousePos = { float(event.motion.x), float(event.motion.y) };
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
				int button = -1;
				if (event.button.button == SDL_BUTTON_LEFT) button = 0;
				if (event.button.button == SDL_BUTTON_RIGHT) button = 1;
				if (event.button.button == SDL_BUTTON_MIDDLE) button = 2;
				if (button != -1) imguiIO.MouseDown[button] = (event.type == SDL_MOUSEBUTTONDOWN);
			}
		}

		int windowWidth, windowHeight;
		SDL_GetWindowSize(window, &windowWidth, &windowHeight);
		{
			ImGui::GetIO().DisplaySize = { float(windowWidth), float(windowHeight) };
			ImGui::NewFrame();
			ImGui::Begin("test");
			ImGui::End();
			ImGui::Render();
		}

		auto& vkFrame = vk->frames[vk->frameIndex];

		unsigned swapChainImageIndex;
		vkCheck(vkAcquireNextImageKHR(vk->device, vk->swapChain, UINT64_MAX, vkFrame.swapChainImageSemaphore, nullptr, &swapChainImageIndex));

		vkCheck(vkWaitForFences(vk->device, 1, &vkFrame.queueFence, true, UINT64_MAX));
		vkCheck(vkResetFences(vk->device, 1, &vkFrame.queueFence));
		vkCheck(vkResetCommandBuffer(vkFrame.cmdBuffer, 0));
		vkCheck(vkResetDescriptorPool(vk->device, vkFrame.descriptorPool, 0));

		uint8* uniformBuffersMappedMemory;
		vkCheck(vkMapMemory(vk->device, vkFrame.uniformBuffersMemory.memory, 0, VK_WHOLE_SIZE, 0, (void**)&uniformBuffersMappedMemory));

		VkCommandBufferBeginInfo commandBufferBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		vkCheck(vkBeginCommandBuffer(vkFrame.cmdBuffer, &commandBufferBeginInfo));
		{
			vkCmdBindPipeline(vkFrame.cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk->rayTracingPipeline);
			
			VkImageMemoryBarrier colorBufferImageMemoryBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_GENERAL,
				.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
				.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
				.image = vk->colorBufferTexture.first,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			vkCmdPipelineBarrier(
				vkFrame.cmdBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
				0, nullptr, 0, nullptr, 1, &colorBufferImageMemoryBarrier
			);

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = vkFrame.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &vk->rayTracingDescriptorSet0Layout
			};
			VkDescriptorSet descriptorSet;
			vkCheck(vkAllocateDescriptorSets(vk->device, &descriptorSetAllocateInfo, &descriptorSet));
			VkDescriptorImageInfo colorBufferImageInfo = { 
				.sampler = nullptr,
				.imageView = vk->colorBufferTexture.second,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL
			};
			VkWriteDescriptorSetAccelerationStructureKHR accelerationStructInfo = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
				.accelerationStructureCount = 1,
				.pAccelerationStructures = &scene->tlas
			};
			VkDescriptorBufferInfo verticesBufferInfos = {
					.buffer = scene->verticesBuffer,
					.range = VK_WHOLE_SIZE
			};
			VkDescriptorBufferInfo indicesBufferInfos = {
					.buffer = scene->indicesBuffer,
					.range = VK_WHOLE_SIZE
			};
			VkDescriptorBufferInfo geometriesBufferInfos = {
					.buffer = scene->geometriesBuffer,
					.range = VK_WHOLE_SIZE
			};
			VkDescriptorBufferInfo materialsBufferInfos = {
					.buffer = scene->materialsBuffer,
					.range = VK_WHOLE_SIZE
			};
			VkDescriptorBufferInfo instancesBufferInfos = {
					.buffer = scene->instancesBuffer,
					.range = VK_WHOLE_SIZE
			};
			std::vector<VkDescriptorImageInfo> textureImageInfos(vk->rayTracingDescriptorSet0TextureCount);
			for (auto [i, info] : enumerate(textureImageInfos)) {
				if (i < scene->textures.size()) {
					info = {
						.sampler = vk->colorBufferTextureSampler,
						.imageView = scene->textures[i].second,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
				}
				else {
					info = {
						.sampler = vk->blankTextureSampler,
						.imageView = vk->blankTexture.second,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
				}
			}
			VkWriteDescriptorSet descriptorSetWrites[] = {
				{
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					.pImageInfo = &colorBufferImageInfo
				},
				{
					.pNext = &accelerationStructInfo,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
				},
				{
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &verticesBufferInfos
				},
				{
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &indicesBufferInfos
				},
				{
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &geometriesBufferInfos
				},
				{
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &materialsBufferInfos
				},
				{
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pBufferInfo = &instancesBufferInfos
				},
				{
					.descriptorCount = (uint32)textureImageInfos.size(),
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = textureImageInfos.data()
				}
			};
			for (auto [i, setWrite] : enumerate(descriptorSetWrites)) {
				setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				setWrite.dstSet = descriptorSet;
				setWrite.dstBinding = (uint32)i;
			}
			vkUpdateDescriptorSets(vk->device, countof(descriptorSetWrites), descriptorSetWrites, 0, nullptr);
			vkCmdBindDescriptorSets(vkFrame.cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk->rayTracingPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			struct { 
				XMMATRIX screenToWorldMat;
				XMVECTOR eyePos;
			} pushConsts = {
				XMMatrixInverse(nullptr, scene->camera.viewProjMat), scene->camera.position
			};
			vkCmdPushConstants(vkFrame.cmdBuffer, vk->rayTracingPipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(pushConsts), &pushConsts);

			vkCmdTraceRays(
				vkFrame.cmdBuffer, 
				&vk->rayTracingSBTBufferRayGenDeviceAddress, 
				&vk->rayTracingSBTBufferMissDeviceAddress, 
				&vk->rayTracingSBTBufferHitGroupDeviceAddress, 
				&vk->rayTracingSBTBufferHitGroupDeviceAddress, 
				(uint32)windowWidth, (uint32)windowHeight, 1
			);

			colorBufferImageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			colorBufferImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			colorBufferImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			colorBufferImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(
				vkFrame.cmdBuffer,
				VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
				0, nullptr, 0, nullptr, 1, &colorBufferImageMemoryBarrier
			);
		}
		VkRenderPassBeginInfo renderPassBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = vk->swapChainRenderPass,
			.framebuffer = vk->swapChainImageFramebuffers[swapChainImageIndex].frameBuffer,
			.renderArea = { {0, 0}, vk->swapChainImageFramebuffers[swapChainImageIndex].imageExtent }
		};
		vkCmdBeginRenderPass(vkFrame.cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdBindPipeline(vkFrame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->swapChainPipeline);

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = vkFrame.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &vk->swapChainDescriptorSet0Layout
			};
			VkDescriptorSet descriptorSet;
			vkCheck(vkAllocateDescriptorSets(vk->device, &descriptorSetAllocateInfo, &descriptorSet));
			VkDescriptorImageInfo descriptorImageInfo = { 
				.sampler = vk->colorBufferTextureSampler, 
				.imageView = vk->colorBufferTexture.second,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			VkWriteDescriptorSet writeDescriptorSet = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &descriptorImageInfo
			};
			vkUpdateDescriptorSets(vk->device, 1, &writeDescriptorSet, 0, nullptr);
			vkCmdBindDescriptorSets(vkFrame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->swapChainPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			vkCmdDraw(vkFrame.cmdBuffer, 3, 1, 0, 0);
		}
		{
			vkCmdBindPipeline(vkFrame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->imguiPipeline);

			struct { float windowSize[2]; } pushConsts = { {float(windowWidth), float(windowHeight)} };
			vkCmdPushConstants(vkFrame.cmdBuffer, vk->imguiPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = vkFrame.descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &vk->imguiDescriptorSet0Layout
			};
			VkDescriptorSet descriptorSet;
			vkCheck(vkAllocateDescriptorSets(vk->device, &descriptorSetAllocateInfo, &descriptorSet));
			VkDescriptorImageInfo descriptorImageInfo = {
				.sampler = vk->imguiTextureSampler,
				.imageView = vk->imguiTexture.second,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			VkWriteDescriptorSet writeDescriptorSet = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &descriptorImageInfo
			};
			vkUpdateDescriptorSets(vk->device, 1, &writeDescriptorSet, 0, nullptr);
			vkCmdBindDescriptorSets(vkFrame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->imguiPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(vkFrame.cmdBuffer, 0, 1, &vkFrame.imguiVertBuffer, &offset);
			vkCmdBindIndexBuffer(vkFrame.cmdBuffer, vkFrame.imguiIdxBuffer, 0, VK_INDEX_TYPE_UINT16);

			uint64 vertBufferOffset = 0;
			uint64 idxBufferOffset = 0;
			const ImDrawData* drawData = ImGui::GetDrawData();
			for (int i = 0; i < drawData->CmdListsCount; i += 1) {
				const ImDrawList& dlist = *drawData->CmdLists[i];
				uint64 verticesSize = dlist.VtxBuffer.Size * sizeof(ImDrawVert);
				uint64 indicesSize = dlist.IdxBuffer.Size * sizeof(ImDrawIdx);
				memcpy(uniformBuffersMappedMemory + vkFrame.imguiVertBufferMemoryOffset + vertBufferOffset, dlist.VtxBuffer.Data, verticesSize);
				memcpy(uniformBuffersMappedMemory + vkFrame.imguiIdxBufferMemoryOffset + idxBufferOffset, dlist.IdxBuffer.Data, indicesSize);
				uint64 vertIndex = vertBufferOffset / sizeof(ImDrawVert);
				uint64 idxIndex = idxBufferOffset / sizeof(ImDrawIdx);
				for (int i = 0; i < dlist.CmdBuffer.Size; i += 1) {
					const ImDrawCmd& drawCmd = dlist.CmdBuffer[i];
					ImVec4 clipRect = drawCmd.ClipRect;
					if (clipRect.x < windowWidth && clipRect.y < windowHeight && clipRect.z >= 0.0f && clipRect.w >= 0.0f) {
						if (clipRect.x < 0) clipRect.x = 0;
						if (clipRect.y < 0) clipRect.y = 0;
						VkRect2D scissor = { { (int32)clipRect.x, (int32)clipRect.y }, { (uint32)(clipRect.z - clipRect.x), (uint32)(clipRect.w - clipRect.y) } };
						vkCmdSetScissor(vkFrame.cmdBuffer, 0, 1, &scissor);
						vkCmdDrawIndexed(vkFrame.cmdBuffer, drawCmd.ElemCount, 1, (uint32)idxIndex, (int32)vertIndex, 0);
					}
					idxIndex += drawCmd.ElemCount;
				}
				vertBufferOffset += align(verticesSize, sizeof(ImDrawVert));
				idxBufferOffset += align(indicesSize, sizeof(ImDrawIdx));
				assert(vertBufferOffset < vkFrame.imguiVertBufferMemorySize);
				assert(idxBufferOffset < vkFrame.imguiIdxBufferMemorySize);
			}
		}
		vkCmdEndRenderPass(vkFrame.cmdBuffer);
		vkCheck(vkEndCommandBuffer(vkFrame.cmdBuffer));

		vkUnmapMemory(vk->device, vkFrame.uniformBuffersMemory.memory);

		VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vkFrame.swapChainImageSemaphore,
			.pWaitDstStageMask = &pipelineStageFlags,
			.commandBufferCount = 1,
			.pCommandBuffers = &vkFrame.cmdBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &vkFrame.queueSemaphore
		};
		vkCheck(vkQueueSubmit(vk->graphicsQueue, 1, &submitInfo, vkFrame.queueFence));

		VkPresentInfoKHR presentInfo = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vkFrame.queueSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &vk->swapChain,
			.pImageIndices = &swapChainImageIndex,
			.pResults = nullptr
		};
		vkCheck(vkQueuePresentKHR(vk->graphicsQueue, &presentInfo));

		vk->frameCount += 1;
		vk->frameIndex = vk->frameCount % vkMaxFrameInFlight;
	}
	return EXIT_SUCCESS;
}