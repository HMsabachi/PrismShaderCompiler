#include "TestFramework.h"

#include <PrismShaderCore/Inner/MacroTable.h>
#include <PrismShaderCore/Inner/PPLexer.h>
#include <PrismShaderCore/PSL/Diagnostics.h>

#include <string>
#include <vector>

namespace
{

using PrismShaderCompiler::DiagnosticCollector;
using PrismShaderCompiler::MacroTable;
using PrismShaderCompiler::PPLexer;
using PrismShaderCompiler::PPMacro;
using PrismShaderCompiler::PPToken;
using PrismShaderCompiler::PPType;

std::vector<PPToken> Lex(const std::string& source)
{
    PPLexer lexer(source, 0);
    std::vector<PPToken> tokens = lexer.Tokenize();

    std::vector<PPToken> out;

    for (PPToken& token : tokens)
    {
        if (token.IsNot(PPType::NewLine) && token.IsNot(PPType::EndOfFile))
            out.push_back(std::move(token));
    }

    return out;
}

PPMacro MakeObject(const std::string& name, const std::string& body)
{
    PPMacro macro;
    macro.Name = name;
    macro.Body = Lex(body);
    return macro;
}

PPMacro MakeFunction(const std::string& name, const std::string& body,
                     const std::vector<std::string>& params, bool variadic = false)
{
    PPMacro macro;
    macro.Name = name;
    macro.FunctionLike = true;
    macro.Variadic = variadic;
    macro.Params = params;
    macro.Body = Lex(body);
    return macro;
}

std::string Concatenate(const std::vector<PPToken>& tokens)
{
    std::string out;

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (i > 0 && tokens[i].LeadingSpace)
            out.push_back(' ');

        out += tokens[i].Spelling;
    }

    return out;
}

std::string Expand(MacroTable& macros, const std::string& source, DiagnosticCollector* diag = nullptr)
{
    return Concatenate(macros.Expand(Lex(source), diag));
}

} // namespace

PSC_TEST(ObjectMacroExpands)
{
    MacroTable macros;
    macros.Define(MakeObject("A", "1"));

    PSC_CHECK_EQ(Expand(macros, "A + A"), std::string("1 + 1"));
}

PSC_TEST(ExpansionRescansForNestedMacros)
{
    MacroTable macros;
    macros.Define(MakeObject("A", "B"));
    macros.Define(MakeObject("B", "2"));

    PSC_CHECK_EQ(Expand(macros, "A"), std::string("2"));
}

PSC_TEST(FunctionMacroSubstitutesParameters)
{
    MacroTable macros;
    macros.Define(MakeFunction("ADD", "a + b", { "a", "b" }));

    PSC_CHECK_EQ(Expand(macros, "ADD(1, 2)"), std::string("1 + 2"));
}

PSC_TEST(FunctionMacroWithoutParenthesesIsLeftAlone)
{
    MacroTable macros;
    macros.Define(MakeFunction("F", "x", { "x" }));

    PSC_CHECK_EQ(Expand(macros, "F"), std::string("F"));
}

PSC_TEST(ArgumentsAreExpandedBeforeSubstitution)
{
    MacroTable macros;
    macros.Define(MakeFunction("F", "x", { "x" }));
    macros.Define(MakeObject("X", "9"));

    PSC_CHECK_EQ(Expand(macros, "F(X)"), std::string("9"));
}

PSC_TEST(ArgumentsSpanningCommasNest)
{
    MacroTable macros;
    macros.Define(MakeFunction("F", "[x]", { "x" }));

    PSC_CHECK_EQ(Expand(macros, "F(g(1, 2))"), std::string("[g(1, 2)]"));
}

PSC_TEST(EmptyArgumentIsAllowed)
{
    MacroTable macros;
    macros.Define(MakeFunction("F", "a b", { "a", "b" }));

    PSC_CHECK_EQ(Expand(macros, "F(, x)"), std::string("x"));
}

PSC_TEST(StringizeUsesRawArgumentText)
{
    MacroTable macros;
    macros.Define(MakeFunction("S", "#a", { "a" }));
    macros.Define(MakeObject("X", "1"));

    PSC_CHECK_EQ(Expand(macros, "S(X)"), std::string("\"X\""));
}

