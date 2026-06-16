#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace PrismShaderCompiler
{

struct UniformReflection
{
    std::string Name;
    std::string GLSLType;
    uint32_t Binding  = 0;
    uint32_t Set      = 0;
    uint32_t Offset   = 0;
    uint32_t Size     = 0;
    uint32_t ArraySize = 1;
};

struct InputReflection
{
    std::string Name;
    uint32_t Location = 0;
    std::string Semantic;
};

struct PassCompileResult
{
    std::string PassName;

    std::string GlslVertex;
    std::string GlslFragment;

    std::vector<uint32_t> SpirvVertex;
    std::vector<uint32_t> SpirvFragment;

    std::vector<UniformReflection> Uniforms;
    std::vector<InputReflection>   Inputs;

    std::vector<std::string> Errors;
    std::vector<std::string> Warnings;
};

struct ShaderCompileResult
{
    std::string ShaderName;
    int LOD = 200;

    std::vector<UniformReflection> GlobalUniforms;
    std::vector<PassCompileResult> Passes;
    std::vector<std::string> Keywords;

    bool Success = false;
    std::vector<std::string> Errors;
    std::vector<std::string> Warnings;
};

} // namespace PrismShaderCompiler
