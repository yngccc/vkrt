#include "vk.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <json.hpp>
using json = nlohmann::json;

struct Camera {
	glm::vec3 position = { 0, 0, 10 };
	glm::vec3 view = { 0, 0, -1 };
};

struct Model {
	std::string name;
	std::filesystem::path filePath;
	cgltf_data* gltfData;
};

struct Light {
	enum Type {
		Point
	};
	Type type;
	glm::vec3 position;
	glm::vec3 color;
};

struct Scene {
	std::filesystem::path filePath;
	Camera camera;
	std::vector<Model> models;
	std::vector<Light> lights;

	struct BLAS {
		VkAccelerationStructureBuildGeometryInfoKHR info;
		VkAccelerationStructureBuildSizesInfoKHR size;
		std::vector<VkAccelerationStructureGeometryKHR> geometries;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> geometryRanges;
	};
	std::vector<BLAS> BLASs;
	VkAccelerationStructureKHR TLAS;

	static Scene* create(const std::filesystem::path& filePath, Vulkan* vk) {
		auto scene = new Scene();
		scene->filePath = filePath;
		scene->loadJson();
		scene->loadModelsData();
		scene->generateAccelerationStructure(vk);
		return scene;
	}
	void loadJson() {
		std::ifstream file(filePath);
		json j;
		file >> j;
		auto camera = j["camera"];
		if (!camera.is_null()) {
			auto position = camera["position"];
			auto view = camera["view"];
			this->camera.position = { position[0], position[1], position[2] };
			this->camera.view = { view[0], view[1], view[2] };
		}
		auto models = j["models"];
		if (!models.is_null()) {
			for (auto& model : models) {
				Model m = {
					.name = model["name"],
					.filePath = std::string(model["path"])
				};
				this->models.push_back(m);
			}
		}
		auto lights = j["lights"];
		if (!lights.is_null()) {
			for (auto& light : lights) {
				auto type = light["type"];
				if (type.is_string() && type == "point") {
					auto position = light["position"];
					auto color = light["color"];
					Light l = {
						.position = { position[0], position[1], position[2] },
						.color = { color[0], color[1], color[2] }
					};
					this->lights.push_back(l);
				}
			}
		}
	}
	void loadModelsData() {
		for (auto& model : models) {
			std::filesystem::path path = filePath.parent_path() / model.filePath;
			cgltf_options gltfOption = {};
			std::string filePath = path.generic_string();
			cgltf_result parseFileResult = cgltf_parse_file(&gltfOption, filePath.c_str(), &model.gltfData);
			assert(parseFileResult == cgltf_result_success);
			assert(model.gltfData->scene);
			cgltf_result loadBuffersResult = cgltf_load_buffers(&gltfOption, model.gltfData, filePath.c_str());
			assert(loadBuffersResult == cgltf_result_success);
		}
	}
	void generateAccelerationStructure(Vulkan* vk) {
		for (uint64 modelIndex = 0; modelIndex < models.size(); modelIndex += 1) {
			Model* model = &models[modelIndex];
			for (uint64 meshIndex = 0; meshIndex < model->gltfData->meshes_count; meshIndex += 1) {
				cgltf_mesh* mesh = &model->gltfData->meshes[meshIndex];
				std::vector<VkAccelerationStructureGeometryKHR> geometries(mesh->primitives_count);
				std::vector<VkAccelerationStructureBuildRangeInfoKHR> geometryRanges(mesh->primitives_count);
				for (uint64 primitiveIndex = 0; primitiveIndex < mesh->primitives_count; primitiveIndex += 1) {
					cgltf_primitive* primitive = &mesh->primitives[primitiveIndex];
					assert(primitive->type == cgltf_primitive_type_triangles);
					assert(primitive->indices->component_type == cgltf_component_type_r_16u);
					assert(primitive->indices->type == cgltf_type_scalar);
					assert(primitive->indices->count % 3 == 0);
					assert(primitive->indices->stride == 2);
					assert(primitive->indices->buffer_view->buffer->data);
					cgltf_attribute* positionAttribute = nullptr;
					for (uint64 attributeIndex = 0; attributeIndex < primitive->attributes_count; attributeIndex += 1) {
						cgltf_attribute* attribute = &primitive->attributes[attributeIndex];
						if (attribute->type == cgltf_attribute_type_position) {
							assert(attribute->data->component_type == cgltf_component_type_r_32f);
							assert(attribute->data->type == cgltf_type_vec3);
							assert(attribute->data->buffer_view->buffer->data);
							positionAttribute = attribute;
							break;
						}
					}
					assert(positionAttribute);

					geometries[primitiveIndex] = {
						.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
						.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
						.geometry = {
							.triangles = {
								.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
								.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
								.vertexData = {
									.hostAddress = (char*)positionAttribute->data->buffer_view->buffer->data + positionAttribute->data->offset + positionAttribute->data->buffer_view->offset
								},
								.vertexStride = positionAttribute->data->stride,
								.maxVertex = uint32(positionAttribute->data->count),
								.indexType = VK_INDEX_TYPE_UINT16,
								.indexData = {
									.hostAddress = (char*)primitive->indices->buffer_view->buffer->data + primitive->indices->offset + primitive->indices->buffer_view->offset
								}
							}
						}
					};

					geometryRanges[primitiveIndex] = { 
						.primitiveCount = uint32(primitive->indices->count / 3) 
					};
				}
				VkAccelerationStructureBuildGeometryInfoKHR geometryInfo = {
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
					.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
					.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
					.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
					.geometryCount = uint32(mesh->primitives_count),
					.pGeometries = geometries.data()
				};
				VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ 
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR 
				};
				std::vector<uint32> maxPrimitiveCounts(mesh->primitives_count);
				for (uint32 i = 0; i < mesh->primitives_count; i += 1) {
					maxPrimitiveCounts[i] = geometryRanges[i].primitiveCount;
				}
				vkGetAccelerationStructureBuildSizes(vk->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &geometryInfo, maxPrimitiveCounts.data(), &sizeInfo);

				BLASs.emplace_back(geometryInfo, sizeInfo, std::move(geometries), std::move(geometryRanges));
			}
		}
		uint64 vertexBufferSize = 0;
		uint64 indexBufferSize = 0;
		for (auto & blas : BLASs) {
			for (uint i = 0; auto& geometry : blas.geometries) {
				auto& range = blas.geometryRanges[i];
				vertexBufferSize += geometry.geometry.triangles.maxVertex * 12;
				indexBufferSize += range.primitiveCount * 3 * 2;
				i += 1;
			}
		}
		// memcpy to stage buffer
		// allocate gpu buffers
		// transfer stage buffer to gpu buffer
		// update blas geometry vertexData/indexData device address
		// free stage buffers
		for (auto& blas : BLASs) {
			VkBufferCreateInfo bufferCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = blas.size.accelerationStructureSize,
				.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
			};
			VkBuffer buffer;
			vkCheck(vkCreateBuffer(vk->device, &bufferCreateInfo, nullptr, &buffer));

