#include "vk.h"

#define _XM_SSE4_INTRINSICS_
#include <directxmath.h>
#include <directxcolors.h>
using namespace DirectX;

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <json.hpp>
using json = nlohmann::json;

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

struct VertexData {
	float position[3];
	float normal[3];
	float uv[2];
};

struct GeometryData {
	uint32 vertexOffset;
	uint32 indexOffset;
	uint32 materialIndex;
};

struct InstanceData {
	float transform[4][4];
	float transformIT[4][4];
	uint32 geometryOffset;
};

struct MaterialData {
	float baseColorFactor[3];
	uint32 baseColorTextureIndex;
	float emissiveFactor[3];
	uint32 emissiveTextureIndex;
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
		auto scene = new Scene();
		scene->filePath = filePath;
		scene->loadJson();
		scene->loadModelsData();
		scene->buildVkResources(vk);
		return scene;
	}
	void loadJson() {
		std::ifstream file(filePath);
		json j;
		file >> j;
		auto jCamera = j["camera"];
		if (!jCamera.is_null()) {
			auto position = jCamera["position"];
			auto view = jCamera["view"];
			camera.position = XMVectorSet(position[0], position[1], position[2], 0);
			camera.view = XMVector3Normalize(XMVectorSet(view[0], view[1], view[2], 0));
			camera.viewMat = XMMatrixLookAtRH(camera.position, camera.position + camera.view, XMVectorSet(0, 1, 0, 0));
			camera.projMat = XMMatrixPerspectiveFovRH((float)80.0_deg, 1920.0f / 1080.0f, 0.1f, 1000.0f);
			camera.viewProjMat = camera.viewMat * camera.projMat;
		}
		auto jModels = j["models"];
		if (!jModels.is_null()) {
			for (auto& jModel : jModels) {
				Model m = {
					.name = jModel["name"],
					.filePath = std::string(jModel["path"])
				};
				models.push_back(m);
			}
		}
		auto jLights = j["lights"];
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
			for (auto& image : makeRange(model.gltfData->images, model.gltfData->images_count)) {
				int width, height, comp;
				std::filesystem::path imagePath = modelFilePath.parent_path() / image.uri;
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
					for (int i = 0; i < width * height; i += 1) {
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
				for (auto& mesh : makeRange(model.gltfData->meshes, model.gltfData->meshes_count)) {
					std::vector<VkAccelerationStructureGeometryKHR> geometries(mesh.primitives_count);
					std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges(mesh.primitives_count);
					std::vector<PrimitiveVerticesData> verticesData(mesh.primitives_count);
					for (auto [primitiveIndex, primitive] : enumerate(makeRange(mesh.primitives, mesh.primitives_count))) {
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
						for (auto& attribute : makeRange(primitive.attributes, primitive.attributes_count)) {
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

						verticesBufferSize += positions->count * sizeof(struct VertexData);
						indicesBufferSize += indices->count * sizeof(uint16);
						geometriesBufferSize += sizeof(struct GeometryData);
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
					for (auto [i, count] : enumerate(maxPrimitiveCounts)) {
						count = ranges[i].primitiveCount;
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
			for (auto [i, info] : enumerate(blasInfos)) {
				auto& size = blasSizes[i];
				VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
					.buffer = blasBuffer,
					.offset = blasBufferOffset,
					.size = size.accelerationStructureSize,
					.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
				};
				vkCheck(vkCreateAccelerationStructure(vk->device, &blasCreateInfo, nullptr, &info.dstAccelerationStructure));
				blas.push_back(info.dstAccelerationStructure);
				blasBufferOffset += size.accelerationStructureSize;
			}
		}

		std::vector<MaterialData> materialsBufferData;
		uint64 materialsBufferSize = 0;
		std::vector<std::vector<uint32>> geometryMaterialIndices;
		{
			for (auto& model : models) {
				for (auto& mesh : makeRange(model.gltfData->meshes, model.gltfData->meshes_count)) {
					std::vector<uint32> materialIndices(mesh.primitives_count);
					for (auto [i, index] : enumerate(materialIndices)) {
						index = (uint32)(materialsBufferData.size() + std::distance(model.gltfData->materials, mesh.primitives[i].material));
					}
					geometryMaterialIndices.push_back(std::move(materialIndices));
				}
				for (auto& material : makeRange(model.gltfData->materials, model.gltfData->materials_count)) {
					MaterialData materialData = {};
					memcpy(materialData.emissiveFactor, material.emissive_factor, 12);
					if (material.has_pbr_metallic_roughness) {
						memcpy(materialData.baseColorFactor, material.pbr_metallic_roughness.base_color_factor, 12);
						if (material.pbr_metallic_roughness.base_color_texture.texture) {
							uint64 textureIndex = std::distance(model.gltfData->images, material.pbr_metallic_roughness.base_color_texture.texture->image);
							materialData.baseColorTextureIndex = (uint32)(textures.size() + textureIndex);
						}
						else {
							materialData.baseColorTextureIndex = UINT32_MAX;
						}
					}
					materialsBufferData.push_back(materialData);
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
		std::vector<InstanceData> instancesBufferData;
		uint64 instancesBufferSize = 0;
		{
			for (uint64 blasOffset = 0; auto& model : models) {
				std::stack<cgltf_node*> nodes;
				std::stack<XMMATRIX> transforms;
				for (auto& node : makeRange(model.gltfData->scene->nodes, model.gltfData->scene->nodes_count)) {
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
						VkAccelerationStructureInstanceKHR instance = {
							.mask = 0xff,
							.accelerationStructureReference = vkGetAccelerationStructureDeviceAddress(vk->device, &asDeviceAddressInfo)
						};
						XMMATRIX transformT = XMMatrixTranspose(transform);
						memcpy(instance.transform.matrix, transformT.r, 12 * sizeof(float));
						tlasBuildInstances.push_back(instance);

						InstanceData instanceData;
						memcpy(instanceData.transform, transform.r, sizeof(transform));
						XMMATRIX transformIT = XMMatrixTranspose(XMMatrixInverse(nullptr, transform));
						memcpy(instanceData.transformIT, transformIT.r, sizeof(transformIT));
						instanceData.geometryOffset = 0;
						for (uint64 i = 0; i < blasIndex; i += 1) {
							instanceData.geometryOffset += blasInfos[i].geometryCount;
						}
						instancesBufferData.push_back(instanceData);
					}
					for (auto& childNode : makeRange(node->children, node->children_count)) {
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
			vkCheck(vkCreateAccelerationStructure(vk->device, &tlasCreateInfo, nullptr, &tlasInfo.dstAccelerationStructure));
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
			for (auto [i, info] : enumerate(blasInfos)) {
				info.scratchData.deviceAddress = scratchBufferDeviceAddress;
				scratchBufferDeviceAddress = align(scratchBufferDeviceAddress + blasSizes[i].buildScratchSize, scratchBufferAlignment);
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
			vkCheck(vkMapMemory(vk->device, vk->stagingBuffersMemory.memory, 0, stageBufferSize, 0, (void**)&stageBufferPtr));
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

			GeometryData geometryData = { .vertexOffset = 0, .indexOffset = 0 };
			for (auto [i, geometries] : enumerate(blasGeometries)) {
				for (auto [j, geometry] : enumerate(geometries)) {
					auto& verticesData = primitiveVerticesData[i][j];
					uint64 verticesSize = verticesData.positions->count * sizeof(struct VertexData);
					uint64 indicesSize = verticesData.indices->count * sizeof(uint16);
					uint8* positions = (uint8*)(verticesData.positions->buffer_view->buffer->data) + verticesData.positions->offset + verticesData.positions->buffer_view->offset;
					uint8* normals = (uint8*)(verticesData.normals->buffer_view->buffer->data) + verticesData.normals->offset + verticesData.normals->buffer_view->offset;
					uint8* uvs = nullptr;
					if (verticesData.uvs) {
						uvs = (uint8*)(verticesData.uvs->buffer_view->buffer->data) + verticesData.uvs->offset + verticesData.uvs->buffer_view->offset;
					}
					for (uint64 i = 0; i < verticesData.positions->count; i += 1) {
						memcpy(verticesBufferPtr + i * sizeof(struct VertexData), positions + i * verticesData.positions->stride, 12);
						memcpy(verticesBufferPtr + i * sizeof(struct VertexData) + 12, normals + i * verticesData.normals->stride, 12);
						if (uvs) {
							memcpy(verticesBufferPtr + i * sizeof(struct VertexData) + 24, uvs + i * verticesData.uvs->stride, 8);
						}
					}
					uint8* indices = (uint8*)(verticesData.indices->buffer_view->buffer->data) + verticesData.indices->offset + verticesData.indices->buffer_view->offset;
					memcpy(indicesBufferPtr, indices, indicesSize);
					verticesBufferPtr += verticesSize;
					indicesBufferPtr += indicesSize;

					geometry.geometry.triangles.vertexData.deviceAddress = vertexBufferDeviceAddress;
					geometry.geometry.triangles.vertexStride = sizeof(struct VertexData);
					geometry.geometry.triangles.indexData.deviceAddress = indexBufferDeviceAddress;
					vertexBufferDeviceAddress += verticesSize;
					indexBufferDeviceAddress += indicesSize;

					geometryData.materialIndex = geometryMaterialIndices[i][j];
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
			VkCommandBufferBeginInfo cmdBufferBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkCheck(vkResetCommandBuffer(vk->computeCommandBuffer, 0));
			vkCheck(vkBeginCommandBuffer(vk->computeCommandBuffer, &cmdBufferBeginInfo));

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
				vkCmdCopyBuffer(vk->computeCommandBuffer, vk->stagingBuffer, bufferInfo.first, 1, &bufferCopy);
				stagingBufferOffset += bufferInfo.second;
			}

			std::vector<VkImageMemoryBarrier> imageBarriers(textures.size());
			for (auto [i, barrier] : enumerate(imageBarriers)) {
				barrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.dstQueueFamilyIndex = vk->graphicsComputeQueueFamilyIndex,
					.image = textures[i].first,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				};
			}
			vkCmdPipelineBarrier(vk->computeCommandBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr, 0, nullptr, (uint32)imageBarriers.size(), imageBarriers.data()
			);

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
					vkCmdCopyBufferToImage(vk->computeCommandBuffer, 
						vk->stagingBuffer, textures[textureIndex].first, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
					stagingBufferOffset = align(stagingBufferOffset + image.size, 16);
					textureIndex += 1;
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
			vkCmdPipelineBarrier(vk->computeCommandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0,
				1, &memoryBarrier, 0, nullptr, (uint32)imageBarriers.size(), imageBarriers.data()
			);

			std::vector<VkAccelerationStructureBuildRangeInfoKHR*> blasRangesPtr(blasRanges.size());
			for (auto [i, range] : enumerate(blasRangesPtr)) {
				range = blasRanges[i].data();
			}
			vkCmdBuildAccelerationStructures(vk->computeCommandBuffer, (uint32)blasInfos.size(), blasInfos.data(), blasRangesPtr.data());

			memoryBarrier = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
				.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
			};
			vkCmdPipelineBarrier(
				vk->computeCommandBuffer,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0,
				1, &memoryBarrier, 0, nullptr, 0, nullptr
			);
			VkAccelerationStructureBuildRangeInfoKHR* tlasRangePtr = &tlasRange;
			vkCmdBuildAccelerationStructures(vk->computeCommandBuffer, 1, &tlasInfo, &tlasRangePtr);

			vkCheck(vkEndCommandBuffer(vk->computeCommandBuffer));

			VkSubmitInfo queueSubmitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &vk->computeCommandBuffer
			};
			vkCheck(vkQueueSubmit(vk->computeQueue, 1, &queueSubmitInfo, nullptr));
			vkQueueWaitIdle(vk->computeQueue);
		}
	}
};

