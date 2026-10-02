#include "Descriptors.hpp"

#include "Context.hpp"

#include <cassert>

void Descriptor::Initialize()
{
	const VkDevice device = Context::Get().GetDevice();

	VkPhysicalDeviceVulkan12Properties properties12
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES
	};

	VkPhysicalDeviceProperties2 properties
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &properties12
	};

	vkGetPhysicalDeviceProperties2(Context::Get().GetPhysicalDevice(), &properties);

	assert(MAX_TEXTURES <= properties12.maxPerStageDescriptorUpdateAfterBindSampledImages && "MAX_TEXTURES exceeds the per-stage update-after-bind limit!");
	assert(MAX_TEXTURES <= properties12.maxDescriptorSetUpdateAfterBindSampledImages && "MAX_TEXTURES exceeds the per-set update-after-bind limit!");
	assert(MAX_STORAGE_IMAGES <= properties12.maxPerStageDescriptorUpdateAfterBindStorageImages && "MAX_STORAGE_IMAGES exceeds the per-stage update-after-bind limit!");
	assert(MAX_STORAGE_IMAGES <= properties12.maxDescriptorSetUpdateAfterBindStorageImages && "MAX_STORAGE_IMAGES exceeds the per-set update-after-bind limit!");
	assert(DEFAULT_SAMPLER_COUNT <= properties12.maxPerStageDescriptorUpdateAfterBindSamplers && "Too many default samplers for the per-stage update-after-bind limit!");
	assert(DEFAULT_SAMPLER_COUNT <= properties12.maxDescriptorSetUpdateAfterBindSamplers && "Too many default samplers for the per-set update-after-bind limit!");

	assert(MAX_TEXTURES + MAX_STORAGE_IMAGES + DEFAULT_SAMPLER_COUNT <= properties12.maxPerStageUpdateAfterBindResources && "Bindless set exceeds maxPerStageUpdateAfterBindResources!");

	VkDescriptorSetLayoutBinding bindings[3]
	{
		{ .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MAX_TEXTURES, .stageFlags = VK_SHADER_STAGE_ALL },
		{ .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = DEFAULT_SAMPLER_COUNT, .stageFlags = VK_SHADER_STAGE_ALL },
		{ .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = MAX_STORAGE_IMAGES, .stageFlags = VK_SHADER_STAGE_ALL }
	};

	VkDescriptorBindingFlags bindingFlags[3]
	{
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo
	{
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount  = 3,
		.pBindingFlags = bindingFlags
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo
	{
		.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext        = &bindingFlagsInfo,
		.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = 3,
		.pBindings    = bindings
	};

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &s_Layout));
	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Bindless Layout", s_Layout);

	VkDescriptorPoolSize poolSizes[3]
	{
		{ .type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MAX_TEXTURES },
		{ .type            = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = DEFAULT_SAMPLER_COUNT },
		{ .type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = MAX_STORAGE_IMAGES }
	};

	VkDescriptorPoolCreateInfo poolInfo
	{
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets       = 1,
		.poolSizeCount = 3,
		.pPoolSizes    = poolSizes
	};

	VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &s_Pool));
	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Bindless Pool", s_Pool);

	VkDescriptorSetAllocateInfo allocateInfo
	{
		.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool     = s_Pool,
		.descriptorSetCount = 1,
		.pSetLayouts        = &s_Layout
	};

	VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &s_Set));
	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Bindless Set", s_Set);

	s_FreeTextureIndices.reserve(MAX_TEXTURES - 1);
	for (uint32_t i = MAX_TEXTURES; i > 1; --i)
	{
		s_FreeTextureIndices.push_back(i - 1);
	}

	s_FreeStorageImageIndices.reserve(MAX_STORAGE_IMAGES);
	for (uint32_t i = MAX_STORAGE_IMAGES; i > 0; --i)
	{
		s_FreeStorageImageIndices.push_back(i - 1);
	}

	CreateDefaultSamplers();
}

void Descriptor::Shutdown()
{
	const VkDevice device = Context::Get().GetDevice();

	for (VkSampler& sampler : s_DefaultSamplers)
	{
		if (sampler == VK_NULL_HANDLE)
			continue;

		vkDestroySampler(device, sampler, nullptr);
		sampler = VK_NULL_HANDLE;
	}

	s_FreeTextureIndices.clear();
	s_FreeStorageImageIndices.clear();

	if (s_Pool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device, s_Pool, nullptr);
		s_Pool = VK_NULL_HANDLE;
	}

	if (s_Layout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device, s_Layout, nullptr);
		s_Layout = VK_NULL_HANDLE;
	}

	s_Set = VK_NULL_HANDLE;
}

