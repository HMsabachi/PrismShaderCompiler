#include "Inner/PPLexer.h"

#include "PSL/Diagnostics.h"

namespace PrismShaderCompiler
{

namespace
{

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

bool IsIdentStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool IsIdentCont(char c) { return IsIdentStart(c) || IsDigit(c); }

bool IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r';
}

// 阶段 4 接入文件表后补上 FilePath
SourceLocation ToDiagLoc(const PPSourceLoc& loc)
{
    SourceLocation out;
    out.Line = loc.Line;
    out.Column = loc.Column;
    return out;
}

// 最长优先
const char* const kPunctuators[] =
{
    "<<=", ">>=", "...",
    "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
    "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", "##", "->",
    "[", "]", "(", ")", "{", "}", ".", "&", "*", "+", "-", "~", "!",
    "/", "%", "<", ">", "^", "|", "?", ":", ";", "=", ",", "#",
};

} // namespace

PPLexer::PPLexer(std::string_view source, PPFileId file)
    : m_Source(source), m_File(file)
{
}

uint32_t PPLexer::SkipSplices(uint32_t pos) const
{
    const uint32_t n = static_cast<uint32_t>(m_Source.size());

    for (;;)
    {
        if (pos >= n || m_Source[pos] != '\\')
            return pos;

        if (pos + 1 < n && m_Source[pos + 1] == '\n')
        {
            pos += 2;
            continue;
        }

        if (pos + 2 < n && m_Source[pos + 1] == '\r' && m_Source[pos + 2] == '\n')
        {
            pos += 3;
            continue;
        }

        return pos;
    }
}

char PPLexer::Peek(size_t ahead) const
{
    uint32_t pos = m_Pos;

    for (size_t i = 0; i <= ahead; ++i)
    {
        pos = SkipSplices(pos);

        if (pos >= m_Source.size())
            return '\0';

        if (i < ahead)
            ++pos;
    }

    return m_Source[pos];
}

bool PPLexer::AtEnd() const
{
    return SkipSplices(m_Pos) >= m_Source.size();
}

void PPLexer::Advance()
{
    const uint32_t next = SkipSplices(m_Pos);

    for (uint32_t i = m_Pos; i < next; ++i)
    {
        if (m_Source[i] == '\n')
        {
            ++m_Line;
            m_Column = 1;
        }
    }

    m_Pos = next;

    if (m_Pos >= m_Source.size())
        return;

    if (m_Source[m_Pos++] == '\n')
    {
        ++m_Line;
        m_Column = 1;
    }
    else
    {
        ++m_Column;
    }
}

void PPLexer::SkipLineComment()
{
    while (!AtEnd() && Cur() != '\n')
        Advance();
}

void PPLexer::SkipBlockComment(DiagnosticCollector* diag)
{
    const PPSourceLoc start{m_File, m_Line, m_Column};
    bool closed = false;
    bool sawNewline = false;

    Advance();
    Advance();

    while (!AtEnd())
    {
        if (Cur() == '*' && Peek(1) == '/')
        {
            Advance();
            Advance();
            closed = true;
            break;
        }

        if (Cur() == '\n')
            sawNewline = true;

        Advance();
    }

    if (!closed && diag)
        diag->Error("未闭合的块注释", ToDiagLoc(start));

    if (sawNewline)
        m_AtLineStart = true;
}

void PPLexer::ScanTrivia(DiagnosticCollector* diag)
{
    for (;;)
    {
        if (AtEnd())
            return;

        const char c = Cur();

        if (IsSpace(c))
        {
            Advance();
            continue;
        }

        if (c == '/' && Peek(1) == '/')
        {
            SkipLineComment();
            continue;
        }

        if (c == '/' && Peek(1) == '*')
        {
            SkipBlockComment(diag);
            continue;
        }

        return;
    }
}

void PPLexer::ScanIdentifier(std::string& out)
{
    while (!AtEnd() && IsIdentCont(Cur()))
    {
        out.push_back(Cur());
        Advance();
    }
}

void PPLexer::ScanNumber(std::string& out)
{
    while (!AtEnd())
    {
        const char c = Cur();

        if ((c == 'e' || c == 'E' || c == 'p' || c == 'P') &&
            (Peek(1) == '+' || Peek(1) == '-'))
        {
            out.push_back(Cur());
            Advance();
            out.push_back(Cur());
            Advance();
            continue;
        }

        if (IsIdentCont(c) || c == '.')
        {
            out.push_back(c);
            Advance();
            continue;
        }

        return;
    }
}

