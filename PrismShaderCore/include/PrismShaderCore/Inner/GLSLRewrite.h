#pragma once

#include "InnerTypes.h"
#include "PPPreprocessor.h"
#include "../PSL/GLSLType.h"
#include "../Property/VertexType.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrismShaderCompiler
{

class PPFileTable;
class DiagnosticCollector;
class GLSLWriter;

// 内层 GLSL 前端是「块级结构改写器」，不是完整 GLSL 语法分析器。
//
// 理由：函数体是逐字透传的（旧 IRGenerator 直接拼 glsl.Vertex.Source），
// 真正参与改写的只有 PSL 级声明 —— attribute / varying / void vert|frag /
// layout(location = N) out。写一套 GLSL 表达式与语句文法没有任何消费者。
//
// 改写跑在预处理输出的「带条件标记的线性 token 流」上：条件标记原样重建，
// deferred 区内的 token 逐字回写原文，重写点在合成文本之后重新锚定 #line。
class GLSLRewriter
{
public:
    GLSLRewriter(const PPFileTable& files, const InnerConfig& config, DiagnosticCollector& diag);

    // vertexStage 决定 attribute / varying 的方向与 main 的装配方式。
    // 解析失败时返回空串。
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
        size_t Begin = 0;       // 'void'
        size_t BodyBegin = 0;   // '{' 之后的首个 item
        size_t BodyEnd = 0;     // '}' 之前的末个 item
        size_t End = 0;         // '}'
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
        std::vector<bool> Skip;                                 // 被声明整体吃掉的 item
        std::unordered_map<size_t, std::string> Replacements;   // 下标 -> 替身文本（空串 = 丢弃）

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
