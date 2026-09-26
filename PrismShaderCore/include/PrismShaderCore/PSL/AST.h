#pragma once

#include "Common.h"
#include "../Property/PropertyType.h"
#include "../Pipeline/PipelineState.h"
#include "Scalar.h"
#include "../Property/PropertyLayout.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace PrismShaderCompiler::AST
{

    struct PragmaDef
    {
        bool IsMultiCompile = false;
        bool IsShaderFeature = false;
        std::vector<std::string> Keywords;
        SourceLocation Loc;
    };

    struct GLSLCode
    {
        std::string RawSource;
        SourceLocation Loc;
        std::vector<PragmaDef> Pragmas;
    };

    struct ShaderUniform
    {
        std::string Name;
        std::string DisplayName;
        PropertyType Type;
        std::vector<Scalar> DefaultValue;

        float RangeMin = 0.0f, RangeMax = 0.0f;
        std::vector<std::string> EnumOptions;

        int32_t BufferOffset = 0;
        int32_t BufferSize = 0;
        int32_t TextureSlot = -1;
    };

    struct PassDef
    {
        std::string Name;
        std::unordered_map<std::string, std::string> Tags;
        std::optional<PipelineState> RenderState;
        GLSLCode Glsl;
    };

    struct UsePassDef
    {
        std::string ShaderName;
        std::string PassName;
        SourceLocation Loc;
    };

    struct ShaderDocument
    {
        std::string ShaderName;
        int LOD = 200;
        std::unordered_map<std::string, std::string> Tags;
        std::vector<ShaderUniform> Uniforms;
        PropertyLayout MaterialLayout;
        std::optional<PipelineState> RenderState;
        std::vector<PassDef> Passes;
        std::vector<UsePassDef> UsePasses;
    };

} // namespace PrismShaderCompiler::AST
