#pragma once

#include "../Base.h"

#include "../PSL/Common.h"
#include "IRGenerator.h"
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

SpirvResult PSC_API CompileGLSL(const std::string& glslSource, ShaderStageType stage, TargetBackend backend = TargetBackend::OpenGL);

} // namespace PrismShaderCompiler
