#include "Compiler.h"
#include "PSL/SourceManager.h"
#include "PSL/TokenStream.h"
#include "PSL/Parser.h"
#include "PSL/Diagnostics.h"
#include "Inner/InnerCompiler.h"
#include "Inner/ComputeRewrite.h"
#include "Generator/SpirvGenerator.h"
#include "Generator/GLSLGenerator.h"
#include "Generator/HLSLGenerator.h"
#include "Generator/MSLGenerator.h"
#include "Generator/ReflectionGenerator.h"
#include <algorithm>
#include <exception>
#include <fstream>
#include <filesystem>

namespace PrismShaderCompiler
{

namespace
{

InnerReadFileFn MakeReadFile(const ReadFileCallback& readFile)
{
    return [readFile](const std::string& path, std::string& out)
    {
        out = readFile(path);
        return !out.empty();
    };
}

void FillInnerConfig(InnerConfig& inner, const CompilerConfig& config, const std::string& sourcePath)
{
    inner.SourcePath = sourcePath;
    inner.IncludeRoot = config.IncludeRoot;
    inner.GlslVersion = config.GlslVersion;
    inner.MaterialBlockName = config.MaterialBlockName;
    inner.OpenGLMaterialUniformBufferBinding = config.OpenGLMaterialUniformBufferBinding;
    inner.OpenGLTextureBeginBinding = config.OpenGLTextureBeginBinding;
    inner.VulkanMaterialUniformBufferSet = config.VulkanMaterialUniformBufferSet;
    inner.VulkanMaterialUniformBufferBinding = config.VulkanMaterialUniformBufferBinding;
    inner.VulkanTextureBeginSet = config.VulkanTextureBeginSet;
    inner.VulkanTextureBeginBinding = config.VulkanTextureBeginBinding;
}

void FillInnerParams(InnerParams& params, const CompiledShader& shader, uint32_t passIndex,
                     const CompilerConfig& config)
{
    const AST::GLSLCode& glsl = shader.Passes[passIndex].Glsl;

    params.GlslBlock = glsl.RawSource;
    params.ReadFile = MakeReadFile(config.ReadFile);

    for (const AST::ShaderUniform& uniform : shader.Uniforms)
    {
        InnerProperty property;
        property.Name = uniform.Name;
        property.Type = uniform.Type;
        property.TextureSlot = static_cast<uint32_t>(uniform.TextureSlot < 0 ? 0 : uniform.TextureSlot);
        params.Properties.push_back(std::move(property));
    }

    FillInnerConfig(params.Config, config, glsl.Loc.FilePath);
}

void FillComputeParams(ComputeInnerParams& params, const CompiledComputeShader& shader,
                       const CompilerConfig& config)
{
    params.Source = shader.Source;
    params.ReadFile = MakeReadFile(config.ReadFile);

    FillInnerConfig(params.Config, config, shader.SourcePath);
}

} // namespace

    ShaderCompiler::ShaderCompiler(const CompilerConfig& config)
        : m_Config(config)
    {
        Log::Instance().SetCallback(config.OnLog);
    }

