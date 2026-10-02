#pragma once

#include "Renderer/RendererTypes.hpp"

#include <volk/volk.h>

#include <cstdlib>

// ==== Present modes ====

constexpr VkPresentModeKHR ToVulkan(PresentMode mode)
{
	switch (mode)
	{
		case PresentMode::FIFO: return VK_PRESENT_MODE_FIFO_KHR;
		case PresentMode::FIFORelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		case PresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
		case PresentMode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR;
	}

	std::abort();
}

// ==== Resolve modes ====

constexpr VkResolveModeFlagBits ToVulkan(ResolveMode mode)
{
	switch (mode)
	{
		case ResolveMode::None: return VK_RESOLVE_MODE_NONE;
		case ResolveMode::SampleZero: return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
		case ResolveMode::Average: return VK_RESOLVE_MODE_AVERAGE_BIT;
		case ResolveMode::Min: return VK_RESOLVE_MODE_MIN_BIT;
		case ResolveMode::Max: return VK_RESOLVE_MODE_MAX_BIT;
	}

	std::abort();
}

// ==== Geometry ====

constexpr VkViewport ToVulkan(const Viewport& viewport)
{
	return {
		.x        = viewport.X,
		.y        = viewport.Y,
		.width    = viewport.Width,
		.height   = viewport.Height,
		.minDepth = viewport.MinDepth,
		.maxDepth = viewport.MaxDepth
	};
}

constexpr VkRect2D ToVulkan(const ScissorRect& scissor)
{
	return {
		.offset = {
			.x = static_cast<int32_t>(scissor.X),
			.y = static_cast<int32_t>(scissor.Y) },
		.extent = { .width = scissor.Width, .height = scissor.Height }
	};
}

constexpr VkExtent3D ToVulkan(const Dimensions& dimensions)
{
	return {
		.width  = dimensions.Width,
		.height = dimensions.Height,
		.depth  = dimensions.Depth
	};
}

constexpr VkExtent2D ToVulkan2D(const Dimensions& dimensions)
{
	return {
		.width  = dimensions.Width,
		.height = dimensions.Height
	};
}

// ==== Index formats ====

constexpr VkIndexType ToVulkan(IndexFormat format)
{
	switch (format)
	{
		case IndexFormat::UInt8: return VK_INDEX_TYPE_UINT8_EXT;
		case IndexFormat::UInt16: return VK_INDEX_TYPE_UINT16;
		case IndexFormat::UInt32: return VK_INDEX_TYPE_UINT32;
	}

	std::abort();
}

// ==== Attachment operations ====

constexpr VkAttachmentLoadOp ToVulkan(LoadOp op)
{
	switch (op)
	{
		case LoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
		case LoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		case LoadOp::None: return VK_ATTACHMENT_LOAD_OP_NONE;
		case LoadOp::Invalid: break;
	}

	std::abort();
}

constexpr VkAttachmentStoreOp ToVulkan(StoreOp op)
{
	switch (op)
	{
		case StoreOp::Store: return VK_ATTACHMENT_STORE_OP_STORE;
		case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		case StoreOp::None: return VK_ATTACHMENT_STORE_OP_NONE;
	}

	std::abort();
}

// ==== Formats ====

