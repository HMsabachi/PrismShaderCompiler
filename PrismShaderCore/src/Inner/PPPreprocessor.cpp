#include "Inner/PPPreprocessor.h"

#include "Inner/PPLexer.h"
#include "PSL/Diagnostics.h"

#include <filesystem>

namespace PrismShaderCompiler
{

PPPreprocessor::PPPreprocessor(PPFileTable& files, DiagnosticCollector& diag)
    : m_Files(files), m_Diag(diag), m_Expr(m_Macros, &diag)
{
}

PPResult PPPreprocessor::Run(const PPParams& params)
{
    PPResult result;

    const size_t diagnosticsBefore = m_Diag.GetDiagnostics().size();

    m_Macros = MacroTable();
    m_Macros.AddDeferredName("PRISM_BACKEND_OPENGL");
    m_Macros.AddDeferredName("PRISM_BACKEND_VULKAN");

    for (const std::string& name : params.Defines)
    {
        PPMacro macro;
        macro.Name = name;

        PPToken body;
        body.Type = PPType::Number;
        body.Spelling = "1";

        macro.Body.push_back(std::move(body));
        m_Macros.Define(std::move(macro));
    }

    m_Conds.clear();
    m_Variants.clear();
    m_IncludeOnce.clear();
    m_IncludeStack.clear();
    m_Depth = 0;
    m_Params = &params;
    m_Out = &result.Items;

    const std::string canonical = std::filesystem::path(params.Path).lexically_normal().generic_string();
    const PPFileId mainFile = m_Files.Add(params.Path, canonical, params.Source);

    {
        PPLexer lexer(params.Source, mainFile);
        const std::vector<PPToken> prescan = lexer.Tokenize(nullptr);
        PrescanDeferredNames(prescan);
    }

    ProcessFile(mainFile);

    if (!m_Conds.empty())
    {
        m_Diag.Error("有 " + std::to_string(m_Conds.size()) + " 个 #if 未闭合",
                     ToDiagnosticLocation(m_Conds.back().Loc, params.Path));
        m_Conds.clear();
    }

    result.Variants = std::move(m_Variants);

    const std::vector<Diagnostic>& all = m_Diag.GetDiagnostics();
    result.Success = true;

    for (size_t i = diagnosticsBefore; i < all.size(); ++i)
    {
        if (all[i].Level != Severity::Warning)
        {
            result.Success = false;
            break;
        }
    }

    return result;
}

void PPPreprocessor::PrescanDeferredNames(const std::vector<PPToken>& tokens)
{
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].Is(PPType::EndOfFile))
            break;

        if (!tokens[i].AtLineStart || !tokens[i].IsPunct("#"))
            continue;

        if (i + 2 >= tokens.size() || !tokens[i + 1].IsIdent("pragma"))
            continue;

        const std::string& kind = tokens[i + 2].Spelling;

        size_t begin = 0;

        if (kind == "multi_compile" || kind == "shader_feature")
        {
            begin = i + 3;
        }
        else if (kind == "kernel")
        {
            begin = i + 3;

            if (begin < tokens.size() && tokens[begin].Is(PPType::Identifier))
                ++begin;
        }
        else
        {
            continue;
        }

        const size_t lineEnd = FindLineEnd(tokens, i);
        std::vector<std::string> names;

        for (size_t k = begin; k < lineEnd; ++k)
        {
            if (tokens[k].Is(PPType::Identifier) && tokens[k].Spelling != "_")
                names.push_back(tokens[k].Spelling);
        }

        m_Macros.AddDeferredNames(names);
    }
}

void PPPreprocessor::ProcessFile(PPFileId file)
{
    std::vector<PPToken> tokens;

    {
        PPLexer lexer(m_Files.GetSource(file), file);
        tokens = lexer.Tokenize(&m_Diag);
    }

    ProcessTokens(file, tokens);
}