    void ShaderCompiler::SetConfig(const CompilerConfig& config)
    {
        m_Config = config;
        Log::Instance().SetCallback(config.OnLog);
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
        result.Tags = std::move(doc.Tags);
        result.Uniforms = std::move(doc.Uniforms);
        result.MaterialLayout = std::move(doc.MaterialLayout);
        result.RenderState = doc.RenderState;

        for (auto& passDef : doc.Passes)
        {
            for (auto& pragma : passDef.Glsl.Pragmas)
            {
                for (auto& kw : pragma.Keywords)
                {
                    auto it = std::find_if(result.Keywords.begin(), result.Keywords.end(),
                        [&](const KeywordDef& d) { return d.Name == kw; });
                    if (it == result.Keywords.end())
                        result.Keywords.push_back({ kw, pragma.IsMultiCompile });
                }
            }

            std::optional<PipelineState> effectiveState = doc.RenderState;
            if (passDef.RenderState)
            {
                if (effectiveState)
                    effectiveState->Merge(*passDef.RenderState);
                else
                    effectiveState = passDef.RenderState;
            }

            PassInfo info;
            info.Name = std::move(passDef.Name);
            info.Tags = std::move(passDef.Tags);
            info.RenderState = effectiveState;
            info.Glsl = std::move(passDef.Glsl);
            result.Passes.push_back(std::move(info));
        }

        // UsePass 处理
        m_CompileInProgress.insert(result.ShaderName);

        for (auto& usePass : doc.UsePasses)
        {
            if (m_CompileInProgress.count(usePass.ShaderName))
            {
                Log::Instance().Error("UsePass: 循环引用 '{}'（在 '{}' 的编译栈中）",
                    usePass.ShaderName, doc.ShaderName);
                continue;
            }

            if (!m_Config.ResolveUsePass)
            {
                Log::Instance().Error("UsePass: ResolveUsePass 回调未设置");
                continue;
            }

            CompiledShader sourceShader = m_Config.ResolveUsePass(usePass.ShaderName);

            bool found = false;
            for (auto& sourcePass : sourceShader.Passes)
            {
                if (sourcePass.Name == usePass.PassName)
                {
                    PassInfo copiedPass;
                    copiedPass.Name = sourcePass.Name;
                    copiedPass.Tags = sourcePass.Tags;
                    copiedPass.RenderState = sourcePass.RenderState;
                    copiedPass.Glsl = sourcePass.Glsl;

                    for (auto& pragma : copiedPass.Glsl.Pragmas)
                        for (auto& kw : pragma.Keywords)
                        {
                            auto it = std::find_if(result.Keywords.begin(), result.Keywords.end(),
                                [&](const KeywordDef& d) { return d.Name == kw; });
                            if (it == result.Keywords.end())
                                result.Keywords.push_back({ kw, pragma.IsMultiCompile });
                        }

                    std::optional<PipelineState> effectiveState = doc.RenderState;
                    if (copiedPass.RenderState)
                    {
                        if (effectiveState) effectiveState->Merge(*copiedPass.RenderState);
                        else effectiveState = copiedPass.RenderState;
                    }
                    copiedPass.RenderState = effectiveState;

                    result.Passes.push_back(std::move(copiedPass));
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                Log::Instance().Error("UsePass: Pass '{}' 在 Shader '{}' 中未找到",
                    usePass.PassName, usePass.ShaderName);
            }
        }
        m_CompileInProgress.erase(result.ShaderName);

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

    PassOutput ShaderCompiler::GenerateGLSL(const CompiledShader& shader,
        uint32_t passIndex,
        const std::vector<std::string>& keywords)
    {
        auto out = GenerateSPIRVImpl(shader, passIndex, keywords, TargetBackend::OpenGL);
        try
        {
            auto glsl = DecompileSPIRV({ out.SpirvVertex, out.SpirvFragment });
            out.VertexShader = std::move(glsl.Vertex);
            out.FragmentShader = std::move(glsl.Fragment);
        }
        catch (const std::exception& e)
        {
            std::string msg = std::string("GLSL cross-compilation failed: ") + e.what();
            out.Warnings.push_back(std::move(msg));
        }
        return out;
    }

    PassOutput ShaderCompiler::GenerateSPIRV(const CompiledShader& shader,
        uint32_t passIndex,
        const std::vector<std::string>& keywords)
    {
        return GenerateSPIRVImpl(shader, passIndex, keywords, TargetBackend::Vulkan);
    }

    PassOutput ShaderCompiler::GenerateSPIRVImpl(const CompiledShader& shader,
        uint32_t passIndex,
        const std::vector<std::string>& keywords,
        TargetBackend backend)
    {
        PassOutput out;
        if (passIndex >= shader.Passes.size())
        {
            Log::Instance().Error("Pass index {} out of range ({} passes)",
                passIndex, shader.Passes.size());
            return out;
        }

        InnerParams inner;
        FillInnerParams(inner, shader, passIndex, m_Config);

        DiagnosticCollector diag;
        InnerCompiler compiler(diag);
        InnerResult ir = compiler.Compile(inner);

        for (const Diagnostic& d : diag.GetDiagnostics())
        {
            if (d.Level == Severity::Warning)
            {
                Log::Instance().Warn("{}", d.Message);
                out.Warnings.push_back(d.Message);
            }
            else
            {
                Log::Instance().Error("{}", d.Message);
                out.Errors.push_back(d.Message);
            }
        }

        if (!ir.Success)
            return out;

        auto vsSPV = CompileGLSL(ir.VertexGlsl, ShaderStageType::Vertex, backend, keywords);
        auto fsSPV = CompileGLSL(ir.FragmentGlsl, ShaderStageType::Fragment, backend, keywords);

        out.SpirvVertex = std::move(vsSPV.Bytecode);
        out.SpirvFragment = std::move(fsSPV.Bytecode);

        if (!out.SpirvVertex.empty() && !out.SpirvFragment.empty())
            out.Reflection = ReflectDescriptors(out.SpirvVertex, out.SpirvFragment);

        auto& log = Log::Instance();
        std::string shaderId = shader.ShaderName;
        if (!shaderId.empty()) shaderId = " '" + shaderId + "'";
        std::string stageSuffix = " (pass " + std::to_string(passIndex) + ")";

        for (auto& e : vsSPV.Errors) { log.Error("VS{}: {}", stageSuffix, e); out.Errors.push_back(std::move(e)); }
        for (auto& w : vsSPV.Warnings) { log.Warn("VS{}: {}", stageSuffix, w); out.Warnings.push_back(std::move(w)); }
        for (auto& e : fsSPV.Errors) { log.Error("FS{}: {}", stageSuffix, e); out.Errors.push_back(std::move(e)); }
        for (auto& w : fsSPV.Warnings) { log.Warn("FS{}: {}", stageSuffix, w); out.Warnings.push_back(std::move(w)); }

        return out;
    }


    PassOutput ShaderCompiler::GenerateIR(const CompiledShader& shader, uint32_t passIndex, const std::vector<std::string>& keywords)
    {
        PassOutput out;
        if (passIndex >= shader.Passes.size())
        {
            Log::Instance().Error("Pass index {} out of range ({} passes)",
                passIndex, shader.Passes.size());
            return out;
        }
        InnerParams inner;
        FillInnerParams(inner, shader, passIndex, m_Config);

        DiagnosticCollector diag;
        InnerCompiler compiler(diag);
        InnerResult ir = compiler.Compile(inner);

        for (const Diagnostic& d : diag.GetDiagnostics())
        {
            if (d.Level == Severity::Warning)
                out.Warnings.push_back(d.Message);
            else
                out.Errors.push_back(d.Message);
        }

        if (!ir.Success)
            return out;

        out.VertexShader = std::move(ir.VertexGlsl);
        out.FragmentShader = std::move(ir.FragmentGlsl);
        return out;
    }

    PassOutput ShaderCompiler::GenerateHLSL(const CompiledShader& shader,
        uint32_t passIndex,
        const std::vector<std::string>& keywords)
    {
        auto out = GenerateSPIRV(shader, passIndex, keywords);
        try
        {
            out.VertexShader = DecompileHLSL(out.SpirvVertex);
            out.FragmentShader = DecompileHLSL(out.SpirvFragment);
        }
        catch (const std::exception& e)
        {
            std::string msg = std::string("HLSL cross-compilation failed: ") + e.what();
            out.Warnings.push_back(std::move(msg));
        }
        return out;
    }

    PassOutput ShaderCompiler::GenerateMSL(const CompiledShader& shader,
        uint32_t passIndex,
        const std::vector<std::string>& keywords)
    {
        auto out = GenerateSPIRV(shader, passIndex, keywords);
        try
        {
            out.VertexShader = DecompileMSL(out.SpirvVertex);
            out.FragmentShader = DecompileMSL(out.SpirvFragment);
        }
        catch (const std::exception& e)
        {
            std::string msg = std::string("MSL cross-compilation failed: ") + e.what();
            out.Warnings.push_back(std::move(msg));
        }
        return out;
    }

    std::unordered_map<std::string, std::string> ShaderCompiler::ScanShaderDirectory(const std::string& searchRoot)
    {
        std::unordered_map<std::string, std::string> result;
        std::error_code ec;
        for (auto& entry : std::filesystem::recursive_directory_iterator(searchRoot, ec))
        {
            if (entry.path().extension() != ".Shader")
                continue;
            SourceManager sm(entry.path().string());
            if (!sm.IsValid()) continue;
            TokenStream stream(sm);
            stream.Advance();
            auto& tok = stream.Current();
            if (tok.Is(TokenType::StringLiteral))
            {
                std::string path = std::filesystem::relative(entry.path(), std::filesystem::current_path()).string();
                for (auto& c : path) if (c == '\\') c = '/';
                result[tok.ToString(sm)] = std::move(path);
            }
        }
        return result;
    }

    CompiledComputeShader ShaderCompiler::CompileCompute(const std::string& source,
        const std::string& virtualPath)
    {
        CompiledComputeShader result;

        ComputeInnerParams params;
        params.Source = source;
        params.ReadFile = MakeReadFile(m_Config.ReadFile);
        FillInnerConfig(params.Config, m_Config, virtualPath);

        DiagnosticCollector diag;
        ComputeRewriter rewriter(diag);

        if (!rewriter.Analyze(params, result))
        {
            diag.PrintAll();
            return CompiledComputeShader();
        }

        if (!virtualPath.empty())
            result.ShaderName = std::filesystem::path(virtualPath).stem().string();

        for (const auto& res : result.Resources)
        {
            CompiledComputeShader::BindingInfo bi;
            bi.Set = res.Set;
            bi.Binding = res.Binding;
            bi.Name = res.Name;
            bi.Kind = res.Kind;
            result.Bindings.push_back(std::move(bi));
        }

        return result;
    }

    CompiledComputeShader ShaderCompiler::CompileComputeFile(const std::string& filePath)
    {
        std::string source = m_Config.ReadFile(filePath);
        if (source.empty())
        {
            Log::Instance().Error("Failed to read '{}'", filePath);
            return {};
        }
        return CompileCompute(source, filePath);
    }

    ComputeKernelOutput ShaderCompiler::GenerateComputeIR(const CompiledComputeShader& shader,
        uint32_t kernelIndex)
    {
        return GenerateComputeIRImpl(shader, kernelIndex, TargetBackend::Vulkan);
    }

    ComputeKernelOutput ShaderCompiler::GenerateComputeIRImpl(const CompiledComputeShader& shader,
        uint32_t kernelIndex, TargetBackend backend)
    {
        ComputeKernelOutput out;
        if (kernelIndex >= shader.Kernels.size())
        {
            Log::Instance().Error("Compute kernel index {} out of range ({} kernels)",
                kernelIndex, shader.Kernels.size());
            return out;
        }

        ComputeInnerParams params;
        FillComputeParams(params, shader, m_Config);

        DiagnosticCollector diag;
        ComputeRewriter rewriter(diag);

        out.Source = rewriter.Emit(params, kernelIndex, backend);

        for (const Diagnostic& d : diag.GetDiagnostics())
        {
            if (d.Level == Severity::Warning)
                out.Warnings.push_back(d.Message);
            else
                out.Errors.push_back(d.Message);
        }

        return out;
    }

    ComputeKernelOutput ShaderCompiler::GenerateComputeSPIRV(const CompiledComputeShader& shader,
        uint32_t kernelIndex)
    {
        return GenerateComputeSPIRVImpl(shader, kernelIndex, TargetBackend::Vulkan);
    }

    ComputeKernelOutput ShaderCompiler::GenerateComputeSPIRVImpl(const CompiledComputeShader& shader,
        uint32_t kernelIndex, TargetBackend backend)
    {
        ComputeKernelOutput out;
        if (kernelIndex >= shader.Kernels.size())
        {
            Log::Instance().Error("Compute kernel index {} out of range ({} kernels)",
                kernelIndex, shader.Kernels.size());
            return out;
        }

        auto ir = GenerateComputeIRImpl(shader, kernelIndex, backend);
        if (ir.Source.empty())
        {
            out.Errors = std::move(ir.Errors);
            return out;
        }

        const std::vector<std::string>& keywords = shader.Kernels[kernelIndex].VariantDefines;
        auto spv = CompileGLSL(ir.Source, ShaderStageType::Compute, backend, keywords);
        out.Spirv = std::move(spv.Bytecode);
        out.Errors = std::move(spv.Errors);
        out.Warnings = std::move(spv.Warnings);
        if (!out.Spirv.empty())
            out.Reflection = ReflectCompute(out.Spirv);
        return out;
    }

    ComputeKernelOutput ShaderCompiler::GenerateComputeGLSL(const CompiledComputeShader& shader,
        uint32_t kernelIndex)
    {
        auto out = GenerateComputeSPIRVImpl(shader, kernelIndex, TargetBackend::OpenGL);
        if (!out.Spirv.empty())
        {
            try
            {
                out.Source = DecompileSPIRV(out.Spirv);
            }
            catch (const std::exception& e)
            {
                std::string msg = std::string("GLSL cross-compilation failed: ") + e.what();
                out.Warnings.push_back(std::move(msg));
            }
        }
        return out;
    }

    ComputeKernelOutput ShaderCompiler::GenerateComputeHLSL(const CompiledComputeShader& shader,
        uint32_t kernelIndex)
    {
        auto out = GenerateComputeSPIRV(shader, kernelIndex);
        if (!out.Spirv.empty())
        {
            try
            {
                out.Source = DecompileHLSL(out.Spirv);
            }
            catch (const std::exception& e)
            {
                std::string msg = std::string("HLSL cross-compilation failed: ") + e.what();
                out.Warnings.push_back(std::move(msg));
            }
        }
        return out;
    }

    ComputeKernelOutput ShaderCompiler::GenerateComputeMSL(const CompiledComputeShader& shader,
        uint32_t kernelIndex)
    {
        auto out = GenerateComputeSPIRV(shader, kernelIndex);
        if (!out.Spirv.empty())
        {
            try
            {
                out.Source = DecompileMSL(out.Spirv);
            }
            catch (const std::exception& e)
            {
                std::string msg = std::string("MSL cross-compilation failed: ") + e.what();
                out.Warnings.push_back(std::move(msg));
            }
        }
        return out;
    }

}
