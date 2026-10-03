#pragma once

static const float PI = 3.14159265359f;
static const float TwoPI = 2.0 * PI;
static const float Epsilon = 0.00001;

float3 RotateVectorAboutY(float angle, float3 vector)
{
	angle = radians(angle);

	const float3x3 rotationMatrix =
	{
		float3( cos(angle), 0.0, sin(angle)),
		float3(        0.0, 1.0,        0.0),
		float3(-sin(angle), 0.0, cos(angle))
	};

	return mul(rotationMatrix, vector);
}

float ConvertSRGBFromLinear(float linearValue)
{
	return linearValue <= 0.0031308f ? linearValue * 12.92f : pow(linearValue, 1.0f / 2.4f) * 1.055f - 0.055f;
}

float3 ConvertSRGBFromLinear(float3 linearValue)
{
	const bool3 cutoff = linearValue <= float3(0.0031308f);

	const float3 lower = linearValue * 12.92f;
	const float3 higher = pow(linearValue, float3(1.0f / 2.4f)) * 1.055f - 0.055f;

	return select(cutoff, lower, higher);
}

float4 ToLinear(float4 sRGB)
{
	const bool4 cutoff = sRGB < float4(0.04045f);

	const float4 higher = pow((sRGB + float4(0.055f)) / float4(1.055f), float4(2.4f));
	const float4 lower = sRGB / float4(12.92f);

	return select(cutoff, lower, higher);
}
