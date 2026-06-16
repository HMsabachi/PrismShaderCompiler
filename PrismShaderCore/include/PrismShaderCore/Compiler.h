#pragma once

#include "Log.h"
#include "Callback.h"
#include "PSL/AST.h"

#include <string>
#include <vector>

namespace PrismShaderCompiler
{

struct PassInfo
{
    std::string Name;
    std::unordered_map<std::string, std::string> Tags;
};

struct CompiledShader
{
    std::string ShaderName;
    int LOD = 200;

    std::vector<AST::ShaderUniform> Uniforms;
    std::vector<PassInfo>           Passes;
    std::vector<std::string>        Keywords;
    PropertyLayout                  MaterialLayout;

    std::vector<AST::GLSLCode> PassGLSL;
};

struct PassOutput
{
    std::string VertexShader;
    std::string FragmentShader;
};

struct CompilerConfig
{
    LogCallback      OnLog    = Callbacks::NullLog;
    ReadFileCallback ReadFile = Callbacks::ReadFileFromDisk;

    std::string IncludeRoot = "Assets/Include";
    std::string EngineRoot  = "Assets/Engine";

    int  GlslVersion      = 450;
    int  BindingMaterial  = 2;
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

    CompiledShader Compile(const std::string& source,
                           const std::string& virtualPath = "");
    CompiledShader CompileFile(const std::string& filePath);

    PassOutput Generate(const CompiledShader& shader,
                        uint32_t passIndex,
                        const std::vector<std::string>& keywords = {});

    const CompilerConfig& GetConfig() const { return m_Config; }

private:
    CompilerConfig m_Config;
};

} // namespace PrismShaderCompiler
