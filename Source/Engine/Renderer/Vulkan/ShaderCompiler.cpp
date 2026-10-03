#include "ShaderCompiler.hpp"

#include "VulkanUtils.hpp"

#include <slang/slang-com-ptr.h>
#include <slang/slang.h>

#include <print>
#include <string>

namespace
{
	bool CreateGlobalSession(Slang::ComPtr<slang::IGlobalSession>& globalSession)
	{
		if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef())))
		{
			std::println("[ShaderCompiler] Failed to create Slang global session.");
			return false;
		}

		return globalSession != nullptr;
	}

	slang::IGlobalSession* GetGlobalSession()
	{
		static Slang::ComPtr<slang::IGlobalSession> s_GlobalSession;

		if (!s_GlobalSession && !CreateGlobalSession(s_GlobalSession))
			return nullptr;

		return s_GlobalSession.get();
	}

	bool CreateSession(slang::IGlobalSession* globalSession, const std::filesystem::path& sourceDirectory, Slang::ComPtr<slang::ISession>& session)
	{
		slang::TargetDesc target
		{
			.format            = SLANG_SPIRV,
			.profile           = globalSession->findProfile("spirv_1_6"),
			.flags             = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
			.floatingPointMode = SLANG_FLOATING_POINT_MODE_PRECISE
		};

#ifdef NDEBUG
		constexpr int debugInfoLevel    = SLANG_DEBUG_INFO_LEVEL_NONE;
		constexpr int optimizationLevel = SLANG_OPTIMIZATION_LEVEL_HIGH;
#else
		constexpr int debugInfoLevel    = SLANG_DEBUG_INFO_LEVEL_MINIMAL;
		constexpr int optimizationLevel = SLANG_OPTIMIZATION_LEVEL_NONE;
#endif

		slang::CompilerOptionEntry compilerOptions[]
		{
			{ slang::CompilerOptionName::DebugInformation,
			{ slang::CompilerOptionValueKind::Int, debugInfoLevel, 0, nullptr, nullptr } },
			{ slang::CompilerOptionName::Optimization,
			{ slang::CompilerOptionValueKind::Int, optimizationLevel, 0, nullptr, nullptr } },
			{ slang::CompilerOptionName::GLSLForceScalarLayout,
			{ slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr } },
			{slang::CompilerOptionName::DisableWarning,
			{ slang::CompilerOptionValueKind::String, 0, 0, "39001", nullptr } }
		};

		const std::string searchPath    = sourceDirectory.string();
		const char*       searchPaths[] = { searchPath.c_str() };

		slang::SessionDesc sessionDesc
		{
			.targets                  = &target,
			.targetCount              = 1,
			.defaultMatrixLayoutMode  = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
			.searchPaths              = searchPaths,
			.searchPathCount          = 1,
			.compilerOptionEntries    = compilerOptions,
			.compilerOptionEntryCount = static_cast<uint32_t>(std::size(compilerOptions))
		};

		if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef())))
		{
			std::println("[ShaderCompiler] Failed to create Slang session.");
			return false;
		}

		return session != nullptr;
	}

	void PrintDiagnostics(slang::IBlob* diagnostics)
	{
		if (!diagnostics || diagnostics->getBufferSize() == 0)
			return;

		std::println("{}", std::string_view(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize()));
	}

	std::vector<uint32_t> BlobToSpirV(slang::IBlob* blob)
	{
		if (!blob || blob->getBufferSize() == 0)
			return {};

		const auto*  words     = static_cast<const uint32_t*>(blob->getBufferPointer());
		const size_t wordCount = blob->getBufferSize() / sizeof(uint32_t);

		return { words, words + wordCount };
	}

	ShaderStage FromSlangStage(SlangStage stage)
	{
		switch (stage)
		{
			case SLANG_STAGE_VERTEX: return ShaderStage::Vertex;
			case SLANG_STAGE_FRAGMENT: return ShaderStage::Fragment;
			case SLANG_STAGE_COMPUTE: return ShaderStage::Compute;
			case SLANG_STAGE_RAY_GENERATION: return ShaderStage::RayGen;
			case SLANG_STAGE_MISS: return ShaderStage::Miss;
			case SLANG_STAGE_CLOSEST_HIT: return ShaderStage::ClosestHit;
			case SLANG_STAGE_ANY_HIT: return ShaderStage::AnyHit;
			case SLANG_STAGE_INTERSECTION: return ShaderStage::Intersection;
			case SLANG_STAGE_CALLABLE: return ShaderStage::Callable;
			case SLANG_STAGE_AMPLIFICATION: return ShaderStage::Task;
			case SLANG_STAGE_MESH: return ShaderStage::Mesh;
			default: return ShaderStage::None;
		}
	}

	VkDescriptorType ResolveDescriptorType(slang::TypeLayoutReflection* typeLayout, slang::ParameterCategory category)
	{
		if (!typeLayout)
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;

		if (typeLayout->getKind() == slang::TypeReflection::Kind::Array)
			typeLayout = typeLayout->getElementTypeLayout();

		if (!typeLayout)
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;

		// Samplers
		if (category == slang::ParameterCategory::SamplerState || typeLayout->getKind() == slang::TypeReflection::Kind::SamplerState)
		{
			return VK_DESCRIPTOR_TYPE_SAMPLER;
		}

		const SlangResourceShape  shape  = typeLayout->getResourceShape();
		const SlangResourceAccess access = typeLayout->getResourceAccess();

		switch (shape & SLANG_RESOURCE_BASE_SHAPE_MASK)
		{
			case SLANG_TEXTURE_1D:
			case SLANG_TEXTURE_2D:
			case SLANG_TEXTURE_3D:
			case SLANG_TEXTURE_CUBE:
			case SLANG_TEXTURE_2D_ARRAY:
			case SLANG_TEXTURE_CUBE_ARRAY:
			{
				return access == SLANG_RESOURCE_ACCESS_READ_WRITE ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			}

			default:
				return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}
	}

	uint32_t ResolveArrayCount(slang::TypeLayoutReflection* typeLayout)
	{
		if (!typeLayout)
			return 0;

		if (typeLayout->getKind() != slang::TypeReflection::Kind::Array)
			return 1;

		const SlangInt count = typeLayout->getElementCount();

		// 0 means unbounded / runtime array
		return (count > 0) ? static_cast<uint32_t>(count) : 0;
	}
}

