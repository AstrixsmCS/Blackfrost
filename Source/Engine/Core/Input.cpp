#include "Input.hpp"

#include "Core/Application.hpp"

#include <SDL3/SDL.h>

void Input::Update()
{
	int             gamepadCount = 0;
	SDL_JoystickID* gamepads     = SDL_GetGamepads(&gamepadCount);

	// Remove disconnected controllers
	for (auto it = s_Controllers.begin(); it != s_Controllers.end();)
	{
		const SDL_JoystickID id    = it->first;
		bool                 found = false;

		for (int i = 0; i < gamepadCount; i++)
		{
			if (gamepads[i] == id)
			{
				found = true;
				break;
			}
		}

		if (found)
			++it;
		else
			it = s_Controllers.erase(it);
	}

	for (int i = 0; i < gamepadCount; i++)
	{
		const SDL_JoystickID id = gamepads[i];

		SDL_Gamepad* gamepad = SDL_GetGamepadFromID(id);
		if (!gamepad)
			continue;

		Controller& controller = s_Controllers[id];

		controller.ID = id;

		if (const char* name = SDL_GetGamepadName(gamepad))
			controller.Name = name;
		else
			controller.Name.clear();

		for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
		{
			const auto button = static_cast<SDL_GamepadButton>(b);
			const bool down   = SDL_GetGamepadButton(gamepad, button);

			if (down && !controller.ButtonDown[button])
				controller.ButtonStates[button].State = KeyState::Pressed;
			else if (!down && controller.ButtonDown[button])
				controller.ButtonStates[button].State = KeyState::Released;

			controller.ButtonDown[button] = down;
		}

		for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; a++)
		{
			const auto  axis  = static_cast<SDL_GamepadAxis>(a);
			const float value = static_cast<float>(SDL_GetGamepadAxis(gamepad, axis)) / 32767.0f;

			controller.AxisStates[axis] = std::abs(value) > controller.DeadZones[axis] ? value : 0.0f;
		}
	}

	SDL_free(gamepads);
}

bool Input::IsKeyPressed(KeyCode keycode)
{
	const auto it = s_KeyData.find(keycode);
	return it != s_KeyData.end() && it->second.State == KeyState::Pressed;
}

bool Input::IsKeyHeld(KeyCode keycode)
{
	const auto it = s_KeyData.find(keycode);
	return it != s_KeyData.end() && it->second.State == KeyState::Held;
}

bool Input::IsKeyDown(KeyCode keycode)
{
	const bool* keys = SDL_GetKeyboardState(nullptr);
	return keys[static_cast<SDL_Scancode>(keycode)];
}

bool Input::IsKeyReleased(KeyCode keycode)
{
	const auto it = s_KeyData.find(keycode);
	return it != s_KeyData.end() && it->second.State == KeyState::Released;
}

bool Input::IsKeyToggledOn(KeyCode keycode)
{
	const SDL_Keymod mods = SDL_GetModState();

	if (keycode == KeyCode::CapsLock)
		return mods & SDL_KMOD_CAPS;

	if (keycode == KeyCode::NumLock)
		return mods & SDL_KMOD_NUM;

	if (keycode == KeyCode::ScrollLock)
		return mods & SDL_KMOD_SCROLL;

	return false;
}

bool Input::IsMouseButtonPressed(MouseButton button)
{
	const auto it = s_MouseData.find(button);
	return it != s_MouseData.end() && it->second.State == KeyState::Pressed;
}

bool Input::IsMouseButtonHeld(MouseButton button)
{
	const auto it = s_MouseData.find(button);
	return it != s_MouseData.end() && it->second.State == KeyState::Held;
}

bool Input::IsMouseButtonDown(MouseButton button)
{
	float x = 0.0f;
	float y = 0.0f;

	const SDL_MouseButtonFlags state = SDL_GetMouseState(&x, &y);
	return state & SDL_BUTTON_MASK(static_cast<int>(button));
}

bool Input::IsMouseButtonReleased(MouseButton button)
{
	const auto it = s_MouseData.find(button);
	return it != s_MouseData.end() && it->second.State == KeyState::Released;
}

float Input::GetMouseX()
{
	const auto [x, y] = GetMousePosition();
	return x;
}

float Input::GetMouseY()
{
	const auto [x, y] = GetMousePosition();
	return y;
}

std::pair<float, float> Input::GetMousePosition()
{
	SDL_Window* window = Application::Get().GetWindow().GetNativeWindow();

	float x = 0.0f;
	float y = 0.0f;

	if (SDL_GetWindowRelativeMouseMode(window))
	{
		SDL_GetRelativeMouseState(&x, &y);
		return { x, y };
	}

	SDL_GetMouseState(&x, &y);
	return { x, y };
}

void Input::SetCursorMode(CursorMode mode)
{
	SDL_Window* window = Application::Get().GetWindow().GetNativeWindow();

	switch (mode)
	{
		case CursorMode::Normal:
			SDL_SetWindowRelativeMouseMode(window, false);
			SDL_ShowCursor();
			break;

		case CursorMode::Hidden:
			SDL_SetWindowRelativeMouseMode(window, false);
			SDL_HideCursor();
			break;

		case CursorMode::Locked:
			SDL_SetWindowRelativeMouseMode(window, true);
			break;
	}
}

CursorMode Input::GetCursorMode()
{
	SDL_Window* window = Application::Get().GetWindow().GetNativeWindow();

	if (SDL_GetWindowRelativeMouseMode(window))
		return CursorMode::Locked;

	if (SDL_CursorVisible())
		return CursorMode::Normal;

	return CursorMode::Hidden;
}

