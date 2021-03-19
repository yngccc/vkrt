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

uint wangHash(inout uint seed) {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

float iqint3(uvec2 x) {
    uvec2 q = 1103515245U * ((x >> 1U) ^ (x.yx));
    uint  n = 1103515245U * ((q.x) ^ (q.y >> 3U));
    return float(n) * (1.0 / float(0xffffffffU));
}

float randomFloat01(inout uint seed) {
    return float(wangHash(seed)) / 4294967296.0f;
}

mat3 orthonormalBasisFrisvad(vec3 normal) {
    float sign = normal.z >= 0.0f ? 1.0f : -1.0f;
    float a = -1.0f / (sign + normal.z);
    float b = normal.x * normal.y * a;
    vec3 b1 = vec3(1.0f + sign * normal.x * normal.x * a, sign * b, -sign * normal.x);
    vec3 b2 = vec3(b, sign + normal.y * normal.y * a, -normal.y);
    return mat3(b1, normal, -b2);
}

vec3 sampleHemisphere(vec2 uv) {
    float r = sqrt(1.0f - uv.x * uv.x);
    float phi = 2.0 * PI * uv.y;
    return vec3(r * cos(phi), uv.x, -r * sin(phi));
}

float sampleHemispherePDF() {
    return 1.0 / (2.0 * PI);
}

vec3 sampleSphere(vec2 uv) {
    float z = 1.0 - 2.0 * uv.x;
    float r = sqrt(1.0f - z * z);
    float phi = 2.0 * PI * uv.y;
    return vec3(r * cos(phi), z, -r * sin(phi));
}

float sampleSpherePDF() {
    return 1.0 / (4.0 * PI);
}

vec3 sampleCosHemisphere(vec2 uv, out float PDF) {
    // sample disk
    float r = sqrt(uv.x);
    float theta = 2.0 * PI * uv.y;
    vec2 disk = vec2(r * cos(theta), r * sin(theta));
    PDF = cos(theta) * (1 / PI);
    return vec3(disk.x, sqrt(max(0.0, 1.0 - dot(disk, disk))), -disk.y);
}


