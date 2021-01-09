#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

hitAttributeEXT vec2 hitAttrib;

layout(location = 0) rayPayloadInEXT vec3 hit;

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

layout(set = 0, binding = 2, scalar) buffer Vertices { Vertex vertices[]; };
layout(set = 0, binding = 3) buffer Indices { uint16_t indices[]; };
layout(set = 0, binding = 4, scalar) buffer Geometries { Geometry geometries[]; };
layout(set = 0, binding = 5, scalar) buffer Materials { Material materials[]; };
layout(set = 0, binding = 6, scalar) buffer Instances { Instance instances[]; };
layout(set = 0, binding = 7) uniform sampler2D textures[];

void main() {
	Instance instance = instances[gl_InstanceID];
	Geometry geometry = geometries[gl_GeometryIndexEXT + instance.geometryOffset];

	uint indices[3] = {
		uint(indices[geometry.indexOffset + gl_PrimitiveID * 3]),
		uint(indices[geometry.indexOffset + gl_PrimitiveID * 3 + 1]),
		uint(indices[geometry.indexOffset + gl_PrimitiveID * 3 + 2])
	};
	vec3 positions[3] = { 
		vertices[geometry.vertexOffset + indices[0]].position,
		vertices[geometry.vertexOffset + indices[1]].position,
		vertices[geometry.vertexOffset + indices[2]].position
	};
	vec3 normals[3] = { 
		vertices[geometry.vertexOffset + indices[0]].normal,
		vertices[geometry.vertexOffset + indices[1]].normal,
		vertices[geometry.vertexOffset + indices[2]].normal
	};
	vec2 uvs[3] = { 
		vertices[geometry.vertexOffset + indices[0]].uv,
		vertices[geometry.vertexOffset + indices[1]].uv,
		vertices[geometry.vertexOffset + indices[2]].uv
	};

	vec3 position = vec3(instance.transform * vec4(barycentric(positions, hitAttrib), 1.0));
	vec3 normal = vec3(instance.transformIT * vec4(barycentric(normals, hitAttrib), 0.0));
	vec2 uv = barycentric(uvs, hitAttrib);

	Material material = materials[geometry.materialIndex];

	vec3 textureColor = vec3(1, 1, 1);
	if (material.baseColorTextureIndex != UINT32_MAX) {
		textureColor = textureLod(textures[material.baseColorTextureIndex], uv, 0).rgb;
	}
	hit = material.baseColorFactor * textureColor;
}