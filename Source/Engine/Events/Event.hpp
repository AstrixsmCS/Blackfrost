#pragma once

#include "Core/Base.hpp"

#include <cstdint>

enum EventCategory
{
	EventCategoryNone        = 0,
	EventCategoryApplication = BIT(0),
	EventCategoryInput       = BIT(1),
	EventCategoryKeyboard    = BIT(2),
	EventCategoryMouse       = BIT(3),
	EventCategoryMouseButton = BIT(4)
};

class Event
{
public:
	virtual ~Event()                             = default;
	virtual const char* GetName() const          = 0;
	virtual uint32_t    GetCategoryFlags() const = 0;

	bool IsInCategory(EventCategory category) const { return GetCategoryFlags() & category; }

	bool m_Handled = false;
};
