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

struct PPItem
{
    enum class Kind : uint8_t
    {
        Token,
        CondBegin,
        CondElse,
        CondEnd,
    };

    Kind K = Kind::Token;

    PPToken Tok;

    std::string Condition;
    PPSourceLoc Loc;
};

struct PPVariantDecl
{
    std::string Name;
    std::vector<std::string> Keywords;
    PPSourceLoc Loc;
};

struct PPResult
{
    std::vector<PPItem> Items;
    std::vector<PPVariantDecl> Variants;
    bool Success = true;
};

using PPReadFileFn = std::function<bool(const std::string& path, std::string& out)>;

struct PPParams
{
    std::string Path;
    std::string Source;
    std::string IncludeRoot;
    PPReadFileFn ReadFile;
    std::vector<std::string> Defines;
};

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
        bool Emitting = true;
        bool ParentEmitting = true;
        bool Taken = false;
        bool SuppressRest = false;
        bool SawElse = false;
        bool MarkerOpened = false;
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

    std::unordered_map<std::string, uint8_t> m_IncludeOnce;
    std::vector<std::string> m_IncludeStack;
    size_t m_Depth = 0;

    static constexpr size_t kMaxIncludeDepth = 64;
};

} // namespace PrismShaderCompiler
