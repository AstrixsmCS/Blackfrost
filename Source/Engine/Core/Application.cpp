#include "Application.hpp"

#include "Events/EventBus.hpp"

#include "Input.hpp"

#include "Renderer/Renderer.hpp"

Application* Application::s_Instance = nullptr;

Application::Application(const ApplicationSpecification &specification)
	: m_Specification(specification)
{
	s_Instance = this;

	Log::Initialize();

	BF_TRACE("Blackfrost Engine {}", BF_VERSION);
	BF_TRACE("Initializing...");

	m_Specification.Window.Title = m_Specification.Name;
	m_Window                     = Window::Create(m_Specification.Window);

	if (m_Specification.Window.Mode == WindowMode::Windowed)
		m_Window->CenterWindow();

	Renderer::Initialize(m_Window->GetNativeWindow());

	EventBus::Subscribe<WindowResizeEvent>([this](WindowResizeEvent& e){ e.m_Handled |= OnWindowResize(e); });
	EventBus::Subscribe<WindowMinimizeEvent>([this](WindowMinimizeEvent& e) { e.m_Handled |= OnWindowMinimize(e); });
	EventBus::Subscribe<WindowCloseEvent>([this](WindowCloseEvent& e) { e.m_Handled |= OnWindowClose(e); });
}

Application::~Application()
{
	BF_TRACE("Shutting down...");

	EventBus::Clear();

	Renderer::Shutdown();

	m_Window.reset();

	Log::Shutdown();

	s_Instance = nullptr;
}

void Application::Run()
{
	OnInitialize();

	m_LastFrameTime = Window::GetTime();

	while (m_Running)
	{
		const double time = Window::GetTime();
		m_Frametime       = static_cast<float>(time - m_LastFrameTime);
		m_TimeStep        = std::min(m_Frametime.GetSeconds(), 0.333f);
		m_LastFrameTime   = time;

		ProcessEvents();

		if (!m_Minimized && Renderer::BeginFrame())
		{
			OnUpdate(m_TimeStep);

			Renderer::EndFrame();
			Renderer::Present();
		}
		// BF_INFO("Frame time: {:.4f}ms | Timestep: {:.4f}ms | FPS: {:.1f}", m_Frametime * 1000.0f, m_TimeStep * 1000.0f, 1.0f / m_Frametime);
	}
	OnShutdown();
}

void Application::Close()
{
	m_Running = false;
}

void Application::ProcessEvents()
{
	Input::TransitionPressedKeys();
	Input::TransitionPressedButtons();
	m_Window->ProcessEvents();

	EventBus::Process();
}

bool Application::OnWindowResize(WindowResizeEvent& e)
{
	if (e.GetWidth() == 0 || e.GetHeight() == 0)
		return false;

	Renderer::GetSwapChain().RequestResize();

	return false;
}

bool Application::OnWindowMinimize(WindowMinimizeEvent& e)
{
	m_Minimized = e.IsMinimized();
	return false;
}

bool Application::OnWindowClose(WindowCloseEvent&)
{
	Close();
	return false; // Allow remaining subscribers to react to the close event.
}

const char* Application::GetConfigurationName()
{
	return BF_BUILD_CONFIG_NAME;
}

const char* Application::GetPlatformName()
{
	return BF_BUILD_PLATFORM_NAME;
}
