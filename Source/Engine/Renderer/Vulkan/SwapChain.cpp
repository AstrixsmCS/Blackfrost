#include "SwapChain.hpp"

#include "VulkanUtils.hpp"

#include "Context.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_vulkan.h>

#include <vma/vk_mem_alloc.h>

#include <algorithm>
#include <cassert>
#include <print>

SwapChain::SwapChain(SDL_Window* windowHandle)
	: m_WindowHandle(windowHandle)
{
}

SwapChain::~SwapChain()
{
	Cleanup();
	vkDestroySurfaceKHR(Context::Get().GetInstance(), m_Surface, nullptr);
}

void SwapChain::Initialize()
{
	assert(m_WindowHandle && "SwapChain window handle is null!");

	CreateSurface();

	// Block on (0,0) extent (launched-minimized); Vulkan rejects zero-sized swapchains.
	int w = 0, h = 0;
	while (w == 0 || h == 0)
	{
		SDL_GetWindowSizeInPixels(m_WindowHandle, &w, &h);
		if (w == 0 || h == 0)
			SDL_WaitEvent(nullptr);
	}

	uint32_t width  = static_cast<uint32_t>(w);
	uint32_t height = static_cast<uint32_t>(h);

	CreateSwapchain(&width, &height);
	CreateImageViews();
	CreateSemaphores();
}

void SwapChain::Cleanup()
{
	DestroySemaphores();

	VkDevice device = Context::Get().GetDevice();

	for (auto& image : m_Images)
	{
		if (image.ImageView)
		{
			vkDestroyImageView(device, image.ImageView, nullptr);
			image.ImageView = VK_NULL_HANDLE;
		}
	}
	m_Images.clear();

	if (m_SwapChain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device, m_SwapChain, nullptr);
		m_SwapChain = VK_NULL_HANDLE;
	}
}

void SwapChain::CreateSemaphores()
{
	VkDevice                    device = Context::Get().GetDevice();
	const VkSemaphoreCreateInfo info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	for (VkSemaphore& sem : m_ImageAvailableSemaphores)
		VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &sem));

	m_RenderFinishedSemaphores.resize(m_Images.size());
	for (VkSemaphore& sem : m_RenderFinishedSemaphores)
		VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &sem));
}

void SwapChain::DestroySemaphores()
{
	VkDevice device = Context::Get().GetDevice();

	for (VkSemaphore& sem : m_ImageAvailableSemaphores)
	{
		if (sem != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, sem, nullptr);
			sem = VK_NULL_HANDLE;
		}
	}

	for (VkSemaphore sem : m_RenderFinishedSemaphores)
		if (sem != VK_NULL_HANDLE)
			vkDestroySemaphore(device, sem, nullptr);

	m_RenderFinishedSemaphores.clear();
}

void SwapChain::OnResize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return;

	VkSurfaceCapabilitiesKHR capabilities{};
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Context::Get().GetPhysicalDevice(), m_Surface, &capabilities));

	if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0)
	{
		m_NeedsResize = true;
		return;
	}

	vkDeviceWaitIdle(Context::Get().GetDevice());

	// Destroy image views but keep the swapchain handle alive
	// so it can be passed as oldSwapchain
	for (auto& image : m_Images)
	{
		vkDestroyImageView(Context::Get().GetDevice(), image.ImageView, nullptr);
		image.ImageView = VK_NULL_HANDLE;
	}
	m_Images.clear();

	VkSwapchainKHR oldSwapchain = m_SwapChain;
	m_SwapChain                 = VK_NULL_HANDLE;

	CreateSwapchain(&width, &height, oldSwapchain);
	CreateImageViews();

	if (oldSwapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(Context::Get().GetDevice(), oldSwapchain, nullptr);

	DestroySemaphores();
	CreateSemaphores();

	m_NeedsResize = false;
}

void SwapChain::CreateSurface()
{
	VkPhysicalDevice physicalDevice = Context::Get().GetPhysicalDevice();

	if (!SDL_Vulkan_CreateSurface(m_WindowHandle, Context::Get().GetInstance(), nullptr, &m_Surface))
		throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());

	VkBool32 presentSupport = VK_FALSE;

	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, static_cast<uint32_t>(Context::Get().GetGraphicsFamily()), m_Surface, &presentSupport));

	if (!presentSupport)
		throw std::runtime_error("Graphics queue family does not support presentation on this surface!");

	FindImageFormatAndColorSpace();
}

