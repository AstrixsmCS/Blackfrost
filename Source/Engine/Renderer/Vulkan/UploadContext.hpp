#pragma once

#include "Allocator.hpp"
#include "CommandBuffer.hpp"
#include "Context.hpp"
#include "TimelineSemaphore.hpp"
#include "Vulkan.hpp"

#include <vma/vk_mem_alloc.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <functional>
#include <numeric>

class UploadContext
{
public:
	static constexpr VkDeviceSize STAGING_SIZE = 64ull * 1024ull * 1024ull; // 64 MB
	static constexpr VkDeviceSize ALIGNMENT    = 16;

	static void Initialize()
	{
		assert(!s_Instance);

		s_Instance = new UploadContext();

		UploadContext& upload = *s_Instance;

		const VkDevice device = Context::Get().GetDevice();

		// ==== Staging buffer ====

		const VkBufferCreateInfo bufferInfo
		{
			.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size        = STAGING_SIZE,
			.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};

		const VmaAllocationCreateInfo allocationInfo
		{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO
		};

		VmaAllocationInfo allocationResult{};

		VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &upload.m_StagingBuffer, &upload.m_Allocation, &allocationResult));
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_BUFFER,    "Upload Staging Buffer", upload.m_StagingBuffer);

		upload.m_MappedData = allocationResult.pMappedData;

		assert(upload.m_MappedData);

		// ==== Command buffer + timeline ====

		const VkCommandPoolCreateInfo poolInfo
		{
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = Context::Get().GetGraphicsFamily()
		};

		VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &upload.m_CommandPool));

		const VkCommandBufferAllocateInfo commandBufferInfo
		{
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = upload.m_CommandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferInfo, &upload.m_CommandBuffer));

		upload.m_Timeline.Initialize(0);
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, "Upload Timeline",       upload.m_Timeline.GetHandle());
	}

	static void Shutdown()
	{
		if (!s_Instance)
			return;

		UploadContext& upload = *s_Instance;

		const VkDevice device = Context::Get().GetDevice();

		upload.m_Timeline.Shutdown();
		vkDestroyCommandPool(device, upload.m_CommandPool, nullptr);
		vmaDestroyBuffer(Allocator::GetAllocator(), upload.m_StagingBuffer, upload.m_Allocation);

		delete s_Instance;
		s_Instance = nullptr;
	}

	static UploadContext& Get()
	{
		assert(s_Instance);
		return *s_Instance;
	}

	// This is the single "immediate submit" path Context::ImmediateSubmit forwards here.
	void Submit(const std::function<void(CommandBuffer&)>& record)
	{
		assert(!m_InSubmit && "UploadContext::Submit is not re-entrant");

		m_InSubmit = true;

		CommandBuffer commandBuffer(Record());
		record(commandBuffer);

		m_InSubmit = false;

		Flush();
	}

	void UploadBuffer(VkBuffer destination, const void* data, VkDeviceSize size, VkDeviceSize destinationOffset = 0)
	{
		assert(!m_InSubmit && "Uploads cannot be issued from inside UploadContext::Submit");
		assert(destination != VK_NULL_HANDLE);
		assert(data);
		assert(size > 0);

		const auto* bytes = static_cast<const std::byte*>(data);

		for (VkDeviceSize copied = 0; copied < size;)
		{
			const VkDeviceSize chunkSize     = std::min(size - copied, STAGING_SIZE);
			const VkDeviceSize stagingOffset = Stage(bytes + copied, chunkSize, ALIGNMENT);

			const VkBufferCopy2 region
			{
				.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
				.srcOffset = stagingOffset,
				.dstOffset = destinationOffset + copied,
				.size      = chunkSize
			};

			const VkCopyBufferInfo2 copyInfo
			{
				.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
				.srcBuffer   = m_StagingBuffer,
				.dstBuffer   = destination,
				.regionCount = 1,
				.pRegions    = &region
			};

			vkCmdCopyBuffer2(Record(), &copyInfo);

			copied += chunkSize;
		}

		const VkBufferMemoryBarrier2 postCopy
		{
			.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
			.srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer              = destination,
			.offset              = destinationOffset,
			.size                = size
		};

		const VkDependencyInfo postCopyDependency
		{
			.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers    = &postCopy
		};

		vkCmdPipelineBarrier2(Record(), &postCopyDependency);

		Flush();
	}

	void UploadImage(VkImage destination, const void* data, VkDeviceSize size, const VkBufferImageCopy2& copyRegion, const VkImageSubresourceRange& subresourceRange, VkImageLayout finalLayout)
	{
		assert(!m_InSubmit && "Uploads cannot be issued from inside UploadContext::Submit");
		assert(destination != VK_NULL_HANDLE);
		assert(data);
		assert(size > 0);

		const VkExtent3D extent     = copyRegion.imageExtent;
		const uint32_t   layerCount = copyRegion.imageSubresource.layerCount;

		const VkDeviceSize texelCount = static_cast<VkDeviceSize>(extent.width) * extent.height * extent.depth * layerCount;

		assert(texelCount > 0 && size % texelCount == 0 && "Image data must be tightly packed and uncompressed");

		const VkDeviceSize texelSize = size / texelCount;
		const VkDeviceSize rowSize   = texelSize * extent.width;

		assert(rowSize <= STAGING_SIZE && "A single image row exceeds the staging buffer");

		const uint32_t rowsPerChunk = static_cast<uint32_t>(std::min<VkDeviceSize>(STAGING_SIZE / rowSize, extent.height));

		const VkDeviceSize alignment = std::lcm(ALIGNMENT, texelSize);

		{
			const VkImageMemoryBarrier2 preCopy
			{
				.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
				.srcAccessMask       = VK_ACCESS_2_NONE,
				.dstStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
				.dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image               = destination,
				.subresourceRange    = subresourceRange
			};

			const VkDependencyInfo preCopyDependency
			{
				.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &preCopy
			};

			vkCmdPipelineBarrier2(Record(), &preCopyDependency);
		}

		// ==== Copies (source data is layer-major, then slice, then row) ====

		const auto* bytes = static_cast<const std::byte*>(data);

		VkDeviceSize sourceOffset = 0;

		for (uint32_t layer = 0; layer < layerCount; ++layer)
		{
			for (uint32_t slice = 0; slice < extent.depth; ++slice)
			{
				for (uint32_t row = 0; row < extent.height; row += rowsPerChunk)
				{
					const uint32_t     rowCount      = std::min(rowsPerChunk, extent.height - row);
					const VkDeviceSize chunkSize     = rowSize * rowCount;
					const VkDeviceSize stagingOffset = Stage(bytes + sourceOffset, chunkSize, alignment);

					const VkBufferImageCopy2 region
					{
						.sType            = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
						.bufferOffset     = stagingOffset,
						.imageSubresource =
						{
							.aspectMask     = copyRegion.imageSubresource.aspectMask,
							.mipLevel       = copyRegion.imageSubresource.mipLevel,
							.baseArrayLayer = copyRegion.imageSubresource.baseArrayLayer + layer,
							.layerCount     = 1
						},
						.imageOffset =
						{
							.x = copyRegion.imageOffset.x,
							.y = copyRegion.imageOffset.y + static_cast<int32_t>(row),
							.z = copyRegion.imageOffset.z + static_cast<int32_t>(slice)
						},
						.imageExtent = { extent.width, rowCount, 1 }
					};

					const VkCopyBufferToImageInfo2 copyInfo
					{
						.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
						.srcBuffer      = m_StagingBuffer,
						.dstImage       = destination,
						.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.regionCount    = 1,
						.pRegions       = &region
					};

					vkCmdCopyBufferToImage2(Record(), &copyInfo);

					sourceOffset += chunkSize;
				}
			}
		}

		{
			const VkImageMemoryBarrier2 postCopy
			{
				.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
				.srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
				.dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT,
				.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout           = finalLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image               = destination,
				.subresourceRange    = subresourceRange
			};

			const VkDependencyInfo postCopyDependency
			{
				.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &postCopy
			};

			vkCmdPipelineBarrier2(Record(), &postCopyDependency);
		}

		Flush();
	}