bool Input::IsControllerPresent(SDL_JoystickID id)
{
	return s_Controllers.contains(id);
}

std::vector<SDL_JoystickID> Input::GetConnectedControllerIDs()
{
	std::vector<SDL_JoystickID> ids;
	ids.reserve(s_Controllers.size());

	for (const auto& [id, controller] : s_Controllers)
		ids.emplace_back(id);

	return ids;
}

const Controller* Input::GetController(SDL_JoystickID id)
{
	const auto it = s_Controllers.find(id);

	if (it == s_Controllers.end())
		return nullptr;

	return &it->second;
}

std::string_view Input::GetControllerName(SDL_JoystickID id)
{
	const Controller* controller = GetController(id);

	if (!controller)
		return {};

	return controller->Name;
}

bool Input::IsControllerButtonPressed(SDL_JoystickID controllerID, SDL_GamepadButton button)
{
	const Controller* controller = GetController(controllerID);

	if (!controller)
		return false;

	const auto it = controller->ButtonStates.find(button);

	return it != controller->ButtonStates.end() && it->second.State == KeyState::Pressed;
}

bool Input::IsControllerButtonHeld(SDL_JoystickID controllerID, SDL_GamepadButton button)
{
	const Controller* controller = GetController(controllerID);

	if (!controller)
		return false;

	const auto it = controller->ButtonStates.find(button);

	return it != controller->ButtonStates.end() && it->second.State == KeyState::Held;
}

bool Input::IsControllerButtonDown(SDL_JoystickID controllerID, SDL_GamepadButton button)
{
	const Controller* controller = GetController(controllerID);

	if (!controller)
		return false;

	const auto it = controller->ButtonDown.find(button);

	if (it == controller->ButtonDown.end())
		return false;

	return it->second;
}

bool Input::IsControllerButtonReleased(SDL_JoystickID controllerID, SDL_GamepadButton button)
{
	const Controller* controller = GetController(controllerID);

	if (!controller)
		return false;

	const auto it = controller->ButtonStates.find(button);

	return it != controller->ButtonStates.end() && it->second.State == KeyState::Released;
}

float Input::GetControllerAxis(SDL_JoystickID controllerID, SDL_GamepadAxis axis)
{
	const Controller* controller = GetController(controllerID);

	if (!controller)
		return 0.0f;

	const auto it = controller->AxisStates.find(axis);

	if (it == controller->AxisStates.end())
		return 0.0f;

	return it->second;
}

float Input::GetControllerDeadzone(SDL_JoystickID controllerID, SDL_GamepadAxis axis)
{
	const Controller* controller = GetController(controllerID);

	if (!controller)
		return 0.0f;

	const auto it = controller->DeadZones.find(axis);

	if (it == controller->DeadZones.end())
		return 0.0f;

	return it->second;
}

void Input::SetControllerDeadzone(SDL_JoystickID controllerID, SDL_GamepadAxis axis, float deadzone)
{
	auto it = s_Controllers.find(controllerID);

	if (it == s_Controllers.end())
		return;

	it->second.DeadZones[axis] = deadzone;
}

void Input::TransitionPressedKeys()
{
	for (auto& [key, keyData] : s_KeyData)
	{
		if (keyData.State == KeyState::Pressed)
		{
			keyData.OldState = keyData.State;
			keyData.State    = KeyState::Held;
		}
	}
}

void Input::TransitionPressedButtons()
{
	for (auto& [button, buttonData] : s_MouseData)
	{
		if (buttonData.State == KeyState::Pressed)
		{
			buttonData.OldState = buttonData.State;
			buttonData.State    = KeyState::Held;
		}
	}

	for (auto& [id, controller] : s_Controllers)
	{
		for (auto& [button, buttonData] : controller.ButtonStates)
		{
			if (buttonData.State == KeyState::Pressed)
			{
				buttonData.OldState = buttonData.State;
				buttonData.State    = KeyState::Held;
			}
		}
	}
}

void Input::UpdateKeyState(KeyCode key, KeyState newState)
{
	KeyData& keyData = s_KeyData[key];

	keyData.Key      = key;
	keyData.OldState = keyData.State;
	keyData.State    = newState;
}

void Input::UpdateKeyMods(KeyMods mods)
{
	s_Mods = mods;
}

void Input::UpdateButtonState(MouseButton button, KeyState newState)
{
	ButtonData& buttonData = s_MouseData[button];

	buttonData.Button   = button;
	buttonData.OldState = buttonData.State;
	buttonData.State    = newState;
}

void Input::UpdateControllerButtonState(SDL_JoystickID controllerID, SDL_GamepadButton button, KeyState newState)
{
	auto controllerIt = s_Controllers.find(controllerID);

	if (controllerIt == s_Controllers.end())
		return;

	ControllerButtonData& buttonData = controllerIt->second.ButtonStates[button];

	buttonData.Button   = button;
	buttonData.OldState = buttonData.State;
	buttonData.State    = newState;
}

void Input::ClearReleasedKeys()
{
	for (auto& [key, keyData] : s_KeyData)
	{
		if (keyData.State == KeyState::Released)
		{
			keyData.OldState = keyData.State;
			keyData.State    = KeyState::None;
		}
	}

	for (auto& [button, buttonData] : s_MouseData)
	{
		if (buttonData.State == KeyState::Released)
		{
			buttonData.OldState = buttonData.State;
			buttonData.State    = KeyState::None;
		}
	}

	for (auto& [id, controller] : s_Controllers)
	{
		for (auto& [button, buttonData] : controller.ButtonStates)
		{
			if (buttonData.State == KeyState::Released)
			{
				buttonData.OldState = buttonData.State;
				buttonData.State    = KeyState::None;
			}
		}
	}
}
