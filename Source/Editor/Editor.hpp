#pragma once

#include "Core/Application.hpp"

#include "Renderer/Camera.hpp"

#include "Renderer/Mesh.hpp"
#include "Renderer/SceneEnvironment.hpp"
#include "Renderer/SceneRenderer.hpp"

#include <memory>

class EditorApplication final : public Application
{
public:
	explicit EditorApplication(const ApplicationSpecification& specification);
	~EditorApplication();

protected:
	void OnInitialize() override;
	void OnUpdate(Timestep) override;
	void OnShutdown() override;

private:
	void UpdateWindowTitle();

private:
	std::unique_ptr<SceneRenderer> m_SceneRenderer;

	Camera           m_Camera;
	Mesh             m_Mesh;
	LightEnvironment m_Lights;
};
