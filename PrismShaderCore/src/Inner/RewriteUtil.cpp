#include "Inner/RewriteUtil.h"

#include "Inner/PPFile.h"

namespace PrismShaderCompiler
{

std::string NormalizePath(const std::string& path)
{
    std::string out = path;

    for (char& c : out)
    {
        if (c == '\\')
            c = '/';
    }

    return out;
}

const PPToken* TokenAt(const std::vector<PPItem>& items, size_t index)
{
    if (index >= items.size() || items[index].K != PPItem::Kind::Token)
        return nullptr;

    return &items[index].Tok;
}

const PPSourceLoc& FirstTokenLoc(const std::vector<PPItem>& items)
{
    static const PPSourceLoc kUnknown;

    for (const PPItem& item : items)
    {
        if (item.K == PPItem::Kind::Token)
            return item.Tok.Loc;
    }

    return kUnknown;
}

void MarkSkipped(const std::vector<PPItem>& items, std::vector<bool>& skip, size_t begin, size_t end)
{
    for (size_t i = begin + 1; i <= end && i < skip.size(); ++i)
        skip[i] = true;

    const size_t after = end + 1;

    if (after < skip.size() && items[after].K == PPItem::Kind::Token
        && items[after].Tok.Is(PPType::NewLine))
    {
        skip[after] = true;
    }
}

GLSLWriter::GLSLWriter(const PPFileTable& files)
    : m_Files(files)
{
}

void GLSLWriter::Raw(std::string_view text)
{
    m_Out.append(text);
    Invalidate();
}

void GLSLWriter::Token(const PPToken& token)
{
    if (!token.HasRaw())
    {
        if (token.LeadingSpace && NeedSpaceBefore())
            m_Out.push_back(' ');

        m_Out += token.Spelling;
        Invalidate();
        return;
    }

    if (!m_Files.IsValid(token.Loc.File))
        return;

    const std::string& source = m_Files.GetSource(token.Loc.File);
    const uint32_t end = token.RawOffset + token.RawLength;

    if (end > source.size() || token.TriviaOffset >= end)
        return;

    const bool contiguous = (token.Loc.File == m_LastFile) && (token.TriviaOffset == m_LastEnd);

    if (contiguous)
    {
        m_Out.append(source, token.TriviaOffset, end - token.TriviaOffset);
    }
    else
    {
        if (token.RawOffset > token.TriviaOffset)
            m_Out.append(source, token.TriviaOffset, token.RawOffset - token.TriviaOffset);

        LineDirective(token.Loc);
        m_Out.append(source, token.RawOffset, token.RawLength);
    }

    m_LastFile = token.Loc.File;
    m_LastEnd = end;
}

void GLSLWriter::LineDirective(const PPSourceLoc& loc)
{
    if (!m_Files.IsValid(loc.File))
        return;

    Directive("#line " + std::to_string(loc.Line) + " \"" + NormalizePath(m_Files.GetPath(loc.File)) + "\"");
}

void GLSLWriter::Directive(const std::string& text)
{
    if (!AtCleanLineStart())
        m_Out.push_back('\n');

    m_Out += text;

    if (m_Out.empty() || m_Out.back() != '\n')
        m_Out.push_back('\n');

    Invalidate();
}

std::string GLSLWriter::Take()
{
    return std::move(m_Out);
}

void GLSLWriter::Invalidate()
{
    m_LastFile = PP_NO_FILE;
    m_LastEnd = kNoRawOffset;
}

bool GLSLWriter::AtCleanLineStart() const
{
    for (size_t i = m_Out.size(); i > 0; --i)
    {
        const char c = m_Out[i - 1];

        if (c == '\n')
            return true;

        if (c != ' ' && c != '\t' && c != '\r')
            return false;
    }

    return true;
}

bool GLSLWriter::NeedSpaceBefore() const
{
    if (m_Out.empty())
        return false;

    const char c = m_Out.back();
    return c != '\n' && c != ' ' && c != '\t';
}

void WriteMarker(GLSLWriter& writer, const PPItem& item)
{
    switch (item.K)
    {
    case PPItem::Kind::CondBegin:
        writer.Directive("#if " + item.Condition);
        break;

    case PPItem::Kind::CondElse:
        writer.Directive("#" + item.Condition);
        break;

    case PPItem::Kind::CondEnd:
        writer.Directive("#endif");
        break;

    default:
        break;
    }
}

} // namespace PrismShaderCompiler
