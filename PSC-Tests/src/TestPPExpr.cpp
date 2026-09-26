#include "TestFramework.h"

#include <PrismShaderCore/Inner/MacroTable.h>
#include <PrismShaderCore/Inner/PPExpr.h>
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
using PrismShaderCompiler::PPExpr;
using PrismShaderCompiler::PPExprIsConst;
using PrismShaderCompiler::PPExprNode;
using PrismShaderCompiler::PPExprToText;
using PrismShaderCompiler::PPSourceLoc;
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

void DefineLiteral(MacroTable& macros, const std::string& name, const std::string& body)
{
    PPMacro macro;
    macro.Name = name;
    macro.Body = Lex(body);
    macros.Define(std::move(macro));
}

PPExprNode Parse(MacroTable& macros, const std::string& source, DiagnosticCollector& diag)
{
    const std::vector<PPToken> expanded = macros.ExpandCondition(Lex(source), &diag);
    PPExpr expr(macros, &diag);
    return expr.Parse(expanded, PPSourceLoc{});
}

int64_t Value(MacroTable& macros, const std::string& source)
{
    DiagnosticCollector diag;
    return PrismShaderCompiler::PPExprConstValue(Parse(macros, source, diag));
}

std::string Text(MacroTable& macros, const std::string& source)
{
    DiagnosticCollector diag;
    return PPExprToText(Parse(macros, source, diag));
}

} // namespace

PSC_TEST(ArithmeticFollowsPrecedence)
{
    MacroTable macros;

    PSC_CHECK_EQ(Value(macros, "1 + 2 * 3"), int64_t(7));
    PSC_CHECK_EQ(Value(macros, "(1 + 2) * 3"), int64_t(9));
    PSC_CHECK_EQ(Value(macros, "7 % 4"), int64_t(3));
}

PSC_TEST(RelationalBindsTighterThanBitOr)
{
    MacroTable macros;

    PSC_CHECK_EQ(Value(macros, "1 | 2 == 2"), int64_t(1));
    PSC_CHECK_EQ(Value(macros, "1 << 2 | 1"), int64_t(5));
}

PSC_TEST(LogicalOperatorsShortCircuitOnConstants)
{
    MacroTable macros;

    PSC_CHECK_EQ(Value(macros, "0 && 1"), int64_t(0));
    PSC_CHECK_EQ(Value(macros, "1 || 0"), int64_t(1));
    PSC_CHECK_EQ(Value(macros, "3 > 2 && 1 < 2"), int64_t(1));
}

PSC_TEST(TernaryPicksOneBranch)
{
    MacroTable macros;

    PSC_CHECK_EQ(Value(macros, "1 ? 2 : 3"), int64_t(2));
    PSC_CHECK_EQ(Value(macros, "0 ? 2 : 3"), int64_t(3));
}

PSC_TEST(UnaryOperatorsFold)
{
    MacroTable macros;

    PSC_CHECK_EQ(Value(macros, "!0"), int64_t(1));
    PSC_CHECK_EQ(Value(macros, "!5"), int64_t(0));
    PSC_CHECK_EQ(Value(macros, "-1"), int64_t(-1));
    PSC_CHECK_EQ(Value(macros, "~0"), int64_t(-1));
}

PSC_TEST(IntegerLiteralBasesAndSuffixes)
{
    MacroTable macros;

    PSC_CHECK_EQ(Value(macros, "0x10"), int64_t(16));
    PSC_CHECK_EQ(Value(macros, "010"), int64_t(8));
    PSC_CHECK_EQ(Value(macros, "1u"), int64_t(1));
    PSC_CHECK_EQ(Value(macros, "2L"), int64_t(2));
}

PSC_TEST(CharLiteralIsItsCode)
{
    MacroTable macros;

    PSC_CHECK_EQ(Value(macros, "'A'"), int64_t(65));
    PSC_CHECK_EQ(Value(macros, "'\\n'"), int64_t(10));
}

PSC_TEST(UnknownIdentifierFoldsToZero)
{
    MacroTable macros;
    DiagnosticCollector diag;
    const PPExprNode node = Parse(macros, "SOME_UNKNOWN_THING", diag);

    PSC_CHECK(PPExprIsConst(node));
    PSC_CHECK_EQ(node.Value, int64_t(0));
    PSC_CHECK(!diag.HasErrors());
}

PSC_TEST(MacroIsExpandedBeforeEvaluation)
{
    MacroTable macros;
    DefineLiteral(macros, "FEATURE_LEVEL", "2");

    PSC_CHECK_EQ(Value(macros, "FEATURE_LEVEL >= 2"), int64_t(1));
}

