#pragma once

#include "Descriptors.hpp"
#include "Format.hpp"
#include "Vulkan.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

#include <vma/vk_mem_alloc.h>

enum class TextureType : uint8_t
{
	Texture2D = 0,
	Texture3D,
	TextureCube
};

enum TextureUsageBits : uint8_t
{
	TextureUsageBits_Sampled         = 1 << 0,
	TextureUsageBits_Storage         = 1 << 1,
	TextureUsageBits_Attachment      = 1 << 2,
	TextureUsageBits_InputAttachment = 1 << 3,
};
using TextureUsageFlags = uint8_t;

struct Offset3D
{
	int32_t X = 0;
	int32_t Y = 0;
	int32_t Z = 0;
};

struct TextureLayers
{
	uint32_t MipLevel  = 0;
	uint32_t Layer     = 0;
	uint32_t NumLayers = 1;
};

struct TextureRangeDesc
{
	Offset3D Offset = {};

	VkExtent3D Size = { 1, 1, 1 };

	uint32_t Layer     = 0;
	uint32_t NumLayers = 1;

	uint32_t MipLevel     = 0;
	uint32_t NumMipLevels = 1;
};

enum class TextureAspect : uint8_t
{
	Default = 0,
	Depth,
	Stencil,
};

enum class Swizzle : uint8_t
{
	Default = 0,
	Zero,
	One,
	R,
	G,
	B,
	A,
};

struct ComponentMapping
{
	Swizzle R = Swizzle::Default;
	Swizzle G = Swizzle::Default;
	Swizzle B = Swizzle::Default;
	Swizzle A = Swizzle::Default;

	bool IsIdentity() const
	{
		return R == Swizzle::Default &&
			   G == Swizzle::Default &&
			   B == Swizzle::Default &&
			   A == Swizzle::Default;
	}
};

static constexpr uint32_t MAX_MIP_LEVELS = 16;
static constexpr uint32_t MAX_CUBE_FACES = 6;

inline uint32_t CalcMipCount(uint32_t width, uint32_t height, uint32_t depth = 1)
{
	assert(width > 0);
	assert(height > 0);
	assert(depth > 0);

	const uint32_t maxDimension = std::max({ width, height, depth });
	return static_cast<uint32_t>(std::floor(std::log2(maxDimension))) + 1;
}

struct TextureSpecification
{
	TextureType Type   = TextureType::Texture2D;
	VkFormat    Format = VK_FORMAT_R8G8B8A8_UNORM;

	VkExtent3D Size = { 1, 1, 1 };

	uint32_t NumLayers    = 1;
	uint32_t NumMipLevels = 1;

	TextureUsageFlags Usage = TextureUsageBits_Sampled;

	const void* Data = nullptr;

	ComponentMapping Components = {};

	bool GenerateMips = false;

	std::string DebugName;
};

struct TextureViewSpecification
{
	TextureType Type = TextureType::Texture2D;

	uint32_t Layer     = 0;
	uint32_t NumLayers = 1;

	uint32_t MipLevel     = 0;
	uint32_t NumMipLevels = 1;

	ComponentMapping Components = {};

	TextureAspect Aspect = TextureAspect::Default;
};

struct VulkanImage
{
	bool IsValid() const { return Image != VK_NULL_HANDLE; }
	bool IsSampled() const { return (UsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) != 0; }
	bool IsStorage() const { return (UsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0; }
	bool IsColorAttachment() const { return (UsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0; }
	bool IsDepthAttachment() const { return (UsageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0; }
	bool IsAttachment() const { return IsColorAttachment() || IsDepthAttachment(); }

	[[nodiscard]] VkImageView CreateView(VkDevice           device,
										 VkImageViewType    viewType,
										 VkFormat           format,
										 VkImageAspectFlags aspectMask,
										 uint32_t           baseMip    = 0,
										 uint32_t           mipCount   = VK_REMAINING_MIP_LEVELS,
										 uint32_t           baseLayer  = 0,
										 uint32_t           layerCount = VK_REMAINING_ARRAY_LAYERS,
										 ComponentMapping   components = {},
										 const char*        debugName  = nullptr) const;

	[[nodiscard]] VkImageView GetOrCreateMipLayerView(VkDevice device, uint32_t mip, uint32_t layer, const char* debugName = nullptr);

	static bool IsDepthFormat(VkFormat format);
	static bool IsStencilFormat(VkFormat format);

	VkImage           Image       = VK_NULL_HANDLE;
	VmaAllocation     Allocation  = VK_NULL_HANDLE;
	VkImageUsageFlags UsageFlags  = 0;
	VkFormat          Format      = VK_FORMAT_UNDEFINED;
	VkExtent3D        Extent      = {};
	uint32_t          MipLevels   = 1;
	uint32_t          ArrayLayers = 1;
	bool              IsDepth     = false;
	bool              IsStencil   = false;

	VkImageView MipLayerViews[MAX_MIP_LEVELS][MAX_CUBE_FACES] = {};
};

class Texture
{
public:
	Texture() = default;
	~Texture() { Destroy(); }

	Texture(const Texture&)            = delete;
	Texture& operator=(const Texture&) = delete;

	void Create(const TextureSpecification& specification);
	void CreateView(const Texture& source, const TextureViewSpecification& viewSpecification, const std::string& debugName = {});

	bool Load(const std::filesystem::path& path, bool sRGB = true);

	void Destroy();
	void GenerateMips();

	bool IsValid() const { return m_Image.IsValid(); }
	bool IsSampled() const { return m_Image.IsSampled(); }
	bool IsStorage() const { return m_Image.IsStorage(); }
	bool IsAttachment() const { return m_Image.IsAttachment(); }
	bool IsColorAttachment() const { return m_Image.IsColorAttachment(); }
	bool IsDepthAttachment() const { return m_Image.IsDepthAttachment(); }

	TextureType GetType() const { return m_Specification.Type; }
	VkFormat    GetFormat() const { return m_Specification.Format; }
	uint32_t    GetWidth() const { return m_Image.Extent.width; }
	uint32_t    GetHeight() const { return m_Image.Extent.height; }
	uint32_t    GetDepth() const { return m_Image.Extent.depth; }
	uint32_t    GetMipLevels() const { return m_Image.MipLevels; }
	uint32_t    GetArrayLayers() const { return m_Image.ArrayLayers; }

	const TextureSpecification& GetSpecification() const { return m_Specification; }

	VkImageView GetView() const { return m_DefaultView; }
	VkImage     GetHandle() const { return m_Image.Image; }

	uint32_t GetBindlessIndex() const { return m_SampledSlot.GetIndex(); }
	uint32_t GetStorageIndex() const { return m_StorageSlot.GetIndex(); }

	VkImageView GetAttachmentView(uint32_t mip = 0, uint32_t layer = 0);

private:
	void SetData(const void* data, size_t size);

	VulkanImage          m_Image;
	TextureSpecification m_Specification;
	bool                 m_OwnsImage = true;

	VkImageView m_DefaultView = VK_NULL_HANDLE;
	VkImageView m_StorageView = VK_NULL_HANDLE;

	BindlessSlot m_SampledSlot; // references m_DefaultView
	BindlessSlot m_StorageSlot; // references m_StorageView
};
