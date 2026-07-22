#pragma once

#include "Compiler.h"
#include "Reflection.h"
#include <string>

namespace PrismShaderCompiler
{

	std::string ToJson(const CompiledShader& shader);
	std::string ToJson(const CompiledComputeShader& shader);
	std::string ToJson(const ShaderReflection& reflection);

} // namespace PrismShaderCompiler
