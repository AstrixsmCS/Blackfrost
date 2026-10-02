#pragma once

#include "Vulkan.hpp"

class TimelineSemaphore
{
public:
	void Initialize(uint64_t initialValue = 0);
	void Shutdown();

	void Signal(uint64_t value);
	void Wait(uint64_t value, uint64_t timeout = UINT64_MAX);

	uint64_t    GetValue() const;
	VkSemaphore GetHandle() const { return m_Semaphore; }

private:
	VkSemaphore m_Semaphore = VK_NULL_HANDLE;
};
