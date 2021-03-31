#version 460

#extension GL_GOOGLE_include_directive : require
#include "rayTraceCommon.glsl"
#include "common.glsl"

hitAttributeEXT vec2 hitAttrib;

layout(location = 0) rayPayloadInEXT PrimaryRayPayload primaryRayPayload;

void main() {
	Instance instance = instances[gl_InstanceID];
	Geometry geometry = geometries[gl_GeometryIndexEXT + instance.geometryOffset];

	uint vertexIndices[3] = {
		uint(indices[geometry.indexOffset + gl_PrimitiveID * 3]),
		uint(indices[geometry.indexOffset + gl_PrimitiveID * 3 + 1]),
		uint(indices[geometry.indexOffset + gl_PrimitiveID * 3 + 2])
	};
	Vertex vertex0 = vertices[geometry.vertexOffset + vertexIndices[0]];
	Vertex vertex1 = vertices[geometry.vertexOffset + vertexIndices[1]];
	Vertex vertex2 = vertices[geometry.vertexOffset + vertexIndices[2]];

	vec3 positions[3] = { vertex0.position, vertex1.position, vertex2.position };
	vec3 normals[3] = { vertex0.normal, vertex1.normal, vertex2.normal };
	vec2 uvs[3] = { vertex0.uv, vertex1.uv, vertex2.uv };

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
