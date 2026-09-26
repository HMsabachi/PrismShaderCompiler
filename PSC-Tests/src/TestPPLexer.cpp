#include "TestFramework.h"

#include <PrismShaderCore/Inner/PPLexer.h>
#include <PrismShaderCore/PSL/Diagnostics.h>

#include <string>
#include <vector>

namespace
{

using PrismShaderCompiler::DiagnosticCollector;
using PrismShaderCompiler::PPLexer;
using PrismShaderCompiler::PPToken;
using PrismShaderCompiler::PPType;

std::vector<PPToken> Lex(const std::string& source, DiagnosticCollector* diag = nullptr)
{
    PPLexer lexer(source, 0);
    return lexer.Tokenize(diag);
}

std::string RoundTrip(const std::string& source)
{
    PPLexer lexer(source, 0);
    const std::vector<PPToken> tokens = lexer.Tokenize();
    return lexer.Detokenize(tokens);
}

std::vector<std::string> Spellings(const std::vector<PPToken>& tokens, PPType type)
{
    std::vector<std::string> out;

    for (const PPToken& token : tokens)
    {
        if (token.Type == type)
            out.push_back(token.Spelling);
    }

    return out;
}

const PPToken* FindPunct(const std::vector<PPToken>& tokens, std::string_view punct)
{
    for (const PPToken& token : tokens)
    {
        if (token.IsPunct(punct))
            return &token;
    }

    return nullptr;
}

} // namespace

PSC_TEST(RoundTripPlainText)
{
    const std::string source =
        "#version 450 core\n"
        "layout(location = 0) in vec3 a_Position;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_Position, 1.0);\n"
        "}\n";

    PSC_CHECK_EQ(RoundTrip(source), source);
}

PSC_TEST(RoundTripWithCommentsAndTrailingSpace)
{
    const std::string source =
        "// 行注释\n"
        "a /* 块\n注释 */ b   \n"
        "\t\n"
        "  ";

    PSC_CHECK_EQ(RoundTrip(source), source);
}

PSC_TEST(RoundTripNoTrailingNewline)
{
    const std::string source = "float x = 1.0;";
    PSC_CHECK_EQ(RoundTrip(source), source);
}

PSC_TEST(LineSpliceInsideIdentifier)
{
    const std::string source = "foo\\\nbar";

    PSC_CHECK_EQ(RoundTrip(source), std::string("foobar"));

    const std::vector<PPToken> tokens = Lex(source);
    PSC_CHECK_EQ(tokens.size(), size_t(2));
    PSC_CHECK(tokens[0].Is(PPType::Identifier));
    PSC_CHECK_EQ(tokens[0].Spelling, std::string("foobar"));
}

PSC_TEST(LineSpliceKeepsPhysicalLineNumber)
{
    const std::vector<PPToken> tokens = Lex("a\\\nb\nc");

    PSC_CHECK_EQ(tokens.size(), size_t(4));
    PSC_CHECK_EQ(tokens[0].Spelling, std::string("ab"));
    PSC_CHECK_EQ(tokens[0].Loc.Line, uint32_t(1));
    PSC_CHECK(tokens[1].Is(PPType::NewLine));
    PSC_CHECK_EQ(tokens[1].Loc.Line, uint32_t(2));
    PSC_CHECK_EQ(tokens[2].Spelling, std::string("c"));
    PSC_CHECK_EQ(tokens[2].Loc.Line, uint32_t(3));
}

PSC_TEST(LineCommentContinuedBySplice)
{
    const std::vector<PPToken> tokens = Lex("a // comment \\\nstill comment\nb");

    PSC_CHECK_EQ(tokens.size(), size_t(4));
    PSC_CHECK_EQ(tokens[0].Spelling, std::string("a"));
    PSC_CHECK(tokens[1].Is(PPType::NewLine));
    PSC_CHECK_EQ(tokens[1].Loc.Line, uint32_t(2));
    PSC_CHECK_EQ(tokens[2].Spelling, std::string("b"));
    PSC_CHECK_EQ(tokens[2].Loc.Line, uint32_t(3));
}

