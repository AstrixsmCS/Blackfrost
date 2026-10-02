#include "Renderer.hpp"
#include "Vulkan/Context.hpp"

#include "Vulkan/Descriptors.hpp"
#include "Vulkan/Shader.hpp"

#include <algorithm>
#include <array>
#include <format>

struct RendererData
{
	ShaderLibrary m_ShaderLibrary;
};

static RendererData *s_Data = nullptr;

void Renderer::Initialize(SDL_Window *windowHandle)
{
	s_Data = new RendererData;

	Context::Initialize();

	Descriptor::Initialize();

	s_SwapChain = std::make_unique<SwapChain>(windowHandle);
	s_SwapChain->Initialize();

	s_FrameData.Initialize();

	// ==== Shaders ====

	ShaderLibrary &shaders = Renderer::GetShaderLibrary();
}

void Renderer::Shutdown()
{
	WaitForGPU();

	delete s_Data;

	s_FrameData.Shutdown();
	s_SwapChain.reset();

	Descriptor::Shutdown();

	Context::Shutdown();
}

ShaderLibrary &Renderer::GetShaderLibrary()
{
	return s_Data->m_ShaderLibrary;
}

void Renderer::WaitForGPU()
{
	vkDeviceWaitIdle(Context::Get().GetDevice());
}

CommandBuffer &Renderer::AcquireCommandBuffer(bool dedicatedCompute)
{
	assert(!dedicatedCompute || s_FrameData.HasAsyncCompute());

	CommandBuffer &commandBuffer = GetCurrentFrame().AcquireCommandBuffer(dedicatedCompute);
	commandBuffer.Begin();

	return commandBuffer;
}

bool Renderer::BeginFrame()
{
	const uint32_t frameSlot = s_FrameData.GetFrameSlot();

	s_FrameData.WaitForSlot();

	s_CurrentImageIndex = s_SwapChain->AcquireNextImage(frameSlot);

	if (s_CurrentImageIndex == UINT32_MAX)
		return false;

	GetCurrentFrame().Reset(s_FrameData.HasAsyncCompute());

	return true;
}

void Renderer::EndFrame()
{
	const uint32_t frameSlot = s_FrameData.GetFrameSlot();
	const bool hasAsyncCompute = s_FrameData.HasAsyncCompute();
	FrameContext &frame = GetCurrentFrame();

	const uint64_t graphicsSignalValue = s_FrameData.NextGraphicsSignalValue();
	const uint64_t computeSignalValue = (hasAsyncCompute && frame.HasComputeWork()) ? s_FrameData.NextComputeSignalValue() : 0;

	const FrameSubmitInfo submitInfo
	{
		.GraphicsQueue = Context::Get().GetGraphicsQueue(),
		.ComputeQueue = hasAsyncCompute ? Context::Get().GetComputeQueue() : VK_NULL_HANDLE,

		.ImageAvailable = s_SwapChain->GetImageAvailableSemaphore(frameSlot),
		.RenderFinished = s_SwapChain->GetRenderFinishedSemaphore(s_CurrentImageIndex),

		.GraphicsTimeline = s_FrameData.GetGraphicsTimeline().GetHandle(),
		.ComputeTimeline = hasAsyncCompute ? s_FrameData.GetComputeTimeline().GetHandle() : VK_NULL_HANDLE,

		.GraphicsSignalValue = graphicsSignalValue,
		.ComputeSignalValue = computeSignalValue,

		// Cross-queue dependencies are intentionally 0.
		// The render graph will set these when resources are shared between queues.
		.ComputeWaitGraphicsValue = 0,
		.GraphicsWaitComputeValue = hasAsyncCompute && frame.HasComputeWork() ? computeSignalValue : 0,
	};

	frame.Submit(submitInfo);
}

void Renderer::Present()
{
	s_SwapChain->Present(s_FrameData.GetFrameSlot());
	s_FrameData.Advance();
}
