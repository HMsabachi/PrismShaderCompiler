#pragma once

#include "CSL/AST.h"
#include "Generator/ReflectionGenerator.h"

#include <string>
#include <vector>
#include <cstdint>

namespace PrismShaderCompiler
{

struct CompiledComputeShader
{
    std::string ShaderName;
    std::string Source;
    std::string SourcePath;
    int GlslVersion = 450;

    struct KernelInfo
    {
        std::string Name;
        std::vector<std::string> VariantDefines;
        uint32_t GroupSizeX = 1;
        uint32_t GroupSizeY = 1;
        uint32_t GroupSizeZ = 1;
    };
    std::vector<KernelInfo> Kernels;

    std::vector<CSL::ComputeResource> Resources;
    std::vector<CSL::ComputeUniform> Uniforms;

    struct BindingInfo
    {
        uint32_t Set;
        uint32_t Binding;
        std::string Name;
        CSL::ResourceKind Kind;
    };
    std::vector<BindingInfo> Bindings;
};

struct ComputeKernelOutput
{
    std::string Source;
    std::vector<uint32_t> Spirv;
    PassReflection Reflection;
    std::vector<std::string> Errors;
    std::vector<std::string> Warnings;
};

} // namespace PrismShaderCompiler
