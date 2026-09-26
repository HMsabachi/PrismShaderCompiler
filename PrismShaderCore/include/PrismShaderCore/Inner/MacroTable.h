#pragma once

#include "PPToken.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PrismShaderCompiler
{

class DiagnosticCollector;

struct PPMacro
{
    std::string Name;
    bool FunctionLike = false;
    bool Variadic = false;
    std::string VariadicName = "__VA_ARGS__";
    std::vector<std::string> Params;
    std::vector<PPToken> Body;
    PPSourceLoc Loc;
};

class MacroTable
{
public:
    void Define(PPMacro macro);
    void Undefine(std::string_view name);

    const PPMacro* Find(std::string_view name) const;
    bool IsDefined(std::string_view name) const { return Find(name) != nullptr; }

    void AddDeferredName(std::string name);
    void AddDeferredNames(const std::vector<std::string>& names);
    bool IsDeferred(std::string_view name) const;

    std::vector<PPToken> Expand(const std::vector<PPToken>& input, DiagnosticCollector* diag);

    std::vector<PPToken> ExpandCondition(const std::vector<PPToken>& input, DiagnosticCollector* diag);

    void SetCounter(uint32_t value) { m_Counter = value; }

private:
    struct ExpToken
    {
        PPToken Tok;
        std::vector<std::string> Hide;
    };

    std::vector<ExpToken> ExpandTokens(std::vector<ExpToken> input, DiagnosticCollector* diag);

    bool CollectArguments(const std::vector<ExpToken>& input, size_t open, const PPMacro& macro,
                          size_t& close, std::vector<std::vector<ExpToken>>& rawArgs,
                          DiagnosticCollector* diag);

    std::vector<ExpToken> Instantiate(const PPMacro& macro,
                                      const std::vector<std::vector<ExpToken>>& rawArgs,
                                      const std::vector<std::vector<ExpToken>>& preArgs,
                                      const std::vector<std::string>& hide,
                                      const PPSourceLoc& invocationLoc,
                                      DiagnosticCollector* diag);

    static bool HasHide(const std::vector<std::string>& hide, std::string_view name);
    static std::vector<std::string> IntersectHide(const std::vector<std::string>& a,
                                                  const std::vector<std::string>& b);

    static ExpToken MakeLiteral(const PPToken& tok, const PPSourceLoc& loc,
                                const std::vector<std::string>& hide);
    static ExpToken MakeSynthetic(PPType type, std::string spelling, const PPSourceLoc& loc,
                                  const std::vector<std::string>& hide, bool leadingSpace = false);
    static ExpToken MakeStringized(const std::vector<ExpToken>& arg, const PPSourceLoc& loc,
                                   const std::vector<std::string>& hide);

    static void AppendAdjusted(std::vector<ExpToken>& out, const std::vector<ExpToken>& src,
                               const std::vector<std::string>& hide);
    static ExpToken PasteTokens(const ExpToken& left, const ExpToken& right,
                                const PPSourceLoc& loc, DiagnosticCollector* diag);
    static void PasteInto(std::vector<ExpToken>& out, const std::vector<ExpToken>& rhs,
                          const PPSourceLoc& loc, DiagnosticCollector* diag);
    static size_t MatchParen(const std::vector<PPToken>& body, size_t open);

    std::unordered_map<std::string, PPMacro> m_Macros;
    std::unordered_set<std::string> m_Deferred;
    uint32_t m_Counter = 0;
};

} // namespace PrismShaderCompiler
