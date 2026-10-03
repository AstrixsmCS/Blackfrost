import Common;
import Buffers;
import Material;

struct VertexInput
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float4 Tangent  : TANGENT;
};

struct VertexOutput
{
	float4 Position : SV_Position;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float4 Tangent  : TANGENT;
};

struct GBufferOutput
{
	float4 AlbedoAO      : SV_Target0; // rgb = base color, a = ambient occlusion          (RGBA8_SRGB)
	float2 Normal        : SV_Target1; // octahedral-encoded world-space normal in [0, 1]  (RG16)
	float4 Material      : SV_Target2; // r = unused, g = roughness, b = metallic (glTF)   (RGBA8_UNorm)
	float4 Emissive      : SV_Target3; // rgb = emitted radiance, a = unused               (RGBA16_Float)
};

[[vk::binding(0, 0)]] Texture2D<float4> g_Textures[];
[[vk::binding(1, 0)]] SamplerState      g_Samplers[];

static const uint ANISOTROPIC_REPEAT_SAMPLER = 3;

struct PushConstants
{
	float4x4         Model;
	UBCamera*        UBCamera;
	GPUMaterialData* SBMaterials;
	uint             MaterialIndex;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> g_PushConstants;

float2 SignNotZero(float2 value)
{
	return float2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}

float2 EncodeOctahedralNormal(float3 normal)
{
	normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);

	float2 encoded = normal.xy;

	if (normal.z < 0.0)
		encoded = (1.0 - abs(encoded.yx)) * SignNotZero(encoded);

	return encoded * 0.5 + 0.5;
}

[shader("vertex")]
VertexOutput main(VertexInput input)
{
	const float4   worldPosition = mul(g_PushConstants.Model, float4(input.Position, 1.0));
	const float3x3 model3x3      = (float3x3)g_PushConstants.Model;

	VertexOutput output;
	output.Position = mul(g_PushConstants.UBCamera->ViewProjection, worldPosition);
	output.Normal   = normalize(mul(model3x3, input.Normal));
	output.TexCoord = input.TexCoord;
	output.Tangent  = float4(normalize(mul(model3x3, input.Tangent.xyz)), input.Tangent.w);

	return output;
}

[shader("fragment")]
GBufferOutput main(VertexOutput input)
{
	const GPUMaterialData mat = g_PushConstants.SBMaterials[g_PushConstants.MaterialIndex];
	const float2          uv  = input.TexCoord;

	// Albedo
	float4 albedo = mat.Albedo;

	if (mat.AlbedoIndex != 0)
		albedo *= g_Textures[mat.AlbedoIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv);

	if (albedo.a < mat.AlphaCutoff)
		discard;

	// Normal
	float3 N = normalize(input.Normal);

	if (mat.NormalIndex != 0)
	{
		float3 T = normalize(input.Tangent.xyz);
		T = normalize(T - N * dot(N, T));

		const float3 B             = cross(N, T) * input.Tangent.w;
		const float3 tangentNormal = g_Textures[mat.NormalIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv).xyz * 2.0 - 1.0;

		N = normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
	}

	// Metallic / roughness (glTF: G = roughness, B = metallic)
	float metalness       = mat.Metalness;
	float linearRoughness = mat.Roughness;

	if (mat.MetallicRoughnessIndex != 0)
	{
		const float4 metallicRoughness = g_Textures[mat.MetallicRoughnessIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv);

		linearRoughness *= metallicRoughness.g;
		metalness       *= metallicRoughness.b;
	}

	metalness       = saturate(metalness);
	linearRoughness = clamp(linearRoughness, 0.045, 1.0);

	// Occlusion
	float occlusion = 1.0;

	if (mat.OcclusionIndex != 0)
		occlusion = g_Textures[mat.OcclusionIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv).r;

	// Emissive: rgb = linear color factor, a = HDR strength.
	float3 emissive = max(mat.Emissive.rgb, float3(0.0, 0.0, 0.0)) * max(mat.Emissive.a, 0.0);

	if (mat.EmissiveIndex != 0)
		emissive *= g_Textures[mat.EmissiveIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv).rgb;

	GBufferOutput output;
	output.AlbedoAO      = float4(saturate(albedo.rgb), saturate(occlusion));
	output.Normal        = EncodeOctahedralNormal(N);
	output.Material      = float4(0.0, linearRoughness, metalness, 0.0);
	output.Emissive      = float4(emissive, 1.0);

	return output;
}
