#pragma once

struct GPUMaterialData
{
	float4 Albedo;

	uint AlbedoIndex;
	uint NormalIndex;
	uint MetallicRoughnessIndex;
	uint OcclusionIndex;
	uint EmissiveIndex;

	float Metalness;
	float Roughness;
	float AlphaCutoff;
	uint Flags;

	float4 Emissive;

	float Transmission;
};
