import Common;

[[vk::binding(0, 0)]] Texture2D<float4> g_Textures[];
[[vk::binding(2, 0)]] [format("rgba8")] RWTexture2D<float4> g_StorageImages[];

struct PushConstants
{
	uint  InputIndex;  // HDR color
	uint  OutputIndex; // LDR color
	uint2 Extent;
	float Exposure;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> g_PushConstants;

float3 Reinhard(float3 color)
{
	return color / (1.0 + color);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	const uint2 pixel = dispatchThreadID.xy;

	if (any(pixel >= g_PushConstants.Extent))
		return;

	const float3 hdr = g_Textures[g_PushConstants.InputIndex].Load(int3(pixel, 0)).rgb;
	const float3 mapped = Reinhard(hdr * g_PushConstants.Exposure);

	g_StorageImages[g_PushConstants.OutputIndex][pixel] = float4(ConvertSRGBFromLinear(mapped), 1.0);
}