void PPLexer::ScanQuoted(std::string& out, char quote, DiagnosticCollector* diag)
{
    const PPSourceLoc start{m_File, m_Line, m_Column};

    out.push_back(Cur());
    Advance();

    while (!AtEnd())
    {
        const char c = Cur();

        if (c == '\n')
            break;

        if (c == '\\')
        {
            out.push_back(c);
            Advance();

            if (!AtEnd() && Cur() != '\n')
            {
                out.push_back(Cur());
                Advance();
            }
            continue;
        }

        out.push_back(c);
        Advance();

        if (c == quote)
            return;
    }

    if (diag)
        diag->Error(quote == '"' ? "未闭合的字符串字面量" : "未闭合的字符字面量", ToDiagLoc(start));
}

const char* PPLexer::MatchPunctuator() const
{
    for (const char* candidate : kPunctuators)
    {
        size_t i = 0;

        for (; candidate[i] != '\0'; ++i)
        {
            if (Peek(i) != candidate[i])
                break;
        }

        if (candidate[i] == '\0')
            return candidate;
    }

    return nullptr;
}

void PPLexer::EmitToken(std::vector<PPToken>& out, PPType type, std::string spelling,
                        uint32_t tokenStart, const PPSourceLoc& loc)
{
    PPToken token;
    token.Type = type;
    token.Loc = loc;
    token.Spelling = std::move(spelling);
    token.TriviaOffset = m_TriviaStart;
    token.TriviaLength = tokenStart - m_TriviaStart;
    token.RawOffset = tokenStart;
    token.RawLength = m_Pos > tokenStart ? m_Pos - tokenStart : 0;
    token.AtLineStart = m_AtLineStart;
    token.LeadingSpace = token.TriviaLength > 0;

    m_TriviaStart = m_Pos;
    out.push_back(std::move(token));
}

std::vector<PPToken> PPLexer::Tokenize(DiagnosticCollector* diag)
{
    std::vector<PPToken> out;

    for (;;)
    {
        ScanTrivia(diag);

        if (AtEnd())
            break;

        const uint32_t tokenStart = m_Pos;
        const PPSourceLoc loc{m_File, m_Line, m_Column};

        if (Cur() == '\n')
        {
            Advance();
            EmitToken(out, PPType::NewLine, "\n", tokenStart, loc);
            m_AtLineStart = true;
            continue;
        }

        const char c = Cur();
        PPType type = PPType::Other;
        std::string spelling;

        if (IsIdentStart(c))
        {
            type = PPType::Identifier;
            ScanIdentifier(spelling);
        }
        else if (IsDigit(c) || (c == '.' && IsDigit(Peek(1))))
        {
            type = PPType::Number;
            ScanNumber(spelling);
        }
        else if (c == '"')
        {
            type = PPType::StringLiteral;
            ScanQuoted(spelling, '"', diag);
        }
        else if (c == '\'')
        {
            type = PPType::CharLiteral;
            ScanQuoted(spelling, '\'', diag);
        }
        else
        {
            const char* punct = MatchPunctuator();

            if (punct != nullptr)
            {
                type = PPType::Punctuator;
                spelling = punct;

                for (size_t i = 0; punct[i] != '\0'; ++i)
                    Advance();
            }
            else
            {
                spelling.assign(1, c);
                Advance();
            }
        }

        EmitToken(out, type, std::move(spelling), tokenStart, loc);
        m_AtLineStart = false;
    }

    // EOF 的 tokenStart 取源末尾，使尾部残余（空白与结尾续行）全部落入其 trivia
    const PPSourceLoc eofLoc{m_File, m_Line, m_Column};
    EmitToken(out, PPType::EndOfFile, std::string(),
              static_cast<uint32_t>(m_Source.size()), eofLoc);

    return out;
}

std::string PPLexer::Detokenize(const std::vector<PPToken>& tokens) const
{
    std::string out;

    for (const PPToken& token : tokens)
    {
        if (token.TriviaLength > 0)
            out.append(m_Source.data() + token.TriviaOffset, token.TriviaLength);

        if (token.Type == PPType::EndOfFile)
            break;

        out.append(token.Spelling);
    }

    return out;
}

const char* PPTypeToString(PPType type)
{
    switch (type)
    {
        case PPType::EndOfFile:     return "EndOfFile";
        case PPType::NewLine:       return "NewLine";
        case PPType::Identifier:    return "Identifier";
        case PPType::Number:        return "Number";
        case PPType::StringLiteral: return "StringLiteral";
        case PPType::CharLiteral:   return "CharLiteral";
        case PPType::Punctuator:    return "Punctuator";
        case PPType::Other:         return "Other";
    }

    return "Unknown";
}

} // namespace PrismShaderCompiler
