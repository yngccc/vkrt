import common;

struct PrimaryRayPayload {
	float3 position;
	float3 normal;
	float3 color;
};

struct Vertex {
	float3 position;
	float3 normal;
	float2 uv;
};

struct Geometry {
	uint vertexOffset;
	uint indexOffset;
	uint materialIndex;
};

struct Instance {
	float4x4 transform;
	float4x4 transformIT;
	uint geometryOffset;
};

struct Material {
	float3 baseColorFactor;
	uint baseColorTextureIndex;
	float3 emissiveFactor;
	uint emissiveTextureIndex;
};

struct Constants {
	float4x4 screenToWorldMat;
	float4 eyePos;
	uint accumulatedFrameCount;
};

float3 getPixelWorldPos(in float4x4 screenToWorldMat, in uint2 resolution, in uint2 pixelIndex) {
	float2 pixelCenterIndex = float2(pixelIndex) + float2(0.5, 0.5);
	float2 screenPos = (pixelCenterIndex / float2(resolution)) * 2.0 - 1.0;
	float4 world = mul(screenToWorldMat, float4(screenPos, 0.0, 1.0));
	return world.xyz / world.w;
}

uniform RWTexture2D<float4> accumulationColorBuffer;
uniform RWTexture2D<float4> colorBuffer;
uniform RaytracingAccelerationStructure tlas;
uniform StructuredBuffer<Vertex> vertices;
uniform StructuredBuffer<uint16_t> indices;
uniform StructuredBuffer<Geometry> geometries;
uniform StructuredBuffer<Material> materials;
uniform StructuredBuffer<Instance> instances;
uniform ConstantBuffer<Constants> constants;
uniform SamplerState sampler;
uniform Texture2D textures[];

[shader("raygeneration")]
void rayGenShader() {
	uint2 resolution = DispatchRaysDimensions().xy;
	uint2 pixelIndex = DispatchRaysIndex().xy;
	
	float3 rayDir = normalize(getPixelWorldPos(constants.screenToWorldMat, resolution, pixelIndex) - constants.eyePos.xyz);
	RayDesc rayDesc = { constants.eyePos.xyz, 0, rayDir, 1000 };
	PrimaryRayPayload primaryRayPayload;
	TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, primaryRayPayload);
	
	if (constants.accumulatedFrameCount == 0) {
		accumulationColorBuffer[pixelIndex] = float4(primaryRayPayload.color, 0);
		colorBuffer[pixelIndex] = float4(primaryRayPayload.color, 0);
	}
	else {
		float3 color = accumulationColorBuffer[pixelIndex].rgb;
		color += primaryRayPayload.color;
		accumulationColorBuffer[pixelIndex] = float4(color, 0);
		color /= constants.accumulatedFrameCount;
		colorBuffer[pixelIndex] = float4(color, 0);
	}
}

[shader("miss")]
void missShader(inout PrimaryRayPayload payload) {
	payload.color = float3(0);
}

[shader("anyhit")]
void anyhitShader() {
	IgnoreHit();
}

[shader("closesthit")]
void closestHitShader(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes hitAttribs) {
	Instance instance = instances[InstanceID()];
	Geometry geometry = geometries[GeometryIndex() + instance.geometryOffset];
	
	uint vertexIndices[3] = {
		uint(indices[geometry.indexOffset + PrimitiveIndex() * 3]),
		uint(indices[geometry.indexOffset + PrimitiveIndex() * 3 + 1]),
		uint(indices[geometry.indexOffset + PrimitiveIndex() * 3 + 2])
	};
	Vertex vertex0 = vertices[geometry.vertexOffset + vertexIndices[0]];
	Vertex vertex1 = vertices[geometry.vertexOffset + vertexIndices[1]];
	Vertex vertex2 = vertices[geometry.vertexOffset + vertexIndices[2]];
	
	float3 positions[3] = { vertex0.position, vertex1.position, vertex2.position };
	float3 normals[3] = { vertex0.normal, vertex1.normal, vertex2.normal };
	float2 uvs[3] = { vertex0.uv, vertex1.uv, vertex2.uv };
	
	float3 position = mul(instance.transform, float4(barycentric(positions, hitAttribs.barycentrics), 1.0)).xyz;
	float3 normal = mul(instance.transformIT, float4(barycentric(normals, hitAttribs.barycentrics), 0.0)).xyz;
	float2 uv = barycentric(uvs, hitAttribs.barycentrics);
	
	Material material = materials[geometry.materialIndex];
	
	float3 textureColor = { 1, 1, 1 };
	if (material.baseColorTextureIndex != uint32Max) {
		textureColor = textures[material.baseColorTextureIndex].SampleLevel(sampler, uv, 0).rgb;
	}
	
	payload.position = position;
	payload.normal = normal;
	payload.color = material.baseColorFactor * textureColor;	
}
