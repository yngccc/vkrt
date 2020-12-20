#version 460

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 colorOut;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main() {
	colorOut = color * texture(tex, uv);
}