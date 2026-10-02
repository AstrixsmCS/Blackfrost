#pragma once

#include "Vulkan.hpp"

#include "Buffer.hpp"

#include <array>
#include <span>
#include <string>
#include <vector>

struct ClearDepthStencil
{
	float    Depth   = 1.0f;
	uint32_t Stencil = 0;
};

union ClearValue
{
	ClearColorValue   Color = { .Float32 = { 0.0f, 0.0f, 0.0f, 1.0f } };
	ClearDepthStencil DepthStencil;
};

struct RenderingAttachmentInfo
{
	VkImageView   ImageView  = VK_NULL_HANDLE;
	LoadOp        LoadOp     = LoadOp::Clear;
	StoreOp       StoreOp    = StoreOp::Store;
	ClearValue    ClearValue = {};
	VkImageLayout Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	ResolveMode   ResolveMode        = ResolveMode::None;
	VkImageView   ResolveImageView   = VK_NULL_HANDLE;
	VkImageLayout ResolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct RenderingInfo
{
	std::span<const RenderingAttachmentInfo> ColorAttachments;
	const RenderingAttachmentInfo*           DepthAttachment   = nullptr;
	const RenderingAttachmentInfo*           StencilAttachment = nullptr;
	ScissorRect                              RenderArea        = {};
	uint32_t                                 LayerCount        = 1;
	VkRenderingFlags                         Flags             = 0;
};

struct StencilFaceState
{
	StencilOp FailOp      = StencilOp::Keep;
	StencilOp PassOp      = StencilOp::Keep;
	StencilOp DepthFailOp = StencilOp::Keep;
	CompareOp Compare     = CompareOp::Always;
	uint32_t  CompareMask = 0xFF;
	uint32_t  WriteMask   = 0xFF;
	uint32_t  Reference   = 0;
};

struct StencilState
{
	bool             Enable = false;
	StencilFaceState Front;
	StencilFaceState Back;
};

struct GraphicsState
{
	VertexBufferLayout VertexLayout;

	Topology    PrimitiveTopology = Topology::Triangle;
	CompareOp   DepthCompare      = CompareOp::Less;
	BlendMode   Blending          = BlendMode::None;
	CullMode    CullMode          = CullMode::Back;
	WindingMode FrontFace         = WindingMode::CCW;
	PolygonMode PolygonMode       = PolygonMode::Fill;

	StencilState Stencil;

	bool  DepthTest        = true;
	bool  DepthWrite       = true;
	bool  DepthBias        = false;
	bool  PrimitiveRestart = false;
	float LineWidth        = 1.0f;
};

class CommandPool;

class CommandBuffer
{
public:
	CommandBuffer() = default;

	void Begin(bool oneTimeSubmit = false);
	void End();

	// ==== Recording commands ====

	void BeginRendering(const RenderingInfo& info);
	void EndRendering();
	void SetGraphicsState(const GraphicsState& state, uint32_t colorAttachmentCount);