constexpr VkFormat ToVulkan(Format format)
{
	switch (format)
	{
		case Format::Invalid: return VK_FORMAT_UNDEFINED;

		// 8-bit
		case Format::R8_UNorm: return VK_FORMAT_R8_UNORM;
		case Format::R8_SNorm: return VK_FORMAT_R8_SNORM;
		case Format::R8_UInt: return VK_FORMAT_R8_UINT;
		case Format::R8_SInt: return VK_FORMAT_R8_SINT;

		case Format::RG8_UNorm: return VK_FORMAT_R8G8_UNORM;
		case Format::RG8_SNorm: return VK_FORMAT_R8G8_SNORM;
		case Format::RG8_UInt: return VK_FORMAT_R8G8_UINT;
		case Format::RG8_SInt: return VK_FORMAT_R8G8_SINT;

		case Format::RGBA8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
		case Format::RGBA8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
		case Format::RGBA8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
		case Format::RGBA8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
		case Format::RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;

		case Format::BGRA8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
		case Format::BGRA8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;

		// 16-bit
		case Format::R16_UNorm: return VK_FORMAT_R16_UNORM;
		case Format::R16_SNorm: return VK_FORMAT_R16_SNORM;
		case Format::R16_UInt: return VK_FORMAT_R16_UINT;
		case Format::R16_SInt: return VK_FORMAT_R16_SINT;
		case Format::R16_Float: return VK_FORMAT_R16_SFLOAT;

		case Format::RG16_UNorm: return VK_FORMAT_R16G16_UNORM;
		case Format::RG16_SNorm: return VK_FORMAT_R16G16_SNORM;
		case Format::RG16_UInt: return VK_FORMAT_R16G16_UINT;
		case Format::RG16_SInt: return VK_FORMAT_R16G16_SINT;
		case Format::RG16_Float: return VK_FORMAT_R16G16_SFLOAT;

		case Format::RGBA16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
		case Format::RGBA16_SNorm: return VK_FORMAT_R16G16B16A16_SNORM;
		case Format::RGBA16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
		case Format::RGBA16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
		case Format::RGBA16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;

		// 32-bit
		case Format::R32_UInt: return VK_FORMAT_R32_UINT;
		case Format::R32_SInt: return VK_FORMAT_R32_SINT;
		case Format::R32_Float: return VK_FORMAT_R32_SFLOAT;

		case Format::RG32_UInt: return VK_FORMAT_R32G32_UINT;
		case Format::RG32_SInt: return VK_FORMAT_R32G32_SINT;
		case Format::RG32_Float: return VK_FORMAT_R32G32_SFLOAT;

		case Format::RGB32_UInt: return VK_FORMAT_R32G32B32_UINT;
		case Format::RGB32_SInt: return VK_FORMAT_R32G32B32_SINT;
		case Format::RGB32_Float: return VK_FORMAT_R32G32B32_SFLOAT;

		case Format::RGBA32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
		case Format::RGBA32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
		case Format::RGBA32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;

		// Packed
		case Format::RGB10A2_UNorm: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
		case Format::BGR10A2_UNorm: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case Format::R11G11B10_UFloat: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case Format::RGB9E5_UFloat: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

		// Depth / Stencil
		case Format::D16_UNorm: return VK_FORMAT_D16_UNORM;
		case Format::D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
		case Format::D32_Float: return VK_FORMAT_D32_SFLOAT;
		case Format::D32_Float_S8_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case Format::S8_UInt: return VK_FORMAT_S8_UINT;

		// BC
		case Format::BC1_RGB_UNorm: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case Format::BC1_RGB_SRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		case Format::BC1_RGBA_UNorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case Format::BC1_RGBA_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;

		case Format::BC2_RGBA_UNorm: return VK_FORMAT_BC2_UNORM_BLOCK;
		case Format::BC2_RGBA_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;

		case Format::BC3_RGBA_UNorm: return VK_FORMAT_BC3_UNORM_BLOCK;
		case Format::BC3_RGBA_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;

		case Format::BC4_R_UNorm: return VK_FORMAT_BC4_UNORM_BLOCK;
		case Format::BC4_R_SNorm: return VK_FORMAT_BC4_SNORM_BLOCK;

		case Format::BC5_RG_UNorm: return VK_FORMAT_BC5_UNORM_BLOCK;
		case Format::BC5_RG_SNorm: return VK_FORMAT_BC5_SNORM_BLOCK;

		case Format::BC6H_RGB_UFloat: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case Format::BC6H_RGB_SFloat: return VK_FORMAT_BC6H_SFLOAT_BLOCK;

		case Format::BC7_RGBA_UNorm: return VK_FORMAT_BC7_UNORM_BLOCK;
		case Format::BC7_RGBA_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;

		// ETC2
		case Format::ETC2_RGB8_UNorm: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
		case Format::ETC2_RGB8_SRGB: return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
		case Format::ETC2_RGBA8_UNorm: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
		case Format::ETC2_RGBA8_SRGB: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;

		// YUV / Video
		case Format::YUV_NV12: return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
		case Format::YUV_420P: return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
	}

	std::abort();
}

// ==== Rasterization ====

constexpr VkPolygonMode ToVulkan(PolygonMode mode)
{
	switch (mode)
	{
		case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
		case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
		case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
	}

	std::abort();
}

constexpr VkCullModeFlags ToVulkan(CullMode mode)
{
	switch (mode)
	{
		case CullMode::None: return VK_CULL_MODE_NONE;
		case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
		case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
	}

	std::abort();
}

constexpr VkFrontFace ToVulkan(WindingMode mode)
{
	switch (mode)
	{
		case WindingMode::CCW: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		case WindingMode::CW: return VK_FRONT_FACE_CLOCKWISE;
	}

	std::abort();
}

