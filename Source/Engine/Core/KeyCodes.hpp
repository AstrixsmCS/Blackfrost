#pragma once

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <ostream>

enum class KeyCode : uint16_t
{
	Space      = SDL_SCANCODE_SPACE,
	Apostrophe = SDL_SCANCODE_APOSTROPHE,
	Comma      = SDL_SCANCODE_COMMA,
	Minus      = SDL_SCANCODE_MINUS,
	Period     = SDL_SCANCODE_PERIOD,
	Slash      = SDL_SCANCODE_SLASH,

	D0 = SDL_SCANCODE_0,
	D1 = SDL_SCANCODE_1,
	D2 = SDL_SCANCODE_2,
	D3 = SDL_SCANCODE_3,
	D4 = SDL_SCANCODE_4,
	D5 = SDL_SCANCODE_5,
	D6 = SDL_SCANCODE_6,
	D7 = SDL_SCANCODE_7,
	D8 = SDL_SCANCODE_8,
	D9 = SDL_SCANCODE_9,

	Semicolon = SDL_SCANCODE_SEMICOLON,
	Equal     = SDL_SCANCODE_EQUALS,

	A = SDL_SCANCODE_A,
	B = SDL_SCANCODE_B,
	C = SDL_SCANCODE_C,
	D = SDL_SCANCODE_D,
	E = SDL_SCANCODE_E,
	F = SDL_SCANCODE_F,
	G = SDL_SCANCODE_G,
	H = SDL_SCANCODE_H,
	I = SDL_SCANCODE_I,
	J = SDL_SCANCODE_J,
	K = SDL_SCANCODE_K,
	L = SDL_SCANCODE_L,
	M = SDL_SCANCODE_M,
	N = SDL_SCANCODE_N,
	O = SDL_SCANCODE_O,
	P = SDL_SCANCODE_P,
	Q = SDL_SCANCODE_Q,
	R = SDL_SCANCODE_R,
	S = SDL_SCANCODE_S,
	T = SDL_SCANCODE_T,
	U = SDL_SCANCODE_U,
	V = SDL_SCANCODE_V,
	W = SDL_SCANCODE_W,
	X = SDL_SCANCODE_X,
	Y = SDL_SCANCODE_Y,
	Z = SDL_SCANCODE_Z,

	LeftBracket  = SDL_SCANCODE_LEFTBRACKET,
	Backslash    = SDL_SCANCODE_BACKSLASH,
	RightBracket = SDL_SCANCODE_RIGHTBRACKET,
	GraveAccent  = SDL_SCANCODE_GRAVE,

	World1 = SDL_SCANCODE_INTERNATIONAL1,
	World2 = SDL_SCANCODE_INTERNATIONAL2,

	// Function keys
	Escape      = SDL_SCANCODE_ESCAPE,
	Enter       = SDL_SCANCODE_RETURN,
	Tab         = SDL_SCANCODE_TAB,
	Backspace   = SDL_SCANCODE_BACKSPACE,
	Insert      = SDL_SCANCODE_INSERT,
	Delete      = SDL_SCANCODE_DELETE,
	Right       = SDL_SCANCODE_RIGHT,
	Left        = SDL_SCANCODE_LEFT,
	Down        = SDL_SCANCODE_DOWN,
	Up          = SDL_SCANCODE_UP,
	PageUp      = SDL_SCANCODE_PAGEUP,
	PageDown    = SDL_SCANCODE_PAGEDOWN,
	Home        = SDL_SCANCODE_HOME,
	End         = SDL_SCANCODE_END,
	CapsLock    = SDL_SCANCODE_CAPSLOCK,
	ScrollLock  = SDL_SCANCODE_SCROLLLOCK,
	NumLock     = SDL_SCANCODE_NUMLOCKCLEAR,
	PrintScreen = SDL_SCANCODE_PRINTSCREEN,
	Pause       = SDL_SCANCODE_PAUSE,

	F1  = SDL_SCANCODE_F1,
	F2  = SDL_SCANCODE_F2,
	F3  = SDL_SCANCODE_F3,
	F4  = SDL_SCANCODE_F4,
	F5  = SDL_SCANCODE_F5,
	F6  = SDL_SCANCODE_F6,
	F7  = SDL_SCANCODE_F7,
	F8  = SDL_SCANCODE_F8,
	F9  = SDL_SCANCODE_F9,
	F10 = SDL_SCANCODE_F10,
	F11 = SDL_SCANCODE_F11,
	F12 = SDL_SCANCODE_F12,
	F13 = SDL_SCANCODE_F13,
	F14 = SDL_SCANCODE_F14,
	F15 = SDL_SCANCODE_F15,
	F16 = SDL_SCANCODE_F16,
	F17 = SDL_SCANCODE_F17,
	F18 = SDL_SCANCODE_F18,
	F19 = SDL_SCANCODE_F19,
	F20 = SDL_SCANCODE_F20,
	F21 = SDL_SCANCODE_F21,
	F22 = SDL_SCANCODE_F22,
	F23 = SDL_SCANCODE_F23,
	F24 = SDL_SCANCODE_F24,

