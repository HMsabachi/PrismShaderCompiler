#include "Generator/SpirvGenerator.h"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <spirv_cross.hpp>

#include <algorithm>

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

    SpirvResult PSC_API CompileGLSL(const std::string& glslSource, ShaderStageType stage)
    {
        SpirvResult result;
        EnsureInitialized();

        EShLanguage lang = ToGlslangStage(stage);
        glslang::TShader shader(lang);
        const char* src = glslSource.c_str();
        shader.setStrings(&src, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientOpenGL, 450);
        shader.setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
        shader.setPreamble("#extension GL_GOOGLE_cpp_style_line_directive : enable\n");

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

    namespace
    {
        uint32_t ArrayCount(const spirv_cross::SPIRType& type)
        {
            return type.array.empty() ? 1u : static_cast<uint32_t>(type.array.front());
        }
    } // namespace

    ShaderReflection PSC_API ReflectSPIRV(const std::vector<uint32_t>& spirv, uint32_t stageFlags)
    {
        ShaderReflection reflection;
        if (spirv.empty())
            return reflection;

        try
        {
            spirv_cross::Compiler compiler(spirv);
            auto resources = compiler.get_shader_resources();

            for (const auto& res : resources.uniform_buffers)
            {
                const auto& type = compiler.get_type(res.base_type_id);
                ReflectedBinding b;
                b.Set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                b.Binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                b.Type = DescriptorType::UniformBuffer;
                b.StageFlags = stageFlags;
                b.Count = ArrayCount(type);
                b.Name = res.name;
                b.BlockName = res.name;
                b.BufferSize = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
                reflection.Bindings.push_back(std::move(b));
            }

            for (const auto& res : resources.storage_buffers)
            {
                const auto& type = compiler.get_type(res.base_type_id);
                ReflectedBinding b;
                b.Set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                b.Binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                b.Type = DescriptorType::StorageBuffer;
                b.StageFlags = stageFlags;
                b.Count = ArrayCount(type);
                b.Name = res.name;
                b.BlockName = res.name;
                b.BufferSize = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
                reflection.Bindings.push_back(std::move(b));
            }

            for (const auto& res : resources.sampled_images)
            {
                const auto& type = compiler.get_type(res.type_id);
                ReflectedBinding b;
                b.Set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                b.Binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                b.Type = DescriptorType::CombinedImageSampler;
                b.StageFlags = stageFlags;
                b.Count = ArrayCount(type);
                b.Name = res.name;
                reflection.Bindings.push_back(std::move(b));
            }

            for (const auto& res : resources.storage_images)
            {
                const auto& type = compiler.get_type(res.type_id);
                ReflectedBinding b;
                b.Set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                b.Binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                b.Type = DescriptorType::StorageImage;
                b.StageFlags = stageFlags;
                b.Count = ArrayCount(type);
                b.Name = res.name;
                reflection.Bindings.push_back(std::move(b));
            }

            for (const auto& res : resources.separate_images)
            {
                const auto& type = compiler.get_type(res.type_id);
                ReflectedBinding b;
                b.Set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                b.Binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                b.Type = DescriptorType::SampledImage;
                b.StageFlags = stageFlags;
                b.Count = ArrayCount(type);
                b.Name = res.name;
                reflection.Bindings.push_back(std::move(b));
            }

            for (const auto& res : resources.separate_samplers)
            {
                ReflectedBinding b;
                b.Set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                b.Binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                b.Type = DescriptorType::Sampler;
                b.StageFlags = stageFlags;
                b.Name = res.name;
                reflection.Bindings.push_back(std::move(b));
            }

            for (const auto& res : resources.push_constant_buffers)
            {
                const auto& type = compiler.get_type(res.base_type_id);
                ReflectedPushConstant pc;
                pc.Offset = 0;
                pc.Size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
                pc.StageFlags = stageFlags;
                pc.Name = res.name;
                reflection.PushConstants.push_back(std::move(pc));
            }
        }
        catch (const std::exception&)
        {
            // 反射失败不阻断
        }

        return reflection;
    }

    ShaderReflection PSC_API MergeReflections(const ShaderReflection& a, const ShaderReflection& b)
    {
        ShaderReflection out = a;

        for (const auto& binding : b.Bindings)
        {
            auto it = std::find_if(out.Bindings.begin(), out.Bindings.end(),
                [&](const ReflectedBinding& e)
                { return e.Set == binding.Set && e.Binding == binding.Binding; });
            if (it != out.Bindings.end())
                it->StageFlags |= binding.StageFlags;
            else
                out.Bindings.push_back(binding);
        }

        for (const auto& pc : b.PushConstants)
        {
            auto it = std::find_if(out.PushConstants.begin(), out.PushConstants.end(),
                [&](const ReflectedPushConstant& e) { return e.Offset == pc.Offset; });
            if (it != out.PushConstants.end())
                it->StageFlags |= pc.StageFlags;
            else
                out.PushConstants.push_back(pc);
        }

        return out;
    }

} // namespace PrismShaderCompiler
