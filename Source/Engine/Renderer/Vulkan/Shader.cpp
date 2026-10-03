#include "Shader.hpp"

#include "CommandBuffer.hpp"
#include "Context.hpp"
#include "Descriptors.hpp"
#include "ShaderCompiler.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace
{
	std::vector<VkPushConstantRange> CreatePushConstantRanges(const ShaderReflectionData& reflectionData)
	{
		std::vector<VkPushConstantRange> ranges;
		ranges.reserve(reflectionData.PushConstantRanges.size());

		for (const PushConstantRange& range : reflectionData.PushConstantRanges)
		{
			ranges.push_back({ .stageFlags = range.StageFlags, .offset     = range.Offset, .size       = range.Size });
		}

		return ranges;
	}

	constexpr VkShaderStageFlags SHADER_OBJECT_STAGES =
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
		VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;

	constexpr VkShaderStageFlags VERTEX_PIPELINE = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	constexpr VkShaderStageFlags MESH_PIPELINE   = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;

	// Only vertex+fragment and task+mesh+fragment sets can be linked, and only without duplicate stages.
	bool CanLinkStages(VkShaderStageFlags mask, size_t stageCount)
	{
		if (static_cast<size_t>(std::popcount(mask)) != stageCount)
			return false;

		return mask == VERTEX_PIPELINE || mask == MESH_PIPELINE || mask == (MESH_PIPELINE | VK_SHADER_STAGE_TASK_BIT_EXT);
	}

	VkShaderStageFlags GetNextStage(VkShaderStageFlagBits stage, VkShaderStageFlags mask)
	{
		switch (stage)
		{
			case VK_SHADER_STAGE_VERTEX_BIT:
			case VK_SHADER_STAGE_MESH_BIT_EXT:
				return mask & VK_SHADER_STAGE_FRAGMENT_BIT;

			case VK_SHADER_STAGE_TASK_BIT_EXT:
				return mask & VK_SHADER_STAGE_MESH_BIT_EXT;

			default:
				return 0;
		}
	}

	const char* GetShaderStageName(VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
			case VK_SHADER_STAGE_VERTEX_BIT: return "Vertex";
			case VK_SHADER_STAGE_FRAGMENT_BIT: return "Fragment";
			case VK_SHADER_STAGE_COMPUTE_BIT: return "Compute";
			case VK_SHADER_STAGE_TASK_BIT_EXT: return "Task";
			case VK_SHADER_STAGE_MESH_BIT_EXT: return "Mesh";
			default: return "Unknown";
		}
	}
}

void Shader::Load(const std::filesystem::path& filePath)
{
	ShaderCompileResult result = ShaderCompiler::Compile(filePath);

	if (!result.IsValid())
		return;

	if (IsValid())
		vkDeviceWaitIdle(Context::Get().GetDevice());

	// Preserve the currently working shader if compilation fails.
	Destroy();

	m_Path           = filePath;
	m_SpirV          = std::move(result.SpirV);
	m_Stages         = std::move(result.Stages);
	m_StageMask      = result.StageMask;
	m_ReflectionData = std::move(result.Reflection);

	assert(!m_SpirV.empty());
	assert(!m_Stages.empty());

	// Both objects use the same descriptor layouts and push constant ranges.
	CreatePipelineLayout();
	CreateShaderObjects();
}

void Shader::Shutdown()
{
	Destroy();

	m_SpirV.clear();
	m_ReflectionData = {};
	m_Path.clear();
}

void Shader::Reload()
{
	if (m_Path.empty())
		return;

	const std::filesystem::path path = m_Path;
	Load(path);
}

void Shader::Bind(CommandBuffer& commandBuffer) const
{
	assert(IsValid());

	const VkCommandBuffer handle = commandBuffer.GetHandle();

	vkCmdBindShadersEXT(handle, static_cast<uint32_t>(m_Stages.size()), m_Stages.data(), m_ShaderObjects.data());

	if (m_StageMask & VK_SHADER_STAGE_COMPUTE_BIT)
		return;

	constexpr VkShaderEXT nullShader = VK_NULL_HANDLE;

	const auto bindNull = [&](VkShaderStageFlagBits stage)
	{
		vkCmdBindShadersEXT(handle, 1, &stage, &nullShader);
	};

	if (m_StageMask & VK_SHADER_STAGE_VERTEX_BIT)
	{
		bindNull(VK_SHADER_STAGE_TASK_BIT_EXT);
		bindNull(VK_SHADER_STAGE_MESH_BIT_EXT);
	}
	else if (m_StageMask & VK_SHADER_STAGE_MESH_BIT_EXT)
	{
		bindNull(VK_SHADER_STAGE_VERTEX_BIT);

		if (!(m_StageMask & VK_SHADER_STAGE_TASK_BIT_EXT))
			bindNull(VK_SHADER_STAGE_TASK_BIT_EXT);
	}

	if (!(m_StageMask & VK_SHADER_STAGE_FRAGMENT_BIT))
		bindNull(VK_SHADER_STAGE_FRAGMENT_BIT);
}

