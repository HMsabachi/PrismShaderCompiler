#include "Inner/MacroTable.h"

#include "Inner/PPFile.h"
#include "Inner/PPLexer.h"
#include "PSL/Diagnostics.h"

#include <algorithm>

namespace PrismShaderCompiler
{

namespace
{

// 条件行里 defined 的操作数不得展开，用哨兵隐藏集把它们钉住。
// \x 转义会吞掉后续所有十六进制位，所以必须拆成两个字面量拼接
const char* const kDefinedOperandMarker = "\x01" "defined";

// 单次展开的 token 处理上限，防病态输入把编译卡死
constexpr int kExpansionStepLimit = 500000;

} // namespace

void MacroTable::Define(PPMacro macro)
{
    m_Macros[macro.Name] = std::move(macro);
}

void MacroTable::Undefine(std::string_view name)
{
    m_Macros.erase(std::string(name));
}

const PPMacro* MacroTable::Find(std::string_view name) const
{
    const auto it = m_Macros.find(std::string(name));
    return it == m_Macros.end() ? nullptr : &it->second;
}

void MacroTable::AddDeferredName(std::string name)
{
    m_Deferred.insert(std::move(name));
}

void MacroTable::AddDeferredNames(const std::vector<std::string>& names)
{
    for (const std::string& name : names)
        m_Deferred.insert(name);
}

bool MacroTable::IsDeferred(std::string_view name) const
{
    return m_Deferred.find(std::string(name)) != m_Deferred.end();
}

bool MacroTable::HasHide(const std::vector<std::string>& hide, std::string_view name)
{
    return std::find(hide.begin(), hide.end(), name) != hide.end();
}

std::vector<std::string> MacroTable::IntersectHide(const std::vector<std::string>& a,
                                                   const std::vector<std::string>& b)
{
    std::vector<std::string> out;
    for (const std::string& name : a)
    {
        if (HasHide(b, name) && !HasHide(out, name))
            out.push_back(name);
    }
    return out;
}

MacroTable::ExpToken MacroTable::MakeLiteral(const PPToken& tok, const PPSourceLoc& loc,
                                             const std::vector<std::string>& hide)
{
    ExpToken e;
    e.Tok = tok;
    e.Tok.Loc = loc;
    e.Tok.RawOffset = 0;
    e.Tok.RawLength = 0;
    e.Tok.TriviaOffset = 0;
    e.Tok.TriviaLength = 0;
    e.Hide = hide;
    return e;
}

MacroTable::ExpToken MacroTable::MakeSynthetic(PPType type, std::string spelling, const PPSourceLoc& loc,
                                               const std::vector<std::string>& hide, bool leadingSpace)
{
    ExpToken e;
    e.Tok.Type = type;
    e.Tok.Loc = loc;
    e.Tok.Spelling = std::move(spelling);
    e.Tok.RawOffset = 0;
    e.Tok.RawLength = 0;
    e.Tok.TriviaOffset = 0;
    e.Tok.TriviaLength = 0;
    e.Tok.LeadingSpace = leadingSpace;
    e.Hide = hide;
    return e;
}

MacroTable::ExpToken MacroTable::MakeStringized(const std::vector<ExpToken>& arg, const PPSourceLoc& loc,
                                                const std::vector<std::string>& hide)
{
    std::string text = "\"";

    for (size_t k = 0; k < arg.size(); ++k)
    {
        if (k > 0 && arg[k].Tok.LeadingSpace)
            text.push_back(' ');

        for (const char c : arg[k].Tok.Spelling)
        {
            if (c == '"' || c == '\\')
                text.push_back('\\');
            text.push_back(c);
        }
    }

    text.push_back('"');
    return MakeSynthetic(PPType::StringLiteral, std::move(text), loc, hide);
}

void MacroTable::AppendAdjusted(std::vector<ExpToken>& out, const std::vector<ExpToken>& src,
                                const std::vector<std::string>& hide)
{
    for (const ExpToken& e : src)
    {
        ExpToken copy = e;

        for (const std::string& name : hide)
        {
            if (!HasHide(copy.Hide, name))
                copy.Hide.push_back(name);
        }

        out.push_back(std::move(copy));
    }
}

MacroTable::ExpToken MacroTable::PasteTokens(const ExpToken& left, const ExpToken& right,
                                             const PPSourceLoc& loc, DiagnosticCollector* diag)
{
    const std::string pasted = left.Tok.Spelling + right.Tok.Spelling;

    PPLexer lexer(pasted, PP_NO_FILE);
    const std::vector<PPToken> lexed = lexer.Tokenize(nullptr);

    ExpToken out;
    out.Hide = IntersectHide(left.Hide, right.Hide);

    const PPToken* single = nullptr;

    for (const PPToken& token : lexed)
    {
        if (token.Is(PPType::EndOfFile))
            continue;

        if (single != nullptr)
        {
            single = nullptr;
            break;
        }

        single = &token;
    }

    if (single != nullptr)
    {
        out.Tok = *single;
    }
    else
    {
        if (diag)
            diag->Warning("## 拼接结果 \"" + pasted + "\" 不是单个 token", ToDiagnosticLocation(loc));

        out.Tok.Type = PPType::Other;
        out.Tok.Spelling = pasted;
    }

    out.Tok.Loc = loc;
    out.Tok.RawOffset = 0;
    out.Tok.RawLength = 0;
    out.Tok.TriviaOffset = 0;
    out.Tok.TriviaLength = 0;
    out.Tok.AtLineStart = false;
    out.Tok.LeadingSpace = left.Tok.LeadingSpace;
    return out;
}

void MacroTable::PasteInto(std::vector<ExpToken>& out, const std::vector<ExpToken>& rhs,
                           const PPSourceLoc& loc, DiagnosticCollector* diag)
{
    // 空实参在 ## 两侧是 placemarker：保留另一侧原样
    if (rhs.empty())
        return;

    if (out.empty())
    {
        out = rhs;
        return;
    }

    ExpToken pasted = PasteTokens(out.back(), rhs.front(), loc, diag);
    out.back() = std::move(pasted);

    for (size_t k = 1; k < rhs.size(); ++k)
        out.push_back(rhs[k]);
}

size_t MacroTable::MatchParen(const std::vector<PPToken>& body, size_t open)
{
    int depth = 0;

    for (size_t k = open; k < body.size(); ++k)
    {
        if (body[k].IsPunct("("))
        {
            ++depth;
        }
        else if (body[k].IsPunct(")"))
        {
            --depth;

            if (depth == 0)
                return k;
        }
    }

    return body.size();
}

bool MacroTable::CollectArguments(const std::vector<ExpToken>& input, size_t open, const PPMacro& macro,
                                  size_t& close, std::vector<std::vector<ExpToken>>& rawArgs,
                                  DiagnosticCollector* diag)
{
    std::vector<std::vector<ExpToken>> segments;
    segments.emplace_back();

    int depth = 1;
    size_t i = open + 1;

    for (; i < input.size(); ++i)
    {
        const PPToken& tok = input[i].Tok;

        if (tok.IsPunct("("))
        {
            ++depth;
        }
        else if (tok.IsPunct(")"))
        {
            --depth;

            if (depth == 0)
                break;
        }
        else if (tok.IsPunct(",") && depth == 1)
        {
            segments.emplace_back();
            continue;
        }

        // 换行在 PP-token 世界里等同空白，实参里一律剔掉
        if (tok.Is(PPType::NewLine))
            continue;

        segments.back().push_back(input[i]);
    }

    if (depth != 0)
    {
        if (diag)
            diag->Error("宏 " + macro.Name + " 的实参列表未闭合", ToDiagnosticLocation(input[open].Tok.Loc));

        return false;
    }

    close = i;

    const size_t fixed = macro.Params.size();
    const size_t total = fixed + (macro.Variadic ? 1 : 0);

    if (total == 0 && segments.size() == 1 && segments.front().empty())
    {
        rawArgs.clear();
        return true;
    }

    if (!macro.Variadic && segments.size() != fixed)
    {
        if (diag)
        {
            diag->Error("宏 " + macro.Name + " 需要 " + std::to_string(fixed) + " 个实参，实际给出 "
                        + std::to_string(segments.size()) + " 个", ToDiagnosticLocation(input[open].Tok.Loc));
        }

        return false;
    }

    if (macro.Variadic && segments.size() < fixed)
    {
        if (diag)
        {
            diag->Error("宏 " + macro.Name + " 至少需要 " + std::to_string(fixed) + " 个实参，实际给出 "
                        + std::to_string(segments.size()) + " 个", ToDiagnosticLocation(input[open].Tok.Loc));
        }

        return false;
    }

    rawArgs.assign(total, {});

    for (size_t k = 0; k < fixed; ++k)
        rawArgs[k] = std::move(segments[k]);

    if (macro.Variadic)
    {
        for (size_t k = fixed; k < segments.size(); ++k)
        {
            if (k > fixed)
                rawArgs[fixed].push_back(MakeSynthetic(PPType::Punctuator, ",", input[open].Tok.Loc, {}));

            for (ExpToken& e : segments[k])
                rawArgs[fixed].push_back(std::move(e));
        }
    }

    return true;
}

std::vector<MacroTable::ExpToken> MacroTable::Instantiate(
    const PPMacro& macro,
    const std::vector<std::vector<ExpToken>>& rawArgs,
    const std::vector<std::vector<ExpToken>>& preArgs,
    const std::vector<std::string>& hide,
    const PPSourceLoc& invocationLoc,
    DiagnosticCollector* diag)
{
    static const std::vector<ExpToken> kEmpty;

    const size_t fixed = macro.Params.size();
    const size_t variadicIndex = fixed;

    auto paramIndex = [&](const std::string& name) -> int
    {
        for (size_t k = 0; k < fixed; ++k)
        {
            if (macro.Params[k] == name)
                return static_cast<int>(k);
        }

        if (macro.Variadic && name == macro.VariadicName)
            return static_cast<int>(variadicIndex);

        return -1;
    };

    auto argAt = [&](size_t index) -> const std::vector<ExpToken>&
    {
        return index < rawArgs.size() ? rawArgs[index] : kEmpty;
    };

    auto preAt = [&](size_t index) -> const std::vector<ExpToken>&
    {
        return index < preArgs.size() ? preArgs[index] : kEmpty;
    };

    const bool variadicEmpty = !macro.Variadic || argAt(variadicIndex).empty();
    const std::vector<PPToken>& body = macro.Body;

    // 替换列表里参数前的空白属于替换列表本身，不随实参走 —— 补到首个代入 token 上
    auto substitute = [&](std::vector<ExpToken>& dst, const std::vector<ExpToken>& src, bool leadingSpace)
    {
        const size_t before = dst.size();
        AppendAdjusted(dst, src, hide);

        if (leadingSpace && dst.size() > before && !dst[before].Tok.LeadingSpace)
            dst[before].Tok.LeadingSpace = true;
    };

    std::vector<ExpToken> out;
    size_t k = 0;

    while (k < body.size())
    {
        const PPToken& bt = body[k];

        // # 参数 —— 取未展开的实参原文
        if (bt.IsPunct("#") && k + 1 < body.size())
        {
            const int pi = paramIndex(body[k + 1].Spelling);

            if (pi >= 0)
            {
                // 字符串化产物顶替的是 body 里的 `#`，空白跟着它走（同 MakeLiteral）
                out.push_back(MakeStringized(argAt(static_cast<size_t>(pi)), bt.Loc, hide));
                out.back().Tok.LeadingSpace = bt.LeadingSpace;
                k += 2;
                continue;
            }
        }

        if (bt.IsPunct("##"))
        {
            ++k;

            if (k >= body.size())
                break;

            const int pi = paramIndex(body[k].Spelling);

            // GNU 扩展 `, ## __VA_ARGS__`：变参为空时连逗号一起去掉，非空时 ## 退化为普通拼接。
            // 标准 C 在这里会把逗号与实参首 token 粘成一个非法 token，所以必须特判。
            if (macro.Variadic && pi == static_cast<int>(variadicIndex)
                && !out.empty() && out.back().Tok.IsPunct(","))
            {
                if (argAt(variadicIndex).empty())
                    out.pop_back();
                else
                    substitute(out, preAt(variadicIndex), bt.LeadingSpace);

                ++k;
                continue;
            }

            std::vector<ExpToken> rhs;

            if (pi >= 0)
                AppendAdjusted(rhs, argAt(static_cast<size_t>(pi)), hide);
            else
                rhs.push_back(MakeLiteral(body[k], invocationLoc, hide));

            ++k;
            PasteInto(out, rhs, bt.Loc, diag);
            continue;
        }

        if (macro.Variadic && bt.IsIdent("__VA_OPT__") && k + 1 < body.size() && body[k + 1].IsPunct("("))
        {
            const size_t close = MatchParen(body, k + 1);

            if (!variadicEmpty && close > k + 2)
            {
                PPMacro sub = macro;
                sub.Body.assign(body.begin() + static_cast<ptrdiff_t>(k + 2),
                                body.begin() + static_cast<ptrdiff_t>(close));

                std::vector<ExpToken> inner = Instantiate(sub, rawArgs, preArgs, hide, invocationLoc, diag);

                for (ExpToken& e : inner)
                    out.push_back(std::move(e));
            }

            k = close + 1;
            continue;
        }

        const int pi = paramIndex(bt.Spelling);

        if (pi >= 0)
        {
            const size_t index = static_cast<size_t>(pi);
            const bool pasteLeft = (k + 1 < body.size() && body[k + 1].IsPunct("##"));

            if (pasteLeft)
            {
                if (macro.Variadic && index == variadicIndex && argAt(index).empty())
                {
                    // GNU 逗号吞并：变参为空时连它前面的逗号一起去掉
                    if (!out.empty() && out.back().Tok.IsPunct(","))
                        out.pop_back();
                }
                else
                {
                    substitute(out, argAt(index), bt.LeadingSpace);
                }
            }
            else
            {
                substitute(out, preAt(index), bt.LeadingSpace);
            }

            ++k;
            continue;
        }

        out.push_back(MakeLiteral(bt, invocationLoc, hide));
        ++k;
    }

    return out;
}

std::vector<MacroTable::ExpToken> MacroTable::ExpandTokens(std::vector<ExpToken> input,
                                                           DiagnosticCollector* diag)
{
    // 调用点前的空白属于调用点本身。替换列表首 token 带的是「宏名与替换列表之间的
    // 分隔符」，不是输出该有的缩进 —— 不覆盖的话 `A + A` 会缩成 `1 +1`。
    auto adoptLeadingSpace = [](std::vector<ExpToken>& repl, bool leadingSpace)
    {
        if (!repl.empty())
            repl.front().Tok.LeadingSpace = leadingSpace;
    };

    std::vector<ExpToken> out;
    size_t i = 0;
    int steps = 0;

    while (i < input.size())
    {
        if (++steps > kExpansionStepLimit)
        {
            if (diag)
                diag->Error("宏展开步数超出上限，已中止", ToDiagnosticLocation(input[i].Tok.Loc));

            break;
        }

        ExpToken& cur = input[i];

        if (HasHide(cur.Hide, kDefinedOperandMarker) || cur.Tok.IsNot(PPType::Identifier))
        {
            out.push_back(std::move(cur));
            ++i;
            continue;
        }

        const std::string name = cur.Tok.Spelling;

        if (name == "__LINE__")
        {
            out.push_back(MakeSynthetic(PPType::Number, std::to_string(cur.Tok.Loc.Line),
                                        cur.Tok.Loc, cur.Hide, cur.Tok.LeadingSpace));
            ++i;
            continue;
        }

        if (name == "__COUNTER__")
        {
            out.push_back(MakeSynthetic(PPType::Number, std::to_string(m_Counter++),
                                        cur.Tok.Loc, cur.Hide, cur.Tok.LeadingSpace));
            ++i;
            continue;
        }

        // deferred 名字一律不展开 —— 内层不知道它是否被定义
        if (IsDeferred(name) || HasHide(cur.Hide, name))
        {
            out.push_back(std::move(cur));
            ++i;
            continue;
        }

        const PPMacro* macro = Find(name);

        if (macro == nullptr)
        {
            out.push_back(std::move(cur));
            ++i;
            continue;
        }

        if (!macro->FunctionLike)
        {
            std::vector<std::string> hide = cur.Hide;
            hide.push_back(name);

            const PPSourceLoc loc = cur.Tok.Loc;
            const bool leadingSpace = cur.Tok.LeadingSpace;
            std::vector<ExpToken> repl = Instantiate(*macro, {}, {}, hide, loc, diag);
            adoptLeadingSpace(repl, leadingSpace);

            input.erase(input.begin() + static_cast<ptrdiff_t>(i));
            input.insert(input.begin() + static_cast<ptrdiff_t>(i), repl.begin(), repl.end());
            continue;
        }

        // 函数宏：实参列表的 '(' 允许跨行
        size_t open = i + 1;

        while (open < input.size() && input[open].Tok.Is(PPType::NewLine))
            ++open;

        if (open >= input.size() || input[open].Tok.IsNot(PPType::Punctuator) || input[open].Tok.Spelling != "(")
        {
            out.push_back(std::move(cur));
            ++i;
            continue;
        }

        size_t close = 0;
        std::vector<std::vector<ExpToken>> rawArgs;

        if (!CollectArguments(input, open, *macro, close, rawArgs, diag))
        {
            out.push_back(std::move(cur));
            ++i;
            continue;
        }

        std::vector<std::vector<ExpToken>> preArgs;
        preArgs.reserve(rawArgs.size());

        for (const std::vector<ExpToken>& arg : rawArgs)
            preArgs.push_back(ExpandTokens(arg, diag));

        std::vector<std::string> hide = IntersectHide(cur.Hide, input[close].Hide);
        hide.push_back(name);

        const PPSourceLoc loc = cur.Tok.Loc;
        const bool leadingSpace = cur.Tok.LeadingSpace;
        std::vector<ExpToken> repl = Instantiate(*macro, rawArgs, preArgs, hide, loc, diag);
        adoptLeadingSpace(repl, leadingSpace);

        input.erase(input.begin() + static_cast<ptrdiff_t>(i),
                    input.begin() + static_cast<ptrdiff_t>(close + 1));
        input.insert(input.begin() + static_cast<ptrdiff_t>(i), repl.begin(), repl.end());
    }

    return out;
}

std::vector<PPToken> MacroTable::Expand(const std::vector<PPToken>& input, DiagnosticCollector* diag)
{
    std::vector<ExpToken> work;
    work.reserve(input.size());

    for (const PPToken& token : input)
        work.push_back({token, {}});

    std::vector<ExpToken> expanded = ExpandTokens(std::move(work), diag);

    std::vector<PPToken> out;
    out.reserve(expanded.size());

    for (ExpToken& e : expanded)
        out.push_back(std::move(e.Tok));

    return out;
}

std::vector<PPToken> MacroTable::ExpandCondition(const std::vector<PPToken>& input,
                                                 DiagnosticCollector* diag)
{
    const std::string marker = kDefinedOperandMarker;

    std::vector<ExpToken> work;
    work.reserve(input.size());

    size_t i = 0;

    while (i < input.size())
    {
        work.push_back({input[i], {}});

        if (input[i].IsIdent("defined"))
        {
            size_t j = i + 1;
            const bool paren = (j < input.size() && input[j].IsPunct("("));

            if (paren)
            {
                work.push_back({input[j], {marker}});
                ++j;
            }

            if (j < input.size())
            {
                work.push_back({input[j], {marker}});
                ++j;
            }

            if (paren && j < input.size() && input[j].IsPunct(")"))
            {
                work.push_back({input[j], {marker}});
                ++j;
            }

            i = j;
            continue;
        }

        ++i;
    }

    std::vector<ExpToken> expanded = ExpandTokens(std::move(work), diag);

    std::vector<PPToken> out;
    out.reserve(expanded.size());

    for (ExpToken& e : expanded)
        out.push_back(std::move(e.Tok));

    return out;
}

} // namespace PrismShaderCompiler
