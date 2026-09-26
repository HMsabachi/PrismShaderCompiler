#pragma once

#include "../Base.h"

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

// backend 与 keywords 走 glslang 的 preamble —— 内层编译器把后端分支和
// multi_compile 关键字都当 deferred 保留在输出里，由这里按变体选边。
SpirvResult PSC_API CompileGLSL(const std::string& glslSource, ShaderStageType stage,
                                TargetBackend backend = TargetBackend::OpenGL,
                                const std::vector<std::string>& keywords = {});

} // namespace PrismShaderCompiler
