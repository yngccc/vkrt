#version 460

#extension GL_GOOGLE_include_directive : require
#include "rayTraceCommon.glsl"
#include "common.glsl"

hitAttributeEXT vec2 hitAttrib;

layout(location = 0) rayPayloadInEXT PrimaryRayPayload primaryRayPayload;

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

	primaryRayPayload.position = position;
	primaryRayPayload.normal = normal;
	primaryRayPayload.color = material.baseColorFactor * textureColor;
}