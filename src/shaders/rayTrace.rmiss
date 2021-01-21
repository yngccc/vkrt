#version 460

#extension GL_GOOGLE_include_directive : require
#include "rayTraceCommon.glsl"

layout(location = 0) rayPayloadInEXT PrimaryRayPayload primaryRayPayload;

void main() {
	primaryRayPayload.color = vec3(0);
}