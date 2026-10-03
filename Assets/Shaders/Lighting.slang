import Common;
import Buffers;

import BRDF;

[[vk::binding(0, 0)]] Texture2D<float4>   g_Textures[];
[[vk::binding(1, 0)]] SamplerState        g_Samplers[];
[[vk::binding(2, 0)]] [format("rgba16f")] RWTexture2D<float4> g_StorageImages[];

static const uint LINEAR_CLAMP_SAMPLER = 1;

struct PushConstants
{
	uint AlbedoIndex;
	uint NormalIndex;
	uint MaterialIndex;
	uint EmissiveIndex;
	uint DepthIndex;
	uint OutputIndex;

	uint2 Extent;

	UBCamera* UBCamera;
	UBScene* UBScene;
	UBPointLights* UBPointLights;
	UBSpotLights* UBSpotLights;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> g_PushConstants;

// ==== GBuffer decoding ====

float2 SignNotZero(float2 value)
{
	return float2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}

float3 DecodeOctahedralNormal(float2 encoded)
{
	const float2 octahedron = encoded * 2.0 - 1.0;

	float3 normal = float3(octahedron, 1.0 - abs(octahedron.x) - abs(octahedron.y));

	if (normal.z < 0.0)
		normal.xy = (1.0 - abs(normal.yx)) * SignNotZero(normal.xy);

	return normalize(normal);
}

float3 ReconstructWorldPosition(uint2 pixel, float depth)
{
	const float2 uv = (float2(pixel) + 0.5) / float2(g_PushConstants.Extent);

	float2 ndcXY = uv * 2.0 - 1.0;
	ndcXY.y = -ndcXY.y;

	const float4 ndc   = float4(ndcXY, depth, 1.0);
	const float4 world = mul(g_PushConstants.UBCamera->InverseViewProjection, ndc);

	return world.xyz / world.w;
}

// ==== Direct lighting ====

// Cook-Torrance specular + Disney diffuse for one light. L points from the surface to the light.
float3 ComputeDirect(float3 N, float3 V, float NdotV, float3 diffuseColor, float3 F0, float F90, float linearRoughness, float roughness, float3 L, float3 radiance)
{
	const float NdotL = saturate(dot(N, L));

	if (NdotL <= 0.0)
		return float3(0.0);

	const float3 H = normalize(L + V);

	const float NdotH = saturate(dot(N, H));
	const float LdotH = saturate(dot(L, H));

	const float3 F = FresnelSchlick(F0, F90, LdotH);
	const float D = DistributionGGX(NdotH, roughness);
	const float Vis = VisibilitySmithGGXCorrelated(NdotV, NdotL, roughness);

	const float3 specular = SpecularCookTorrance(F, D, Vis);

	const float Fd = DiffuseDisney(NdotV, NdotL, LdotH, linearRoughness);
	const float3 diffuse = diffuseColor * Fd / PI;

	return (diffuse + specular) * radiance * NdotL;
}

// Frostbite punctual light inverse-square falloff with a smooth window reaching 0 at radius.
float DistanceAttenuation(float3 toLight, float radius)
{
	const float safeRadius      = max(radius, 1e-3); // uploaded unclamped from the scene
	const float distanceSquared = dot(toLight, toLight);
	const float factor          = distanceSquared / (safeRadius * safeRadius);
	const float window          = saturate(1.0 - factor * factor);

	return (window * window) / max(distanceSquared, 0.01 * 0.01);
}

// Frostbite spot cone smooth falloff between the outer and inner cone (cosines of half-angles).
float AngleAttenuation(float3 L, float3 spotDirection, float cosInner, float cosOuter)
{
	const float scale       = 1.0 / max(cosInner - cosOuter, 1e-4);
	const float offset      = -cosOuter * scale;
	const float attenuation = saturate(dot(-L, spotDirection) * scale + offset);

	return attenuation * attenuation;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	const uint2 pixel = dispatchThreadID.xy;

	if (any(pixel >= g_PushConstants.Extent))
		return;

	const int3 texel = int3(pixel, 0);

	// ==== Depth ====

	const float depth = g_Textures[g_PushConstants.DepthIndex].Load(texel).r;

	// Reversed-Z 0.0 is the cleared background (infinitely far).
	if (depth <= 0.0)
	{
		g_StorageImages[g_PushConstants.OutputIndex][pixel] = float4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	// ==== G-buffer ====

	const float4 albedoAO = g_Textures[g_PushConstants.AlbedoIndex].Load(texel);
	const float2 encodedNormal = g_Textures[g_PushConstants.NormalIndex].Load(texel).rg;
	const float4 material = g_Textures[g_PushConstants.MaterialIndex].Load(texel);
	const float3 emissive = g_Textures[g_PushConstants.EmissiveIndex].Load(texel).rgb;

	const float3 baseColor = albedoAO.rgb;
	const float occlusion = saturate(albedoAO.a);
	const float3 N = DecodeOctahedralNormal(encodedNormal);

	const float materialRoughness = saturate(material.g);
	const float linearRoughness = max(materialRoughness, 0.045);
	const float metalness = saturate(material.b);

	const float roughness = linearRoughness * linearRoughness;

	// ==== Reconstruction ====

	const float3 worldPosition = ReconstructWorldPosition(pixel, depth);
	const float3 V = normalize(g_PushConstants.UBScene->CameraPosition - worldPosition);

	// ==== Material ====

	const float3 diffuseColor = baseColor * (1.0 - metalness);
	const float3 F0 = lerp(float3(0.04 * 1.0), baseColor, metalness);
	const float F90 = saturate(50.0 * dot(F0, float3(0.33)));
	const float NdotV = abs(dot(N, V)) + 1e-5;

	float3 directLighting = float3(0.0);

	// ==== Directional light ====

	{
		const float3 L = -normalize(g_PushConstants.UBScene->Light.Direction);

		const float3 radiance = max(g_PushConstants.UBScene->Light.Radiance, float3(0.0)) * max(g_PushConstants.UBScene->Light.Intensity, 0.0);

		directLighting += ComputeDirect(N, V, NdotV, diffuseColor, F0, F90, linearRoughness, roughness, L, radiance);
	}

	// ==== Point lights ====

	{
		UBPointLights* pointLights = g_PushConstants.UBPointLights;
		const uint pointLightCount = min(pointLights->Count, 1024);

		for (uint i = 0; i < pointLightCount; ++i)
		{
			const PointLight light = pointLights->PointLights[i];

			const float3 toLight     = light.Position - worldPosition;
			const float  attenuation = DistanceAttenuation(toLight, light.Radius);

			if (attenuation <= 0.0)
				continue;

			const float3 radiance = max(light.Radiance, float3(0.0)) * max(light.Intensity, 0.0) * attenuation;

			directLighting += ComputeDirect(N, V, NdotV, diffuseColor, F0, F90, linearRoughness, roughness, normalize(toLight), radiance);
		}
	}

	// ==== Spot lights ====

	{
		UBSpotLights* spotLights = g_PushConstants.UBSpotLights;
		const uint spotLightCount = min(spotLights->Count, 1000);

		for (uint i = 0; i < spotLightCount; ++i)
		{
			const SpotLight light = spotLights->SpotLights[i];

			const float3 toLight = light.Position - worldPosition;
			const float3 L       = normalize(toLight);

			// Angles are uploaded as degrees (half-angles) straight from the scene.
			const float cosOuter = cos(radians(light.OuterAngle));
			const float cosInner = cos(radians(min(light.InnerAngle, light.OuterAngle)));

			const float attenuation = DistanceAttenuation(toLight, light.Range) * AngleAttenuation(L, light.Direction, cosInner, cosOuter);

			if (attenuation <= 0.0)
				continue;

			const float3 radiance = max(light.Radiance, float3(0.0)) * max(light.Intensity, 0.0) * attenuation;

			directLighting += ComputeDirect(N, V, NdotV, diffuseColor, F0, F90, linearRoughness, roughness, L, radiance);
		}
	}

	// ==== Ambient (placeholder until IBL is implemented) ====

		const float3 indirectLighting = diffuseColor * 0.03 * occlusion;

	// ==== Final lighting ====

	g_StorageImages[g_PushConstants.OutputIndex][pixel] = float4(directLighting + indirectLighting + emissive, 1.0);
}