void Descriptor::CreateDefaultSamplers()
{
	const VkDevice device = Context::Get().GetDevice();

	const VkPhysicalDeviceProperties& properties = Context::Get().GetPhysicalDeviceProperties();

	auto CreateSampler = [&](const char* name, VkFilter filter, VkSamplerMipmapMode mipMode, VkSamplerAddressMode addressMode, bool anisotropy, bool compare) -> VkSampler
	{
		VkSamplerCreateInfo info
		{
			.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter        = filter,
			.minFilter        = filter,
			.mipmapMode       = mipMode,
			.addressModeU     = addressMode,
			.addressModeV     = addressMode,
			.addressModeW     = addressMode,
			.mipLodBias       = 0.0f,
			.anisotropyEnable = anisotropy ? VK_TRUE : VK_FALSE,
			.maxAnisotropy    = anisotropy ? properties.limits.maxSamplerAnisotropy : 1.0f,
			.compareEnable    = compare ? VK_TRUE : VK_FALSE,
			.compareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
			.minLod           = 0.0f,
			.maxLod           = VK_LOD_CLAMP_NONE,
			.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK
		};

		VkSampler sampler = VK_NULL_HANDLE;

		VK_CHECK(vkCreateSampler(device, &info, nullptr, &sampler));
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SAMPLER, name, sampler);

		return sampler;
	};

	s_DefaultSamplers[static_cast<uint32_t>(DefaultSampler::LinearRepeat)] =
		CreateSampler("Linear Repeat Sampler", VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, false, false);

	s_DefaultSamplers[static_cast<uint32_t>(DefaultSampler::LinearClamp)] =
		CreateSampler("Linear Clamp Sampler", VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false, false);

	s_DefaultSamplers[static_cast<uint32_t>(DefaultSampler::NearestClamp)] =
		CreateSampler("Nearest Clamp Sampler", VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false, false);

	s_DefaultSamplers[static_cast<uint32_t>(DefaultSampler::AnisotropicRepeat)] =
		CreateSampler("Anisotropic Repeat Sampler", VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, true, false);

	s_DefaultSamplers[static_cast<uint32_t>(DefaultSampler::ShadowCompare)] =
		CreateSampler("Shadow Compare Sampler", VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false, true);

	for (uint32_t i = 0; i < DEFAULT_SAMPLER_COUNT; ++i)
	{
		WriteSampler(i, s_DefaultSamplers[i]);
	}
}

void Descriptor::WriteSampler(uint32_t index, VkSampler sampler)
{
	VkDescriptorImageInfo imageInfo
	{
		.sampler     = sampler,
		.imageView   = VK_NULL_HANDLE,
		.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkWriteDescriptorSet write
	{
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = s_Set,
		.dstBinding      = 1,
		.dstArrayElement = index,
		.descriptorCount = 1,
		.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo      = &imageInfo
	};

	vkUpdateDescriptorSets(Context::Get().GetDevice(), 1, &write, 0, nullptr);
}

uint32_t Descriptor::AllocateSampledImage(VkImageView imageView, VkImageLayout layout)
{
	assert(s_Set != VK_NULL_HANDLE);
	assert(imageView != VK_NULL_HANDLE);
	assert(!s_FreeTextureIndices.empty());

	const uint32_t index = s_FreeTextureIndices.back();

	s_FreeTextureIndices.pop_back();

	VkDescriptorImageInfo imageInfo
	{
		.sampler     = VK_NULL_HANDLE,
		.imageView   = imageView,
		.imageLayout = layout
	};

	VkWriteDescriptorSet write
	{
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = s_Set,
		.dstBinding      = 0,
		.dstArrayElement = index,
		.descriptorCount = 1,
		.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.pImageInfo      = &imageInfo
	};

	vkUpdateDescriptorSets(Context::Get().GetDevice(), 1, &write, 0, nullptr);

	return index;
}

uint32_t Descriptor::AllocateStorageImage(VkImageView imageView)
{
	assert(s_Set != VK_NULL_HANDLE);
	assert(imageView != VK_NULL_HANDLE);
	assert(!s_FreeStorageImageIndices.empty());

	const uint32_t index = s_FreeStorageImageIndices.back();

	s_FreeStorageImageIndices.pop_back();

	VkDescriptorImageInfo imageInfo
	{
		.sampler     = VK_NULL_HANDLE,
		.imageView   = imageView,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};

	VkWriteDescriptorSet write
	{
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = s_Set,
		.dstBinding      = 2,
		.dstArrayElement = index,
		.descriptorCount = 1,
		.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.pImageInfo      = &imageInfo
	};

	vkUpdateDescriptorSets(Context::Get().GetDevice(), 1, &write, 0, nullptr);

	return index;
}

void Descriptor::ReleaseSampledImage(uint32_t index)
{
	assert(s_Set != VK_NULL_HANDLE && "Texture destroyed after Descriptor::Shutdown()");
	assert(index != NULL_TEXTURE && index < MAX_TEXTURES);

	s_FreeTextureIndices.push_back(index);
}

void Descriptor::ReleaseStorageImage(uint32_t index)
{
	assert(s_Set != VK_NULL_HANDLE && "Texture destroyed after Descriptor::Shutdown()");
	assert(index < MAX_STORAGE_IMAGES);

	s_FreeStorageImageIndices.push_back(index);
}

BindlessSlot::BindlessSlot(BindlessType type, VkImageView imageView, VkImageLayout layout)
	: m_Type(type)
{
	m_Index = (type == BindlessType::SampledImage) ? Descriptor::AllocateSampledImage(imageView, layout) : Descriptor::AllocateStorageImage(imageView);
}

void BindlessSlot::Reset()
{
	if (m_Index == Descriptor::INVALID_INDEX)
		return;

	if (m_Type == BindlessType::SampledImage)
		Descriptor::ReleaseSampledImage(m_Index);
	else
		Descriptor::ReleaseStorageImage(m_Index);

	m_Index = Descriptor::INVALID_INDEX;
}