void PPPreprocessor::ProcessTokens(PPFileId file, const std::vector<PPToken>& tokens)
{
    size_t i = 0;

    while (i < tokens.size())
    {
        if (tokens[i].Is(PPType::EndOfFile))
            break;

        if (tokens[i].AtLineStart && tokens[i].IsPunct("#"))
        {
            HandleDirective(file, tokens, i);
            continue;
        }

        const size_t next = FindNextDirective(tokens, i);

        EmitRun(std::vector<PPToken>(tokens.begin() + static_cast<ptrdiff_t>(i),
                                     tokens.begin() + static_cast<ptrdiff_t>(next)));
        i = next;
    }
}

size_t PPPreprocessor::FindLineEnd(const std::vector<PPToken>& tokens, size_t start) const
{
    for (size_t i = start; i < tokens.size(); ++i)
    {
        if (tokens[i].Is(PPType::NewLine) || tokens[i].Is(PPType::EndOfFile))
            return i;
    }

    return tokens.size();
}

size_t PPPreprocessor::FindNextDirective(const std::vector<PPToken>& tokens, size_t start) const
{
    for (size_t i = start; i < tokens.size(); ++i)
    {
        if (tokens[i].Is(PPType::EndOfFile))
            return i;

        if (tokens[i].AtLineStart && tokens[i].IsPunct("#"))
            return i;
    }

    return tokens.size();
}

void PPPreprocessor::HandleDirective(PPFileId file, const std::vector<PPToken>& tokens, size_t& index)
{
    const size_t lineEnd = FindLineEnd(tokens, index);
    const size_t nextIndex = (lineEnd < tokens.size() && tokens[lineEnd].Is(PPType::NewLine))
        ? lineEnd + 1 : lineEnd;

    const bool named = (index + 1 < lineEnd) && tokens[index + 1].Is(PPType::Identifier);

    if (!named)
    {
        if (IsEmitting())
            EmitRaw(tokens, index, lineEnd);

        index = nextIndex;
        return;
    }

    const std::string name = tokens[index + 1].Spelling;

    if (name == "define")       HandleDefine(tokens, index, lineEnd);
    else if (name == "undef")   HandleUndef(tokens, index, lineEnd);
    else if (name == "include") HandleInclude(file, tokens, index, lineEnd);
    else if (name == "pragma")  HandlePragma(file, tokens, index, lineEnd);
    else if (name == "if" || name == "ifdef" || name == "ifndef"
             || name == "elif" || name == "else" || name == "endif")
        HandleConditional(file, tokens, index + 1, lineEnd);
    else if (name == "error")   HandleError(tokens, index, lineEnd, true);
    else if (name == "warning") HandleError(tokens, index, lineEnd, false);
    else if (IsEmitting())      EmitRaw(tokens, index, lineEnd);

    index = nextIndex;
}

void PPPreprocessor::HandleDefine(const std::vector<PPToken>& tokens, size_t hashIndex, size_t lineEnd)
{
    if (InDeferred())
    {
        if (IsEmitting())
            EmitRaw(tokens, hashIndex, lineEnd);

        return;
    }

    if (!IsEmitting())
        return;

    const size_t nameIndex = hashIndex + 2;

    if (nameIndex >= lineEnd || tokens[nameIndex].IsNot(PPType::Identifier))
    {
        m_Diag.Error("#define 缺少宏名", ToDiagnosticLocation(tokens[hashIndex].Loc, FilePathOf(tokens[hashIndex].Loc.File)));
        return;
    }

    PPMacro macro;
    macro.Name = tokens[nameIndex].Spelling;
    macro.Loc = tokens[nameIndex].Loc;

    size_t cursor = nameIndex + 1;

    if (cursor < lineEnd && tokens[cursor].IsPunct("(") && !tokens[cursor].LeadingSpace)
    {
        macro.FunctionLike = true;
        ++cursor;

        while (cursor < lineEnd && !tokens[cursor].IsPunct(")"))
        {
            if (tokens[cursor].IsPunct("..."))
            {
                macro.Variadic = true;
                ++cursor;
                break;
            }

            if (tokens[cursor].Is(PPType::Identifier))
            {
                const std::string param = tokens[cursor].Spelling;
                ++cursor;

                if (cursor < lineEnd && tokens[cursor].IsPunct("..."))
                {
                    macro.Variadic = true;
                    macro.VariadicName = param;
                    ++cursor;
                    break;
                }

                macro.Params.push_back(param);

                if (cursor < lineEnd && tokens[cursor].IsPunct(","))
                    ++cursor;

                continue;
            }

            ++cursor;
        }

        if (cursor < lineEnd && tokens[cursor].IsPunct(")"))
            ++cursor;
    }

    macro.Body.assign(tokens.begin() + static_cast<ptrdiff_t>(cursor),
                      tokens.begin() + static_cast<ptrdiff_t>(lineEnd));

    m_Macros.Define(std::move(macro));
}

