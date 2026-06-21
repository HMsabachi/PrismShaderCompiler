#pragma once

#include "Common.h"
#include "../Property/PropertyType.h"
#include "../Pipeline/PipelineState.h"
#include "Scalar.h"
#include "../Property/PropertyLayout.h"
#include "../Property/VertexType.h"
#include "GLSLType.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace PrismShaderCompiler::AST
{

struct VertexAttribute
{
    GLSLType Type = GLSLType::None;
    std::string Name;
    PrismShaderCompiler::VertexSemantic Semantic;
    uint32_t InsertID = 0;
    SourceLocation Loc;
};

struct VaryingMember
{
    GLSLType Type = GLSLType::None;
    std::string Name;
    uint32_t ArraySize = 1;
};

struct VaryingBlock
{
    std::string StructName;
    std::string InstanceName;
    std::vector<VaryingMember> Members;
    uint32_t InsertID = 0;
    SourceLocation Loc;
};

struct PragmaDef
{
    bool IsMultiCompile = false;
    bool IsShaderFeature = false;
    std::vector<std::string> Keywords;
    uint32_t InsertID = 0;
    SourceLocation Loc;
};

struct EntryPointSource
{
    std::string Source;
    SourceLocation LocBegin;
    SourceLocation LocEnd;
};

struct IncludeDef
{
    std::string Path;
    uint32_t InsertID = 0;
    SourceLocation Loc;
};

struct FragmentOutput
{
    GLSLType Type = GLSLType::None;
    std::string Name;
    int Location = 0;
    uint32_t InsertID = 0;
    SourceLocation Loc;
};

struct GLSLCode
{
    std::string SharedSource;
    EntryPointSource Vertex;
    EntryPointSource Fragment;
    SourceLocation Loc;

    std::vector<VertexAttribute> Attributes;
    std::optional<VaryingBlock> Varying;
    std::vector<PragmaDef> Pragmas;
    std::vector<IncludeDef> Includes;
    std::vector<FragmentOutput> FragmentOutputs;
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

struct ShaderDocument
{
    std::string ShaderName;
    int LOD = 200;
    std::vector<ShaderUniform> Uniforms;
    PropertyLayout MaterialLayout;
    std::optional<PipelineState> RenderState;
    std::vector<PassDef> Passes;
};

} // namespace PrismShaderCompiler::AST
