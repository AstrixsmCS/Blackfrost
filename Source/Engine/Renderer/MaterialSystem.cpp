#include "MaterialSystem.hpp"

#include <cassert>
#include <print>

std::array<Buffer, MaterialSystem::FRAMES_IN_FLIGHT> MaterialSystem::s_Buffers;
std::vector<MaterialSystem::MaterialSlot>             MaterialSystem::s_Slots;
std::vector<uint32_t>                                 MaterialSystem::s_FreeIndices;
std::unordered_map<const Material*, uint32_t>         MaterialSystem::s_MaterialIndex;

void MaterialSystem::Initialize()
{
	s_Slots.resize(MAX_MATERIALS);

	s_FreeIndices.reserve(MAX_MATERIALS - 1);
	for (uint32_t i = MAX_MATERIALS; i > 1; --i)
		s_FreeIndices.push_back(i - 1);

	const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(MAX_MATERIALS) * MATERIAL_SIZE;

	for (Buffer& buffer : s_Buffers)
	{
		buffer.Create(
		{
			.DebugName = "Material Buffer",
			.Usage     = BufferUsage::Storage,
			.Memory    = BufferMemory::HostVisible,
			.Size      = bufferSize
		});
	}

	// Upload default fallback material to all frame buffers
	const GPUMaterialData fallback{};
	for (Buffer& buffer : s_Buffers)
		buffer.SetData(&fallback, MATERIAL_SIZE, 0);
}

void MaterialSystem::Shutdown()
{
	s_MaterialIndex.clear();
	s_Slots.clear();
	s_FreeIndices.clear();

	for (Buffer& buffer : s_Buffers)
		buffer.Destroy();
}

uint32_t MaterialSystem::PrepareMaterial(const std::shared_ptr<Material>& material)
{
	if (!material)
		return FallbackIndex;

	const auto it = s_MaterialIndex.find(material.get());
	if (it != s_MaterialIndex.end())
		return it->second;

	if (s_FreeIndices.empty())
	{
		std::println("[MaterialSystem] Out of material slots, returning fallback.");
		return FallbackIndex;
	}

	const uint32_t index = s_FreeIndices.back();
	s_FreeIndices.pop_back();

	s_Slots[index].MaterialRef       = material;
	s_Slots[index].UploadedRevisions = {};

	s_MaterialIndex[material.get()] = index;

	return index;
}

void MaterialSystem::UploadPendingMaterials(uint32_t frameIndex)
{
	assert(frameIndex < FRAMES_IN_FLIGHT);

	Buffer& buffer = s_Buffers[frameIndex];

	for (uint32_t index = 1; index < MAX_MATERIALS; ++index)
	{
		MaterialSlot& slot = s_Slots[index];

		if (!slot.MaterialRef)
			continue;

		const uint64_t currentRevision = slot.MaterialRef->GetRevision();

		if (slot.UploadedRevisions[frameIndex] == currentRevision)
			continue;

		slot.MaterialRef->UpdateGPUData();

		const GPUMaterialData& data   = slot.MaterialRef->GetGPUData();
		const VkDeviceSize     offset = static_cast<VkDeviceSize>(index) * MATERIAL_SIZE;

		buffer.SetData(&data, MATERIAL_SIZE, offset);

		slot.UploadedRevisions[frameIndex] = currentRevision;
	}
}

void MaterialSystem::Clear()
{
	for (uint32_t index = 1; index < MAX_MATERIALS; ++index)
	{
		MaterialSlot& slot = s_Slots[index];

		if (!slot.MaterialRef)
			continue;

		slot.MaterialRef.reset();
		slot.UploadedRevisions = {};
	}

	s_MaterialIndex.clear();

	s_FreeIndices.clear();
	s_FreeIndices.reserve(MAX_MATERIALS - 1);
	for (uint32_t i = MAX_MATERIALS; i > 1; --i)
		s_FreeIndices.push_back(i - 1);
}
