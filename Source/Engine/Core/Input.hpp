#pragma once

#include "KeyCodes.hpp"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ControllerButtonData
{
	SDL_GamepadButton Button;
	KeyState          State    = KeyState::None;
	KeyState          OldState = KeyState::None;
};

struct Controller
{
	SDL_JoystickID ID = 0;
	std::string    Name;

	std::map<SDL_GamepadButton, bool>                 ButtonDown;
	std::map<SDL_GamepadButton, ControllerButtonData> ButtonStates;
	std::map<SDL_GamepadAxis, float>                  AxisStates;
	std::map<SDL_GamepadAxis, float>                  DeadZones;
};

enum class KeyMods : uint16_t
{
	None     = SDL_KMOD_NONE,
	LShift   = SDL_KMOD_LSHIFT,
	RShift   = SDL_KMOD_RSHIFT,
	Shift    = SDL_KMOD_SHIFT,
	LCtrl    = SDL_KMOD_LCTRL,
	RCtrl    = SDL_KMOD_RCTRL,
	Ctrl     = SDL_KMOD_CTRL,
	LAlt     = SDL_KMOD_LALT,
	RAlt     = SDL_KMOD_RALT,
	Alt      = SDL_KMOD_ALT,
	LSuper   = SDL_KMOD_LGUI,
	RSuper   = SDL_KMOD_RGUI,
	Super    = SDL_KMOD_GUI,
	CapsLock = SDL_KMOD_CAPS,
	NumLock  = SDL_KMOD_NUM,
	Scroll   = SDL_KMOD_SCROLL
};

struct KeyData
{
	KeyCode  Key;
	KeyState State    = KeyState::None;
	KeyState OldState = KeyState::None;
};

struct ButtonData
{
	MouseButton Button;
	KeyState    State    = KeyState::None;
	KeyState    OldState = KeyState::None;
};

class Input
{
public:
	static void Update();

	static bool IsKeyPressed(KeyCode keycode);
	static bool IsKeyHeld(KeyCode keycode);
	static bool IsKeyDown(KeyCode keycode);
	static bool IsKeyReleased(KeyCode keycode);
	static bool IsKeyToggledOn(KeyCode keycode);

	static bool IsMouseButtonPressed(MouseButton button);
	static bool IsMouseButtonHeld(MouseButton button);
	static bool IsMouseButtonDown(MouseButton button);
	static bool IsMouseButtonReleased(MouseButton button);

	static float                   GetMouseX();
	static float                   GetMouseY();
	static std::pair<float, float> GetMousePosition();

	static void       SetCursorMode(CursorMode mode);
	static CursorMode GetCursorMode();

	// Controllers
	static bool                        IsControllerPresent(SDL_JoystickID id);
	static std::vector<SDL_JoystickID> GetConnectedControllerIDs();

	static const Controller* GetController(SDL_JoystickID id);
	static std::string_view  GetControllerName(SDL_JoystickID id);

	static bool IsControllerButtonPressed(SDL_JoystickID controllerID, SDL_GamepadButton button);
	static bool IsControllerButtonHeld(SDL_JoystickID controllerID, SDL_GamepadButton button);
	static bool IsControllerButtonDown(SDL_JoystickID controllerID, SDL_GamepadButton button);
	static bool IsControllerButtonReleased(SDL_JoystickID controllerID, SDL_GamepadButton button);

	static float GetControllerAxis(SDL_JoystickID controllerID, SDL_GamepadAxis axis);
	static float GetControllerDeadzone(SDL_JoystickID controllerID, SDL_GamepadAxis axis);
	static void  SetControllerDeadzone(SDL_JoystickID controllerID, SDL_GamepadAxis axis, float deadzone);

	static const std::map<SDL_JoystickID, Controller>& GetControllers()
	{
		return s_Controllers;
	}

	// Internal
	static void TransitionPressedKeys();
	static void TransitionPressedButtons();
	static void UpdateKeyState(KeyCode key, KeyState newState);
	static void UpdateKeyMods(KeyMods mods);
	static void UpdateButtonState(MouseButton button, KeyState newState);
	static void UpdateControllerButtonState(SDL_JoystickID controllerID, SDL_GamepadButton button, KeyState newState);
	static void ClearReleasedKeys();

private:
	inline static std::map<KeyCode, KeyData>           s_KeyData;
	inline static std::map<MouseButton, ButtonData>    s_MouseData;
	inline static std::map<SDL_JoystickID, Controller> s_Controllers;

	inline static KeyMods s_Mods = KeyMods::None;
};