private:
	UploadContext() = default;

	VkCommandBuffer Record()
	{
		if (!m_Recording)
		{
			const VkCommandBufferBeginInfo beginInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};

			VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));

			m_Recording = true;
		}

		return m_CommandBuffer;
	}

	// Copies data into the staging buffer and returns its offset.
	// If it doesnt fit the pending copies are flushed first and the buffer is reused from the start.
	VkDeviceSize Stage(const void* data, VkDeviceSize size, VkDeviceSize alignment)
	{
		assert(size <= STAGING_SIZE);

		VkDeviceSize offset = (m_Head + alignment - 1) / alignment * alignment;

		if (offset + size > STAGING_SIZE)
		{
			Flush();
			offset = 0;
		}

		std::memcpy(static_cast<std::byte*>(m_MappedData) + offset, data, static_cast<size_t>(size));

		VK_CHECK(vmaFlushAllocation(Allocator::GetAllocator(), m_Allocation, offset, size));

		m_Head = offset + size;

		return offset;
	}

	void Flush()
	{
		if (!m_Recording)
			return;

		VK_CHECK(vkEndCommandBuffer(m_CommandBuffer));

		const uint64_t value = ++m_LastSubmittedValue;

		const VkCommandBufferSubmitInfo commandBufferInfo
		{
			.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = m_CommandBuffer
		};

		const VkSemaphoreSubmitInfo signalInfo
		{
			.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = m_Timeline.GetHandle(),
			.value     = value,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
		};

		const VkSubmitInfo2 submitInfo
		{
			.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.commandBufferInfoCount   = 1,
			.pCommandBufferInfos      = &commandBufferInfo,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos    = &signalInfo
		};

		VK_CHECK(vkQueueSubmit2(Context::Get().GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));

		m_Timeline.Wait(value);

		m_Recording = false;
		m_Head      = 0;
	}

private:
	inline static UploadContext* s_Instance = nullptr;

	VkBuffer      m_StagingBuffer = VK_NULL_HANDLE;
	VmaAllocation m_Allocation    = VK_NULL_HANDLE;
	void*         m_MappedData    = nullptr;
	VkDeviceSize  m_Head          = 0; // Next free byte in the staging buffer

	VkCommandPool   m_CommandPool   = VK_NULL_HANDLE;
	VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
	bool            m_Recording     = false;
	bool            m_InSubmit      = false;

	TimelineSemaphore m_Timeline;
	uint64_t          m_LastSubmittedValue = 0; // Value signalled by the most recent Flush()
};
