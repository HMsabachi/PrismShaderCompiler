#include "Generator/SpirvGenerator.h"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>

namespace PrismShaderCompiler
{

    static EShLanguage ToGlslangStage(ShaderStageType stage)
    {
        switch (stage)
        {
        case ShaderStageType::Vertex:   return EShLangVertex;
        case ShaderStageType::Fragment: return EShLangFragment;
        case ShaderStageType::Compute:  return EShLangCompute;
        }
        return EShLangVertex;
    }

    static void EnsureInitialized()
    {
        static bool s_Initialized = false;
        if (!s_Initialized)
        {
            glslang::InitializeProcess();
            s_Initialized = true;
        }
    }

    SpirvResult PSC_API CompileGLSL(const std::string& glslSource, ShaderStageType stage,
                                    TargetBackend backend, const std::vector<std::string>& keywords)
    {
        SpirvResult result;
        EnsureInitialized();

        EShLanguage lang = ToGlslangStage(stage);
        glslang::TShader shader(lang);
        const char* src = glslSource.c_str();
        shader.setStrings(&src, 1);

        if (backend == TargetBackend::Vulkan)
        {
            shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 450);
            shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        }
        else
        {
            shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientOpenGL, 450);
            shader.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
        }
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

        // preamble 在 #version 之前处理，但不影响 #version 的合法性（glslang 明文保证）
        std::string preamble = "#extension GL_GOOGLE_cpp_style_line_directive : enable\n";
        preamble += (backend == TargetBackend::Vulkan)
            ? "#define PRISM_BACKEND_VULKAN 1\n"
            : "#define PRISM_BACKEND_OPENGL 1\n";

        for (const std::string& keyword : keywords)
            preamble += "#define " + keyword + "\n";

        shader.setPreamble(preamble.c_str());

        TBuiltInResource resources = *GetDefaultResources();
        EShMessages messages = EShMsgDefault;

        if (!shader.parse(&resources, 450, EProfile::ECoreProfile, false, true, messages))
        {
            result.Errors.push_back(shader.getInfoLog());
            result.Errors.push_back(shader.getInfoDebugLog());
            return result;
        }

        glslang::TProgram program;
        program.addShader(&shader);

        if (!program.link(messages))
        {
            result.Errors.push_back(program.getInfoLog());
            result.Errors.push_back(program.getInfoDebugLog());
            return result;
        }

        auto* intermediate = program.getIntermediate(lang);
        if (!intermediate)
        {
            result.Errors.push_back("glslang: no intermediate representation produced");
            return result;
        }

        glslang::GlslangToSpv(*intermediate, result.Bytecode);
        result.Success = !result.Bytecode.empty();
        return result;
    }

} // namespace PrismShaderCompiler
