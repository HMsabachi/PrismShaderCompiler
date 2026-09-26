#include "Inner/InnerCompiler.h"

#include "Inner/GLSLRewrite.h"
#include "Inner/PPPreprocessor.h"
#include "PSL/Diagnostics.h"

namespace PrismShaderCompiler
{

namespace
{

void MergeVariants(std::vector<PPVariantDecl>& out, const std::vector<PPVariantDecl>& incoming)
{
    for (const PPVariantDecl& decl : incoming)
    {
        bool duplicate = false;

        for (const PPVariantDecl& existing : out)
        {
            if (existing.Name == decl.Name && existing.Keywords == decl.Keywords)
            {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
            out.push_back(decl);
    }
}

} // namespace

InnerCompiler::InnerCompiler(DiagnosticCollector& diag)
    : m_Diag(diag)
{
}

InnerResult InnerCompiler::Compile(const InnerParams& params)
{
    InnerResult result;

    if (params.GlslBlock.empty())
    {
        m_Diag.Error("GLSL 块为空", ToDiagnosticLocation(PPSourceLoc{}, params.Config.SourcePath));
        return result;
    }

    const auto runStage = [&](const char* stageDefine) -> PPResult
    {
        PPParams pp;
        pp.Path = params.Config.SourcePath;
        pp.Source = params.GlslBlock;
        pp.IncludeRoot = params.Config.IncludeRoot;
        pp.ReadFile = params.ReadFile;
        pp.Defines.push_back(stageDefine);

        PPPreprocessor preprocessor(m_Files, m_Diag);
        return preprocessor.Run(pp);
    };

    const PPResult vertexStage = runStage("PRISM_VERTEX_SHADER");
    const PPResult fragmentStage = runStage("PRISM_FRAGMENT_SHADER");

    if (!vertexStage.Success || !fragmentStage.Success)
        return result;

    MergeVariants(result.Variants, vertexStage.Variants);
    MergeVariants(result.Variants, fragmentStage.Variants);

    GLSLRewriter rewriter(m_Files, params.Config, m_Diag);

    result.VertexGlsl = rewriter.Emit(vertexStage.Items, params.Properties, true);
    result.FragmentGlsl = rewriter.Emit(fragmentStage.Items, params.Properties, false);

    if (result.VertexGlsl.empty() || result.FragmentGlsl.empty())
        return result;

    result.Success = true;
    return result;
}

} // namespace PrismShaderCompiler