void Shader::CreatePipelineLayout()
{
	VkDevice device = Context::Get().GetDevice();

	const std::vector<VkPushConstantRange> pushConstantRanges = CreatePushConstantRanges(m_ReflectionData);

	const VkDescriptorSetLayout descriptorSetLayout = Descriptor::GetLayout();

	const VkPipelineLayoutCreateInfo layoutInfo
	{
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount         = 1,
		.pSetLayouts            = &descriptorSetLayout,
		.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
		.pPushConstantRanges    = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data()
	};

	VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PipelineLayout));

	if (!m_Path.empty())
	{
		const std::string name = m_Path.stem().string() + " Layout";
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, name, m_PipelineLayout);
	}
}

void Shader::CreateShaderObjects()
{
	VkDevice device = Context::Get().GetDevice();

	const VkDescriptorSetLayout descriptorSetLayout = Descriptor::GetLayout();

	const std::vector<VkPushConstantRange> pushConstantRanges = CreatePushConstantRanges(m_ReflectionData);

	const uint32_t stageCount = static_cast<uint32_t>(m_Stages.size());
	const bool     linkStages = CanLinkStages(m_StageMask, m_Stages.size());

	m_ShaderObjects.assign(stageCount, VK_NULL_HANDLE);

	std::vector<VkShaderCreateInfoEXT> createInfos;
	createInfos.reserve(stageCount);

	for (uint32_t i = 0; i < stageCount; ++i)
	{
		const VkShaderStageFlagBits stage = m_Stages[i];

		// Ray-tracing stages still require a ray-tracing pipeline and SBT.
		assert((SHADER_OBJECT_STAGES & stage) != 0);

		VkShaderCreateFlagsEXT shaderFlags = linkStages ? VK_SHADER_CREATE_LINK_STAGE_BIT_EXT : VkShaderCreateFlagsEXT{ 0 };

		if (stage == VK_SHADER_STAGE_MESH_BIT_EXT && !(m_StageMask & VK_SHADER_STAGE_TASK_BIT_EXT))
			shaderFlags |= VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT;

		createInfos.push_back(
		{
			.sType                  = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
			.flags                  = shaderFlags,
			.stage                  = stage,
			.nextStage              = GetNextStage(stage, m_StageMask),
			.codeType               = VK_SHADER_CODE_TYPE_SPIRV_EXT,
			.codeSize               = m_SpirV.size() * sizeof(uint32_t),
			.pCode                  = m_SpirV.data(),
			.pName                  = "main",
			.setLayoutCount         = 1,
			.pSetLayouts            = &descriptorSetLayout,
			.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
			.pPushConstantRanges    = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data(),
			.pSpecializationInfo    = nullptr
		});
	}

	VK_CHECK(vkCreateShadersEXT(device, stageCount, createInfos.data(), nullptr, m_ShaderObjects.data()));

	if (!m_Path.empty())
	{
		for (uint32_t i = 0; i < stageCount; ++i)
		{
			const std::string name = m_Path.stem().string() + " [" + GetShaderStageName(m_Stages[i]) + "]";
			SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SHADER_EXT, name, m_ShaderObjects[i]);
		}
	}
}

void Shader::Destroy()
{
	VkDevice device = Context::Get().GetDevice();

	for (VkShaderEXT shader : m_ShaderObjects)
	{
		if (shader != VK_NULL_HANDLE)
			vkDestroyShaderEXT(device, shader, nullptr);
	}

	m_ShaderObjects.clear();
	m_Stages.clear();
	m_StageMask = 0;

	if (m_PipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);

		m_PipelineLayout = VK_NULL_HANDLE;
	}
}

// ==== Shader library ====

ShaderLibrary::~ShaderLibrary()
{
	for (auto& [name, shader] : m_Shaders)
	{
		if (shader)
			shader->Shutdown();
	}
}

void ShaderLibrary::Add(const std::shared_ptr<Shader>& shader)
{
	assert(shader);

	const std::string name = shader->GetPath().stem().string();

	Add(name, shader);
}

void ShaderLibrary::Add(std::string_view name, const std::shared_ptr<Shader>& shader)
{
	assert(shader);
	assert(!name.empty());
	assert(!Contains(name));

	m_Shaders.emplace(std::string(name), shader);
}

void ShaderLibrary::Load(const std::filesystem::path& filePath)
{
	const std::string name = filePath.stem().string();

	Load(name, filePath);
}

void ShaderLibrary::Load(std::string_view name, const std::filesystem::path& filePath)
{
	assert(!name.empty());
	assert(!Contains(name));

	auto shader = std::make_shared<Shader>();
	shader->Load(filePath);

	assert(shader->IsValid());

	m_Shaders.emplace(std::string(name), std::move(shader));
}

const std::shared_ptr<Shader>& ShaderLibrary::Get(std::string_view name) const
{
	const auto it = m_Shaders.find(std::string(name));

	assert(it != m_Shaders.end());

	return it->second;
}

bool ShaderLibrary::Contains(std::string_view name) const
{
	return m_Shaders.contains(std::string(name));
}
