#include "Editor.hpp"

#include "Renderer/Renderer.hpp"

#include <format>

constexpr const char* MESH_PATH = "Assets/Meshes/Sponza/Sponza.gltf";

constexpr float CAMERA_FOV  = 60.0f;   // Degrees
constexpr float CAMERA_NEAR = 0.1f;
constexpr float CAMERA_FAR  = 1000.0f; // Unused by the infinite reversed-Z projection

float GetSwapChainAspect()
{
	const SwapChain& swapChain = Renderer::GetSwapChain();
	return static_cast<float>(swapChain.GetWidth()) / static_cast<float>(swapChain.GetHeight());
}

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	UpdateWindowTitle();

	m_SceneRenderer = std::make_unique<SceneRenderer>();

	if (!m_Mesh.Load(MESH_PATH))
		BF_ERROR("Failed to load '{}'", MESH_PATH);

	m_Camera = Camera(glm::radians(CAMERA_FOV), GetSwapChainAspect(), CAMERA_NEAR, CAMERA_FAR);
	m_Camera.SetPosition(glm::vec3(0.0f, 2.0f, 0.0f));

	m_Lights.DirectionalLights[0] =
	{
		.Direction = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.2f)),
		.Radiance  = glm::vec3(1.0f, 0.95f, 0.9f),
		.Intensity = 1.0f,
	};

	m_Lights.SpotLights.push_back(
	{
		.Position   = glm::vec3(-0.75f, 4.0f, -3.0f),
		.Intensity  = 60.0f,
		.Direction  = glm::vec3(0.0f, -1.0f, 0.0f),
		.Range      = 10.0f,
		.Radiance   = glm::vec3(1.0f, 0.1f, 0.1f),
		.InnerAngle = 20.0f,
		.OuterAngle = 30.0f,
	});

	m_Lights.SpotLights.push_back(
	{
		.Position   = glm::vec3(0.75f, 4.0f, -3.0f),
		.Intensity  = 60.0f,
		.Direction  = glm::vec3(0.0f, -1.0f, 0.0f),
		.Range      = 10.0f,
		.Radiance   = glm::vec3(0.1f, 0.1f, 1.0f),
		.InnerAngle = 20.0f,
		.OuterAngle = 30.0f,
	});

	m_Lights.PointLights.push_back(
	{
		.Position  = glm::vec3(3.0f, 1.5f, -3.0f),
		.Intensity = 30.0f,
		.Radiance  = glm::vec3(1.0f, 0.8f, 0.5f),
		.Radius    = 8.0f,
	});
}

void EditorApplication::OnShutdown()
{
	m_SceneRenderer.reset();
	m_Mesh.Destroy();
}

void EditorApplication::UpdateWindowTitle()
{
	const std::string title = std::format("Blackfrost-Editor {}", BF_VERSION);
	Application::Get().GetWindow().SetTitle(title);
}

void EditorApplication::OnUpdate(Timestep ts)
{
	const SwapChain& swapChain = Renderer::GetSwapChain();

	m_SceneRenderer->SetViewportSize(swapChain.GetWidth(), swapChain.GetHeight());

	m_Camera.SetPerspective(glm::radians(CAMERA_FOV), GetSwapChainAspect(), CAMERA_NEAR, CAMERA_FAR);
	m_Camera.OnUpdate(ts);

	const SceneRendererCamera sceneCamera
	{
		.Camera     = m_Camera,
		.ViewMatrix = m_Camera.GetView(),
		.Near       = CAMERA_NEAR,
		.Far        = CAMERA_FAR,
		.FOV        = CAMERA_FOV,
	};

	m_SceneRenderer->BeginScene(sceneCamera, m_Lights);
	m_SceneRenderer->SubmitMesh(m_Mesh);
	m_SceneRenderer->EndScene();
}