constexpr VkPrimitiveTopology ToVulkan(Topology topology)
{
	switch (topology)
	{
		case Topology::Point: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case Topology::Line: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case Topology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case Topology::Triangle: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case Topology::Patch: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	}

	std::abort();
}

// ==== Shader data types ====

constexpr VkFormat ToVulkan(ShaderDataType type)
{
	switch (type)
	{
		case ShaderDataType::None: return VK_FORMAT_UNDEFINED;

		case ShaderDataType::Float: return VK_FORMAT_R32_SFLOAT;
		case ShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
		case ShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
		case ShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;

		case ShaderDataType::Int: return VK_FORMAT_R32_SINT;
		case ShaderDataType::Int2: return VK_FORMAT_R32G32_SINT;
		case ShaderDataType::Int3: return VK_FORMAT_R32G32B32_SINT;
		case ShaderDataType::Int4: return VK_FORMAT_R32G32B32A32_SINT;

		case ShaderDataType::UInt: return VK_FORMAT_R32_UINT;
		case ShaderDataType::UInt2: return VK_FORMAT_R32G32_UINT;
		case ShaderDataType::UInt3: return VK_FORMAT_R32G32B32_UINT;
		case ShaderDataType::UInt4: return VK_FORMAT_R32G32B32A32_UINT;

		case ShaderDataType::Mat3:
		case ShaderDataType::Mat4:
		case ShaderDataType::Bool:
			break;
	}

	std::abort();
}

// ==== Depth / Stencil ====

constexpr VkCompareOp ToVulkan(CompareOp op)
{
	switch (op)
	{
		case CompareOp::Never: return VK_COMPARE_OP_NEVER;
		case CompareOp::Less: return VK_COMPARE_OP_LESS;
		case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
		case CompareOp::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
		case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
	}

	std::abort();
}

constexpr VkStencilOp ToVulkan(StencilOp op)
{
	switch (op)
	{
		case StencilOp::Keep: return VK_STENCIL_OP_KEEP;
		case StencilOp::Zero: return VK_STENCIL_OP_ZERO;
		case StencilOp::Replace: return VK_STENCIL_OP_REPLACE;
		case StencilOp::IncrementClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case StencilOp::DecrementClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case StencilOp::Invert: return VK_STENCIL_OP_INVERT;
		case StencilOp::IncrementWrap: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case StencilOp::DecrementWrap: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
	}

	std::abort();
}

// ==== Texture types ====

constexpr VkImageType ToVulkan(TextureType type)
{
	switch (type)
	{
		case TextureType::Texture2D: return VK_IMAGE_TYPE_2D;
		case TextureType::Texture3D: return VK_IMAGE_TYPE_3D;
		case TextureType::TextureCube: return VK_IMAGE_TYPE_2D;
	}

	std::abort();
}

// Converts the base type only; array views require an additional layer/type choice.
constexpr VkImageViewType ToVulkanImageViewType(TextureType type)
{
	switch (type)
	{
		case TextureType::Texture2D: return VK_IMAGE_VIEW_TYPE_2D;
		case TextureType::Texture3D: return VK_IMAGE_VIEW_TYPE_3D;
		case TextureType::TextureCube: return VK_IMAGE_VIEW_TYPE_CUBE;
	}

	std::abort();
}

// ==== Color spaces ====

constexpr VkColorSpaceKHR ToVulkan(ColorSpace colorSpace)
{
	switch (colorSpace)
	{
		case ColorSpace::SRGBNonlinear: return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		case ColorSpace::SRGBExtendedLinear: return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
		case ColorSpace::HDR10: return VK_COLOR_SPACE_HDR10_ST2084_EXT;
		case ColorSpace::BT709Linear: return VK_COLOR_SPACE_BT709_LINEAR_EXT;
	}

	std::abort();
}

// ==== Shader stages ====

constexpr VkShaderStageFlags ToVulkan(ShaderStage stage)
{
	switch (stage)
	{
		case ShaderStage::None: return 0;
		case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
		case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
		case ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
		case ShaderStage::RayGen: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case ShaderStage::Miss: return VK_SHADER_STAGE_MISS_BIT_KHR;
		case ShaderStage::ClosestHit: return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		case ShaderStage::AnyHit: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		case ShaderStage::Intersection: return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		case ShaderStage::Callable: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		case ShaderStage::Task: return VK_SHADER_STAGE_TASK_BIT_EXT;
		case ShaderStage::Mesh: return VK_SHADER_STAGE_MESH_BIT_EXT;
	}

	std::abort();
}
