#include "Editor.hpp"

#include <format>

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	UpdateWindowTitle();
}

void EditorApplication::OnShutdown()
{
}

void EditorApplication::UpdateWindowTitle()
{
	const std::string title = std::format("Blackfrost-Editor {}", BF_VERSION);
	Application::Get().GetWindow().SetTitle(title);
}

void EditorApplication::OnUpdate(Timestep ts)
{
}