	// Keypad
	KP0        = SDL_SCANCODE_KP_0,
	KP1        = SDL_SCANCODE_KP_1,
	KP2        = SDL_SCANCODE_KP_2,
	KP3        = SDL_SCANCODE_KP_3,
	KP4        = SDL_SCANCODE_KP_4,
	KP5        = SDL_SCANCODE_KP_5,
	KP6        = SDL_SCANCODE_KP_6,
	KP7        = SDL_SCANCODE_KP_7,
	KP8        = SDL_SCANCODE_KP_8,
	KP9        = SDL_SCANCODE_KP_9,
	KPDecimal  = SDL_SCANCODE_KP_DECIMAL,
	KPDivide   = SDL_SCANCODE_KP_DIVIDE,
	KPMultiply = SDL_SCANCODE_KP_MULTIPLY,
	KPSubtract = SDL_SCANCODE_KP_MINUS,
	KPAdd      = SDL_SCANCODE_KP_PLUS,
	KPEnter    = SDL_SCANCODE_KP_ENTER,
	KPEqual    = SDL_SCANCODE_KP_EQUALS,

	LeftShift    = SDL_SCANCODE_LSHIFT,
	LeftControl  = SDL_SCANCODE_LCTRL,
	LeftAlt      = SDL_SCANCODE_LALT,
	LeftSuper    = SDL_SCANCODE_LGUI,
	RightShift   = SDL_SCANCODE_RSHIFT,
	RightControl = SDL_SCANCODE_RCTRL,
	RightAlt     = SDL_SCANCODE_RALT,
	RightSuper   = SDL_SCANCODE_RGUI,
	Menu         = SDL_SCANCODE_MENU
};

enum class KeyState
{
	None = -1,
	Pressed,
	Held,
	Released
};

enum class CursorMode
{
	Normal,
	Hidden,
	Locked
};

enum class MouseButton : uint16_t
{
	Button0 = SDL_BUTTON_LEFT,
	Button1 = SDL_BUTTON_RIGHT,
	Button2 = SDL_BUTTON_MIDDLE,
	Button3 = SDL_BUTTON_X1,
	Button4 = SDL_BUTTON_X2,

	Left   = Button0,
	Right  = Button1,
	Middle = Button2
};

inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
{
	return os << static_cast<uint16_t>(keyCode);
}

inline std::ostream& operator<<(std::ostream& os, MouseButton button)
{
	return os << static_cast<uint16_t>(button);
}

#define BF_KEY_SPACE      ::KeyCode::Space
#define BF_KEY_APOSTROPHE ::KeyCode::Apostrophe
#define BF_KEY_COMMA      ::KeyCode::Comma
#define BF_KEY_MINUS      ::KeyCode::Minus
#define BF_KEY_PERIOD     ::KeyCode::Period
#define BF_KEY_SLASH      ::KeyCode::Slash

#define BF_KEY_0 ::KeyCode::D0
#define BF_KEY_1 ::KeyCode::D1
#define BF_KEY_2 ::KeyCode::D2
#define BF_KEY_3 ::KeyCode::D3
#define BF_KEY_4 ::KeyCode::D4
#define BF_KEY_5 ::KeyCode::D5
#define BF_KEY_6 ::KeyCode::D6
#define BF_KEY_7 ::KeyCode::D7
#define BF_KEY_8 ::KeyCode::D8
#define BF_KEY_9 ::KeyCode::D9

#define BF_KEY_SEMICOLON ::KeyCode::Semicolon
#define BF_KEY_EQUAL     ::KeyCode::Equal

#define BF_KEY_A ::KeyCode::A
#define BF_KEY_B ::KeyCode::B
#define BF_KEY_C ::KeyCode::C
#define BF_KEY_D ::KeyCode::D
#define BF_KEY_E ::KeyCode::E
#define BF_KEY_F ::KeyCode::F
#define BF_KEY_G ::KeyCode::G
#define BF_KEY_H ::KeyCode::H
#define BF_KEY_I ::KeyCode::I
#define BF_KEY_J ::KeyCode::J
#define BF_KEY_K ::KeyCode::K
#define BF_KEY_L ::KeyCode::L
#define BF_KEY_M ::KeyCode::M
#define BF_KEY_N ::KeyCode::N
#define BF_KEY_O ::KeyCode::O
#define BF_KEY_P ::KeyCode::P
#define BF_KEY_Q ::KeyCode::Q
#define BF_KEY_R ::KeyCode::R
#define BF_KEY_S ::KeyCode::S
#define BF_KEY_T ::KeyCode::T
#define BF_KEY_U ::KeyCode::U
#define BF_KEY_V ::KeyCode::V
#define BF_KEY_W ::KeyCode::W
#define BF_KEY_X ::KeyCode::X
#define BF_KEY_Y ::KeyCode::Y
#define BF_KEY_Z ::KeyCode::Z

#define BF_KEY_LEFT_BRACKET  ::KeyCode::LeftBracket
#define BF_KEY_BACKSLASH     ::KeyCode::Backslash
#define BF_KEY_RIGHT_BRACKET ::KeyCode::RightBracket
#define BF_KEY_GRAVE_ACCENT  ::KeyCode::GraveAccent
#define BF_KEY_WORLD_1       ::KeyCode::World1
#define BF_KEY_WORLD_2       ::KeyCode::World2

