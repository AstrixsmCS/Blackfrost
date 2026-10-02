#include "Window.hpp"

#include "Events/ApplicationEvent.hpp"
#include "Events/EventBus.hpp"
#include "Events/KeyEvent.hpp"
#include "Events/MouseEvent.hpp"

#include "Input.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>
#include <stb/stb_image.h>

#include <cassert>

static void SDLErrorCallback(const char* context)
{
	BF_ERROR("SDL Error ({}): {}", context, SDL_GetError());
}

std::unique_ptr<Window> Window::Create(const WindowSpecification& specification)
{
	return std::make_unique<Window>(specification);
}

Window::Window(const WindowSpecification& specification)
	: m_Specification(specification)
{
	BF_INFO("Creating window {} ({}, {})", specification.Title, specification.Width, specification.Height);

	SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD);

	const Uint32 windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN;

	switch (specification.Mode)
	{
		case WindowMode::Windowed:
			m_Window = SDL_CreateWindow(specification.Title.c_str(), (int)specification.Width, (int)specification.Height, windowFlags);
			break;

		case WindowMode::BorderlessFullscreen:
		{
			const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
			m_Window                    = SDL_CreateWindow(specification.Title.c_str(), mode->w, mode->h, windowFlags | SDL_WINDOW_BORDERLESS);
			SDL_SetWindowPosition(m_Window, 0, 0);
			break;
		}

		case WindowMode::ExclusiveFullscreen:
		{
			const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
			m_Window                    = SDL_CreateWindow(specification.Title.c_str(), mode->w, mode->h, windowFlags | SDL_WINDOW_FULLSCREEN);
			break;
		}

		case WindowMode::Maximized:
			m_Window = SDL_CreateWindow(specification.Title.c_str(), (int)specification.Width, (int)specification.Height, windowFlags | SDL_WINDOW_MAXIMIZED);
			break;
	}

	assert(m_Window && "Could not create window");

	if (!SDL_SetWindowResizable(m_Window, specification.Resizable))
		SDLErrorCallback("SDL_SetWindowResizable");

	// Set icon
	{
		constexpr const char* ICON_PATH = "Assets/Blackfrost-Logo-2026.png";

		int width    = 0;
		int height   = 0;
		int channels = 0;

		stbi_uc* pixels = stbi_load(ICON_PATH, &width, &height, &channels, STBI_rgb_alpha);

		if (!pixels)
		{
			BF_WARN("Failed to load window icon '{}': {}", ICON_PATH, stbi_failure_reason());
		}
		else if (SDL_Surface* icon = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4))
		{
			if (!SDL_SetWindowIcon(m_Window, icon))
				SDLErrorCallback("SDL_SetWindowIcon");

			SDL_DestroySurface(icon);
		}
		else
		{
			SDLErrorCallback("SDL_CreateSurfaceFrom");
		}

		stbi_image_free(pixels);
	}

	int w, h;
	SDL_GetWindowSize(m_Window, &w, &h);
	m_Specification.Width  = static_cast<uint32_t>(w);
	m_Specification.Height = static_cast<uint32_t>(h);
}

Window::~Window()
{
	if (m_Window)
	{
		SDL_DestroyWindow(m_Window);
		m_Window = nullptr;
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD);
}

