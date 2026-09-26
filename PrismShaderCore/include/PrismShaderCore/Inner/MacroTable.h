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
    std::string VariadicName = "__VA_ARGS__";   // 具名变参（C++20 `Args...`）时的名字
    std::vector<std::string> Params;            // 仅固定参数
    std::vector<PPToken> Body;                  // 替换列表，不含自身名字
    PPSourceLoc Loc;
};

// 宏表与展开引擎。
//
// deferred 集合内的名字一律不展开、原样透传 —— 内层无从知道它是否被定义，
// 交由真实编译期的 glslang 按注入的 #define 处理。
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

    // 完全展开（含 defined 操作数）
    std::vector<PPToken> Expand(const std::vector<PPToken>& input, DiagnosticCollector* diag);

    // 条件行展开：defined 的操作数不展开
    std::vector<PPToken> ExpandCondition(const std::vector<PPToken>& input, DiagnosticCollector* diag);

    // 已展开至无宏可扩（用于判断参数是否涉及宏）
    void SetCounter(uint32_t value) { m_Counter = value; }

private:
    // 展开过程中的 token —— 比 PPToken 多一个隐藏集，实现蓝漆（不再重复展开自身）
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

    // 替换列表里的 token 一律脱掉源范围 —— 它们的位置属于宏定义，不属于输出
    static ExpToken MakeLiteral(const PPToken& tok, const PPSourceLoc& loc,
                                const std::vector<std::string>& hide);
    // LeadingSpace 由调用点决定：合成 token 顶替的是调用点，空白得跟着调用点走
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
