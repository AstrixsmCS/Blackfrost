#pragma once

struct DirectionalLight
{
	float3 Direction;
	float3 Radiance;
	float  Intensity;
};

struct PointLight
{
	float3 Position;
	float  Intensity;

	float3 Radiance;
	float  Radius;
};

struct SpotLight
{
	float3 Position;
	float  Intensity;

	float3 Direction; // cone axis, pointing away from the light (normalized)
	float  Range;

	float3 Radiance;
	float  InnerAngle; // degrees, half-angle

	float  OuterAngle; // degrees, half-angle
};

struct UBCamera
{
	float4x4 ViewProjection;
	float4x4 InverseViewProjection;

	float4x4 Projection;
	float4x4 InverseProjection;

	float4x4 View;
	float4x4 InverseView;
};

struct UBScreenData
{
	float2 InvFullResolution;
	float2 FullResolution;

	float2 InvHalfResolution;
	float2 HalfResolution;
};

struct UBScene
{
	DirectionalLight Light;

	float3 CameraPosition;
	float EnvironmentMapIntensity;
};

struct UBPointLights
{
	uint Count;
	PointLight PointLights[1024];
};

struct UBSpotLights
{
	uint Count;
	SpotLight SpotLights[1000];
};