#define BF_KEY_ESCAPE       ::KeyCode::Escape
#define BF_KEY_ENTER        ::KeyCode::Enter
#define BF_KEY_TAB          ::KeyCode::Tab
#define BF_KEY_BACKSPACE    ::KeyCode::Backspace
#define BF_KEY_INSERT       ::KeyCode::Insert
#define BF_KEY_DELETE       ::KeyCode::Delete
#define BF_KEY_RIGHT        ::KeyCode::Right
#define BF_KEY_LEFT         ::KeyCode::Left
#define BF_KEY_DOWN         ::KeyCode::Down
#define BF_KEY_UP           ::KeyCode::Up
#define BF_KEY_PAGE_UP      ::KeyCode::PageUp
#define BF_KEY_PAGE_DOWN    ::KeyCode::PageDown
#define BF_KEY_HOME         ::KeyCode::Home
#define BF_KEY_END          ::KeyCode::End
#define BF_KEY_CAPS_LOCK    ::KeyCode::CapsLock
#define BF_KEY_SCROLL_LOCK  ::KeyCode::ScrollLock
#define BF_KEY_NUM_LOCK     ::KeyCode::NumLock
#define BF_KEY_PRINT_SCREEN ::KeyCode::PrintScreen
#define BF_KEY_PAUSE        ::KeyCode::Pause

#define BF_KEY_F1  ::KeyCode::F1
#define BF_KEY_F2  ::KeyCode::F2
#define BF_KEY_F3  ::KeyCode::F3
#define BF_KEY_F4  ::KeyCode::F4
#define BF_KEY_F5  ::KeyCode::F5
#define BF_KEY_F6  ::KeyCode::F6
#define BF_KEY_F7  ::KeyCode::F7
#define BF_KEY_F8  ::KeyCode::F8
#define BF_KEY_F9  ::KeyCode::F9
#define BF_KEY_F10 ::KeyCode::F10
#define BF_KEY_F11 ::KeyCode::F11
#define BF_KEY_F12 ::KeyCode::F12
#define BF_KEY_F13 ::KeyCode::F13
#define BF_KEY_F14 ::KeyCode::F14
#define BF_KEY_F15 ::KeyCode::F15
#define BF_KEY_F16 ::KeyCode::F16
#define BF_KEY_F17 ::KeyCode::F17
#define BF_KEY_F18 ::KeyCode::F18
#define BF_KEY_F19 ::KeyCode::F19
#define BF_KEY_F20 ::KeyCode::F20
#define BF_KEY_F21 ::KeyCode::F21
#define BF_KEY_F22 ::KeyCode::F22
#define BF_KEY_F23 ::KeyCode::F23
#define BF_KEY_F24 ::KeyCode::F24

#define BF_KEY_KP_0        ::KeyCode::KP0
#define BF_KEY_KP_1        ::KeyCode::KP1
#define BF_KEY_KP_2        ::KeyCode::KP2
#define BF_KEY_KP_3        ::KeyCode::KP3
#define BF_KEY_KP_4        ::KeyCode::KP4
#define BF_KEY_KP_5        ::KeyCode::KP5
#define BF_KEY_KP_6        ::KeyCode::KP6
#define BF_KEY_KP_7        ::KeyCode::KP7
#define BF_KEY_KP_8        ::KeyCode::KP8
#define BF_KEY_KP_9        ::KeyCode::KP9
#define BF_KEY_KP_DECIMAL  ::KeyCode::KPDecimal
#define BF_KEY_KP_DIVIDE   ::KeyCode::KPDivide
#define BF_KEY_KP_MULTIPLY ::KeyCode::KPMultiply
#define BF_KEY_KP_SUBTRACT ::KeyCode::KPSubtract
#define BF_KEY_KP_ADD      ::KeyCode::KPAdd
#define BF_KEY_KP_ENTER    ::KeyCode::KPEnter
#define BF_KEY_KP_EQUAL    ::KeyCode::KPEqual

#define BF_KEY_LEFT_SHIFT    ::KeyCode::LeftShift
#define BF_KEY_LEFT_CONTROL  ::KeyCode::LeftControl
#define BF_KEY_LEFT_ALT      ::KeyCode::LeftAlt
#define BF_KEY_LEFT_SUPER    ::KeyCode::LeftSuper
#define BF_KEY_RIGHT_SHIFT   ::KeyCode::RightShift
#define BF_KEY_RIGHT_CONTROL ::KeyCode::RightControl
#define BF_KEY_RIGHT_ALT     ::KeyCode::RightAlt
#define BF_KEY_RIGHT_SUPER   ::KeyCode::RightSuper
#define BF_KEY_MENU          ::KeyCode::Menu

#define BF_MOUSE_BUTTON_LEFT   ::MouseButton::Left
#define BF_MOUSE_BUTTON_RIGHT  ::MouseButton::Right
#define BF_MOUSE_BUTTON_MIDDLE ::MouseButton::Middle