ShaderCompileResult ShaderCompiler::Compile(const std::filesystem::path& sourcePath)
{
	ShaderCompileResult result;

	slang::IGlobalSession* globalSession = GetGlobalSession();
	if (!globalSession)
		return result;

	Slang::ComPtr<slang::ISession> session;
	if (!CreateSession(globalSession, sourcePath.parent_path(), session))
		return result;

	Slang::ComPtr<slang::IBlob> diagnostics;
	const std::string           moduleName = sourcePath.stem().string();

	slang::IModule* module = session->loadModule(moduleName.c_str(), diagnostics.writeRef());

	PrintDiagnostics(diagnostics);

	if (!module)
		return result;

	const SlangInt entryPointCount = module->getDefinedEntryPointCount();
	if (entryPointCount == 0)
	{
		std::println("[ShaderCompiler] No entry points found in '{}'.", sourcePath.string());
		return result;
	}

	for (SlangInt i = 0; i < entryPointCount; ++i)
	{
		Slang::ComPtr<slang::IEntryPoint> entryPoint;
		if (SLANG_FAILED(module->getDefinedEntryPoint(static_cast<SlangInt32>(i), entryPoint.writeRef())))
			continue;

		const ShaderStage stage = FromSlangStage(entryPoint->getLayout(0)->getEntryPointByIndex(0)->getStage());

		if (stage != ShaderStage::None)
			result.Stages.push_back(stage);
	}

	if (result.Stages.empty())
	{
		std::println("[ShaderCompiler] No valid shader stages found in '{}'.", sourcePath.string());
		return result;
	}

	Slang::ComPtr<slang::IBlob> spirv;
	diagnostics = nullptr;

	const SlangResult codeResult = module->getTargetCode(0, spirv.writeRef(), diagnostics.writeRef());

	PrintDiagnostics(diagnostics);

	if (SLANG_FAILED(codeResult))
		return result;

	result.SpirV = BlobToSpirV(spirv);

	VkShaderStageFlags stageFlags = 0;

	for (ShaderStage stage : result.Stages)
		stageFlags |= ToVulkan(stage);

	Reflect(module->getLayout(0), result.Reflection, stageFlags);

	return result;
}

