#pragma once

#include "../Base.h"
#include "../Reflection.h"

#include "../PSL/Common.h"
#include <vector>
#include <string>
#include <cstdint>

namespace PrismShaderCompiler
{

struct SpirvResult
{
    std::vector<uint32_t> Bytecode;
    std::vector<std::string> Errors;
    std::vector<std::string> Warnings;
    bool Success = false;
};

SpirvResult PSC_API CompileGLSL(const std::string& glslSource, ShaderStageType stage);

ShaderReflection PSC_API ReflectSPIRV(const std::vector<uint32_t>& spirv, uint32_t stageFlags);
ShaderReflection PSC_API MergeReflections(const ShaderReflection& a, const ShaderReflection& b);

} // namespace PrismShaderCompiler
