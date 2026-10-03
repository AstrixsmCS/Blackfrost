#include "Texture.hpp"

#include "Allocator.hpp"
#include "CommandBuffer.hpp"
#include "Context.hpp"
#include "UploadContext.hpp"

#include <stb/stb_image.h>

#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>
#include <print>
#include <vector>

static VkComponentSwizzle ToVulkan(Swizzle swizzle)
{
	switch (swizzle)
	{
		case Swizzle::Default: return VK_COMPONENT_SWIZZLE_IDENTITY;
		case Swizzle::Zero: return VK_COMPONENT_SWIZZLE_ZERO;
		case Swizzle::One: return VK_COMPONENT_SWIZZLE_ONE;
		case Swizzle::R: return VK_COMPONENT_SWIZZLE_R;
		case Swizzle::G: return VK_COMPONENT_SWIZZLE_G;
		case Swizzle::B: return VK_COMPONENT_SWIZZLE_B;
		case Swizzle::A: return VK_COMPONENT_SWIZZLE_A;
	}
	return VK_COMPONENT_SWIZZLE_IDENTITY;
}

VkImageView VulkanImage::CreateView(VkDevice           device,
									VkImageViewType    viewType,
									VkFormat           format,
									VkImageAspectFlags aspectMask,
									uint32_t           baseMip,
									uint32_t           mipCount,
									uint32_t           baseLayer,
									uint32_t           layerCount,
									ComponentMapping   components,
									const char*        debugName) const
{
	const VkImageViewCreateInfo info
	{
		.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image      = Image,
		.viewType   = viewType,
		.format     = format,
		.components = {
			.r = ToVulkan(components.R),
			.g = ToVulkan(components.G),
			.b = ToVulkan(components.B),
			.a = ToVulkan(components.A) },
		.subresourceRange = { .aspectMask = aspectMask, .baseMipLevel = baseMip, .levelCount = mipCount, .baseArrayLayer = baseLayer, .layerCount = layerCount }
	};

	VkImageView view = VK_NULL_HANDLE;
	VK_CHECK(vkCreateImageView(device, &info, nullptr, &view));

	if (debugName)
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, debugName, view);

	return view;
}

VkImageView VulkanImage::GetOrCreateMipLayerView(VkDevice device, uint32_t mip, uint32_t layer, const char* debugName)
{
	assert(mip < MAX_MIP_LEVELS);
	assert(layer < MAX_CUBE_FACES);

	if (MipLayerViews[mip][layer] != VK_NULL_HANDLE)
		return MipLayerViews[mip][layer];

	// Attachment views of depth/stencil formats cover both aspects so the view can be
	// bound as the depth attachment, the stencil attachment, or both.
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	if (IsDepth)
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT | (IsStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
	else if (IsStencil)
		aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

	MipLayerViews[mip][layer] = CreateView(device, VK_IMAGE_VIEW_TYPE_2D, Format, aspect, mip, 1, layer, 1, {}, debugName);

	return MipLayerViews[mip][layer];
}

bool VulkanImage::IsDepthFormat(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
			return true;
		default:
			return false;
	}
}

bool VulkanImage::IsStencilFormat(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default:
			return false;
	}
}