PSC_TEST(BlockCommentWithNewlineSetsLineStart)
{
    const std::vector<PPToken> tokens = Lex("/* a\nb */ # define X 1");
    const PPToken* hash = FindPunct(tokens, "#");

    PSC_CHECK(hash != nullptr);
    PSC_CHECK(hash->AtLineStart);
    PSC_CHECK_EQ(hash->Loc.Line, uint32_t(2));
}

PSC_TEST(HashAfterLineCommentIsLineStart)
{
    const std::vector<PPToken> tokens = Lex("a\n// c\n#define X 1");
    const PPToken* hash = FindPunct(tokens, "#");

    PSC_CHECK(hash != nullptr);
    PSC_CHECK(hash->AtLineStart);
    PSC_CHECK_EQ(hash->Loc.Line, uint32_t(3));
}

PSC_TEST(NumberSpellings)
{
    const std::vector<std::string> got = Spellings(Lex("1 .5 1e-5 1.0E+10 0x1p-3 0xFFu 42.0f"), PPType::Number);

    PSC_CHECK_EQ(got.size(), size_t(7));
    PSC_CHECK_EQ(got[0], std::string("1"));
    PSC_CHECK_EQ(got[1], std::string(".5"));
    PSC_CHECK_EQ(got[2], std::string("1e-5"));
    PSC_CHECK_EQ(got[3], std::string("1.0E+10"));
    PSC_CHECK_EQ(got[4], std::string("0x1p-3"));
    PSC_CHECK_EQ(got[5], std::string("0xFFu"));
    PSC_CHECK_EQ(got[6], std::string("42.0f"));
}

PSC_TEST(PunctuatorLongestMatch)
{
    const std::vector<std::string> got = Spellings(Lex("<<= >> >= == ## << -> ++ . ..."), PPType::Punctuator);

    PSC_CHECK_EQ(got.size(), size_t(10));
    PSC_CHECK_EQ(got[0], std::string("<<="));
    PSC_CHECK_EQ(got[1], std::string(">>"));
    PSC_CHECK_EQ(got[2], std::string(">="));
    PSC_CHECK_EQ(got[3], std::string("=="));
    PSC_CHECK_EQ(got[4], std::string("##"));
    PSC_CHECK_EQ(got[5], std::string("<<"));
    PSC_CHECK_EQ(got[6], std::string("->"));
    PSC_CHECK_EQ(got[7], std::string("++"));
    PSC_CHECK_EQ(got[8], std::string("."));
    PSC_CHECK_EQ(got[9], std::string("..."));
}

PSC_TEST(LineStartAndLeadingSpaceFlags)
{
    const std::vector<PPToken> tokens = Lex("a b\n  c\n");

    PSC_CHECK_EQ(tokens.size(), size_t(6));
    PSC_CHECK(tokens[0].AtLineStart);
    PSC_CHECK(!tokens[0].LeadingSpace);
    PSC_CHECK(!tokens[1].AtLineStart);
    PSC_CHECK(tokens[1].LeadingSpace);
    PSC_CHECK(tokens[2].Is(PPType::NewLine));
    PSC_CHECK(tokens[3].AtLineStart);
    PSC_CHECK(tokens[3].LeadingSpace);
    PSC_CHECK(tokens[4].Is(PPType::NewLine));
    PSC_CHECK(tokens[5].Is(PPType::EndOfFile));
}

PSC_TEST(ColumnTracking)
{
    const std::vector<PPToken> tokens = Lex("ab cd\n  ef");

    PSC_CHECK_EQ(tokens[0].Loc.Column, uint32_t(1));
    PSC_CHECK_EQ(tokens[1].Loc.Column, uint32_t(4));
    PSC_CHECK_EQ(tokens[3].Loc.Line, uint32_t(2));
    PSC_CHECK_EQ(tokens[3].Loc.Column, uint32_t(3));
}

PSC_TEST(UnterminatedBlockCommentReportsError)
{
    DiagnosticCollector diag;
    Lex("a /* never closed", &diag);

    PSC_CHECK(diag.HasErrors());
}

PSC_TEST(UnterminatedStringReportsError)
{
    DiagnosticCollector diag;
    Lex("x = \"abc\n", &diag);

    PSC_CHECK(diag.HasErrors());
}