			VkMemoryRequirements memoryRequirements;
			vkGetBufferMemoryRequirements(vk->device, buffer, &memoryRequirements);
			vk->gpuBuffersMemory.offset = align(vk->gpuBuffersMemory.offset, memoryRequirements.alignment);
			vkBindBufferMemory(vk->device, buffer, vk->gpuBuffersMemory.memory, vk->gpuBuffersMemory.offset);
			vk->gpuBuffersMemory.offset += memoryRequirements.size;

			VkAccelerationStructureCreateInfoKHR asCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
				.buffer = buffer,
				.offset = 0,
				.size = bufferCreateInfo.size,
				.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
			};
			vkCheck(vkCreateAccelerationStructure(vk->device, &asCreateInfo, nullptr, &blas.info.dstAccelerationStructure));
		}
		uint64 offsetToRestore = vk->gpuBuffersMemory.offset;
		for (auto& blas : BLASs) {
			vk->gpuBuffersMemory.offset = align(vk->gpuBuffersMemory.offset, vk->accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);
			blas.info.scratchData.deviceAddress = vk->gpuBuffersMemory.offset;
			vk->gpuBuffersMemory.offset += blas.size.buildScratchSize;
		}
		vk->gpuBuffersMemory.offset = offsetToRestore;

		//VkCommandBufferBeginInfo cmdBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		//cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		//vkCheck(vkResetCommandBuffer(vk->computeCommandBuffer, 0));
		//vkCheck(vkBeginCommandBuffer(vk->computeCommandBuffer, &cmdBufferBeginInfo));
		//vkCmdBuildAccelerationStructures(vk->computeCommandBuffer, uint32(blasGeometryInfos.size()), blasGeometryInfos.data(), blasRangeInfos.data());
		//vkCheck(vkEndCommandBuffer(vk->computeCommandBuffer));
		//VkSubmitInfo queueSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		//queueSubmitInfo.commandBufferCount = 1;
		//queueSubmitInfo.pCommandBuffers = &vk->computeCommandBuffer;
		//vkCheck(vkQueueSubmit(vk->computeQueue, 1, &queueSubmitInfo, nullptr));
		//vkQueueWaitIdle(vk->computeQueue);

		// VkAccelerationStructureInstanceKHR;
	}
};

//auto getTransformMat = [](cgltf_node* node) -> glm::mat4 {
//	if (node->has_matrix) {
//		return glm::make_mat4(node->matrix);
//	}
//	else {
//		glm::mat4 mat = glm::identity<glm::mat4>();
//		if (node->has_scale) {
//			mat = glm::scale(mat, glm::make_vec3(node->scale));
//		}
//		if (node->has_rotation) {
//			mat = (glm::mat4)glm::make_quat(node->rotation) * mat;
//		}
//		if (node->has_translation) {
//			mat = glm::translate(mat, glm::make_vec3(node->translation));
//		}
//		return mat;
//	}
//};
//std::stack<cgltf_node*> nodes;
//std::stack<glm::mat4> transforms;
//for (uint64 i = 0; i < gltfData->scene->nodes_count; i += 1) {
//	cgltf_node* node = gltfData->scene->nodes[i];
//	nodes.push(node);
//	transforms.push(getTransformMat(node));
//}
//while (!nodes.empty()) {
//	cgltf_node* node = nodes.top();
//	glm::mat4 transform = transforms.top();
//	nodes.pop();
//	transforms.pop();
//	for (uint64 i = 0; i < node->children_count; i += 1) {
//		cgltf_node* childNode = node->children[i];
//		nodes.push(childNode);
//		transforms.push(getTransformMat(childNode) * transform);
//	}
//}
