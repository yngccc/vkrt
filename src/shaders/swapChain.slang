import common;

struct VertexOut {
	float2 uv;
	float4 position : SV_POSITION;
};

[shader("vertex")]
VertexOut vertexShader(uint vertexID : SV_VertexID) {
	VertexOut vertexOut;
	vertexOut.uv = float2((vertexID << 1) & 2, vertexID & 2);
	vertexOut.position = float4(vertexOut.uv * float2(2, -2) + float2(-1, 1), 0, 1);
	return vertexOut;
}

uniform Texture2D<float4> colorBuffer;
uniform SamplerState sampler;

[shader("fragment")]
float4 fragmentShader(VertexOut vertexOut) {
	float4 color = colorBuffer.Sample(sampler, vertexOut.uv);
	color.rgb = linearToSRGB(color.rgb);
	return color;
}
