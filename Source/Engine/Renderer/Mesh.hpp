#pragma once

#include "Vulkan/Buffer.hpp"
#include "Material.hpp"
#include "Vulkan/Texture.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoord;
	glm::vec4 Tangent;
};

struct Submesh
{
	uint32_t BaseVertex    = 0;
	uint32_t BaseIndex     = 0;
	uint32_t IndexCount    = 0;
	uint32_t VertexCount   = 0;
	uint32_t MaterialIndex = UINT32_MAX;

	glm::mat4 Transform      { 1.0f }; // baked world transform
	glm::mat4 LocalTransform { 1.0f }; // node local transform

	std::string MeshName;
	std::string NodeName;
};

struct Node
{
	uint32_t              Parent = UINT32_MAX;
	std::vector<uint32_t> Children;
	std::vector<uint32_t> Submeshes;
	std::string           Name;
	glm::mat4             LocalTransform { 1.0f };

	bool IsRoot() const { return Parent == UINT32_MAX; }
};

enum class MeshType : uint8_t
{
	Static  = 0,
	Skinned = 1
};

class Mesh
{
public:
	Mesh()  = default;
	~Mesh() { Destroy(); }

	Mesh(const Mesh&)            = delete;
	Mesh& operator=(const Mesh&) = delete;

	bool Load(const std::filesystem::path& path);
	void Destroy();

	bool IsValid()   const { return m_VertexBuffer.GetHandle() != VK_NULL_HANDLE; }
	bool IsStatic()  const { return m_MeshType == MeshType::Static;  }
	bool IsSkinned() const { return m_MeshType == MeshType::Skinned; }

	MeshType GetMeshType() const { return m_MeshType; }

	VkBuffer GetVertexBuffer() const { return m_VertexBuffer.GetHandle(); }
	VkBuffer GetIndexBuffer()  const { return m_IndexBuffer.GetHandle();  }

	const std::vector<Submesh>&                   GetSubmeshes() const { return m_Submeshes; }
	const std::vector<Node>&                      GetNodes()     const { return m_Nodes;     }
	const std::vector<uint32_t>&                  GetRootNodes() const { return m_RootNodes; }
	const std::vector<std::shared_ptr<Material>>& GetMaterials() const { return m_Materials; }
	const std::vector<std::shared_ptr<Texture>>&  GetTextures()  const { return m_Textures;  }

	const std::string& GetName()      const { return m_Name;      }
	const std::string& GetSceneName() const { return m_SceneName; }

private:
	void BakeTransforms();

	std::string m_Name;
	std::string m_SceneName;

	MeshType m_MeshType = MeshType::Static;

	std::vector<Submesh>                   m_Submeshes;
	std::vector<Node>                      m_Nodes;
	std::vector<uint32_t>                  m_RootNodes;
	std::vector<std::shared_ptr<Material>> m_Materials;
	std::vector<std::shared_ptr<Texture>>  m_Textures;

	Buffer m_VertexBuffer;
	Buffer m_IndexBuffer;
};
