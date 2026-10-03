#pragma once

#include "Camera.hpp"
#include "Mesh.hpp"
#include "Renderer.hpp"
#include "SceneEnvironment.hpp"

#include "Renderer/Vulkan/Buffer.hpp"
#include "Renderer/Vulkan/Texture.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

struct SceneRendererCamera
{
	Camera    Camera;
	glm::mat4 ViewMatrix { 1.0f };
	float     Near = 0.1f, Far = 1000.0f; // Physical distances (Near < Far)
	float     FOV  = 45.0f;
};

enum class DebugView : uint32_t
{
	None = 0,
	Albedo,
	Normal,
	Roughness,
	Metallic,
	AO,
	Emissive,
	Depth,
	HDR,
};

struct SceneRendererSpecification
{
	uint32_t ViewportWidth = 0, ViewportHeight = 0; // 0 means application window size
};

class SceneRenderer
{
public:
	explicit SceneRenderer(SceneRendererSpecification specification = {});
	~SceneRenderer();

	SceneRenderer(const SceneRenderer&)            = delete;
	SceneRenderer& operator=(const SceneRenderer&) = delete;

	void Initialize();
	void Shutdown();

	void SetViewportSize(uint32_t width, uint32_t height);

	void BeginScene(const SceneRendererCamera& camera, const LightEnvironment& lights);
	void EndScene();

	void SubmitMesh(const Mesh& mesh, const glm::mat4& transform = glm::mat4(1.0f));

	const Texture& GetFinalImage() const { return m_LDRColor; }

	const SceneRendererSpecification& GetSpecification() const { return m_Specification; }

	uint32_t GetViewportWidth() const { return m_ViewportWidth; }
	uint32_t GetViewportHeight() const { return m_ViewportHeight; }

	float GetExposure() const { return m_Exposure; }
	void  SetExposure(float exposure) { m_Exposure = exposure; }
private:
	void ResizeTargets();
	void UpdateUniformBuffers(const SceneRendererCamera& camera, const LightEnvironment& lights);

	void FlushDrawList();

	void ClearPass();

	// Passes
	void GeometryPass();
	void LightingPass();

	// Post-Processing
	void TonemapPass();
	void CompositePass();

private:
	SceneRendererSpecification m_Specification;

	CommandBuffer* m_CommandBuffer = nullptr;

	// ==== GPU data (layouts must match the shaders) ====

	struct UBCamera
	{
		glm::mat4 ViewProjection;
		glm::mat4 InverseViewProjection;

		glm::mat4 Projection;
		glm::mat4 InverseProjection;

		glm::mat4 View;
		glm::mat4 InverseView;
	} m_CameraUB;

	struct DirLight
	{
		glm::vec3 Direction;
		glm::vec3 Radiance;
		float     Intensity;
	};

	struct UBScene
	{
		DirLight Lights;

		glm::vec3 CameraPosition;
		float     EnvironmentMapIntensity = 0.0f; // No environment yet kept so the shader layout is unchanged
	} m_SceneUB;

	static constexpr uint32_t MAX_POINT_LIGHTS = 1024;
	static constexpr uint32_t MAX_SPOT_LIGHTS  = 1000;

	struct UBPointLights
	{
		uint32_t   Count = 0;
		PointLight PointLights[1024] {};
	} m_PointLightsUB;

	struct UBSpotLights
	{
		uint32_t  Count = 0;
		SpotLight SpotLights[1000] {};
	} m_SpotLightsUB;

	std::array<Buffer, Renderer::GetFramesInFlight()> m_CameraBuffers;
	std::array<Buffer, Renderer::GetFramesInFlight()> m_SceneBuffers;
	std::array<Buffer, Renderer::GetFramesInFlight()> m_PointLightBuffers;
	std::array<Buffer, Renderer::GetFramesInFlight()> m_SpotLightBuffers;

	struct DrawCommand
	{
		const Mesh* Mesh = nullptr;
		glm::mat4   Transform { 1.0f };
	};

	std::vector<DrawCommand> m_DrawList;

	// ==== Geometry pass ====

	Material      m_GBufferMaterial;
	GraphicsState m_GBufferState;

	struct GBuffer
	{
		Texture Albedo;
		Texture Normal;
		Texture Material;
		Texture Emissive;
		Texture DepthStencil;
	} m_GBuffer;

	// ==== Lighting pass ====

	Material m_LightingMaterial;
	Texture  m_HDRColor;

	// ==== Tonemap pass ====

	Material m_TonemapMaterial;
	Texture  m_LDRColor; // RGBA8_UNorm, gamma-encoded (sRGB curve applied in the shader)
	float    m_Exposure = 1.0f;

	// ==== Composite pass ====

	Material      m_CompositeMaterial;
	GraphicsState m_CompositeState;

	// ==== State ====

	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

	bool m_NeedsResize = false;
	bool m_Active      = false;
	bool m_Initialized = false;
};
