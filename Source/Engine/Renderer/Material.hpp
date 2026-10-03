#pragma once

#include "Vulkan/Shader.hpp"
#include "Vulkan/Texture.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>

enum class MapType : uint8_t
{
	Albedo = 0,
	Normal,
	MetallicRoughness,
	Occlusion,
	Emissive,
};

struct MapInfo
{
	std::shared_ptr<Texture> Texture;

	MapType Type = MapType::Albedo;
	uint32_t UvIndex = 0;
	bool     Enabled = true;
};

// GPU-friendly material data (scalar); must match the shader-side struct.
struct GPUMaterialData
{
	glm::vec4 Albedo{ 1.0f, 1.0f, 1.0f, 1.0f };

	// Texture Indices (Bindless; slot 0 = reserved null/white)
	uint32_t AlbedoIndex = 0;
	uint32_t NormalIndex = 0;
	uint32_t MetallicRoughnessIndex = 0;
	uint32_t OcclusionIndex = 0;
	uint32_t EmissiveIndex = 0;

	// Factors
	float Metalness = 0.0f;
	float Roughness = 0.5f;
	float AlphaCutoff = 0.5f;
	uint32_t Flags = 0;

	glm::vec4 Emissive = { 0.0f, 0.0f, 0.0f, 1.0f };

	float Transmission = 0.0f;
};
static_assert(sizeof(GPUMaterialData) == 72);

enum class MaterialRenderMode : uint8_t
{
	Opaque,
	Cutout,
	Transparent,
	Fade
};

class Material
{
public:
	Material() = default;
	explicit Material(std::shared_ptr<Shader> shader);

	// Shader
	void SetShader(std::shared_ptr<Shader> shader);
	const std::shared_ptr<Shader>& GetShader() const { return m_Shader; }

	// Push constant access
	template<typename T>
	void Set(const std::string& name, const T& value)
	{
		SetUniformData(name, &value, sizeof(T));
	}

	template<typename T>
	T Get(const std::string& name, T defaultValue = T()) const
	{
		T value;
		return GetUniformData(name, &value, sizeof(T)) ? value : defaultValue;
	}

	const std::vector<uint8_t>& GetUniformStorage() const { return m_UniformStorage; }

	// Texture maps
	void AddTexture(const MapInfo& map);
	void SetTexture(const MapInfo& map);

	const std::vector<MapInfo>& GetTextures() const { return m_Maps; }

	std::optional<uint32_t> GetUVIndex(MapType type) const;

	void SetMapEnabled(MapType type, bool enable);
	bool IsMapEnabled(MapType type) const;

	// Render mode
	MaterialRenderMode GetRenderMode() const { return m_RenderMode; }

	void SetRenderMode(MaterialRenderMode mode)
	{
		m_RenderMode = mode;

		switch (mode)
		{
			case MaterialRenderMode::Opaque:
			case MaterialRenderMode::Cutout:
				m_BlendMode = BlendMode::None;
				break;

			case MaterialRenderMode::Transparent:
			case MaterialRenderMode::Fade:
				m_BlendMode = BlendMode::Alpha;
				break;
		}

		MarkDirty();
	}

	bool IsTransparent()   const { return m_RenderMode == MaterialRenderMode::Transparent || m_RenderMode == MaterialRenderMode::Fade; }
	bool IsTransmissive()   const { return m_GPUData.Transmission > 0.0f; }
	bool NeedsForwardPass() const { return IsTransparent() || IsTransmissive(); }

	// Blending
	BlendMode GetBlendMode() const { return m_BlendMode; }

	void SetBlendMode(BlendMode mode)
	{
		m_BlendMode = mode;
		MarkDirty();
	}

	// Face culling
	CullMode GetCullMode() const { return m_CullMode; }

	void SetCullMode(CullMode mode)
	{
		m_CullMode = mode;
		MarkDirty();
	}

	// Alpha cutoff - used when RenderMode == Cutout
	float GetAlphaCutoff() const          { return m_GPUData.AlphaCutoff; }
	void  SetAlphaCutoff(float cutoff)    { m_GPUData.AlphaCutoff = cutoff; MarkDirty(); }

	// PBR factors
	glm::vec4 GetColor()    const { return m_GPUData.Albedo; }
	float     GetMetalness() const { return m_GPUData.Metalness; }
	float     GetRoughness() const { return m_GPUData.Roughness; }
	float     GetTransmission() const { return m_GPUData.Transmission; }
	glm::vec3 GetEmissiveColor() const { return glm::vec3(m_GPUData.Emissive); }
	float     GetEmissiveStrength() const { return m_GPUData.Emissive.a; }

	void SetColor(const glm::vec4& color)         { m_GPUData.Albedo = color;                                     MarkDirty(); }
	void SetMetalness(float metalness)            { m_GPUData.Metalness = metalness;                              MarkDirty(); }
	void SetRoughness(float roughness)            { m_GPUData.Roughness = roughness;                              MarkDirty(); }
	void SetTransmission(float transmission)      { m_GPUData.Transmission = transmission;                        MarkDirty(); }
	void SetEmissiveColor(const glm::vec3& color) { m_GPUData.Emissive = glm::vec4(color, m_GPUData.Emissive.a);  MarkDirty(); }
	void SetEmissiveStrength(float strength)      { m_GPUData.Emissive.a = strength;                              MarkDirty(); }

	// GPU data
	const GPUMaterialData& GetGPUData() const { return m_GPUData; }
	void UpdateGPUData();

	// Revision tracking
	uint64_t GetRevision() const { return m_Revision; }
	void     MarkDirty()         { ++m_Revision;      }

	static const char* ToString(MapType type);
private:
	bool SetUniformData(const std::string& name, const void* data, uint32_t size);
	bool GetUniformData(const std::string& name, void* outData, uint32_t size) const;
	void InitializeStorage();
private:
	std::shared_ptr<Shader> m_Shader;

	std::vector<uint8_t> m_UniformStorage;
	std::vector<MapInfo> m_Maps;

	GPUMaterialData m_GPUData;

	MaterialRenderMode m_RenderMode = MaterialRenderMode::Opaque;

	// Graphics state applied by the renderer when drawing.
	BlendMode m_BlendMode = BlendMode::None;
	CullMode  m_CullMode  = CullMode::Back;

	uint64_t m_Revision = 1;
};

inline std::ostream& operator<<(std::ostream& os, const MapType type)
{
	return os << Material::ToString(type);
}
