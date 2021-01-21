#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_scalar_block_layout : require

struct PrimaryRayPayload {
	vec3 position;
	vec3 normal;
	vec3 color;
};

struct Vertex {
	vec3 position;
	vec3 normal;
	vec2 uv;
};

struct Geometry {
	uint vertexOffset;
	uint indexOffset;
	uint materialIndex;
};

struct Instance {
	mat4 transform;
	mat4 transformIT;
	uint geometryOffset;
};

struct Material {
	vec3 baseColorFactor;
	uint baseColorTextureIndex;
	vec3 emissiveFactor;
	uint emissiveTextureIndex;
};

layout(set = 0, binding = 0, rgba32f) uniform image2D image;
layout(set = 0, binding = 1) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 2, scalar) buffer Vertices { Vertex vertices[]; };
layout(set = 0, binding = 3) buffer Indices { uint16_t indices[]; };
layout(set = 0, binding = 4, scalar) buffer Geometries { Geometry geometries[]; };
layout(set = 0, binding = 5, scalar) buffer Materials { Material materials[]; };
layout(set = 0, binding = 6, scalar) buffer Instances { Instance instances[]; };
layout(set = 0, binding = 7) uniform sampler2D textures[];
