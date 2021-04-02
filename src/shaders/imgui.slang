import common;

struct VertexIn {
	float2 position : POSITION;
	float2 uv : TEXCOORD;
	float4 color : COLOR;
};

struct VertexOut {
	float2 uv;
	float4 color;
	float4 position : SV_POSITION;
};

[shader("vertex")]
VertexOut vertexShader(VertexIn vertexIn, uniform float2 windowSize) {
	VertexOut vertexOut;
	vertexOut.uv = vertexIn.uv;
	vertexOut.color = vertexIn.color;
	vertexOut.position = float4(vertexIn.position / windowSize * 2 - 1, 0, 1);
	return vertexOut;
}

uniform Texture2D tex;
uniform SamplerState sampler;

[shader("fragment")] 
float4 fragmentShader(VertexOut vertexOut) {
	return vertexOut.color * tex.Sample(sampler, vertexOut.uv);
}

