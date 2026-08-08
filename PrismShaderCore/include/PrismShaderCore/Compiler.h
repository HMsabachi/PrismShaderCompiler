#pragma once

#include "Log.h"
#include "Callback.h"
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
        std::vector<std::string> Errors;
        std::vector<std::string> Warnings;
    };

    enum class TargetBackend : uint8_t;

    struct CompilerConfig
    {
        LogCallback OnLog = Callbacks::NullLog;
        ReadFileCallback ReadFile = Callbacks::ReadFileFromDisk;
        ResolveUsePassCallback ResolveUsePass = nullptr;
        std::string IncludeRoot = "Assets/Include";
        uint32_t GlslVersion = 450;
        std::string MaterialBlockName = "PrismMaterial";

        uint32_t OpenGLMaterialUniformBufferBinding = 20;
        uint32_t OpenGLTextureBeginBinding = 12;

        uint32_t VulkanMaterialUniformBufferSet = 3;
        uint32_t VulkanMaterialUniformBufferBinding = 0;
        uint32_t VulkanTextureBeginSet = 3;
        uint32_t VulkanTextureBeginBinding = 1;
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
        PassOutput GenerateSPIRVImpl(const CompiledShader& shader,
            uint32_t passIndex,
            const std::vector<std::string>& keywords,
            TargetBackend backend);

        CompilerConfig m_Config;
        std::unordered_set<std::string> m_CompileInProgress;
    };

} // namespace PrismShaderCompiler
