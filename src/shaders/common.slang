static const float pi = 3.14159265358979323846264338327950288;
static const uint uint32Max = 4294967295;

float3 linearToSRGB(in float3 rgb) {
	float3 rgbClamped = clamp(rgb, float3(0), float3(1));
	return max(1.055 * pow(rgbClamped, float3(0.416666667)) - 0.055, 0);
}

float2 barycentricLerp(in float2 attribs[3], in float2 barycentric) {
	return attribs[0] + barycentric.x * (attribs[1] - attribs[0]) + barycentric.y * (attribs[2] - attribs[0]);
}

float3 barycentricLerp(in float3 attribs[3], in float2 barycentric) {
	return attribs[0] + barycentric.x * (attribs[1] - attribs[0]) + barycentric.y * (attribs[2] - attribs[0]);
}

float4 barycentricLerp(in float4 attribs[3], in float2 barycentric) {
	return attribs[0] + barycentric.x * (attribs[1] - attribs[0]) + barycentric.y * (attribs[2] - attribs[0]);
}

uint wang_hash(uint x) {
    x = (x ^ 61) ^ (x >> 16);
    x *= 9;
    x = x ^ (x >> 4);
    x *= 0x27d4eb2d;
    x = x ^ (x >> 15);
    return x;
}

float iqint3(uint2 x) {
    uint2 q = 1103515245U * ((x >> 1U) ^ (x.yx));
    uint  n = 1103515245U * ((q.x) ^ (q.y >> 3U));
    return float(n) * (1.0 / float(0xffffffffU));
}

float3x3 orthonormalBasisFrisvad(float3 normal) {
    float sign = normal.z >= 0.0f ? 1.0f : -1.0f;
    float a = -1.0f / (sign + normal.z);
    float b = normal.x * normal.y * a;
    float3 b1 = float3(1.0f + sign * normal.x * normal.x * a, sign * b, -sign * normal.x);
    float3 b2 = float3(b, sign + normal.y * normal.y * a, -normal.y);
    return float3x3(b1, normal, -b2);
}

float3 sampleHemisphere(float2 uv) {
    float r = sqrt(1.0f - uv.x * uv.x);
    float phi = 2.0 * pi * uv.y;
    return float3(r * cos(phi), uv.x, -r * sin(phi));
}

float sampleHemispherePDF() {
    return 1.0 / (2.0 * pi);
}

float3 sampleSphere(float2 uv) {
    float z = 1.0 - 2.0 * uv.x;
    float r = sqrt(1.0f - z * z);
    float phi = 2.0 * pi * uv.y;
    return float3(r * cos(phi), z, -r * sin(phi));
}

float sampleSpherePDF() {
    return 1.0 / (4.0 * pi);
}

float3 sampleCosHemisphere(float2 uv, out float PDF) {
    // sample disk
    float r = sqrt(uv.x);
    float theta = 2.0 * pi * uv.y;
    float2 disk = float2(r * cos(theta), r * sin(theta));
    PDF = cos(theta) * (1 / pi);
    return float3(disk.x, sqrt(max(0.0, 1.0 - dot(disk, disk))), -disk.y);
}
