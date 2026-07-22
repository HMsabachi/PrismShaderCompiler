#pragma once

#include "Log.h"
#include "Callback.h"
#include "Reflection.h"
#include "PSL/AST.h"
#include "CompilerCompute.h"
#include "Pipeline/PipelineState.h"

#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>

namespace PrismShaderCompiler
{

    struct PassInfo
    {
        std::string Name;
        std::unordered_map<std::string, std::string> Tags;
        std::optional<PipelineState> RenderState;
        AST::GLSLCode Glsl;
    };

    struct KeywordDef
    {
        std::string Name;
        bool IsMultiCompile = false;
    };

    struct CompiledShader
    {
        std::string ShaderName;
        int LOD = 200;

        std::vector<AST::ShaderUniform> Uniforms;
        std::vector<PassInfo> Passes;
        std::vector<KeywordDef> Keywords;
        PropertyLayout MaterialLayout;
        std::optional<PipelineState> RenderState;
    };

    struct PassOutput
    {
        std::string VertexShader;
        std::string FragmentShader;
        std::vector<uint32_t> SpirvVertex;
        std::vector<uint32_t> SpirvFragment;
        ShaderReflection Reflection;
        std::vector<std::string> Errors;
        std::vector<std::string> Warnings;
    };

    struct CompilerConfig
    {
        LogCallback OnLog = Callbacks::NullLog;
        ReadFileCallback ReadFile = Callbacks::ReadFileFromDisk;
        ResolveUsePassCallback ResolveUsePass = nullptr;

        std::string IncludeRoot = "Assets/Include";
        std::string EngineRoot = "Assets/Engine";

        int  GlslVersion = 450;
        int  BindingMaterial = 2;
        std::string MaterialBlockName = "PrismMaterial";

        std::vector<std::string> EngineHeaders = {
            "Bindings.glsl", "PrismFrame.glsl", "PrismObject.glsl",
            "PrismShadow.glsl", "PrismEngineTextures.glsl",
        };
    };

    class ShaderCompiler
    {
    public:
        ShaderCompiler(const CompilerConfig& config = {});
        void SetConfig(const CompilerConfig& config);

        CompiledShader Compile(const std::string& source,
            const std::string& virtualPath = "");
        CompiledShader CompileFile(const std::string& filePath);

        CompiledComputeShader CompileCompute(const std::string& source,
            const std::string& virtualPath = "");
        CompiledComputeShader CompileComputeFile(const std::string& filePath);

        PassOutput GenerateGLSL(const CompiledShader& shader,
            uint32_t passIndex,
            const std::vector<std::string>& keywords = {});
        PassOutput GenerateHLSL(const CompiledShader& shader,
            uint32_t passIndex,
            const std::vector<std::string>& keywords = {});
        PassOutput GenerateMSL(const CompiledShader& shader,
            uint32_t passIndex,
            const std::vector<std::string>& keywords = {});
        PassOutput GenerateSPIRV(const CompiledShader& shader,
            uint32_t passIndex,
            const std::vector<std::string>& keywords = {});
        PassOutput GenerateIR(const CompiledShader& shader,
            uint32_t passIndex,
            const std::vector<std::string>& keywords = {});

        ComputeKernelOutput GenerateComputeIR(const CompiledComputeShader& shader,
            uint32_t kernelIndex);
        ComputeKernelOutput GenerateComputeSPIRV(const CompiledComputeShader& shader,
            uint32_t kernelIndex);
        ComputeKernelOutput GenerateComputeGLSL(const CompiledComputeShader& shader,
            uint32_t kernelIndex);
        ComputeKernelOutput GenerateComputeHLSL(const CompiledComputeShader& shader,
            uint32_t kernelIndex);
        ComputeKernelOutput GenerateComputeMSL(const CompiledComputeShader& shader,
            uint32_t kernelIndex);

        const CompilerConfig& GetConfig() const { return m_Config; }

        std::unordered_map<std::string, std::string> ScanShaderDirectory(const std::string& searchRoot);

    private:
        CompilerConfig m_Config;
        std::unordered_set<std::string> m_CompileInProgress;
    };

} // namespace PrismShaderCompiler
