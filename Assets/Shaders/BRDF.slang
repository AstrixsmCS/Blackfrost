#pragma once

import Common;

// Schlick Fresnel
float3 FresnelSchlick(float3 F0, float F90, float cosTheta)
{
	const float oneMinusCos = 1.0 - cosTheta;
	const float oneMinusCos2 = oneMinusCos * oneMinusCos;
	const float oneMinusCos5 = oneMinusCos2 * oneMinusCos2 * oneMinusCos;

	return F0 + (F90 - F0) * oneMinusCos5;
}

// Frostbite's renormalized Disney diffuse
float DiffuseDisney(float NdotV, float NdotL, float LdotH, float linearRoughness)
{
	const float energyBias = lerp(0.0, 0.5, linearRoughness);
	const float energyFactor = lerp(1.0, 1.0 / 1.51, linearRoughness);

	const float FD90 = energyBias + 2.0 * LdotH * LdotH * linearRoughness;

	const float lightScatter = FresnelSchlick(float3(1.0), FD90, NdotL).r;
	const float viewScatter = FresnelSchlick(float3(1.0), FD90, NdotV).r;

	return lightScatter * viewScatter * energyFactor;
}

// Trowbridge-Reitz GGX NDF
float DistributionGGX(float NdotH, float roughness)
{
	const float roughness2 = roughness * roughness;
	const float denominator = (NdotH * roughness2 - NdotH) * NdotH + 1.0;

	return roughness2 / max(denominator * denominator, 0.000001);
}

// Height-correlated Smith GGX visibility
float VisibilitySmithGGXCorrelated(float NdotV, float NdotL, float roughness)
{
	const float roughness2 = roughness * roughness;

	const float GGXV = NdotL * sqrt(max( (-NdotV * roughness2 + NdotV) * NdotV + roughness2, 0.000001));
	const float GGXL = NdotV * sqrt(max((-NdotL * roughness2 + NdotL) * NdotL + roughness2, 0.000001));

	return 0.5 / max(GGXV + GGXL, 0.000001);
}

// Cook-Torrance specular
float3 SpecularCookTorrance(float3 F, float D, float V)
{
	return D * F * V / PI;
}
