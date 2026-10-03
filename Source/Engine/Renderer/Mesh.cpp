#include "Mesh.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <meshoptimizer.h>
#include <stb/stb_image.h>

#include <cassert>
#include <format>
#include <print>

// ==== glTF helpers ====

static glm::mat4 NodeToMatrix(const fastgltf::Node& node)
{
	if (const auto* trs = std::get_if<fastgltf::TRS>(&node.transform))
	{
		const glm::vec3 translation = glm::make_vec3(trs->translation.data());
		const glm::quat rotation    = glm::make_quat(trs->rotation.data());
		const glm::vec3 scale       = glm::make_vec3(trs->scale.data());

		return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
	}

	if (const auto* matrix = std::get_if<fastgltf::math::fmat4x4>(&node.transform))
		return glm::make_mat4(matrix->data());

	return glm::mat4(1.0f);
}

static void MarkLinear(std::vector<VkFormat>& textureFormats, const fastgltf::TextureInfo& textureInfo)
{
	if (textureInfo.textureIndex < textureFormats.size())
		textureFormats[textureInfo.textureIndex] = VK_FORMAT_R8G8B8A8_UNORM;
}

static stbi_uc* DecodeImage(const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& directory, int& width, int& height)
{
	int channels = 0;

	if (const auto* uri = std::get_if<fastgltf::sources::URI>(&image.data))
	{
		const std::filesystem::path imagePath = directory / uri->uri.path();
		return stbi_load(imagePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
	}

	if (const auto* array = std::get_if<fastgltf::sources::Array>(&image.data))
	{
		return stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(array->bytes.data()), static_cast<int>(array->bytes.size()),
			&width, &height, &channels, STBI_rgb_alpha);
	}

	if (const auto* bufferView = std::get_if<fastgltf::sources::BufferView>(&image.data))
	{
		const fastgltf::BufferView& view   = asset.bufferViews[bufferView->bufferViewIndex];
		const fastgltf::Buffer&     buffer = asset.buffers[view.bufferIndex];

		if (const auto* array = std::get_if<fastgltf::sources::Array>(&buffer.data))
		{
			return stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(array->bytes.data() + view.byteOffset), static_cast<int>(view.byteLength),
				&width, &height, &channels, STBI_rgb_alpha);
		}
	}

	return nullptr;
}

static void SetMaterialTexture(Material& material, const std::vector<std::shared_ptr<Texture>>& textures, const fastgltf::TextureInfo& textureInfo, MapType type)
{
	const size_t textureIndex = textureInfo.textureIndex;

	if (textureIndex >= textures.size() || !textures[textureIndex])
		return;

	material.SetTexture(
	{
		.Texture = textures[textureIndex],
		.Type    = type,
		.UvIndex = static_cast<uint32_t>(textureInfo.texCoordIndex),
		.Enabled = true
	});
}

static void ProcessPrimitive(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, bool generateTangents)
{
	if (vertices.empty() || indices.empty())
		return;

	if (generateTangents)
	{
		std::vector<float> cornerTangents(indices.size() * 4);

		meshopt_generateTangents(cornerTangents.data(), indices.data(), indices.size(),
			&vertices[0].Position.x, vertices.size(), sizeof(Vertex),
			&vertices[0].Normal.x,   sizeof(Vertex),
			&vertices[0].TexCoord.x, sizeof(Vertex),
			0);

		std::vector<Vertex> cornerVertices(indices.size());

		for (size_t i = 0; i < indices.size(); i++)
		{
			cornerVertices[i]         = vertices[indices[i]];
			cornerVertices[i].Tangent = glm::make_vec4(&cornerTangents[i * 4]);
			indices[i]                = static_cast<uint32_t>(i);
		}

		vertices = std::move(cornerVertices);
	}

	std::vector<uint32_t> remap(vertices.size());

	const size_t uniqueCount = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(Vertex));

	std::vector<Vertex> uniqueVertices(uniqueCount);
	meshopt_remapVertexBuffer(uniqueVertices.data(), vertices.data(), vertices.size(), sizeof(Vertex), remap.data());
	meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());

	vertices = std::move(uniqueVertices);

	meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
	meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(Vertex));
}

