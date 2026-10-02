#pragma once

#include <volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include <cassert>
#include <chrono>
#include <print>
#include <source_location>
#include <thread>

inline void VulkanCheckResult(VkResult result, std::source_location location = std::source_location::current())
{
	if (result == VK_SUCCESS)
		return;

	BF_ERROR("Vulkan call failed with {} at {}:{}", string_VkResult(result), location.file_name(), location.line());

	assert(false && "VK_CHECK failed");
	std::abort();
}

#define VK_CHECK(f)             \
	do                          \
	{                           \
		VulkanCheckResult((f)); \
	} while (0)

inline void SetDebugUtilsObjectName(VkDevice device, VkObjectType objectType, const std::string& name, const void* handle)
{
	if (!vkSetDebugUtilsObjectNameEXT)
		return;

	const VkDebugUtilsObjectNameInfoEXT nameInfo{
		.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType   = objectType,
		.objectHandle = reinterpret_cast<uint64_t>(handle),
		.pObjectName  = name.c_str(),
	};

	VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &nameInfo));
}
