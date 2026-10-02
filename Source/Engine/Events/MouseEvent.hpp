#pragma once

#include "Core/KeyCodes.hpp"
#include "Event.hpp"

class MouseMovedEvent : public Event
{
public:
	MouseMovedEvent(float x, float y) : m_MouseX(x), m_MouseY(y) {}

	inline float GetX() const { return m_MouseX; }
	inline float GetY() const { return m_MouseY; }

	virtual const char* GetName() const override { return "MouseMovedEvent"; }
	virtual uint32_t    GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryInput; }

private:
	float m_MouseX, m_MouseY;
};

class MouseScrolledEvent : public Event
{
public:
	MouseScrolledEvent(float xOffset, float yOffset) : m_XOffset(xOffset), m_YOffset(yOffset) {}

	inline float GetXOffset() const { return m_XOffset; }
	inline float GetYOffset() const { return m_YOffset; }

	virtual const char* GetName() const override { return "MouseScrolledEvent"; }
	virtual uint32_t    GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryInput; }
	static const char*  GetStaticName() { return "MouseScrolledEvent"; }

private:
	float m_XOffset, m_YOffset;
};

class MouseButtonEvent : public Event
{
public:
	inline MouseButton GetMouseButton() const { return m_Button; }

	virtual uint32_t GetCategoryFlags() const override { return EventCategoryMouse | EventCategoryMouseButton | EventCategoryInput; }

protected:
	MouseButtonEvent(MouseButton button) : m_Button(button) {}

	MouseButton m_Button;
};

class MouseButtonPressedEvent : public MouseButtonEvent
{
public:
	MouseButtonPressedEvent(MouseButton button) : MouseButtonEvent(button) {}

	virtual const char* GetName() const override { return "MouseButtonPressedEvent"; }
	static const char*  GetStaticName() { return "MouseButtonPressedEvent"; }
};

class MouseButtonReleasedEvent : public MouseButtonEvent
{
public:
	MouseButtonReleasedEvent(MouseButton button) : MouseButtonEvent(button) {}

	virtual const char* GetName() const override { return "MouseButtonReleasedEvent"; }
	static const char*  GetStaticName() { return "MouseButtonReleasedEvent"; }
};