void SwapChain::CreateSwapchain(uint32_t* width, uint32_t* height, VkSwapchainKHR oldSwapchain)
{
	VkPhysicalDevice physicalDevice = Context::Get().GetPhysicalDevice();
	VkDevice         device         = Context::Get().GetDevice();

	VkSurfaceCapabilitiesKHR capabilities{};
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &capabilities));

	// === Present Mode ===

	uint32_t presentModeCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr));
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, presentModes.data()));

	PresentMode preferredPresentMode = PresentMode::Mailbox;

	VkPresentModeKHR presentMode = ToVulkan(PresentMode::FIFO);

	const VkPresentModeKHR preferred = ToVulkan(preferredPresentMode);

	if (std::ranges::find(presentModes, preferred) != presentModes.end())
		presentMode = preferred;

	// === Extent ===

	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		m_Extent = capabilities.currentExtent;

		*width  = m_Extent.width;
		*height = m_Extent.height;
	}
	else
	{
		m_Extent = {
			.width  = std::clamp(*width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			.height = std::clamp(*height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};

		*width  = m_Extent.width;
		*height = m_Extent.height;
	}

	if (*width == 0 || *height == 0)
		return;

	// === Image Count ===

	uint32_t imageCount = std::max(2u, capabilities.minImageCount + 1);

	if (capabilities.maxImageCount > 0)
		imageCount = std::min(imageCount, capabilities.maxImageCount);

	// === Composite Alpha ===

	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	constexpr VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[]
	{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for (VkCompositeAlphaFlagBitsKHR flag : compositeAlphaFlags)
	{
		if (capabilities.supportedCompositeAlpha & flag)
		{
			compositeAlpha = flag;
			break;
		}
	}

	// === Swapchain ===

	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// Useful for blitting/clearing to the swapchain.
	if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkSwapchainCreateInfoKHR swapchainInfo
	{
		.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface          = m_Surface,
		.minImageCount    = imageCount,
		.imageFormat      = m_ColorFormat,
		.imageColorSpace  = m_ColorSpace,
		.imageExtent      = m_Extent,
		.imageArrayLayers = 1,
		.imageUsage       = imageUsage,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform     = capabilities.currentTransform,
		.compositeAlpha   = compositeAlpha,
		.presentMode      = presentMode,
		.clipped          = VK_TRUE,
		.oldSwapchain     = oldSwapchain
	};

	VK_CHECK(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &m_SwapChain));

	// === Images ===

	VK_CHECK(vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, nullptr));
	std::vector<VkImage> images(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, images.data()));
	m_Images.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		m_Images[i].Image = images[i];
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE, std::format("Swapchain Image {}", i), m_Images[i].Image);
	}
}

void SwapChain::CreateImageViews()
{
	VkDevice device = Context::Get().GetDevice();

	for (uint32_t i = 0; i < static_cast<uint32_t>(m_Images.size()); ++i)
	{
		SwapchainImage& image = m_Images[i];

		VkImageViewCreateInfo createInfo
		{
			.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image      = image.Image,
			.viewType   = VK_IMAGE_VIEW_TYPE_2D,
			.format     = m_ColorFormat,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY },
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
		};

		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &image.ImageView));
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, std::format("Swapchain Image View {}", i), image.ImageView);
	}
}

uint32_t SwapChain::AcquireNextImage(uint32_t frameSlot)
{
	if (m_NeedsResize)
	{
		int w = 0, h = 0;
		SDL_GetWindowSizeInPixels(m_WindowHandle, &w, &h);
		if (w > 0 && h > 0)
			OnResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
		return UINT32_MAX;
	}

	VkResult result = vkAcquireNextImageKHR(Context::Get().GetDevice(), m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphores[frameSlot], VK_NULL_HANDLE, &m_CurrentImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		int w = 0, h = 0;
		SDL_GetWindowSizeInPixels(m_WindowHandle, &w, &h);
		if (w > 0 && h > 0)
			OnResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
		return UINT32_MAX;
	}

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		VK_CHECK(result);
		return UINT32_MAX;
	}

	return m_CurrentImageIndex;
}

void SwapChain::Present(uint32_t frameSlot)
{
	(void)frameSlot; // available for future per-slot present fences

	VkSemaphore renderFinished = m_RenderFinishedSemaphores[m_CurrentImageIndex];

	VkPresentInfoKHR presentInfo
	{
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores    = &renderFinished,
		.swapchainCount     = 1,
		.pSwapchains        = &m_SwapChain,
		.pImageIndices      = &m_CurrentImageIndex
	};

	VkResult result = vkQueuePresentKHR(Context::Get().GetGraphicsQueue(), &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		m_NeedsResize = true;
	else if (result != VK_SUCCESS)
		VK_CHECK(result);
}

void SwapChain::FindImageFormatAndColorSpace()
{
	const VkPhysicalDevice physicalDevice = Context::Get().GetPhysicalDevice();

	uint32_t formatCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr));

	assert(formatCount > 0);

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, surfaceFormats.data()));

	surfaceFormats.resize(formatCount);

	struct PreferredFormat
	{
		VkFormat VulkanFormat;
		Format   RendererFormat;
	};

	constexpr PreferredFormat preferredFormats[] = {
		{ VK_FORMAT_B8G8R8A8_UNORM, Format::BGRA8_UNorm },
		{ VK_FORMAT_R8G8B8A8_UNORM, Format::RGBA8_UNorm },
	};

	for (const auto& preferred : preferredFormats)
	{
		for (const auto& available : surfaceFormats)
		{
			const bool unrestricted = surfaceFormats.size() == 1 && available.format == VK_FORMAT_UNDEFINED;

			if (available.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				continue;

			if (!unrestricted && available.format != preferred.VulkanFormat)
				continue;

			m_ColorFormat = preferred.VulkanFormat;
			m_Format      = preferred.RendererFormat;
			m_ColorSpace  = available.colorSpace;

			std::println("[SwapChain] Format: {}, Color space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR", m_ColorFormat == VK_FORMAT_B8G8R8A8_UNORM ? "VK_FORMAT_B8G8R8A8_UNORM" : "VK_FORMAT_R8G8B8A8_UNORM");
			return;
		}
	}

	std::println("[SwapChain] No supported sRGB surface format found.");
	std::abort();
}
