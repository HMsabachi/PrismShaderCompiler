#include "Compiler.h"
#include "PSL/SourceManager.h"
#include "PSL/TokenStream.h"
#include "PSL/Parser.h"
#include "PSL/Diagnostics.h"
#include "Generator/GLSLGenerator.h"

namespace PrismShaderCompiler
{

ShaderCompiler::ShaderCompiler(const CompilerConfig& config)
    : m_Config(config)
{
    Log::Instance().SetCallback(config.OnLog);
    GLSLGen::SetConfig(config);
}

CompiledShader ShaderCompiler::Compile(const std::string& source,
                                        const std::string& virtualPath)
{
    CompiledShader result;

    DiagnosticCollector diag;
    SourceManager sm(source.c_str(), static_cast<uint32_t>(source.size()));
    sm.SetFilePath(virtualPath);

    if (!sm.IsValid())
    {
        Log::Instance().Error("SourceManager failed for '{}'", virtualPath);
        return result;
    }

    TokenStream stream(sm, &diag);
    Parser parser(stream, &diag);
    auto doc = parser.ParseShader();

    if (diag.HasErrors())
    {
        diag.PrintAll();
        return result;
    }

    result.ShaderName = std::move(doc.ShaderName);
    result.LOD = doc.LOD;
    result.Uniforms = std::move(doc.Uniforms);
    result.MaterialLayout = std::move(doc.MaterialLayout);
    result.RenderState = doc.RenderState;

    for (auto& passDef : doc.Passes)
    {
        for (auto& pragma : passDef.Glsl.Pragmas)
        {
            for (auto& kw : pragma.Keywords)
            {
                if (std::find(result.Keywords.begin(), result.Keywords.end(), kw) == result.Keywords.end())
                    result.Keywords.push_back(kw);
            }
        }

        result.Passes.push_back({passDef.Name, passDef.Tags, passDef.RenderState});
        result.PassGLSL.push_back(std::move(passDef.Glsl));
    }

    return result;
}

CompiledShader ShaderCompiler::CompileFile(const std::string& filePath)
{
    std::string source = m_Config.ReadFile(filePath);
    if (source.empty())
    {
        Log::Instance().Error("Failed to read '{}'", filePath);
        return {};
    }
    return Compile(source, filePath);
}

PassOutput ShaderCompiler::Generate(const CompiledShader& shader,
                                     uint32_t passIndex,
                                     const std::vector<std::string>& keywords)
{
    PassOutput out;
    if (passIndex >= shader.PassGLSL.size())
    {
        Log::Instance().Error("Pass index {} out of range ({} passes)",
                              passIndex, shader.PassGLSL.size());
        return out;
    }

    auto glsl = GLSLGen::Generate(shader.PassGLSL[passIndex],
                                  shader.Uniforms,
                                  shader.ShaderName,
                                  keywords);
    out.VertexShader = std::move(glsl.Vertex);
    out.FragmentShader = std::move(glsl.Fragment);
    return out;
}

} // namespace PrismShaderCompiler
