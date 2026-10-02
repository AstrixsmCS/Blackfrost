#pragma once

#include "Vulkan/Vulkan.hpp"
#include "Vulkan/SwapChain.hpp"
#include "Vulkan/FrameData.hpp"

#include "Vulkan/Texture.hpp"

#include <memory>

struct SDL_Window;

class ShaderLibrary;

class Renderer
{
public:
	static void Initialize(SDL_Window* windowHandle);
	static void Shutdown();

	static ShaderLibrary& GetShaderLibrary();

	static void WaitForGPU();

	static SwapChain&     GetSwapChain()           { return *s_SwapChain; }
	static FrameContext&  GetCurrentFrame()         { return s_FrameData.Current(); }
	static FrameContext&  GetPreviousFrame()        { return s_FrameData.Previous(); }

	static CommandBuffer& AcquireCommandBuffer(bool dedicatedCompute = false);

	static bool HasAsyncCompute() { return s_FrameData.HasAsyncCompute(); }

	static uint32_t GetFrameSlot()      { return s_FrameData.GetFrameSlot(); }
	static uint64_t GetFrameIndex()     { return s_FrameData.GetFrameIndex(); }
	static uint32_t GetCurrentImage()   { return s_CurrentImageIndex; }
	static constexpr uint32_t GetFramesInFlight() { return MAX_FRAMES_IN_FLIGHT; }

	// ==== ~Actual~ Renderer here... TODO: remove confusion later ====

	static bool BeginFrame();
	static void EndFrame();
	static void Present();
private:
	inline static std::unique_ptr<SwapChain> s_SwapChain;
	inline static FrameData                  s_FrameData;
	inline static uint32_t                   s_CurrentImageIndex = UINT32_MAX;
};
