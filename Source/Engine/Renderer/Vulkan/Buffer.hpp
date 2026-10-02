#pragma once

#include "Vulkan.hpp"

#include "Renderer/RendererTypes.hpp"

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

inline uint32_t ShaderDataTypeSize(ShaderDataType type)
{
	switch (type)
	{
		case ShaderDataType::Float: return 4;
		case ShaderDataType::Float2: return 4 * 2;
		case ShaderDataType::Float3: return 4 * 3;
		case ShaderDataType::Float4: return 4 * 4;

		case ShaderDataType::Mat3: return 4 * 3 * 3;
		case ShaderDataType::Mat4: return 4 * 4 * 4;

		case ShaderDataType::Int: return 4;
		case ShaderDataType::Int2: return 4 * 2;
		case ShaderDataType::Int3: return 4 * 3;
		case ShaderDataType::Int4: return 4 * 4;

		case ShaderDataType::UInt: return 4;
		case ShaderDataType::UInt2: return 4 * 2;
		case ShaderDataType::UInt3: return 4 * 3;
		case ShaderDataType::UInt4: return 4 * 4;

		case ShaderDataType::Bool: return 1;

		case ShaderDataType::None:
			break;
	}

	return 0;
}

struct VertexBufferElement
{
	std::string Name;

	ShaderDataType Type = ShaderDataType::None;

	uint32_t Size   = 0;
	uint32_t Offset = 0;

	bool Normalized = false;

	VertexBufferElement() = default;

	VertexBufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
		: Name(name), Type(type), Size(ShaderDataTypeSize(type)), Normalized(normalized)
	{
	}

	uint32_t GetComponentCount() const
	{
		switch (Type)
		{
			case ShaderDataType::Float:
			case ShaderDataType::Int:
			case ShaderDataType::UInt:
			case ShaderDataType::Bool:
				return 1;

			case ShaderDataType::Float2:
			case ShaderDataType::Int2:
			case ShaderDataType::UInt2:
				return 2;

			case ShaderDataType::Float3:
			case ShaderDataType::Int3:
			case ShaderDataType::UInt3:
				return 3;

			case ShaderDataType::Float4:
			case ShaderDataType::Int4:
			case ShaderDataType::UInt4:
				return 4;

			case ShaderDataType::Mat3:
				return 3 * 3;

			case ShaderDataType::Mat4:
				return 4 * 4;

			case ShaderDataType::None:
				break;
		}

		return 0;
	}
};

class VertexBufferLayout
{
public:
	VertexBufferLayout() = default;

	VertexBufferLayout(std::initializer_list<VertexBufferElement> elements) : m_Elements(elements)
	{
		CalculateOffsetsAndStride();
	}

	uint32_t                                GetStride() const { return m_Stride; }
	uint32_t                                GetElementCount() const { return static_cast<uint32_t>(m_Elements.size()); }
	const std::vector<VertexBufferElement>& GetElements() const { return m_Elements; }

	auto begin() { return m_Elements.begin(); }
	auto end() { return m_Elements.end(); }

	auto begin() const { return m_Elements.begin(); }
	auto end() const { return m_Elements.end(); }

private:
	void CalculateOffsetsAndStride()
	{
		uint32_t offset = 0;

		m_Stride = 0;

		for (VertexBufferElement& element : m_Elements)
		{
			element.Offset = offset;

			offset += element.Size;
			m_Stride += element.Size;
		}
	}

private:
	std::vector<VertexBufferElement> m_Elements;

	uint32_t m_Stride = 0;
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
