#include "TestFramework.h"

#include <PrismShaderCore/Inner/PPPreprocessor.h>
#include <PrismShaderCore/PSL/Diagnostics.h>

#include <map>
#include <string>
#include <vector>

namespace
{

using PrismShaderCompiler::DiagnosticCollector;
using PrismShaderCompiler::PPFileTable;
using PrismShaderCompiler::PPItem;
using PrismShaderCompiler::PPParams;
using PrismShaderCompiler::PPPreprocessor;
using PrismShaderCompiler::PPResult;
using PrismShaderCompiler::PPType;

struct Harness
{
    PPFileTable Files;
    DiagnosticCollector Diag;
    std::map<std::string, std::string> Vfs;

    PPResult Run(const std::string& source, const std::string& path = "Test.Shader",
                 std::vector<std::string> defines = {})
    {
        PPParams params;
        params.Path = path;
        params.Source = source;
        params.IncludeRoot = "Assets/Include";
        params.Defines = std::move(defines);
        params.ReadFile = [this](const std::string& p, std::string& out)
        {
            const auto it = Vfs.find(p);

            if (it == Vfs.end())
                return false;

            out = it->second;
            return true;
        };

        PPPreprocessor preprocessor(Files, Diag);
        return preprocessor.Run(params);
    }
};

std::vector<std::string> Tokens(const PPResult& result)
{
    std::vector<std::string> out;

    for (const PPItem& item : result.Items)
    {
        if (item.K != PPItem::Kind::Token)
            continue;

        if (item.Tok.Is(PPType::NewLine) || item.Tok.Is(PPType::EndOfFile))
            continue;

        out.push_back(item.Tok.Spelling);
    }

    return out;
}

std::vector<std::string> Markers(const PPResult& result)
{
    std::vector<std::string> out;

    for (const PPItem& item : result.Items)
    {
        switch (item.K)
        {
        case PPItem::Kind::CondBegin: out.push_back("#if " + item.Condition); break;
        case PPItem::Kind::CondElse:  out.push_back("#" + item.Condition); break;
        case PPItem::Kind::CondEnd:   out.push_back("#endif"); break;
        default: break;
        }
    }

    return out;
}

int CountToken(const PPResult& result, const std::string& spelling)
{
    int count = 0;

    for (const std::string& token : Tokens(result))
    {
        if (token == spelling)
            ++count;
    }

    return count;
}

} // namespace

