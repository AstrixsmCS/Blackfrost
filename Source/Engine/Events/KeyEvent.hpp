#pragma once

#include "Core/KeyCodes.hpp"
#include "Event.hpp"

class KeyEvent : public Event
{
public:
	inline KeyCode GetKeyCode() const { return m_KeyCode; }

	virtual uint32_t GetCategoryFlags() const override { return EventCategoryKeyboard | EventCategoryInput; }

protected:
	KeyEvent(KeyCode keycode) : m_KeyCode(keycode) {}
	KeyCode m_KeyCode;
};

class KeyPressedEvent : public KeyEvent
{
public:
	KeyPressedEvent(KeyCode keycode, int repeatCount) : KeyEvent(keycode), m_RepeatCount(repeatCount) {}

	inline int GetRepeatCount() const { return m_RepeatCount; }

	virtual const char* GetName() const override { return "KeyPressedEvent"; }
	static const char*  GetStaticName() { return "KeyPressedEvent"; }

private:
	int m_RepeatCount;
};

class KeyReleasedEvent : public KeyEvent
{
public:
	KeyReleasedEvent(KeyCode keycode) : KeyEvent(keycode) {}

	virtual const char* GetName() const override { return "KeyReleasedEvent"; }
	static const char*  GetStaticName() { return "KeyReleasedEvent"; }
};

class KeyTypedEvent : public Event
{
public:
	explicit KeyTypedEvent(uint32_t codepoint)
		: m_Codepoint(codepoint)
	{
	}

	uint32_t GetCodepoint() const { return m_Codepoint; }

	const char*        GetName() const override { return "KeyTypedEvent"; }
	uint32_t           GetCategoryFlags() const override { return EventCategoryKeyboard | EventCategoryInput; }
	static const char* GetStaticName() { return "KeyTypedEvent"; }

private:
	uint32_t m_Codepoint;
};
