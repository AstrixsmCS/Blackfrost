#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>

enum class WindowMode
{
	Windowed = 0,
	BorderlessFullscreen,
	ExclusiveFullscreen,
	Maximized
};

struct WindowSpecification
{
	std::string Title     = "Blackfrost";
	uint32_t    Width     = 1600;
	uint32_t    Height    = 900;
	WindowMode  Mode      = WindowMode::Windowed;
	bool        VSync     = true;
	bool        Resizable = true;
};

class Window
{
public:
	explicit Window(const WindowSpecification& specification);
	~Window();

	Window(const Window&)            = delete;
	Window& operator=(const Window&) = delete;

	static std::unique_ptr<Window> Create(const WindowSpecification& specification = {});

	void ProcessEvents();

	uint32_t GetWidth() const { return m_Specification.Width; }
	uint32_t GetHeight() const { return m_Specification.Height; }

	std::pair<uint32_t, uint32_t> GetSize() const { return { m_Specification.Width, m_Specification.Height }; }
	std::pair<float, float>       GetWindowPos() const;

	// Window attributes
	void SetVSync(bool enabled);
	bool IsVSync() const { return m_Specification.VSync; }
	void SetResizable(bool resizable);

	void Maximize();
	void CenterWindow();

	const std::string& GetTitle() const { return m_Specification.Title; }
	void               SetTitle(const std::string& title);

	SDL_Window* GetNativeWindow() const { return m_Window; }

	static double GetTime();

private:
	void PollEvents();

private:
	SDL_Window* m_Window = nullptr;
	SDL_Event   m_Event  = {};

	WindowSpecification m_Specification;
};
