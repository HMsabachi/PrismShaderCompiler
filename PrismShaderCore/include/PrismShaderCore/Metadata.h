#pragma once

#include "Compiler.h"
#include <string>

namespace PrismShaderCompiler
{

	std::string ToJson(const CompiledShader& shader);
	std::string ToJson(const CompiledComputeShader& shader);

} // namespace PrismShaderCompiler
