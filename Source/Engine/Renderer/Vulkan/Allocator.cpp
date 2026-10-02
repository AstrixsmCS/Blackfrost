#include "Allocator.hpp"

#include "Context.hpp"

#include "Vulkan.hpp"

#include <vma/vk_mem_alloc.h>

VmaAllocator Allocator::s_VmaAllocator = VK_NULL_HANDLE;

void Allocator::Initialize()
{
	auto& context = Context::Get();

	VmaVulkanFunctions     vmaFuncInfo{};
	VmaAllocatorCreateInfo vmaAllocInfo
	{
		.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice   = context.GetPhysicalDevice(),
		.device           = context.GetDevice(),
		.pVulkanFunctions = &vmaFuncInfo,
		.instance         = context.GetInstance(),
		.vulkanApiVersion = VK_API_VERSION_1_4
	};

	vmaImportVulkanFunctionsFromVolk(&vmaAllocInfo, &vmaFuncInfo);

	VK_CHECK(vmaCreateAllocator(&vmaAllocInfo, &s_VmaAllocator));
}

void Allocator::Shutdown()
{
	vmaDestroyAllocator(s_VmaAllocator);
	s_VmaAllocator = VK_NULL_HANDLE;
}