PSC_TEST(PlainTextIsPreservedInOrder)
{
    Harness harness;
    const PPResult result = harness.Run("a b c\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(3));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("a"));
    PSC_CHECK_EQ(Tokens(result)[2], std::string("c"));
    PSC_CHECK_EQ(Markers(result).size(), size_t(0));
}

PSC_TEST(DefineAndUseExpands)
{
    Harness harness;
    const PPResult result = harness.Run("#define X 1\nX\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("1"));
}

PSC_TEST(UndefStopsExpansion)
{
    Harness harness;
    const PPResult result = harness.Run("#define X 1\n#undef X\nX\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("X"));
}

PSC_TEST(ImmediateConditionKeepsLiveBranchOnly)
{
    Harness harness;
    const PPResult result = harness.Run("#if 1\nkept\n#else\ndropped\n#endif\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("kept"));
    PSC_CHECK_EQ(Markers(result).size(), size_t(0));
}

PSC_TEST(ImmediateFalseBranchIsDroppedEntirely)
{
    Harness harness;
    const PPResult result = harness.Run("#if 0\nnope\n#endif\nok\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("ok"));
    PSC_CHECK_EQ(Markers(result).size(), size_t(0));
}

PSC_TEST(NestedImmediateConditionIsFullyEvaluated)
{
    Harness harness;
    const PPResult result = harness.Run("#define A 1\n#if A\n#if 0\nno\n#else\nyes\n#endif\n#endif\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("yes"));
    PSC_CHECK_EQ(Markers(result).size(), size_t(0));
}

PSC_TEST(ImmediateElifChainPicksOneBranch)
{
    Harness harness;
    const PPResult result = harness.Run("#if 0\na\n#elif 1\nb\n#else\nc\n#endif\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("b"));
    PSC_CHECK_EQ(Markers(result).size(), size_t(0));
}

PSC_TEST(DeferredConditionKeepsBothBranchesAndMarkers)
{
    Harness harness;
    const PPResult result =
        harness.Run("#pragma shader_feature SKINNED\n#ifdef SKINNED\non\n#else\noff\n#endif\n");

    PSC_CHECK(result.Success);

    const std::vector<std::string> tokens = Tokens(result);
    PSC_CHECK_EQ(tokens.size(), size_t(2));
    PSC_CHECK_EQ(tokens[0], std::string("on"));
    PSC_CHECK_EQ(tokens[1], std::string("off"));

    const std::vector<std::string> markers = Markers(result);
    PSC_CHECK_EQ(markers.size(), size_t(3));
    PSC_CHECK_EQ(markers[0], std::string("#if defined(SKINNED)"));
    PSC_CHECK_EQ(markers[1], std::string("#else"));
    PSC_CHECK_EQ(markers[2], std::string("#endif"));
}

PSC_TEST(DeferredBlockStillEvaluatesNestedImmediateCondition)
{
    Harness harness;
    const PPResult result =
        harness.Run("#pragma shader_feature SKINNED\n#ifdef SKINNED\n#if 1\nkept\n#endif\n#endif\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("kept"));

    const std::vector<std::string> markers = Markers(result);
    PSC_CHECK_EQ(markers.size(), size_t(2));
    PSC_CHECK_EQ(markers[0], std::string("#if defined(SKINNED)"));
    PSC_CHECK_EQ(markers[1], std::string("#endif"));
}

PSC_TEST(ImmediateFalseOuterHidesDeferredInner)
{
    Harness harness;
    const PPResult result = harness.Run(
        "#if 0\n#pragma shader_feature SKINNED\n#ifdef SKINNED\nno\n#endif\n#endif\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(0));
    PSC_CHECK_EQ(Markers(result).size(), size_t(0));
}

PSC_TEST(DefineInsideDeferredIsEmittedButNotExecuted)
{
    Harness harness;
    const PPResult result =
        harness.Run("#pragma shader_feature X\n#ifdef X\n#define Y 1\n#endif\nY\n");

    PSC_CHECK(result.Success);

    // 条件块里的 #define 原样留在输出里，交给真实编译期；本层不执行它
    PSC_CHECK(CountToken(result, "define") == 1);

    const std::vector<std::string> tokens = Tokens(result);
    PSC_CHECK_EQ(tokens.back(), std::string("Y"));
}

PSC_TEST(MacroUseInsideDeferredStillExpands)
{
    Harness harness;
    const PPResult result = harness.Run("#define ON 1\n#pragma shader_feature X\n#ifdef X\nON\n#endif\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("1"));
}

PSC_TEST(MultiCompileRegistersVariantsAndEmitsNothing)
{
    Harness harness;
    const PPResult result = harness.Run("#pragma multi_compile _ LOW MEDIUM HIGH\nbody\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(result.Variants.size(), size_t(1));
    PSC_CHECK_EQ(result.Variants[0].Name, std::string("multi_compile"));
    PSC_CHECK_EQ(result.Variants[0].Keywords.size(), size_t(3));
    PSC_CHECK_EQ(result.Variants[0].Keywords[0], std::string("LOW"));
    PSC_CHECK_EQ(result.Variants[0].Keywords[2], std::string("HIGH"));
    PSC_CHECK_EQ(CountToken(result, "multi_compile"), 0);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
}

PSC_TEST(VariantKeywordMakesFollowingConditionDeferred)
{
    Harness harness;
    const PPResult result = harness.Run("#pragma multi_compile _ A B\n#if A\non\n#endif\n");

    PSC_CHECK(result.Success);

    const std::vector<std::string> markers = Markers(result);
    PSC_CHECK_EQ(markers.size(), size_t(2));
    PSC_CHECK_EQ(markers[0], std::string("#if A"));
    PSC_CHECK_EQ(markers[1], std::string("#endif"));
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
}

PSC_TEST(DeferredElifThatFoldsFalseDoesNotSuppressLaterBranches)
{
    Harness harness;
    const PPResult result = harness.Run(
        "#pragma multi_compile _ A B\n#if A\nx\n#elif 0\ny\n#elif B\nz\n#else\nw\n#endif\n");

    PSC_CHECK(result.Success);

    const std::vector<std::string> markers = Markers(result);
    PSC_CHECK_EQ(markers.size(), size_t(4));
    PSC_CHECK_EQ(markers[0], std::string("#if A"));
    PSC_CHECK_EQ(markers[1], std::string("#elif B"));
    PSC_CHECK_EQ(markers[2], std::string("#else"));
    PSC_CHECK_EQ(markers[3], std::string("#endif"));

    const std::vector<std::string> tokens = Tokens(result);
    PSC_CHECK_EQ(tokens.size(), size_t(3));
    PSC_CHECK_EQ(tokens[0], std::string("x"));
    PSC_CHECK_EQ(tokens[1], std::string("z"));
    PSC_CHECK_EQ(tokens[2], std::string("w"));
}

PSC_TEST(DeferredElifThatFoldsTrueSwallowsLaterBranches)
{
    Harness harness;
    const PPResult result =
        harness.Run("#pragma multi_compile _ A B\n#if A\nx\n#elif 1\ny\n#else\nz\n#endif\n");

    PSC_CHECK(result.Success);

    const std::vector<std::string> markers = Markers(result);
    PSC_CHECK_EQ(markers.size(), size_t(3));
    PSC_CHECK_EQ(markers[0], std::string("#if A"));
    PSC_CHECK_EQ(markers[1], std::string("#else"));
    PSC_CHECK_EQ(markers[2], std::string("#endif"));

    const std::vector<std::string> tokens = Tokens(result);
    PSC_CHECK_EQ(tokens.size(), size_t(2));
    PSC_CHECK_EQ(tokens[0], std::string("x"));
    PSC_CHECK_EQ(tokens[1], std::string("y"));
}

PSC_TEST(StageDefineIsImmediateNotDeferred)
{
    Harness harness;
    const PPResult result = harness.Run(
        "#ifdef PRISM_VERTEX_SHADER\nvert_side\n#else\nfrag_side\n#endif\n", "Test.Shader",
        { "PRISM_VERTEX_SHADER" });

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(result)[0], std::string("vert_side"));
    PSC_CHECK_EQ(Markers(result).size(), size_t(0));
}

PSC_TEST(BackendDefineStaysDeferred)
{
    Harness harness;
    const PPResult result = harness.Run("#if defined(PRISM_BACKEND_OPENGL)\nogl\n#endif\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(Tokens(result).size(), size_t(1));

    const std::vector<std::string> markers = Markers(result);
    PSC_CHECK_EQ(markers.size(), size_t(2));
    PSC_CHECK_EQ(markers[0], std::string("#if defined(PRISM_BACKEND_OPENGL)"));
}

PSC_TEST(IncludeIsExpandedInline)
{
    Harness harness;
    harness.Vfs["Assets/Include/Bits.glsl"] = "from_include\n";

    const PPResult result = harness.Run("before\n#include \"Bits.glsl\"\nafter\n");

    PSC_CHECK(result.Success);

    const std::vector<std::string> tokens = Tokens(result);
    PSC_CHECK_EQ(tokens.size(), size_t(3));
    PSC_CHECK_EQ(tokens[0], std::string("before"));
    PSC_CHECK_EQ(tokens[1], std::string("from_include"));
    PSC_CHECK_EQ(tokens[2], std::string("after"));
}

PSC_TEST(IncludeGuardSuppressesSecondExpansion)
{
    Harness harness;
    harness.Vfs["Assets/Include/Guarded.glsl"] =
        "#ifndef GUARDED_GLSL\n#define GUARDED_GLSL\nshared_token\n#endif\n";

    const PPResult result = harness.Run("#include \"Guarded.glsl\"\n#include \"Guarded.glsl\"\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(CountToken(result, "shared_token"), 1);
}

PSC_TEST(IncludeOnceSuppressesSecondExpansion)
{
    Harness harness;
    harness.Vfs["Assets/Include/Once.glsl"] = "#pragma once\nonce_token\n";

    const PPResult result = harness.Run("#include \"Once.glsl\"\n#include \"Once.glsl\"\n");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(CountToken(result, "once_token"), 1);
}

PSC_TEST(RelativeIncludeResolvesNextToIncludingFile)
{
    Harness harness;
    harness.Vfs["Pass/Sibling.glsl"] = "sibling_token\n";

    const PPResult result = harness.Run("#include \"Sibling.glsl\"\n", "Pass/Test.Shader");

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(CountToken(result, "sibling_token"), 1);
}

PSC_TEST(MissingIncludeReportsError)
{
    Harness harness;
    const PPResult result = harness.Run("#include \"Nope.glsl\"\n");

    PSC_CHECK(!result.Success);
    PSC_CHECK(harness.Diag.HasErrors());
}

PSC_TEST(UnclosedConditionalReportsError)
{
    Harness harness;
    const PPResult result = harness.Run("#pragma shader_feature X\n#ifdef X\nfoo\n");

    PSC_CHECK(!result.Success);
    PSC_CHECK(harness.Diag.HasErrors());
}

PSC_TEST(ErrorDirectiveReportsError)
{
    Harness harness;
    const PPResult result = harness.Run("#error something went wrong\n");

    PSC_CHECK(!result.Success);
    PSC_CHECK(harness.Diag.HasErrors());
}

PSC_TEST(UnknownPragmaIsPreservedInOutput)
{
    Harness harness;
    const PPResult result = harness.Run("#pragma foo bar\n");

    PSC_CHECK(result.Success);
    PSC_CHECK(result.Variants.empty());
    PSC_CHECK_EQ(CountToken(result, "pragma"), 1);
    PSC_CHECK_EQ(CountToken(result, "foo"), 1);
}

PSC_TEST(EachRunStartsWithACleanMacroTable)
{
    // 同一实例连跑两个输出阶段：上一阶段的 include guard 不能把这一阶段的 include 挡掉
    Harness harness;
    PPPreprocessor preprocessor(harness.Files, harness.Diag);

    PPParams params;
    params.Path = "Test.Shader";
    params.Source = "#ifndef ONCE_GUARD\n#define ONCE_GUARD\nbody\n#endif\n";
    params.IncludeRoot = "Assets/Include";

    const PPResult first = preprocessor.Run(params);
    const PPResult second = preprocessor.Run(params);

    PSC_CHECK(first.Success);
    PSC_CHECK(second.Success);
    PSC_CHECK_EQ(Tokens(first).size(), size_t(1));
    PSC_CHECK_EQ(Tokens(second).size(), size_t(1));
}
