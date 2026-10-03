#include "Material.hpp"

#include <algorithm>
#include <cstring>

Material::Material(std::shared_ptr<Shader> shader)
	: m_Shader(std::move(shader))
{
	InitializeStorage();
}

void Material::SetShader(std::shared_ptr<Shader> shader)
{
	m_Shader = std::move(shader);
	InitializeStorage();
	MarkDirty();
}

void Material::InitializeStorage()
{
	m_UniformStorage.clear();

	if (!m_Shader)
		return;

	const auto& ranges = m_Shader->GetPushConstantRanges();

	if (ranges.empty())
		return;

	uint32_t storageSize = 0;

	for (const auto& range : ranges)
		storageSize = std::max(storageSize, range.Offset + range.Size);

	if (storageSize == 0)
		return;

	m_UniformStorage.resize(storageSize, 0);
}

bool Material::SetUniformData(const std::string& name, const void* data, uint32_t size)
{
	if (!m_Shader)
		return false;

	if (m_UniformStorage.empty())
		InitializeStorage();

	if (m_UniformStorage.empty())
		return false;

	for (const auto& member : m_Shader->GetPushConstantMembers())
	{
		if (member.Name != name)
			continue;

		if (member.Size != size || member.Offset + size > m_UniformStorage.size())
			return false;

		std::memcpy(m_UniformStorage.data() + member.Offset, data, size);
		return true;
	}

	return false;
}

bool Material::GetUniformData(const std::string& name, void* outData, uint32_t size) const
{
	if (!m_Shader || m_UniformStorage.empty())
		return false;

	for (const auto& member : m_Shader->GetPushConstantMembers())
	{
		if (member.Name != name)
			continue;

		if (member.Size != size || member.Offset + size > m_UniformStorage.size())
			return false;

		std::memcpy(outData, m_UniformStorage.data() + member.Offset, size);
		return true;
	}

	return false;
}

void Material::AddTexture(const MapInfo& map)
{
	m_Maps.push_back(map);
	MarkDirty();
}

void Material::SetTexture(const MapInfo& map)
{
	for (MapInfo& existing : m_Maps)
	{
		if (existing.Type != map.Type)
			continue;

		existing = map;
		MarkDirty();
		return;
	}

	m_Maps.push_back(map);
	MarkDirty();
}

std::optional<uint32_t> Material::GetUVIndex(MapType type) const
{
	for (const MapInfo& map : m_Maps)
	{
		if (map.Type == type)
			return map.UvIndex;
	}
	return std::nullopt;
}

void Material::SetMapEnabled(MapType type, bool enable)
{
	for (MapInfo& map : m_Maps)
	{
		if (map.Type != type)
			continue;

		map.Enabled = enable;
		MarkDirty();
		return;
	}
}

bool Material::IsMapEnabled(MapType type) const
{
	for (const MapInfo& map : m_Maps)
	{
		if (map.Type == type)
			return map.Enabled;
	}
	return false;
}

void Material::UpdateGPUData()
{
	m_GPUData.AlbedoIndex            = 0;
	m_GPUData.NormalIndex            = 0;
	m_GPUData.MetallicRoughnessIndex = 0;
	m_GPUData.OcclusionIndex         = 0;
	m_GPUData.EmissiveIndex          = 0;
	m_GPUData.Flags                  = 0;

	for (const MapInfo& map : m_Maps)
	{
		if (!map.Enabled || !map.Texture)
			continue;

		const uint32_t index = map.Texture->GetBindlessIndex();

		if (index == Descriptor::INVALID_INDEX)
			continue;

		uint32_t* slot       = nullptr;
		uint32_t  flagBit    = 0;
		uint32_t  uvBitShift = 0;

		switch (map.Type)
		{
			case MapType::Albedo:
				slot       = &m_GPUData.AlbedoIndex;
				flagBit    = 3;
				uvBitShift = 16;
				break;
			case MapType::Normal:
				slot       = &m_GPUData.NormalIndex;
				flagBit    = 0;
				uvBitShift = 18;
				break;
			case MapType::MetallicRoughness:
				slot       = &m_GPUData.MetallicRoughnessIndex;
				flagBit    = 1;
				uvBitShift = 20;
				break;
			case MapType::Occlusion:
				slot       = &m_GPUData.OcclusionIndex;
				flagBit    = 2;
				uvBitShift = 22;
				break;
			case MapType::Emissive:
				slot       = &m_GPUData.EmissiveIndex;
				flagBit    = 4;
				uvBitShift = 24;
				break;
			default:
				continue;
		}

		*slot             = index;
		m_GPUData.Flags  |= (1u << flagBit);
		m_GPUData.Flags  |= ((map.UvIndex & 0x3u) << uvBitShift);
	}
}

const char* Material::ToString(MapType type)
{
	switch (type)
	{
		case MapType::Albedo:            return "Albedo";
		case MapType::Normal:            return "Normal";
		case MapType::MetallicRoughness: return "MetallicRoughness";
		case MapType::Occlusion:         return "Occlusion";
		case MapType::Emissive:          return "Emissive";
		default:                         return "Unknown";
	}
}