PSC_TEST(StringizeJoinsMultipleTokensWithOneSpace)
{
    MacroTable macros;
    macros.Define(MakeFunction("S", "#a", { "a" }));

    PSC_CHECK_EQ(Expand(macros, "S(x   y)"), std::string("\"x y\""));
}

PSC_TEST(PasteBuildsOneToken)
{
    MacroTable macros;
    macros.Define(MakeFunction("CAT", "a##b", { "a", "b" }));

    PSC_CHECK_EQ(Expand(macros, "CAT(fo, o)"), std::string("foo"));
}

PSC_TEST(PasteOperandsAreNotExpanded)
{
    MacroTable macros;
    macros.Define(MakeFunction("CAT", "a##b", { "a", "b" }));
    macros.Define(MakeObject("X", "1"));

    PSC_CHECK_EQ(Expand(macros, "CAT(X, 2)"), std::string("X2"));
}

PSC_TEST(SelfReferenceTerminates)
{
    MacroTable macros;
    macros.Define(MakeObject("A", "A"));

    PSC_CHECK_EQ(Expand(macros, "A"), std::string("A"));
}

PSC_TEST(MutualRecursionTerminates)
{
    MacroTable macros;
    macros.Define(MakeObject("A", "B"));
    macros.Define(MakeObject("B", "A"));

    PSC_CHECK_EQ(Expand(macros, "A"), std::string("A"));
}

PSC_TEST(VariadicCollectsRemainingArguments)
{
    MacroTable macros;
    macros.Define(MakeFunction("V", "a __VA_ARGS__", { "a" }, true));

    PSC_CHECK_EQ(Expand(macros, "V(1, 2, 3)"), std::string("1 2, 3"));
}

PSC_TEST(GnuCommaSwallowDropsCommaWhenVariadicIsEmpty)
{
    MacroTable macros;
    macros.Define(MakeFunction("LOG", "print(fmt, ##__VA_ARGS__)", { "fmt" }, true));

    PSC_CHECK_EQ(Expand(macros, "LOG(\"x\")"), std::string("print(\"x\")"));
}

PSC_TEST(GnuCommaSwallowPastesNormallyWhenVariadicIsGiven)
{
    MacroTable macros;
    macros.Define(MakeFunction("LOG", "print(fmt, ##__VA_ARGS__)", { "fmt" }, true));

    PSC_CHECK_EQ(Expand(macros, "LOG(\"x\", 1)"), std::string("print(\"x\", 1)"));
}

PSC_TEST(VaOptKeepsContentOnlyWhenVariadicIsGiven)
{
    MacroTable macros;
    macros.Define(MakeFunction("V", "a __VA_OPT__(,) __VA_ARGS__", { "a" }, true));

    PSC_CHECK_EQ(Expand(macros, "V(1)"), std::string("1"));
    PSC_CHECK_EQ(Expand(macros, "V(1,2)"), std::string("1, 2"));
}

PSC_TEST(DeferredNameIsNeverExpanded)
{
    MacroTable macros;
    macros.Define(MakeObject("SKINNED", "1"));
    macros.AddDeferredName("SKINNED");

    PSC_CHECK_EQ(Expand(macros, "SKINNED"), std::string("SKINNED"));
}

PSC_TEST(LineMacroExpandsToCurrentLine)
{
    MacroTable macros;

    PSC_CHECK_EQ(Expand(macros, "__LINE__"), std::string("1"));
}

PSC_TEST(CounterMacroIncrements)
{
    MacroTable macros;
    macros.SetCounter(7);

    PSC_CHECK_EQ(Expand(macros, "__COUNTER__ __COUNTER__"), std::string("7 8"));
}

PSC_TEST(ConditionExpansionLeavesDefinedOperandAlone)
{
    MacroTable macros;
    macros.Define(MakeObject("X", "1"));

    const std::vector<PPToken> expanded = macros.ExpandCondition(Lex("defined(X)"), nullptr);

    PSC_CHECK_EQ(Concatenate(expanded), std::string("defined(X)"));
}

PSC_TEST(WrongArgumentCountReportsError)
{
    MacroTable macros;
    macros.Define(MakeFunction("F", "a + b", { "a", "b" }));

    DiagnosticCollector diag;
    Expand(macros, "F(1)", &diag);

    PSC_CHECK(diag.HasErrors());
}
