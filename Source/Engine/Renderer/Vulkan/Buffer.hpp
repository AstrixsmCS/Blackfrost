#pragma once

#include "Vulkan.hpp"

#include <vma/vk_mem_alloc.h>

#include <cstdint>
#include <string>

enum class BufferUsage : uint32_t
{
	None     = 0,
	Vertex   = 1 << 0,
	Index    = 1 << 1,
	Uniform  = 1 << 2,
	Storage  = 1 << 3,
	Indirect = 1 << 4
};

constexpr BufferUsage operator|(BufferUsage a, BufferUsage b)
{
	return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr BufferUsage operator&(BufferUsage a, BufferUsage b)
{
	return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr bool HasFlag(BufferUsage value, BufferUsage flag)
{
	return (value & flag) != BufferUsage::None;
}

enum class BufferMemory
{
	Device,
	HostVisible
};

struct BufferSpecification
{
	std::string DebugName;

	BufferUsage  Usage  = BufferUsage::None;
	BufferMemory Memory = BufferMemory::Device;

	VkDeviceSize Size = 0;

	const void* Data = nullptr;
};

class Buffer
{
public:
	Buffer() = default;
	~Buffer();

	Buffer(const Buffer&)            = delete;
	Buffer& operator=(const Buffer&) = delete;

	Buffer(Buffer&& other) noexcept;
	Buffer& operator=(Buffer&& other) noexcept;

	void Create(const BufferSpecification& specification);
	void Destroy();

	void SetData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

	VkBuffer GetHandle() const { return m_Handle; }

	VmaAllocation   GetAllocation() const { return m_Allocation; }
	VkDeviceAddress GetDeviceAddress() const { return m_DeviceAddress; }
	VkDeviceSize    GetSize() const { return m_Specification.Size; }
	BufferUsage     GetUsage() const { return m_Specification.Usage; }
	BufferMemory    GetMemory() const { return m_Specification.Memory; }

	const BufferSpecification& GetSpecification() const { return m_Specification; }

	bool IsValid() const { return m_Handle != VK_NULL_HANDLE; }

private:
	BufferSpecification m_Specification{};

	VkBuffer      m_Handle     = VK_NULL_HANDLE;
	VmaAllocation m_Allocation = VK_NULL_HANDLE;

	VkDeviceAddress m_DeviceAddress = 0;

	void* m_MappedData = nullptr;
};
