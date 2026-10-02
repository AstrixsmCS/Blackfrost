#pragma once

#include "TimeStep.hpp"
#include "Window.hpp"

#include "Events/ApplicationEvent.hpp"

#include <memory>
#include <string>

struct ApplicationSpecification
{
	std::string         Name = "Blackfrost";
	WindowSpecification Window;
};

class Application
{
public:
	explicit Application(const ApplicationSpecification& specification);
	virtual ~Application();

	Application(const Application&)            = delete;
	Application& operator=(const Application&) = delete;

	void Run();
	void Close();

	inline Window& GetWindow() { return *m_Window; }

	static inline Application& Get() { return *s_Instance; }

	Timestep GetTimestep() const { return m_TimeStep; }
	Timestep GetFrametime() const { return m_Frametime; }

	static const char* GetConfigurationName();
	static const char* GetPlatformName();

	const ApplicationSpecification& GetSpecification() const { return m_Specification; }

protected:
	virtual void OnInitialize() {}
	virtual void OnUpdate(Timestep ts) {}
	virtual void OnShutdown() {}

private:
	void ProcessEvents();

	bool OnWindowResize(WindowResizeEvent& e);
	bool OnWindowMinimize(WindowMinimizeEvent& e);
	bool OnWindowClose(WindowCloseEvent&);

private:
	std::unique_ptr<Window>  m_Window;
	ApplicationSpecification m_Specification;

	bool m_Running = true, m_Minimized = false;

	Timestep m_Frametime;
	Timestep m_TimeStep;
	double   m_LastFrameTime = 0.0f;

	static Application* s_Instance;
};