// ==== Mesh ====

void Mesh::BakeTransforms()
{
	struct PendingNode
	{
		uint32_t  Index;
		glm::mat4 ParentTransform;
	};

	std::vector<PendingNode> stack;
	stack.reserve(m_Nodes.size());

	for (uint32_t root : m_RootNodes)
		stack.push_back({ root, glm::mat4(1.0f) });

	while (!stack.empty())
	{
		const PendingNode pending = stack.back();
		stack.pop_back();

		const Node&     node           = m_Nodes[pending.Index];
		const glm::mat4 worldTransform = pending.ParentTransform * node.LocalTransform;

		for (uint32_t submeshIndex : node.Submeshes)
		{
			Submesh& submesh       = m_Submeshes[submeshIndex];
			submesh.Transform      = worldTransform;
			submesh.LocalTransform = node.LocalTransform;
			submesh.NodeName       = node.Name;
		}

		for (uint32_t childIndex : node.Children)
			stack.push_back({ childIndex, worldTransform });
	}
}

bool Mesh::Load(const std::filesystem::path& path)
{
	if (!std::filesystem::exists(path))
	{
		std::println("[Mesh] File not found: {}", path.string());
		return false;
	}

	constexpr fastgltf::Options options =
		fastgltf::Options::GenerateMeshIndices |
		fastgltf::Options::LoadExternalBuffers;

	fastgltf::Parser parser(
		fastgltf::Extensions::KHR_materials_transmission |
		fastgltf::Extensions::KHR_materials_emissive_strength);

	auto dataResult = fastgltf::GltfDataBuffer::FromPath(path);
	if (dataResult.error() != fastgltf::Error::None)
	{
		std::println("[Mesh] Failed to read file '{}': {}", path.string(), fastgltf::getErrorMessage(dataResult.error()));
		return false;
	}

	const std::string ext = path.extension().string();

	fastgltf::Expected<fastgltf::Asset> assetResult { fastgltf::Error::None };

	if (ext == ".glb")
		assetResult = parser.loadGltfBinary(dataResult.get(), path.parent_path(), options);
	else if (ext == ".gltf")
		assetResult = parser.loadGltf(dataResult.get(), path.parent_path(), options);
	else
	{
		std::println("[Mesh] Unsupported extension '{}': expected .gltf or .glb", ext);
		return false;
	}

	if (assetResult.error() != fastgltf::Error::None)
	{
		std::println("[Mesh] Failed to parse '{}': {}", path.string(), fastgltf::getErrorMessage(assetResult.error()));
		return false;
	}

	const fastgltf::Asset& asset = assetResult.get();

	m_Name = path.stem().string();

	// ==== Texture color spaces ====

	std::vector<VkFormat> textureFormats(asset.textures.size(), VK_FORMAT_R8G8B8A8_SRGB);

	for (const fastgltf::Material& gltfMaterial : asset.materials)
	{
		if (gltfMaterial.normalTexture.has_value())
			MarkLinear(textureFormats, gltfMaterial.normalTexture.value());

		if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value())
			MarkLinear(textureFormats, gltfMaterial.pbrData.metallicRoughnessTexture.value());

		if (gltfMaterial.occlusionTexture.has_value())
			MarkLinear(textureFormats, gltfMaterial.occlusionTexture.value());
	}

	// ==== Textures ====

	m_Textures.reserve(asset.textures.size());

	for (size_t i = 0; i < asset.textures.size(); i++)
	{
		const fastgltf::Texture& gltfTexture = asset.textures[i];

		if (!gltfTexture.imageIndex.has_value())
		{
			m_Textures.push_back(nullptr);
			continue;
		}

		const fastgltf::Image& gltfImage = asset.images[gltfTexture.imageIndex.value()];
		const VkFormat format = textureFormats[i];

		std::string debugName(gltfImage.name);

		if (debugName.empty())
		{
			if (const auto* uri = std::get_if<fastgltf::sources::URI>(&gltfImage.data))
				debugName = std::filesystem::path(uri->uri.path()).filename().string();
			else
				debugName = std::format("Mesh Texture {}", i);
		}

		int width  = 0;
		int height = 0;
		stbi_uc* pixels = DecodeImage(asset, gltfImage, path.parent_path(), width, height);

		if (!pixels)
		{
			const char* reason = stbi_failure_reason();
			std::println("[Mesh] Failed to decode texture '{}': {}", debugName, reason ? reason : "unsupported image source");

			m_Textures.push_back(nullptr);
			continue;
		}

		auto texture = std::make_shared<Texture>();

		texture->Create(
		{
			.Type         = TextureType::Texture2D,
			.Format       = format,
			.Size         = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
			.Usage        = TextureUsageBits_Sampled,
			.Data         = pixels,
			.GenerateMips = true,
			.DebugName    = debugName
		});

		stbi_image_free(pixels);

		if (!texture->IsValid())
		{
			std::println("[Mesh] Failed to create texture '{}'", debugName);

			m_Textures.push_back(nullptr);
			continue;
		}

		std::println("[Mesh] Loaded texture '{}' ({})", debugName, format == VK_FORMAT_R8G8B8A8_SRGB ? "sRGB" : "Linear");

		m_Textures.push_back(std::move(texture));
	}

	// ==== Materials ====

	m_Materials.reserve(asset.materials.size());

	for (const fastgltf::Material& gltfMaterial : asset.materials)
	{
		auto material = std::make_shared<Material>();

		// Alpha mode
		switch (gltfMaterial.alphaMode)
		{
			case fastgltf::AlphaMode::Opaque:
				material->SetRenderMode(MaterialRenderMode::Opaque);
				material->SetAlphaCutoff(0.0f);
				break;

			case fastgltf::AlphaMode::Mask:
				material->SetRenderMode(MaterialRenderMode::Cutout);
				material->SetAlphaCutoff(static_cast<float>(gltfMaterial.alphaCutoff));
				break;

			case fastgltf::AlphaMode::Blend:
				material->SetRenderMode(MaterialRenderMode::Transparent);
				material->SetAlphaCutoff(0.0f);
				break;
		}

		// Face culling
		material->SetCullMode(gltfMaterial.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT);

		// PBR factors
		const auto& pbr = gltfMaterial.pbrData;
		const auto& c   = pbr.baseColorFactor;
		material->SetColor({ c[0], c[1], c[2], c[3] });
		material->SetMetalness(pbr.metallicFactor);
		material->SetRoughness(pbr.roughnessFactor);

		const auto& emissive = gltfMaterial.emissiveFactor;
		material->SetEmissiveColor(
		{
			static_cast<float>(emissive[0]),
			static_cast<float>(emissive[1]),
			static_cast<float>(emissive[2])
		});
		material->SetEmissiveStrength(static_cast<float>(gltfMaterial.emissiveStrength));

		if (gltfMaterial.transmission)
			material->SetTransmission(static_cast<float>(gltfMaterial.transmission->transmissionFactor));

		// Textures
		if (pbr.baseColorTexture.has_value())
			SetMaterialTexture(*material, m_Textures, pbr.baseColorTexture.value(), MapType::Albedo);

		if (gltfMaterial.normalTexture.has_value())
			SetMaterialTexture(*material, m_Textures, gltfMaterial.normalTexture.value(), MapType::Normal);

		if (pbr.metallicRoughnessTexture.has_value())
			SetMaterialTexture(*material, m_Textures, pbr.metallicRoughnessTexture.value(), MapType::MetallicRoughness);

		if (gltfMaterial.occlusionTexture.has_value())
			SetMaterialTexture(*material, m_Textures, gltfMaterial.occlusionTexture.value(), MapType::Occlusion);

		if (gltfMaterial.emissiveTexture.has_value())
			SetMaterialTexture(*material, m_Textures, gltfMaterial.emissiveTexture.value(), MapType::Emissive);

		m_Materials.push_back(std::move(material));
	}

	// ==== Geometry ====

	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	std::vector<Submesh>               geometrySubmeshes;
	std::vector<std::vector<uint32_t>> meshSubmeshes(asset.meshes.size());

	std::vector<Vertex>   primVertices;
	std::vector<uint32_t> primIndices;

	for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); meshIndex++)
	{
		for (const fastgltf::Primitive& primitive : asset.meshes[meshIndex].primitives)
		{
			if (primitive.type != fastgltf::PrimitiveType::Triangles)
				continue;

			const auto positionIt = primitive.findAttribute("POSITION");
			const auto normalIt   = primitive.findAttribute("NORMAL");
			const auto texCoordIt = primitive.findAttribute("TEXCOORD_0");
			const auto tangentIt  = primitive.findAttribute("TANGENT");

			const bool hasNormals   = normalIt   != primitive.attributes.end();
			const bool hasTexCoords = texCoordIt != primitive.attributes.end();
			const bool hasTangents  = tangentIt  != primitive.attributes.end();

			assert(positionIt != primitive.attributes.end());
			assert(primitive.indicesAccessor.has_value());

			const fastgltf::Accessor& positionAccessor = asset.accessors[positionIt->accessorIndex];
			const fastgltf::Accessor& indexAccessor    = asset.accessors[primitive.indicesAccessor.value()];

			if (indexAccessor.count % 3 != 0)
			{
				std::println("[Mesh] Invalid index count: {}", indexAccessor.count);
				return false;
			}

			primVertices.assign(positionAccessor.count, Vertex{});
			primIndices.resize(indexAccessor.count);

			fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
				[&](const glm::vec3& position, size_t i) { primVertices[i].Position = position; });

			if (hasNormals)
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normalIt->accessorIndex],
					[&](const glm::vec3& normal, size_t i) { primVertices[i].Normal = normal; });
			}

			if (hasTexCoords)
			{
				fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[texCoordIt->accessorIndex],
					[&](const glm::vec2& texCoord, size_t i) { primVertices[i].TexCoord = texCoord; });
			}

			// xyz tangent, w handedness
			if (hasTangents)
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[tangentIt->accessorIndex],
					[&](const glm::vec4& tangent, size_t i) { primVertices[i].Tangent = tangent; });
			}

			fastgltf::copyFromAccessor<uint32_t>(asset, indexAccessor, primIndices.data());

			ProcessPrimitive(primVertices, primIndices, !hasTangents && hasNormals && hasTexCoords);

			meshSubmeshes[meshIndex].push_back(static_cast<uint32_t>(geometrySubmeshes.size()));

			Submesh& submesh    = geometrySubmeshes.emplace_back();
			submesh.BaseVertex  = static_cast<uint32_t>(vertices.size());
			submesh.BaseIndex   = static_cast<uint32_t>(indices.size());
			submesh.VertexCount = static_cast<uint32_t>(primVertices.size());
			submesh.IndexCount  = static_cast<uint32_t>(primIndices.size());
			submesh.MeshName    = asset.meshes[meshIndex].name;

			if (primitive.materialIndex.has_value())
				submesh.MaterialIndex = static_cast<uint32_t>(primitive.materialIndex.value());

			vertices.insert(vertices.end(), primVertices.begin(), primVertices.end());
			indices.insert(indices.end(), primIndices.begin(), primIndices.end());
		}
	}

	// ==== Nodes ====

	m_Nodes.resize(asset.nodes.size());

	for (size_t i = 0; i < asset.nodes.size(); i++)
	{
		const fastgltf::Node& gltfNode = asset.nodes[i];
		Node&  node  = m_Nodes[i];

		node.Name           = gltfNode.name;
		node.LocalTransform = NodeToMatrix(gltfNode);

		// Children
		node.Children.reserve(gltfNode.children.size());

		for (size_t childIndex : gltfNode.children)
		{
			assert(childIndex < m_Nodes.size());
			node.Children.push_back(static_cast<uint32_t>(childIndex));
		}
	}

	// Parent relationships
	for (size_t i = 0; i < m_Nodes.size(); i++)
	{
		for (uint32_t childIndex : m_Nodes[i].Children)
			m_Nodes[childIndex].Parent = static_cast<uint32_t>(i);
	}

	// Root nodes
	if (!asset.scenes.empty())
	{
		const size_t sceneIndex = asset.defaultScene.value_or(0);
		assert(sceneIndex < asset.scenes.size());

		const fastgltf::Scene& scene = asset.scenes[sceneIndex];
		m_SceneName = scene.name;

		for (size_t nodeIndex : scene.nodeIndices)
		{
			assert(nodeIndex < m_Nodes.size());
			m_RootNodes.push_back(static_cast<uint32_t>(nodeIndex));
		}
	}
	else
	{
		// No scene declared
		for (size_t i = 0; i < m_Nodes.size(); i++)
		{
			if (m_Nodes[i].IsRoot())
				m_RootNodes.push_back(static_cast<uint32_t>(i));
		}
	}

	{
		std::vector<uint32_t> stack(m_RootNodes.rbegin(), m_RootNodes.rend());

		while (!stack.empty())
		{
			const uint32_t nodeIndex = stack.back();
			stack.pop_back();

			const fastgltf::Node& gltfNode = asset.nodes[nodeIndex];
			Node&  node  = m_Nodes[nodeIndex];

			if (gltfNode.meshIndex.has_value())
			{
				const size_t meshIndex = gltfNode.meshIndex.value();
				assert(meshIndex < meshSubmeshes.size());

				for (uint32_t geometryIndex : meshSubmeshes[meshIndex])
				{
					node.Submeshes.push_back(static_cast<uint32_t>(m_Submeshes.size()));
					m_Submeshes.push_back(geometrySubmeshes[geometryIndex]);
				}
			}

			stack.insert(stack.end(), node.Children.rbegin(), node.Children.rend());
		}
	}

	BakeTransforms();

	// ==== GPU upload ====

	if (!vertices.empty())
	{
		m_VertexBuffer.Create(
		{
			.DebugName = m_Name + " Vertex Buffer",
			.Usage     = BufferUsage::Vertex,
			.Memory    = BufferMemory::Device,
			.Size      = vertices.size() * sizeof(Vertex),
			.Data      = vertices.data()
		});
	}

	if (!indices.empty())
	{
		m_IndexBuffer.Create(
		{
			.DebugName = m_Name + " Index Buffer",
			.Usage     = BufferUsage::Index,
			.Memory    = BufferMemory::Device,
			.Size      = indices.size() * sizeof(uint32_t),
			.Data      = indices.data()
		});
	}

	std::println("[Mesh] Loaded '{}' - {} vertices, {} indices, {} submeshes, {} nodes, {} textures, {} materials",
		m_Name,
		vertices.size(),
		indices.size(),
		m_Submeshes.size(),
		m_Nodes.size(),
		m_Textures.size(),
		m_Materials.size());

	return true;
}

void Mesh::Destroy()
{
	m_VertexBuffer.Destroy();
	m_IndexBuffer.Destroy();

	m_Materials.clear();
	m_Textures.clear();
	m_Submeshes.clear();
	m_Nodes.clear();
	m_RootNodes.clear();
	m_Name.clear();
	m_SceneName.clear();
	m_MeshType = MeshType::Static;
}