void Texture::Create(const TextureSpecification& specification)
{
	assert(specification.Size.width > 0);
	assert(specification.Size.height > 0);
	assert(specification.Size.depth > 0);
	assert(specification.NumMipLevels > 0);
	assert(specification.Usage != 0);

	assert(specification.Type != TextureType::Texture3D || specification.NumLayers == 1);
	assert(specification.Type != TextureType::TextureCube || specification.Size.width == specification.Size.height);

	Destroy();

	m_Specification = specification;
	m_OwnsImage     = true;

	VkDevice device = Context::Get().GetDevice();

	const VkFormat vkFormat    = specification.Format;
	const uint32_t arrayLayers = (specification.Type == TextureType::TextureCube) ? specification.NumLayers * 6 : specification.NumLayers;

	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
	switch (specification.Type)
	{
		case TextureType::Texture2D:
			viewType = arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			break;
		case TextureType::Texture3D:
			viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		case TextureType::TextureCube:
			viewType = arrayLayers > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			break;
	}

	const uint32_t depth = (specification.Type == TextureType::Texture3D) ? specification.Size.depth : 1u;

	const uint32_t mipLevels = specification.GenerateMips ? CalcMipCount(specification.Size.width, specification.Size.height, depth) : specification.NumMipLevels;

	assert(mipLevels <= MAX_MIP_LEVELS);
	assert(arrayLayers <= MAX_CUBE_FACES || specification.Type != TextureType::TextureCube);

	VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if (specification.Usage & TextureUsageBits_Sampled)
		usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if (specification.Usage & TextureUsageBits_Storage)
		usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

	if (specification.Usage & TextureUsageBits_Attachment)
	{
		usageFlags |= VulkanImage::IsDepthFormat(vkFormat) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	if (specification.Usage & TextureUsageBits_InputAttachment)
		usageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	const VkImageCreateInfo imageInfo
	{
		.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags         = (specification.Type == TextureType::TextureCube) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : VkImageCreateFlags(0),
		.imageType     = (specification.Type == TextureType::Texture3D) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
		.format        = vkFormat,
		.extent        = { specification.Size.width, specification.Size.height, depth },
		.mipLevels     = mipLevels,
		.arrayLayers   = arrayLayers,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.tiling        = VK_IMAGE_TILING_OPTIMAL,
		.usage         = usageFlags,
		.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	const VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };

	VK_CHECK(vmaCreateImage(Allocator::GetAllocator(), &imageInfo, &allocInfo, &m_Image.Image, &m_Image.Allocation, nullptr));

	m_Image.UsageFlags  = usageFlags;
	m_Image.Format      = vkFormat;
	m_Image.Extent      = imageInfo.extent;
	m_Image.MipLevels   = mipLevels;
	m_Image.ArrayLayers = arrayLayers;
	m_Image.IsDepth     = VulkanImage::IsDepthFormat(vkFormat);
	m_Image.IsStencil   = VulkanImage::IsStencilFormat(vkFormat);

	if (!specification.DebugName.empty())
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE, specification.DebugName, m_Image.Image);

	const VkImageAspectFlags aspect = m_Image.IsDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : m_Image.IsStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	// Default view uses the spec's component mapping
	m_DefaultView = m_Image.CreateView(
		device, viewType, vkFormat, aspect,
		0, VK_REMAINING_MIP_LEVELS,
		0, VK_REMAINING_ARRAY_LAYERS,
		specification.Components,
		specification.DebugName.empty() ? nullptr : (specification.DebugName + " view").c_str());

	// Storage view always identity swizzle
	if (usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
	{
		m_StorageView = m_Image.CreateView(
			device, viewType, vkFormat, aspect,
			0, VK_REMAINING_MIP_LEVELS,
			0, VK_REMAINING_ARRAY_LAYERS,
			{},
			specification.DebugName.empty() ? nullptr : (specification.DebugName + " storage view").c_str());
	}

	if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
		m_SampledSlot = BindlessSlot(BindlessType::SampledImage, m_DefaultView);

	if (usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
		m_StorageSlot = BindlessSlot(BindlessType::StorageImage, m_StorageView);

	if (specification.Data)
	{
		const size_t byteSize = static_cast<size_t>(specification.Size.width) * specification.Size.height * depth * arrayLayers * GetFormatBytesPerPixel(specification.Format);
		SetData(specification.Data, byteSize);

		if (mipLevels > 1)
			GenerateMips();
	}
}

void Texture::CreateView(const Texture& source, const TextureViewSpecification& viewSpecification, const std::string& debugName)
{
	assert(source.IsValid());

	Destroy();

	m_Image            = source.m_Image;
	m_Image.Allocation = VK_NULL_HANDLE;
	m_OwnsImage        = false;

	// Cached attachment views belong to the source texture. This texture creates and destroys its own.
	for (auto& mipRow : m_Image.MipLayerViews)
		std::ranges::fill(mipRow, VK_NULL_HANDLE);
	m_Specification = source.m_Specification;

	VkDevice device = Context::Get().GetDevice();

	const uint32_t arrayLayers = (viewSpecification.Type == TextureType::TextureCube) ? viewSpecification.NumLayers * 6 : viewSpecification.NumLayers;

	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
	switch (viewSpecification.Type)
	{
		case TextureType::Texture2D:
			viewType = arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			break;
		case TextureType::Texture3D:
			viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		case TextureType::TextureCube:
			viewType = arrayLayers > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			break;
	}

	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	switch (viewSpecification.Aspect)
	{
		case TextureAspect::Depth: aspect = VK_IMAGE_ASPECT_DEPTH_BIT; break;
		case TextureAspect::Stencil: aspect = VK_IMAGE_ASPECT_STENCIL_BIT; break;
		default:
			if (m_Image.IsDepth)
				aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			else if (m_Image.IsStencil)
				aspect = VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
	}

	// Default view uses the view spec's component mapping
	m_DefaultView = m_Image.CreateView(
		device, viewType, m_Image.Format, aspect,
		viewSpecification.MipLevel, viewSpecification.NumMipLevels,
		viewSpecification.Layer, arrayLayers,
		viewSpecification.Components,
		debugName.empty() ? nullptr : debugName.c_str());

	// Storage view always identity swizzle
	if (m_Image.UsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
	{
		m_StorageView = m_Image.CreateView(
			device, viewType, m_Image.Format, aspect,
			viewSpecification.MipLevel, viewSpecification.NumMipLevels,
			viewSpecification.Layer, arrayLayers,
			{},
			debugName.empty() ? nullptr : (debugName + " storage").c_str());
	}

	if (m_Image.UsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
		m_SampledSlot = BindlessSlot(BindlessType::SampledImage, m_DefaultView);

	if (m_Image.UsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
		m_StorageSlot = BindlessSlot(BindlessType::StorageImage, m_StorageView);
}

bool Texture::Load(const std::filesystem::path& path, bool sRGB)
{
	const std::string pathString = path.string();

	int w = 0, h = 0, channels = 0;

	// ==== HDR ====
	if (stbi_is_hdr(pathString.c_str()))
	{
		float* pixels = stbi_loadf(pathString.c_str(), &w, &h, &channels, STBI_rgb_alpha);

		if (!pixels || w <= 0 || h <= 0)
		{
			std::println("[Texture] stbi_loadf failed for '{}': {}", pathString, stbi_failure_reason());
			if (pixels)
				stbi_image_free(pixels);
			return false;
		}

		constexpr float HALF_MAX = 65504.0f;

		const size_t texelCount = static_cast<size_t>(w) * static_cast<size_t>(h);

		std::vector<uint64_t> halfPixels(texelCount); // 4 x float16 per texel, R in the low bits

		for (size_t i = 0; i < texelCount; ++i)
		{
			const float* texel = pixels + i * 4;

			const glm::vec4 color{
				std::min(texel[0], HALF_MAX),
				std::min(texel[1], HALF_MAX),
				std::min(texel[2], HALF_MAX),
				std::min(texel[3], HALF_MAX)
			};

			halfPixels[i] = glm::packHalf4x16(color);
		}

		stbi_image_free(pixels);

		Create(
		{
			.Type      = TextureType::Texture2D,
			.Format    = VK_FORMAT_R16G16B16A16_SFLOAT,
			.Size      = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 },
			.Data      = halfPixels.data(),
			.DebugName = path.filename().string()
		});

		return IsValid();
	}

	// ==== LDR (png, jpg, tga, ...) ====
	stbi_uc* pixels = stbi_load(pathString.c_str(), &w, &h, &channels, STBI_rgb_alpha);

	if (!pixels || w <= 0 || h <= 0)
	{
		std::println("[Texture] stbi_load failed for '{}': {}", pathString, stbi_failure_reason());
		if (pixels)
			stbi_image_free(pixels);
		return false;
	}

	Create(
	{
		.Type         = TextureType::Texture2D,
		.Format       = sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
		.Size         = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 },
		.Data         = pixels,
		.GenerateMips = true,
		.DebugName    = path.filename().string()
	});

	stbi_image_free(pixels);
	return IsValid();
}

void Texture::Destroy()
{
	if (!m_Image.IsValid())
		return;

	VkDevice device = Context::Get().GetDevice();

	m_StorageSlot.Reset();
	m_SampledSlot.Reset();

	for (uint32_t mip = 0; mip < MAX_MIP_LEVELS; ++mip)
		for (uint32_t layer = 0; layer < MAX_CUBE_FACES; ++layer)
			if (m_Image.MipLayerViews[mip][layer] != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, m_Image.MipLayerViews[mip][layer], nullptr);
				m_Image.MipLayerViews[mip][layer] = VK_NULL_HANDLE;
			}

	if (m_StorageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, m_StorageView, nullptr);
		m_StorageView = VK_NULL_HANDLE;
	}

	if (m_DefaultView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, m_DefaultView, nullptr);
		m_DefaultView = VK_NULL_HANDLE;
	}

	if (m_OwnsImage)
		vmaDestroyImage(Allocator::GetAllocator(), m_Image.Image, m_Image.Allocation);

	m_Image         = {};
	m_Specification = {};
}