PSC_TEST(DeferredIdentifierBecomesSymbol)
{
    MacroTable macros;
    macros.AddDeferredName("SKINNED");

    DiagnosticCollector diag;
    const PPExprNode node = Parse(macros, "SKINNED", diag);

    PSC_CHECK(!PPExprIsConst(node));
    PSC_CHECK_EQ(PPExprToText(node), std::string("SKINNED"));
}

PSC_TEST(ConstantAndDeferredFoldToConstant)
{
    MacroTable macros;
    macros.AddDeferredName("SKINNED");

    PSC_CHECK_EQ(Value(macros, "0 && SKINNED"), int64_t(0));
    PSC_CHECK_EQ(Value(macros, "1 || SKINNED"), int64_t(1));
}

PSC_TEST(NonDecisiveConstantKeepsSymbol)
{
    MacroTable macros;
    macros.AddDeferredName("SKINNED");

    PSC_CHECK_EQ(Text(macros, "1 && SKINNED"), std::string("SKINNED"));
    PSC_CHECK_EQ(Text(macros, "0 || SKINNED"), std::string("SKINNED"));
}

PSC_TEST(ArithmeticOnSymbolStaysSymbolic)
{
    MacroTable macros;
    macros.AddDeferredName("SKINNED");

    PSC_CHECK_EQ(Text(macros, "SKINNED + 1"), std::string("SKINNED + 1"));
    PSC_CHECK_EQ(Text(macros, "!SKINNED"), std::string("!SKINNED"));
    PSC_CHECK_EQ(Text(macros, "SKINNED ? 1 : 2"), std::string("SKINNED ? 1 : 2"));
}

PSC_TEST(SymbolicTextKeepsNeededParentheses)
{
    MacroTable macros;
    macros.AddDeferredName("SKINNED");
    macros.AddDeferredName("FLASH");
    macros.AddDeferredName("SHADOW");

    PSC_CHECK_EQ(Text(macros, "(SKINNED || FLASH) && SHADOW"), std::string("(SKINNED || FLASH) && SHADOW"));
    PSC_CHECK_EQ(Text(macros, "SKINNED && FLASH || SHADOW"), std::string("SKINNED && FLASH || SHADOW"));
}

PSC_TEST(DefinedOfMacroFoldsToConstant)
{
    MacroTable macros;
    DefineLiteral(macros, "HAS_NORMAL", "1");

    PSC_CHECK_EQ(Value(macros, "defined(HAS_NORMAL)"), int64_t(1));
    PSC_CHECK_EQ(Value(macros, "defined HAS_NORMAL"), int64_t(1));
    PSC_CHECK_EQ(Value(macros, "defined(NEVER_DEFINED)"), int64_t(0));
    PSC_CHECK_EQ(Value(macros, "!defined(NEVER_DEFINED)"), int64_t(1));
}

PSC_TEST(DefinedOfDeferredNameStaysSymbolic)
{
    MacroTable macros;
    macros.AddDeferredName("PRISM_BACKEND_OPENGL");

    PSC_CHECK_EQ(Text(macros, "defined(PRISM_BACKEND_OPENGL)"),
                 std::string("defined(PRISM_BACKEND_OPENGL)"));
    PSC_CHECK_EQ(Text(macros, "defined(PRISM_BACKEND_OPENGL) && 1"),
                 std::string("defined(PRISM_BACKEND_OPENGL)"));
}

PSC_TEST(DeferredSymbolIsNotDefinedByMacroTable)
{
    MacroTable macros;
    macros.AddDeferredName("SKINNED");

    DiagnosticCollector diag;
    const PPExprNode node = Parse(macros, "defined(SKINNED)", diag);

    PSC_CHECK(!PPExprIsConst(node));
    PSC_CHECK_EQ(node.Symbol, std::string("defined(SKINNED)"));
}

PSC_TEST(DivisionByZeroWarnsAndYieldsZero)
{
    MacroTable macros;
    DiagnosticCollector diag;

    const PPExprNode node = Parse(macros, "1 / 0", diag);

    PSC_CHECK(PPExprIsConst(node));
    PSC_CHECK_EQ(node.Value, int64_t(0));
    PSC_CHECK(diag.HasWarnings());
    PSC_CHECK(!diag.HasErrors());
}

PSC_TEST(UnbalancedParenthesisReportsError)
{
    MacroTable macros;
    DiagnosticCollector diag;

    Parse(macros, "(1", diag);

    PSC_CHECK(diag.HasErrors());
}

PSC_TEST(MissingOperandReportsError)
{
    MacroTable macros;
    DiagnosticCollector diag;

    Parse(macros, "1 +", diag);

    PSC_CHECK(diag.HasErrors());
}

PSC_TEST(TrailingTokensReportError)
{
    MacroTable macros;
    DiagnosticCollector diag;

    Parse(macros, "1 2", diag);

    PSC_CHECK(diag.HasErrors());
}
