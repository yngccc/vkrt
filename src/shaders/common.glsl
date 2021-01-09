#define PI 3.14159265358979323846264338327950288
#define UINT32_MAX 4294967295

vec3 linearToSRGB(in vec3 rgb) {
	vec3 rgbClamped = clamp(rgb, vec3(0), vec3(1));
	return max(1.055 * pow(rgbClamped, vec3(0.416666667)) - 0.055, 0);
}

vec2 barycentric(in vec2 attribs[3], in vec2 barycentric) {
	return attribs[0] + barycentric.x * (attribs[1] - attribs[0]) + barycentric.y * (attribs[2] - attribs[0]);
}

vec3 barycentric(in vec3 attribs[3], in vec2 barycentric) {
	return attribs[0] + barycentric.x * (attribs[1] - attribs[0]) + barycentric.y * (attribs[2] - attribs[0]);
}

vec4 barycentric(in vec4 attribs[3], in vec2 barycentric) {
	return attribs[0] + barycentric.x * (attribs[1] - attribs[0]) + barycentric.y * (attribs[2] - attribs[0]);
}