void Texture::SetData(const void* data, size_t size)
{
	assert(data && size > 0);
	assert(!m_Image.IsDepth && !m_Image.IsStencil && "Uploading depth/stencil data is not supported");

	const VkImageSubresourceRange mipZero
	{
		.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel   = 0,
		.levelCount     = 1,
		.baseArrayLayer = 0,
		.layerCount     = m_Image.ArrayLayers
	};

	const VkBufferImageCopy2 region
	{
		.sType            = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.imageSubresource = {
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel       = 0,
			.baseArrayLayer = 0,
			.layerCount     = m_Image.ArrayLayers },
		.imageExtent = m_Image.Extent
	};

	const VkImageLayout finalLayout = m_Image.MipLevels > 1 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	UploadContext::Get().UploadImage(m_Image.Image, data, static_cast<VkDeviceSize>(size), region, mipZero, finalLayout);
}

void Texture::GenerateMips()
{
	assert(m_OwnsImage && "Generate mips on the owning texture, not on a view");
	assert(m_Image.MipLevels > 1);
	assert(!m_Image.IsDepth && !m_Image.IsStencil && "Depth/stencil images cannot be blitted with filtering");

	VkFormatProperties formatProperties{};
	vkGetPhysicalDeviceFormatProperties(Context::Get().GetPhysicalDevice(), m_Image.Format, &formatProperties);

	const VkFormatFeatureFlags features = formatProperties.optimalTilingFeatures;

	assert((features & VK_FORMAT_FEATURE_BLIT_SRC_BIT) && (features & VK_FORMAT_FEATURE_BLIT_DST_BIT) && "Format does not support blit-based mip generation");

	const VkFilter filter = (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	const VkImage  image      = m_Image.Image;
	const uint32_t mipCount   = m_Image.MipLevels;
	const uint32_t layerCount = m_Image.ArrayLayers; // 6 per cube, 1 for 3D

	Context::Get().ImmediateSubmit([&](CommandBuffer& commandBuffer)
	{
		int32_t width  = static_cast<int32_t>(m_Image.Extent.width);
		int32_t height = static_cast<int32_t>(m_Image.Extent.height);
		int32_t depth  = static_cast<int32_t>(m_Image.Extent.depth); // 1 unless 3D

		for (uint32_t mip = 1; mip < mipCount; ++mip)
		{
			const int32_t nextWidth  = std::max(width / 2, 1);
			const int32_t nextHeight = std::max(height / 2, 1);
			const int32_t nextDepth  = std::max(depth / 2, 1);

			const VkImageSubresourceRange dstRange
			{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = mip,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = layerCount
			};

			commandBuffer.ImageBarrier(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstRange,
									   VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_BLIT_BIT);

			const VkImageBlit2 blit
			{
				.sType          = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
				.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, layerCount },
				.srcOffsets     = { { 0, 0, 0 }, { width, height, depth } },
				.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, layerCount },
				.dstOffsets     = { { 0, 0, 0 }, { nextWidth, nextHeight, nextDepth } }
			};

			commandBuffer.BlitImage(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blit, filter);

			// This mip becomes the source for the next one.
			commandBuffer.ImageBarrier(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstRange,
									   VK_PIPELINE_STAGE_2_BLIT_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT);

			width  = nextWidth;
			height = nextHeight;
			depth  = nextDepth;
		}

		const VkImageSubresourceRange allMips
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = mipCount,
			.baseArrayLayer = 0,
			.layerCount     = layerCount
		};

		commandBuffer.ImageBarrier(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, allMips,
								   VK_PIPELINE_STAGE_2_BLIT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT);
	});
}

// ==== Attachment views ====

VkImageView Texture::GetAttachmentView(uint32_t mip, uint32_t layer)
{
	assert(IsAttachment());
	assert(GetType() != TextureType::Texture3D && "Rendering into 3D slices needs VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT");
	assert(mip < m_Image.MipLevels && mip < MAX_MIP_LEVELS);
	assert(layer < m_Image.ArrayLayers && layer < MAX_CUBE_FACES);

	const std::string name = m_Specification.DebugName.empty() ? std::string{} : std::format("{} attachment mip {} layer {}", m_Specification.DebugName, mip, layer);

	return m_Image.GetOrCreateMipLayerView(Context::Get().GetDevice(), mip, layer, name.empty() ? nullptr : name.c_str());
}
