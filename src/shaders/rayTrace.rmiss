#version 460

#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hit;

void main() {
	hit = vec3(0);
}