#version 460

#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 colorOut;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main() {
    colorOut = texture(tex, uv);
    colorOut.rgb = linearToSRGB(colorOut.rgb);
}