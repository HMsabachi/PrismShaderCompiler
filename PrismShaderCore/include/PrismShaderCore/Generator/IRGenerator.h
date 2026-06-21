#pragma once

#include "../Base.h"
#include "../PSL/AST.h"
#include "../Property/PropertyType.h"

#include <string>
#include <vector>

namespace PrismShaderCompiler
{

struct CompilerConfig;

namespace IRGen
{
    struct Output { std::string Vertex; std::string Fragment; };

    void PSC_API SetConfig(const CompilerConfig& config);

    Output PSC_API Generate(
        const AST::GLSLCode& glsl,
        const std::vector<AST::ShaderUniform>& uniforms,
        const std::string& filePath,
        const std::vector<std::string>& activeKeywords = {}
    );

    uint32_t PSC_API VaryingLocationSlots(const AST::VaryingBlock& varying);
}

} // namespace PrismShaderCompiler