void ShaderCompiler::Reflect(slang::ProgramLayout* layout, ShaderReflectionData& outReflection, VkShaderStageFlags stageFlags)
{
	if (!layout)
		return;

	for (SlangInt i = 0; i < layout->getParameterCount(); ++i)
	{
		slang::VariableLayoutReflection* param = layout->getParameterByIndex(static_cast<uint32_t>(i));

		if (!param)
			continue;

		slang::TypeLayoutReflection* typeLayout = param->getTypeLayout();

		if (!typeLayout)
			continue;

		const slang::ParameterCategory category = typeLayout->getParameterCategory();

		// Push Constants
		if (category == slang::ParameterCategory::PushConstantBuffer)
		{
			slang::TypeLayoutReflection* elementTypeLayout = typeLayout->getElementTypeLayout();

			if (!elementTypeLayout)
				continue;

			const uint32_t size = static_cast<uint32_t>(elementTypeLayout->getSize());

			if (size == 0)
				continue;

			PushConstantRange range
			{
				.StageFlags = stageFlags,
				.Offset     = 0,
				.Size       = size
			};

			outReflection.PushConstantRanges.push_back(range);

			std::println("[ShaderCompiler] Reflect - push constant: size={} stages={:#x}", range.Size, range.StageFlags);

			const SlangInt fieldCount = elementTypeLayout->getFieldCount();

			for (SlangInt fieldIndex = 0; fieldIndex < fieldCount; fieldIndex++)
			{
				slang::VariableLayoutReflection* field = elementTypeLayout->getFieldByIndex(static_cast<uint32_t>(fieldIndex));

				if (!field)
					continue;

				slang::TypeLayoutReflection* fieldTypeLayout = field->getTypeLayout();

				if (!fieldTypeLayout)
					continue;

				PushConstantMember member
				{
					.Name   = field->getName() ? field->getName() : "",
					.Offset = static_cast<uint32_t>(field->getOffset()),
					.Size   = static_cast<uint32_t>(fieldTypeLayout->getSize())
				};

				outReflection.PushConstantMembers.push_back(member);

				std::println("[ShaderCompiler] Reflect - push constant member: name='{}' offset={} size={}", member.Name, member.Offset, member.Size);
			}

			continue;
		}

		// Descriptor Bindings
		if (category != slang::ParameterCategory::DescriptorTableSlot &&
			category != slang::ParameterCategory::ShaderResource &&
			category != slang::ParameterCategory::SamplerState &&
			category != slang::ParameterCategory::UnorderedAccess)
		{
			continue;
		}

		const VkDescriptorType descriptorType = ResolveDescriptorType(typeLayout, category);

		if (descriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
			continue;

		DescriptorBinding binding;

		binding.Name           = param->getName() ? param->getName() : "";
		binding.Set            = static_cast<uint32_t>(param->getBindingSpace());
		binding.Binding        = static_cast<uint32_t>(param->getBindingIndex());
		binding.Count          = ResolveArrayCount(typeLayout);
		binding.DescriptorType = descriptorType;
		binding.StageFlags     = stageFlags;

		outReflection.DescriptorBindings.push_back(binding);

		std::println("[ShaderCompiler] Reflect - descriptor: name='{}' set={} binding={} count={} type={} stages={:#x}",
					binding.Name,
					binding.Set,
					binding.Binding,
					binding.Count,
					static_cast<int>(binding.DescriptorType),
					binding.StageFlags);
	}
}

bool ShaderCompiler::Available()
{
	return GetGlobalSession() != nullptr;
}
