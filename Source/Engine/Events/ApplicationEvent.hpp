#pragma once

#include "Event.hpp"

// TODO: Should this store previous size?
class WindowResizeEvent : public Event
{
public:
	WindowResizeEvent(uint32_t width, uint32_t height) : m_Width(width), m_Height(height) {}

	inline uint32_t GetWidth() const { return m_Width; }
	inline uint32_t GetHeight() const { return m_Height; }

	virtual const char* GetName() const override { return "WindowResizeEvent"; }
	virtual uint32_t    GetCategoryFlags() const override { return EventCategoryApplication; }

	static const char* GetStaticName() { return "WindowResizeEvent"; }

private:
	uint32_t m_Width, m_Height;
};

class WindowMinimizeEvent : public Event
{
public:
	WindowMinimizeEvent(bool minimized) : m_Minimized(minimized) {}

	bool IsMinimized() const { return m_Minimized; }

	virtual const char* GetName() const override { return "WindowMinimizeEvent"; }
	virtual uint32_t    GetCategoryFlags() const override { return EventCategoryApplication; }

	static const char* GetStaticName() { return "WindowMinimizeEvent"; }

private:
	bool m_Minimized = false;
};

class WindowCloseEvent : public Event
{
public:
	WindowCloseEvent() {}

	virtual const char* GetName() const override { return "WindowCloseEvent"; }
	virtual uint32_t    GetCategoryFlags() const override { return EventCategoryApplication; }

	static const char* GetStaticName() { return "WindowCloseEvent"; }
};