void PPPreprocessor::HandleUndef(const std::vector<PPToken>& tokens, size_t hashIndex, size_t lineEnd)
{
    if (InDeferred())
    {
        if (IsEmitting())
            EmitRaw(tokens, hashIndex, lineEnd);

        return;
    }

    if (!IsEmitting())
        return;

    const size_t nameIndex = hashIndex + 2;

    if (nameIndex < lineEnd && tokens[nameIndex].Is(PPType::Identifier))
        m_Macros.Undefine(tokens[nameIndex].Spelling);
}

void PPPreprocessor::HandlePragma(PPFileId file, const std::vector<PPToken>& tokens,
                                  size_t hashIndex, size_t lineEnd)
{
    const size_t cursor = hashIndex + 2;

    if (cursor >= lineEnd)
    {
        if (IsEmitting())
            EmitRaw(tokens, hashIndex, lineEnd);

        return;
    }

    const std::string kind = tokens[cursor].Spelling;

    if (kind == "once")
    {
        if (!InDeferred())
            m_IncludeOnce[m_Files.Get(file).CanonicalPath] |= 1;

        return;
    }

    if (kind == "multi_compile" || kind == "shader_feature")
    {
        PPVariantDecl decl;
        decl.Name = kind;
        decl.Loc = tokens[cursor].Loc;

        for (size_t i = cursor + 1; i < lineEnd; ++i)
        {
            if (tokens[i].IsNot(PPType::Identifier) || tokens[i].Spelling == "_")
                continue;

            decl.Keywords.push_back(tokens[i].Spelling);
        }

        if (!decl.Keywords.empty())
        {
            m_Macros.AddDeferredNames(decl.Keywords);

            if (IsEmitting())
                m_Variants.push_back(std::move(decl));
        }

        return;
    }

    if (IsEmitting())
        EmitRaw(tokens, hashIndex, lineEnd);
}

void PPPreprocessor::HandleError(const std::vector<PPToken>& tokens, size_t hashIndex,
                                 size_t lineEnd, bool fatal)
{
    if (!IsEmitting())
        return;

    if (InDeferred())
    {
        EmitRaw(tokens, hashIndex, lineEnd);
        return;
    }

    std::string text;

    for (size_t i = hashIndex + 2; i < lineEnd; ++i)
    {
        if (!text.empty() && tokens[i].LeadingSpace)
            text.push_back(' ');

        text += tokens[i].Spelling;
    }

    const PPSourceLoc loc = (hashIndex + 1 < lineEnd) ? tokens[hashIndex + 1].Loc : tokens[hashIndex].Loc;

    if (fatal)
        m_Diag.Error("#error " + text, ToDiagnosticLocation(loc, FilePathOf(loc.File)));
    else
        m_Diag.Warning("#warning " + text, ToDiagnosticLocation(loc, FilePathOf(loc.File)));
}

