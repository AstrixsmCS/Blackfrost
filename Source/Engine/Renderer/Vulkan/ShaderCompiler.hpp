#pragma once

#include "Shader.hpp"

#include <slang/slang.h>

#include <cstdint>
#include <filesystem>
#include <vector>

struct ShaderCompileResult
{
	std::vector<uint32_t>              SpirV;
	std::vector<VkShaderStageFlagBits> Stages;
	VkShaderStageFlags                 StageMask = 0;
	ShaderReflectionData               Reflection;

	bool IsValid() const { return !SpirV.empty() && !Stages.empty(); }
};

class ShaderCompiler
{
public:
	static ShaderCompileResult Compile(const std::filesystem::path& sourcePath);
	static bool                Available();

private:
	static void Reflect(slang::ProgramLayout* layout, ShaderReflectionData& outReflection, VkShaderStageFlags stageFlags);
};
