#version 460

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 uvOut;
layout(location = 1) out vec4 colorOut;

layout(push_constant) uniform pushConstant {
	vec2 windowSize;
};

void main() {
	gl_Position = vec4(position / windowSize * 2 - 1, 0, 1);
	uvOut = uv;
	colorOut = color;
}