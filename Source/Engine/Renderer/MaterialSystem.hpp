#pragma once

#include "Material.hpp"
#include "Vulkan/Buffer.hpp"
#include "Renderer.hpp"

#include <array>
#include <unordered_map>
#include <vector>

class MaterialSystem
{
public:
	static void Initialize();
	static void Shutdown();

	static void UploadPendingMaterials(uint32_t frameIndex);
	static void Clear();

	static uint32_t PrepareMaterial(const std::shared_ptr<Material>& material);

	static const Buffer& GetBuffer(uint32_t frameIndex) { return s_Buffers[frameIndex]; }

	static constexpr uint32_t FallbackIndex = 0;

private:
	static constexpr uint32_t MAX_MATERIALS      = 16384;
	static constexpr uint32_t MATERIAL_SIZE      = sizeof(GPUMaterialData);
	static constexpr uint32_t FRAMES_IN_FLIGHT   = Renderer::GetFramesInFlight();

	struct MaterialSlot
	{
		std::shared_ptr<Material>              MaterialRef;
		std::array<uint64_t, FRAMES_IN_FLIGHT> UploadedRevisions{};
	};

	static std::array<Buffer, FRAMES_IN_FLIGHT> s_Buffers;

	static std::vector<MaterialSlot> s_Slots;
	static std::vector<uint32_t>     s_FreeIndices;

	static std::unordered_map<const Material*, uint32_t> s_MaterialIndex;
};
