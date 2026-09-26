#pragma once

#include "MacroTable.h"
#include "PPExpr.h"
#include "PPFile.h"
#include "PPToken.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrismShaderCompiler
{

class DiagnosticCollector;

// 预处理输出是「带条件标记的线性 token 流」，不是树 ——
// 改写与代码生成都是单趟线性扫描，条件嵌套用下面三个标记表达。
struct PPItem
{
    enum class Kind : uint8_t
    {
        Token,      // 普通 token（原文片段，或宏展开出来的合成 token）
        CondBegin,  // 保留的 #if / #ifdef / #ifndef
        CondElse,   // 保留的 #elif / #else
        CondEnd,    // #endif
    };

    Kind K = Kind::Token;

    PPToken Tok;                // Kind == Token 时有效

    std::string Condition;      // CondBegin：条件文本；CondElse："else" 或 "elif <expr>"
    PPSourceLoc Loc;            // 标记对应的指令位置
};

struct PPVariantDecl
{
    std::string Name;                       // "multi_compile" / "shader_feature"
    std::vector<std::string> Keywords;      // 已剔除占位符 _
    PPSourceLoc Loc;
};

struct PPResult
{
    std::vector<PPItem> Items;
    std::vector<PPVariantDecl> Variants;
    bool Success = true;
};

// 读文件回调：返回 false 表示打不开
using PPReadFileFn = std::function<bool(const std::string& path, std::string& out)>;

struct PPParams
{
    std::string Path;                       // 该 .Shader 自身的路径，供 include 相对解析与 #line
    std::string Source;                     // GLSL 块原文
    std::string IncludeRoot;
    PPReadFileFn ReadFile;
    std::vector<std::string> Defines;       // 立即定义的一批名字（如 PRISM_VERTEX_SHADER）
};

// 一次 Run 对应一个输出阶段 —— PRISM_VERTEX_SHADER / PRISM_FRAGMENT_SHADER 是立即宏，
// 两个阶段的条件求值结果不同，故上层的 GLSL 块要跑两遍。
class PPPreprocessor
{
public:
    PPPreprocessor(PPFileTable& files, DiagnosticCollector& diag);

    PPResult Run(const PPParams& params);

    MacroTable& Macros() { return m_Macros; }

private:
    struct CondFrame
    {
        enum class State : uint8_t { Active, Skipped, Deferred };

        State S = State::Active;
        bool Emitting = true;       // 自本帧起是否输出
        bool ParentEmitting = true;
        bool Taken = false;         // 立即模式：已有分支命中
        bool SuppressRest = false;  // deferred 模式：某分支恒真，其后分支不再保留
        bool SawElse = false;
        bool MarkerOpened = false;  // 已向输出写入 CondBegin，收尾时必须配一个 CondEnd
        PPSourceLoc Loc;
    };

    void ProcessFile(PPFileId file);
    void ProcessTokens(PPFileId file, const std::vector<PPToken>& tokens);

    size_t FindLineEnd(const std::vector<PPToken>& tokens, size_t start) const;
    size_t FindNextDirective(const std::vector<PPToken>& tokens, size_t start) const;

    void HandleDirective(PPFileId file, const std::vector<PPToken>& tokens, size_t& index);
    void HandleDefine(const std::vector<PPToken>& tokens, size_t hashIndex, size_t lineEnd);
    void HandleUndef(const std::vector<PPToken>& tokens, size_t hashIndex, size_t lineEnd);
    void HandleInclude(PPFileId file, const std::vector<PPToken>& tokens, size_t hashIndex, size_t lineEnd);
    void HandlePragma(PPFileId file, const std::vector<PPToken>& tokens, size_t hashIndex, size_t lineEnd);
    void HandleConditional(PPFileId file, const std::vector<PPToken>& tokens, size_t nameIndex, size_t lineEnd);
    void HandleError(const std::vector<PPToken>& tokens, size_t hashIndex, size_t lineEnd, bool fatal);

    void EmitRun(const std::vector<PPToken>& run);
    void EmitRaw(const std::vector<PPToken>& tokens, size_t begin, size_t end);
    void EmitMarker(PPItem::Kind kind, const std::string& condition, const PPSourceLoc& loc);

    PPExprNode EvaluateCondition(const std::vector<PPToken>& tokens, size_t begin, size_t end,
                                 const PPSourceLoc& loc, bool& ok);
    PPExprNode EvaluateHead(const std::vector<PPToken>& tokens, size_t nameIndex, size_t lineEnd,
                            const std::string& kind, bool& ok);

    bool ResolveInclude(PPFileId fromFile, const std::string& spec, bool angled,
                        std::string& outPath, std::string& outCanonical, std::string& outContent);

    void PrescanDeferredNames(const std::vector<PPToken>& tokens);

    bool IsEmitting() const { return m_Conds.empty() ? true : m_Conds.back().Emitting; }
    bool InDeferred() const;

    std::string FilePathOf(PPFileId file) const { return m_Files.IsValid(file) ? m_Files.GetPath(file) : std::string(); }

    PPFileTable& m_Files;
    DiagnosticCollector& m_Diag;
    MacroTable m_Macros;
    PPExpr m_Expr;

    const PPParams* m_Params = nullptr;

    std::vector<PPItem>* m_Out = nullptr;
    std::vector<CondFrame> m_Conds;
    std::vector<PPVariantDecl> m_Variants;

    std::unordered_map<std::string, uint8_t> m_IncludeOnce;   // bit0 = 无条件展开过, bit1 = deferred 内展开过
    std::vector<std::string> m_IncludeStack;
    size_t m_Depth = 0;

    static constexpr size_t kMaxIncludeDepth = 64;
};

} // namespace PrismShaderCompiler
