#pragma once

#include "InnerTypes.h"
#include "PPPreprocessor.h"
#include "RewriteUtil.h"
#include "../PSL/GLSLType.h"
#include "../Property/VertexType.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrismShaderCompiler
{

class DiagnosticCollector;

class GLSLRewriter
{
public:
    GLSLRewriter(const PPFileTable& files, const InnerConfig& config, DiagnosticCollector& diag);

    std::string Emit(const std::vector<PPItem>& items,
                     const std::vector<InnerProperty>& properties,
                     bool vertexStage);

private:
    struct AttributeDecl
    {
        size_t Begin = 0;
        size_t End = 0;
        GLSLType Type = GLSLType::None;
        std::string Name;
        VertexSemantic Semantic = VertexSemantic::Unknown;
        PPSourceLoc Loc;
    };

    struct VaryingMember
    {
        GLSLType Type = GLSLType::None;
        std::string Name;
        uint32_t ArraySize = 1;
    };

    struct VaryingDecl
    {
        size_t Begin = 0;
        size_t End = 0;
        std::string StructName;
        std::string InstanceName;
        std::vector<VaryingMember> Members;
        PPSourceLoc Loc;
    };

    struct FragOutputDecl
    {
        size_t Begin = 0;
        size_t End = 0;
        GLSLType Type = GLSLType::None;
        std::string Name;
        uint32_t Location = 0;
        PPSourceLoc Loc;
    };

    struct EntryDecl
    {
        bool Valid = false;
        size_t Begin = 0;
        size_t BodyBegin = 0;
        size_t BodyEnd = 0;
        size_t End = 0;
        PPSourceLoc Loc;
    };

    struct FlatVar
    {
        std::string Name;
        GLSLType Type = GLSLType::None;
        uint32_t Location = 0;
        std::string UserExpr;
    };

    struct Plan
    {
        std::vector<bool> Skip;
        std::unordered_map<size_t, std::string> Replacements;

        std::vector<FragOutputDecl> FragOutputs;

        bool HasVarying = false;
        VaryingDecl Varying;

        EntryDecl Vertex;
        EntryDecl Fragment;
    };

    bool Collect(const std::vector<PPItem>& items, Plan& plan, bool vertexStage) const;

    bool ParseAttribute(const std::vector<PPItem>& items, size_t begin, AttributeDecl& out) const;
    bool ParseVarying(const std::vector<PPItem>& items, size_t begin, VaryingDecl& out) const;
    bool ParseFragOutput(const std::vector<PPItem>& items, size_t begin, FragOutputDecl& out) const;
    bool ParseEntry(const std::vector<PPItem>& items, size_t begin, EntryDecl& out) const;

    void FlattenVarying(const VaryingDecl& varying, std::vector<FlatVar>& out) const;
    std::string BuildVaryingStruct(const VaryingDecl& varying) const;

    void WritePropertyBlock(GLSLWriter& writer, const std::vector<InnerProperty>& properties) const;
    void WriteVaryingInterface(GLSLWriter& writer, const std::vector<FlatVar>& flats, bool vertexStage) const;
    void WriteFragmentOutputs(GLSLWriter& writer, const Plan& plan) const;
    void WriteMain(GLSLWriter& writer, const std::vector<PPItem>& items, const Plan& plan,
                   const std::vector<FlatVar>& flats, bool vertexStage) const;

    std::string PathOf(const PPSourceLoc& loc) const;

    const PPFileTable& m_Files;
    const InnerConfig& m_Config;
    DiagnosticCollector& m_Diag;
};

} // namespace PrismShaderCompiler