PPExprNode PPPreprocessor::EvaluateCondition(const std::vector<PPToken>& tokens, size_t begin, size_t end,
                                             const PPSourceLoc& loc, bool& ok)
{
    const std::vector<PPToken> raw(tokens.begin() + static_cast<ptrdiff_t>(begin),
                                   tokens.begin() + static_cast<ptrdiff_t>(end));
    const std::vector<PPToken> expanded = m_Macros.ExpandCondition(raw, &m_Diag);

    PPExprNode node = m_Expr.Parse(expanded, loc);
    ok = !m_Expr.Failed();
    return node;
}

PPExprNode PPPreprocessor::EvaluateHead(const std::vector<PPToken>& tokens, size_t nameIndex,
                                        size_t lineEnd, const std::string& kind, bool& ok)
{
    ok = true;

    const PPSourceLoc loc = tokens[nameIndex].Loc;

    if (kind == "ifdef" || kind == "ifndef")
    {
        if (nameIndex + 1 >= lineEnd || tokens[nameIndex + 1].IsNot(PPType::Identifier))
        {
            m_Diag.Error("#" + kind + " 缺少标识符", ToDiagnosticLocation(loc, FilePathOf(loc.File)));
            ok = false;
            return PPExprMakeConst(0, loc);
        }

        const std::string name = tokens[nameIndex + 1].Spelling;

        PPExprNode cond = m_Macros.IsDeferred(name)
            ? PPExprMakeSymbol("defined(" + name + ")", loc)
            : PPExprMakeConst(m_Macros.IsDefined(name) ? 1 : 0, loc);

        if (kind == "ifndef")
            cond = PPExprNegate(cond);

        return cond;
    }

    return EvaluateCondition(tokens, nameIndex + 1, lineEnd, loc, ok);
}

