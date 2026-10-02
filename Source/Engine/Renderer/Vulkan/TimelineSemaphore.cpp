#include "TimelineSemaphore.hpp"

#include "Context.hpp"

#include <stdexcept>

void TimelineSemaphore::Initialize(uint64_t initialValue)
{
	VkSemaphoreTypeCreateInfo typeCreateInfo = {
		.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue  = initialValue,
	};

	VkSemaphoreCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &typeCreateInfo,
	};

	VK_CHECK(vkCreateSemaphore(Context::Get().GetDevice(), &createInfo, nullptr, &m_Semaphore));
}

void TimelineSemaphore::Shutdown()
{
	if (m_Semaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(Context::Get().GetDevice(), m_Semaphore, nullptr);
		m_Semaphore = VK_NULL_HANDLE;
	}
}

void TimelineSemaphore::Signal(uint64_t value)
{
	VkSemaphoreSignalInfo signalInfo = {
		.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
		.semaphore = m_Semaphore,
		.value     = value,
	};

	VK_CHECK(vkSignalSemaphore(Context::Get().GetDevice(), &signalInfo));
}

void TimelineSemaphore::Wait(uint64_t value, uint64_t timeout)
{
	VkSemaphoreWaitInfo waitInfo = {
		.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores    = &m_Semaphore,
		.pValues        = &value
	};
	VK_CHECK(vkWaitSemaphores(Context::Get().GetDevice(), &waitInfo, timeout));
}

uint64_t TimelineSemaphore::GetValue() const
{
	uint64_t value = 0;
	VK_CHECK(vkGetSemaphoreCounterValue(Context::Get().GetDevice(), m_Semaphore, &value));

	return value;
}
