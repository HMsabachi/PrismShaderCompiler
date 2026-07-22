#pragma once

#include "CSL/AST.h"
#include "Reflection.h"

#include <string>
#include <vector>
#include <cstdint>

namespace PrismShaderCompiler
{

struct CompiledComputeShader
{
    std::string ShaderName;
    int GlslVersion = 450;
    SourceLocation SharedStartLoc;

    struct KernelInfo
    {
        std::string Name;
        std::vector<std::string> VariantDefines;
        uint32_t GroupSizeX, GroupSizeY, GroupSizeZ;
        std::string FunctionSource;

        SourceLocation DeclLoc;
        SourceLocation DefLoc;
        uint32_t DeclInsertID = 0;
        uint32_t DefInsertID = 0;
        SourceLocation DeclAfterLoc;
        SourceLocation DefAfterLoc;
    };
    std::vector<KernelInfo> Kernels;

    std::string SharedSource;
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
    ShaderReflection Reflection;
    std::vector<std::string> Errors;
    std::vector<std::string> Warnings;
};

} // namespace PrismShaderCompiler
