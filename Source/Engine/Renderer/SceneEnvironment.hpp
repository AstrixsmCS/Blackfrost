#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

struct DirectionalLight
{
	glm::vec3 Direction { -1.0f, -1.0f, -1.0f };
	glm::vec3 Radiance  { 1.0f, 1.0f, 1.0f };
	float     Intensity = 0.0f;
};

struct PointLight
{
	glm::vec3 Position { 0.0f, 0.0f, 0.0f };
	float     Intensity = 1.0f;

	glm::vec3 Radiance { 1.0f, 1.0f, 1.0f };
	float     Radius    = 10.0f;
};

struct SpotLight
{
	glm::vec3 Position  { 0.0f, 0.0f, 0.0f };
	float     Intensity  = 1.0f;

	glm::vec3 Direction { 0.0f, -1.0f, 0.0f }; // cone axis, pointing away from the light (normalized)
	float     Range      = 10.0f;

	glm::vec3 Radiance  { 1.0f, 1.0f, 1.0f };
	float     InnerAngle = 20.0f; // degrees, half-angle: full intensity inside

	float     OuterAngle = 30.0f; // degrees, half-angle: zero outside
};


struct LightEnvironment
{
	static constexpr uint32_t MaxDirectionalLights = 4;

	DirectionalLight DirectionalLights[MaxDirectionalLights];
	std::vector<PointLight>       PointLights;
	std::vector<SpotLight>        SpotLights;

	void Clear()
	{
		for (DirectionalLight& light : DirectionalLights)
			light = {};

		PointLights.clear();
		SpotLights.clear();
	}
};
