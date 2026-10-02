#pragma once

#include "Vulkan.hpp"

#include "Renderer/RendererTypes.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

class BindlessSlot;

class Descriptor
{
public:
	static constexpr uint32_t INVALID_INDEX = UINT32_MAX;
	static constexpr uint32_t NULL_TEXTURE  = 0;

	static void Initialize();
	static void Shutdown();

	static uint32_t GetDefaultSamplerIndex(DefaultSampler sampler) { return static_cast<uint32_t>(sampler); }

	static VkDescriptorSetLayout GetLayout() { return s_Layout; }
	static VkDescriptorSet       GetSet() { return s_Set; }

private:
	friend class BindlessSlot;

	static uint32_t AllocateSampledImage(VkImageView imageView, VkImageLayout layout);
	static uint32_t AllocateStorageImage(VkImageView imageView);
	static void     ReleaseSampledImage(uint32_t index);
	static void     ReleaseStorageImage(uint32_t index);

	static void CreateDefaultSamplers();
	static void WriteSampler(uint32_t index, VkSampler sampler);

private:
	static constexpr uint32_t MAX_TEXTURES          = 4096;
	static constexpr uint32_t MAX_STORAGE_IMAGES    = 1024;
	static constexpr uint32_t DEFAULT_SAMPLER_COUNT = static_cast<uint32_t>(DefaultSampler::Count);

	static inline VkDescriptorSetLayout s_Layout = VK_NULL_HANDLE;
	static inline VkDescriptorPool      s_Pool   = VK_NULL_HANDLE;
	static inline VkDescriptorSet       s_Set    = VK_NULL_HANDLE;

	static inline std::array<VkSampler, DEFAULT_SAMPLER_COUNT> s_DefaultSamplers{};

	static inline std::vector<uint32_t> s_FreeTextureIndices;
	static inline std::vector<uint32_t> s_FreeStorageImageIndices;
};

enum class BindlessType : uint8_t
{
	SampledImage, // set 0, binding 0
	StorageImage, // set 0, binding 2
};

class BindlessSlot
{
public:
	BindlessSlot() = default;
	BindlessSlot(BindlessType type, VkImageView imageView, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	~BindlessSlot() { Reset(); }

	BindlessSlot(const BindlessSlot&)            = delete;
	BindlessSlot& operator=(const BindlessSlot&) = delete;

	BindlessSlot(BindlessSlot&& other) noexcept
		: m_Type(other.m_Type), m_Index(std::exchange(other.m_Index, Descriptor::INVALID_INDEX))
	{
	}

	BindlessSlot& operator=(BindlessSlot&& other) noexcept
	{
		if (this != &other)
		{
			Reset();
			m_Type  = other.m_Type;
			m_Index = std::exchange(other.m_Index, Descriptor::INVALID_INDEX);
		}
		return *this;
	}

	void Reset();

	bool     IsValid() const { return m_Index != Descriptor::INVALID_INDEX; }
	uint32_t GetIndex() const { return m_Index; }

private:
	BindlessType m_Type  = BindlessType::SampledImage;
	uint32_t     m_Index = Descriptor::INVALID_INDEX;
};
