#pragma once

#include "../Base.h"
#include "../Compiler.h"
#include <string>

namespace PrismShaderCompiler
{

std::string PSC_API DecompileMSL(const std::vector<uint32_t>& spirv);

} // namespace PrismShaderCompiler