	void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t baseInstance = 0);
	void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t baseInstance = 0);

	void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ = 1);

	// ==== Indirect ====

	void DrawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride = 0);
	void DrawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride = 0);
	void DrawIndirectCount(VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countOffset, uint32_t maxDrawCount, uint32_t stride = 0);
	void DrawIndexedIndirectCount(VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countOffset, uint32_t maxDrawCount, uint32_t stride = 0);
	void DispatchIndirect(VkBuffer buffer, VkDeviceSize offset = 0);

	// ==== Mesh shading ====

	void DrawMeshTasks(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);
	void DrawMeshTasksIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride = 0);
	void DrawMeshTasksIndirectCount(VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countOffset, uint32_t maxDrawCount, uint32_t stride = 0);

	// ==== Bindings ====

	void BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset = 0, uint32_t binding = 0);
	void BindIndexBuffer(VkBuffer buffer, IndexFormat format, VkDeviceSize offset = 0);

	void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, const void* data, uint32_t size, uint32_t offset = 0);
	template<typename T>
	void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, const T& data, uint32_t offset = 0)
	{
		PushConstants(layout, stages, &data, static_cast<uint32_t>(sizeof(T)), offset);
	}

	// ==== Dynamic state overrides ====

	void SetViewport(const Viewport& viewport);
	void SetScissor(const ScissorRect& rect);
	void SetDepthBias(float constantFactor, float slopeFactor, float clamp = 0.0f);
	void SetBlendConstants(const std::array<float, 4>& constants);

	// ==== Buffer transfers ====

	void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);
	void FillBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t value);
	void UpdateBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data);
	template<typename T>
	void UpdateBuffer(VkBuffer buffer, const T& data, VkDeviceSize offset = 0)
	{
		UpdateBuffer(buffer, offset, sizeof(T), &data);
	}

	// ==== Images ====

	void ClearColorImage(VkImage image, VkImageLayout layout, const ClearColorValue& color, VkImageSubresourceRange range);
	void CopyImage(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageCopy2& region);
	void BlitImage(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageBlit2& region, VkFilter filter = VK_FILTER_LINEAR);

	void CopyBufferToImage(VkBuffer src, VkImage dst, VkImageLayout dstLayout, const VkBufferImageCopy2& region);
	void CopyImageToBuffer(VkImage src, VkImageLayout srcLayout, VkBuffer dst, const VkBufferImageCopy2& region);

	// ==== Queries ====

	void ResetQueryPool(VkQueryPool pool, uint32_t firstQuery, uint32_t queryCount);
	void WriteTimestamp(VkQueryPool pool, uint32_t query, VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

	// ==== Debug labels ====

	void PushDebugLabel(const char* label, uint32_t colorRGBA = 0xffffffff) const;
	void PopDebugLabel() const;
	void InsertDebugLabel(const char* label, uint32_t colorRGBA = 0xffffffff) const;

	// ==== Barriers ====

	void PipelineBarrier(const VkDependencyInfo& dep);

	void        ImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange range, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage);
	inline void ImageBarrier(VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage)
	{
		ImageBarrier(image, oldLayout, newLayout,
			VkImageSubresourceRange
			{
				.aspectMask     = aspectMask,
				.baseMipLevel   = 0,
				.levelCount     = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount     = VK_REMAINING_ARRAY_LAYERS
			},
			srcStage, dstStage);
	}

	void        BufferBarrier(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess, uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
	inline void BufferBarrier(VkBuffer buffer, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess, uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
	{
		BufferBarrier(buffer, 0, VK_WHOLE_SIZE, srcStage, srcAccess, dstStage, dstAccess, srcQueueFamilyIndex, dstQueueFamilyIndex);
	}

	// ==== Raw handle ====

	VkCommandBuffer GetHandle() const { return m_Handle; }
	bool            IsValid() const { return m_Handle != VK_NULL_HANDLE; }

private:
	friend class CommandPool;
	friend class UploadContext;

	explicit CommandBuffer(VkCommandBuffer handle) : m_Handle(handle) {}

	VkCommandBuffer m_Handle = VK_NULL_HANDLE;
};

class DebugLabelScope
{
public:
	explicit DebugLabelScope(const CommandBuffer& commandBuffer, const char* label, uint32_t colorRGBA = 0xffffffff);
	~DebugLabelScope();

	DebugLabelScope(const DebugLabelScope&)            = delete;
	DebugLabelScope& operator=(const DebugLabelScope&) = delete;

private:
	const CommandBuffer& m_CommandBuffer;
};

class CommandPool
{
public:
	static constexpr uint32_t MAX_COMMAND_BUFFERS = 16;

	CommandPool() = default;
	~CommandPool();

	void Create(uint32_t queueFamilyIndex, uint32_t commandBufferCount = MAX_COMMAND_BUFFERS);
	void Destroy();
	void Reset();

	// Returns the next available preallocated command buffer.
	// Asserts if all buffers have been acquired.
	CommandBuffer& Acquire();

	// All command buffers acquired this frame in acquisition order.
	std::span<CommandBuffer> GetAcquiredCommandBuffers() { return std::span<CommandBuffer>(m_CommandBuffers.data(), static_cast<size_t>(m_NextCommandBuffer)); }

	uint32_t GetAcquiredCount() const { return m_NextCommandBuffer; }

	VkCommandPool GetHandle() const { return m_Handle; }

private:
	VkCommandPool m_Handle = VK_NULL_HANDLE;

	std::vector<CommandBuffer> m_CommandBuffers;
	uint32_t                   m_NextCommandBuffer = 0;
};