void PPPreprocessor::HandleConditional(PPFileId file, const std::vector<PPToken>& tokens,
                                       size_t nameIndex, size_t lineEnd)
{
    const std::string kind = tokens[nameIndex].Spelling;
    const PPSourceLoc loc = tokens[nameIndex].Loc;
    const std::string path = FilePathOf(file);

    if (kind == "endif")
    {
        if (m_Conds.empty())
        {
            m_Diag.Error("#endif 没有匹配的 #if", ToDiagnosticLocation(loc, path));
            return;
        }

        const bool opened = m_Conds.back().MarkerOpened;
        m_Conds.pop_back();

        if (opened)
            EmitMarker(PPItem::Kind::CondEnd, std::string(), loc);

        return;
    }

    if (kind == "else" || kind == "elif")
    {
        if (m_Conds.empty())
        {
            m_Diag.Error("#" + kind + " 没有匹配的 #if", ToDiagnosticLocation(loc, path));
            return;
        }

        CondFrame& frame = m_Conds.back();

        if (frame.SawElse)
        {
            m_Diag.Error("#" + kind + " 出现在 #else 之后", ToDiagnosticLocation(loc, path));
            return;
        }

        if (kind == "else")
        {
            frame.SawElse = true;

            if (frame.S == CondFrame::State::Deferred)
            {
                frame.Emitting = frame.ParentEmitting && !frame.SuppressRest;

                if (frame.Emitting)
                    EmitMarker(PPItem::Kind::CondElse, "else", loc);
            }
            else
            {
                frame.Emitting = frame.ParentEmitting && !frame.Taken;
                frame.Taken = true;
                frame.S = frame.Emitting ? CondFrame::State::Active : CondFrame::State::Skipped;
            }

            return;
        }

        if (frame.S == CondFrame::State::Deferred)
        {
            bool ok = true;
            const PPExprNode cond = EvaluateCondition(tokens, nameIndex + 1, lineEnd, loc, ok);

            if (!ok)
            {
                frame.Emitting = false;
                return;
            }

            if (PPExprIsConst(cond))
            {
                if (PPExprConstValue(cond) != 0 && !frame.SuppressRest)
                {
                    frame.SuppressRest = true;
                    frame.Emitting = frame.ParentEmitting;
                }
                else
                {
                    frame.Emitting = false;
                }

                if (frame.Emitting)
                    EmitMarker(PPItem::Kind::CondElse, "else", loc);

                return;
            }

            frame.Emitting = frame.ParentEmitting && !frame.SuppressRest;

            if (frame.Emitting)
                EmitMarker(PPItem::Kind::CondElse, "elif " + PPExprToText(cond), loc);

            return;
        }

        if (frame.Taken)
        {
            frame.Emitting = false;
            frame.S = CondFrame::State::Skipped;
            return;
        }

        bool ok = true;
        const PPExprNode cond = EvaluateCondition(tokens, nameIndex + 1, lineEnd, loc, ok);

        if (!ok)
        {
            frame.Emitting = false;
            frame.S = CondFrame::State::Skipped;
            return;
        }

        if (PPExprIsConst(cond))
        {
            const bool truth = PPExprConstValue(cond) != 0;
            frame.Emitting = frame.ParentEmitting && truth;
            frame.Taken = truth;
            frame.S = frame.Emitting ? CondFrame::State::Active : CondFrame::State::Skipped;
            return;
        }

        frame.S = CondFrame::State::Deferred;
        frame.Emitting = frame.ParentEmitting;

        if (frame.Emitting)
        {
            EmitMarker(PPItem::Kind::CondBegin, PPExprToText(cond), loc);
            frame.MarkerOpened = true;
        }

        return;
    }

    bool ok = true;
    const PPExprNode cond = EvaluateHead(tokens, nameIndex, lineEnd, kind, ok);

    CondFrame frame;
    frame.ParentEmitting = IsEmitting();
    frame.Loc = loc;

    if (!ok)
    {
        frame.S = CondFrame::State::Skipped;
        frame.Emitting = false;
        frame.Taken = true;
    }
    else if (PPExprIsConst(cond))
    {
        const bool truth = PPExprConstValue(cond) != 0;
        frame.S = truth ? CondFrame::State::Active : CondFrame::State::Skipped;
        frame.Emitting = frame.ParentEmitting && truth;
        frame.Taken = truth;
    }
    else
    {
        frame.S = CondFrame::State::Deferred;
        frame.Emitting = frame.ParentEmitting;

        if (frame.Emitting)
        {
            EmitMarker(PPItem::Kind::CondBegin, PPExprToText(cond), loc);
            frame.MarkerOpened = true;
        }
    }

    m_Conds.push_back(frame);
}

void PPPreprocessor::HandleInclude(PPFileId file, const std::vector<PPToken>& tokens,
                                   size_t hashIndex, size_t lineEnd)
{
    if (!IsEmitting())
        return;

    const size_t cursor = hashIndex + 2;

    if (cursor >= lineEnd)
    {
        m_Diag.Error("#include 缺少路径", ToDiagnosticLocation(tokens[hashIndex].Loc, FilePathOf(file)));
        return;
    }

    const std::vector<PPToken> raw(tokens.begin() + static_cast<ptrdiff_t>(cursor),
                                   tokens.begin() + static_cast<ptrdiff_t>(lineEnd));
    const std::vector<PPToken> spec = m_Macros.Expand(raw, &m_Diag);

    bool angled = false;
    std::string path;

    if (!spec.empty() && spec.front().Is(PPType::StringLiteral))
    {
        const std::string& text = spec.front().Spelling;

        if (text.size() >= 2)
            path = text.substr(1, text.size() - 2);
    }
    else if (!spec.empty() && spec.front().IsPunct("<"))
    {
        angled = true;

        for (size_t i = 1; i < spec.size() && !spec[i].IsPunct(">"); ++i)
            path += spec[i].Spelling;
    }

    if (path.empty())
    {
        m_Diag.Error("#include 路径无法解析", ToDiagnosticLocation(tokens[hashIndex].Loc, FilePathOf(file)));
        return;
    }

    std::string resolved;
    std::string canonical;
    std::string content;

    if (!ResolveInclude(file, path, angled, resolved, canonical, content))
    {
        m_Diag.Error("找不到 include 文件: " + path, ToDiagnosticLocation(tokens[hashIndex].Loc, FilePathOf(file)));
        return;
    }

    const bool deferred = InDeferred();
    uint8_t& flags = m_IncludeOnce[canonical];

    if ((flags & 1) != 0)
    {
        if (deferred)
        {
            m_Diag.Error("文件 " + path + " 既被无条件包含，又被 deferred 条件内包含 —— include-once 无法同时满足两者",
                         ToDiagnosticLocation(tokens[hashIndex].Loc, FilePathOf(file)));
        }

        return;
    }

    flags |= deferred ? 2 : 1;

    if (m_Depth >= kMaxIncludeDepth)
    {
        m_Diag.Error("include 嵌套超过 " + std::to_string(kMaxIncludeDepth) + " 层",
                     ToDiagnosticLocation(tokens[hashIndex].Loc, FilePathOf(file)));
        return;
    }

    for (const std::string& active : m_IncludeStack)
    {
        if (active == canonical)
        {
            m_Diag.Error("include 出现环: " + path, ToDiagnosticLocation(tokens[hashIndex].Loc, FilePathOf(file)));
            return;
        }
    }

    const PPFileId included = m_Files.Add(resolved, canonical, std::move(content));

    m_IncludeStack.push_back(canonical);
    ++m_Depth;
    ProcessFile(included);
    --m_Depth;
    m_IncludeStack.pop_back();
}

