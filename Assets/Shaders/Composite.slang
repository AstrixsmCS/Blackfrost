[[vk::binding(0, 0)]] Texture2D    g_Textures[];
[[vk::binding(1, 0)]] SamplerState g_Samplers[];

static const uint LINEAR_CLAMP_SAMPLER = 1;

struct PushConstants
{
	uint InputIndex;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> g_PushConstants;

struct VertexOutput
{
	float4 Position : SV_Position;
	float2 UV       : TEXCOORD0;
};

[shader("vertex")]
VertexOutput main(uint vertexID : SV_VertexID)
{
	const float2 uv = float2((vertexID << 1) & 2, vertexID & 2);

	VertexOutput output;
	output.Position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
	output.UV       = uv;
	return output;
}

[shader("fragment")]
float4 main(VertexOutput input) : SV_Target
{
	return float4(g_Textures[g_PushConstants.InputIndex].Sample(g_Samplers[LINEAR_CLAMP_SAMPLER], input.UV).rgb, 1.0);
}