void Window::PollEvents()
{
	const SDL_WindowID windowID = SDL_GetWindowID(m_Window);
	if (windowID == 0)
	{
		SDLErrorCallback("SDL_GetWindowID");
		return;
	}

	while (SDL_PollEvent(&m_Event))
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData)
		{
			ImGui_ImplSDL3_ProcessEvent(&m_Event);
		}

		switch (m_Event.type)
		{
			case SDL_EVENT_QUIT:
			{
				EventBus::Publish<WindowCloseEvent>();
				break;
			}

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			{
				if (m_Event.window.windowID == windowID)
					EventBus::Publish<WindowCloseEvent>();

				break;
			}

			case SDL_EVENT_WINDOW_RESIZED:
			{
				if (m_Event.window.windowID != windowID)
					break;

				const uint32_t width  = static_cast<uint32_t>(m_Event.window.data1);
				const uint32_t height = static_cast<uint32_t>(m_Event.window.data2);

				m_Specification.Width  = width;
				m_Specification.Height = height;

				EventBus::Publish<WindowResizeEvent>(width, height);

				break;
			}

			case SDL_EVENT_WINDOW_MINIMIZED:
			{
				if (m_Event.window.windowID == windowID)
					EventBus::Publish<WindowMinimizeEvent>(true);

				break;
			}

			case SDL_EVENT_WINDOW_MAXIMIZED:
			case SDL_EVENT_WINDOW_RESTORED:
			{
				if (m_Event.window.windowID == windowID)
					EventBus::Publish<WindowMinimizeEvent>(false);

				break;
			}

			case SDL_EVENT_KEY_DOWN:
			{
				if (m_Event.key.windowID != windowID)
					break;

				Input::UpdateKeyMods(static_cast<KeyMods>(m_Event.key.mod));

				const KeyCode key = static_cast<KeyCode>(m_Event.key.scancode);

				if (m_Event.key.repeat)
				{
					Input::UpdateKeyState(key, KeyState::Held);
					EventBus::Publish<KeyPressedEvent>(key, 1);
				}
				else
				{
					Input::UpdateKeyState(key, KeyState::Pressed);
					EventBus::Publish<KeyPressedEvent>(key, 0);
				}

				break;
			}

			case SDL_EVENT_KEY_UP:
			{
				if (m_Event.key.windowID != windowID)
					break;

				Input::UpdateKeyMods(static_cast<KeyMods>(m_Event.key.mod));

				const KeyCode key = static_cast<KeyCode>(m_Event.key.scancode);
				Input::UpdateKeyState(key, KeyState::Released);

				EventBus::Publish<KeyReleasedEvent>(key);

				break;
			}

			case SDL_EVENT_TEXT_INPUT:
			{
				if (m_Event.text.windowID != windowID)
					break;

				const char* text = m_Event.text.text;

				while (const Uint32 codepoint = SDL_StepUTF8(&text, nullptr))
					EventBus::Publish<KeyTypedEvent>(codepoint);

				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			{
				if (m_Event.button.windowID != windowID)
					break;

				const MouseButton button = static_cast<MouseButton>(m_Event.button.button);
				Input::UpdateButtonState(button, KeyState::Pressed);

				EventBus::Publish<MouseButtonPressedEvent>(button);

				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				if (m_Event.button.windowID != windowID)
					break;

				const MouseButton button = static_cast<MouseButton>(m_Event.button.button);
				Input::UpdateButtonState(button, KeyState::Released);

				EventBus::Publish<MouseButtonReleasedEvent>(button);

				break;
			}

			case SDL_EVENT_MOUSE_WHEEL:
			{
				if (m_Event.wheel.windowID != windowID)
					break;

				EventBus::Publish<MouseScrolledEvent>(m_Event.wheel.x, m_Event.wheel.y);

				break;
			}

			case SDL_EVENT_MOUSE_MOTION:
			{
				if (m_Event.motion.windowID != windowID)
					break;

				if (SDL_GetWindowRelativeMouseMode(m_Window))
				{
					EventBus::Publish<MouseMovedEvent>(m_Event.motion.xrel, m_Event.motion.yrel);
				}
				else
				{
					EventBus::Publish<MouseMovedEvent>(m_Event.motion.x, m_Event.motion.y);
				}

				break;
			}

			case SDL_EVENT_GAMEPAD_ADDED:
			{
				if (!SDL_OpenGamepad(m_Event.gdevice.which))
					SDLErrorCallback("SDL_OpenGamepad");
				break;
			}

			case SDL_EVENT_GAMEPAD_REMOVED:
			{
				if (SDL_Gamepad* gamepad = SDL_GetGamepadFromID(m_Event.gdevice.which))
					SDL_CloseGamepad(gamepad);
				break;
			}
		}
	}
}

std::pair<float, float> Window::GetWindowPos() const
{
	int x, y;
	if (!SDL_GetWindowPosition(m_Window, &x, &y))
	{
		BF_WARN("Failed to get window position: {}", SDL_GetError());
		return { 0.0f, 0.0f };
	}
	return { static_cast<float>(x), static_cast<float>(y) };
}

void Window::ProcessEvents()
{
	PollEvents();
	Input::Update();
}

void Window::SetVSync(bool enabled)
{
	m_Specification.VSync = enabled;
	// swapchain recreate/update
}

void Window::SetResizable(bool resizable)
{
	m_Specification.Resizable = resizable;
	if (!SDL_SetWindowResizable(m_Window, resizable))
		SDLErrorCallback("SDL_SetWindowResizable");
}

void Window::Maximize()
{
	if (m_Specification.Mode == WindowMode::Windowed || m_Specification.Mode == WindowMode::Maximized)
		SDL_MaximizeWindow(m_Window);
}

void Window::CenterWindow()
{
	SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void Window::SetTitle(const std::string& title)
{
	m_Specification.Title = title;
	if (!SDL_SetWindowTitle(m_Window, title.c_str()))
		SDLErrorCallback("SDL_SetWindowTitle");
}

double Window::GetTime()
{
	static const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
	static const double start     = static_cast<double>(SDL_GetPerformanceCounter()) / frequency;
	return static_cast<double>(SDL_GetPerformanceCounter()) / frequency - start;
}