bool PPPreprocessor::ResolveInclude(PPFileId fromFile, const std::string& spec, bool angled,
                                    std::string& outPath, std::string& outCanonical, std::string& outContent)
{
    if (m_Params == nullptr || !m_Params->ReadFile)
        return false;

    std::vector<std::string> candidates;

    if (!angled)
    {
        const std::string& from = m_Files.Get(fromFile).Path;
        const size_t slash = from.find_last_of("/\\");

        if (slash != std::string::npos)
            candidates.push_back(from.substr(0, slash + 1) + spec);
    }

    if (!m_Params->IncludeRoot.empty())
    {
        std::string root = m_Params->IncludeRoot;

        if (root.back() != '/' && root.back() != '\\')
            root.push_back('/');

        candidates.push_back(root + spec);
    }

    candidates.push_back(spec);

    for (const std::string& candidate : candidates)
    {
        std::string content;

        if (!m_Params->ReadFile(candidate, content))
            continue;

        outPath = candidate;
        outCanonical = std::filesystem::path(candidate).lexically_normal().generic_string();
        outContent = std::move(content);
        return true;
    }

    return false;
}

void PPPreprocessor::EmitRun(const std::vector<PPToken>& run)
{
    if (!IsEmitting() || run.empty())
        return;

    const std::vector<PPToken> expanded = m_Macros.Expand(run, &m_Diag);

    for (const PPToken& token : expanded)
    {
        PPItem item;
        item.K = PPItem::Kind::Token;
        item.Tok = token;
        m_Out->push_back(std::move(item));
    }
}

void PPPreprocessor::EmitRaw(const std::vector<PPToken>& tokens, size_t begin, size_t end)
{
    for (size_t i = begin; i < end; ++i)
    {
        PPItem item;
        item.K = PPItem::Kind::Token;
        item.Tok = tokens[i];
        m_Out->push_back(std::move(item));
    }
}

void PPPreprocessor::EmitMarker(PPItem::Kind kind, const std::string& condition, const PPSourceLoc& loc)
{
    PPItem item;
    item.K = kind;
    item.Condition = condition;
    item.Loc = loc;
    m_Out->push_back(std::move(item));
}

bool PPPreprocessor::InDeferred() const
{
    for (const CondFrame& frame : m_Conds)
    {
        if (frame.S == CondFrame::State::Deferred)
            return true;
    }

    return false;
}

} // namespace PrismShaderCompiler
