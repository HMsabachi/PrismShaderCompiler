#include "Metadata.h"
#include "PSL/GLSLType.h"
#include "Property/VertexType.h"
#include <json/json.hpp>

namespace PrismShaderCompiler
{

    static nlohmann::json SerializeRenderState(const std::optional<PipelineState>& state)
    {
        if (!state) return {};
        auto& rs = *state;
        nlohmann::json j;
        j["blend"]["enabled"]   = rs.BlendEnabled;
        j["blend"]["srcRGB"]    = (int)rs.SrcFactor;
        j["blend"]["dstRGB"]    = (int)rs.DstFactor;
        j["blend"]["srcAlpha"]  = (int)rs.SrcAlpha;
        j["blend"]["dstAlpha"]  = (int)rs.DstAlpha;
        j["depth"]["test"]      = rs.DepthTest;
        j["depth"]["write"]     = rs.DepthWrite;
        j["depth"]["compare"]   = (int)rs.DepthCompare;
        j["cull"]               = (int)rs.Cull;
        j["colorMask"]          = (int)rs.WriteMask;
        j["depthBias"]["factor"] = rs.DepthBiasFactor;
        j["depthBias"]["units"]  = rs.DepthBiasUnits;
        j["stencil"]["test"]        = rs.StencilTest;
        j["stencil"]["compare"]     = (int)rs.StencilCompare;
        j["stencil"]["ref"]         = rs.StencilRef;
        j["stencil"]["readMask"]    = rs.StencilReadMask;
        j["stencil"]["writeMask"]   = rs.StencilWriteMask;
        j["stencil"]["failOp"]      = (int)rs.StencilFailOp;
        j["stencil"]["depthFailOp"] = (int)rs.StencilDepthFailOp;
        j["stencil"]["passOp"]      = (int)rs.StencilPassOp;
        j["polygonMode"] = (int)rs.FillMode;
        j["lineWidth"]   = rs.LineWidth;
        return j;
    }

    static nlohmann::json SerializeUniform(const AST::ShaderUniform& u)
    {
        nlohmann::json ju;
        ju["name"]        = u.Name;
        ju["display"]     = u.DisplayName;
        ju["type"]        = (int)u.Type;
        ju["offset"]      = u.BufferOffset;
        ju["size"]        = u.BufferSize;
        ju["textureSlot"] = u.TextureSlot;

        switch (u.Type)
        {
        case PropertyType::Bool:
            ju["default"] = u.DefaultValue.empty() ? false : u.DefaultValue[0].Bool;
            break;
        case PropertyType::Int:
        case PropertyType::Enum:
            ju["default"] = u.DefaultValue.empty() ? 0 : u.DefaultValue[0].Int;
            break;
        case PropertyType::Float:
        case PropertyType::Matrix3:
        case PropertyType::Matrix4:
            ju["default"] = u.DefaultValue.empty() ? 0.0f : u.DefaultValue[0].Float;
            break;
        case PropertyType::Range:
            {
                float val = u.DefaultValue.empty() ? 0.0f : u.DefaultValue[0].Float;
                ju["default"] = { {"value", val}, {"min", u.RangeMin}, {"max", u.RangeMax} };
            }
            break;
        case PropertyType::Color:
        case PropertyType::Color3:
        case PropertyType::Vector2:
        case PropertyType::Vector3:
        case PropertyType::Vector4:
            {
                auto& arr = ju["default"] = nlohmann::json::array();
                for (auto& s : u.DefaultValue)
                    arr.push_back(s.Float);
            }
            break;
        default:
            ju["default"] = nullptr;
            break;
        }

        if (u.Type == PropertyType::Enum)
            ju["enumOptions"] = u.EnumOptions;

        return ju;
    }

    std::string ToJson(const CompiledShader& shader)
    {
        nlohmann::json j;

        j["name"] = shader.ShaderName;
        j["lod"]  = shader.LOD;

        auto jrs = SerializeRenderState(shader.RenderState);
        if (!jrs.empty()) j["renderState"] = std::move(jrs);

        auto& junis = j["uniforms"] = nlohmann::json::array();
        for (auto& u : shader.Uniforms)
            junis.push_back(SerializeUniform(u));

        auto& jpasses = j["passes"] = nlohmann::json::array();
        for (size_t i = 0; i < shader.Passes.size(); i++)
        {
            auto& p = shader.Passes[i];
            nlohmann::json jp;
            jp["name"] = p.Name;
            jp["tags"] = p.Tags;
            auto jprs = SerializeRenderState(p.RenderState);
            if (!jprs.empty()) jp["renderState"] = std::move(jprs);

            {
                auto& glsl = p.Glsl;

                auto& jattrs = jp["attributes"] = nlohmann::json::array();
                for (auto& attr : glsl.Attributes)
                {
                    nlohmann::json ja;
                    ja["name"]     = attr.Name;
                    ja["type"]     = GLSLTypeUtil::ToString(attr.Type);
                    ja["semantic"] = (int)attr.Semantic;
                    ja["location"] = SemanticToLocation(attr.Semantic);
                    jattrs.push_back(std::move(ja));
                }

                if (glsl.Varying)
                {
                    auto& v = *glsl.Varying;
                    auto& jv = jp["varying"];
                    jv["struct"] = v.StructName;
                    jv["instance"] = v.InstanceName;
                    auto& jmembers = jv["members"] = nlohmann::json::array();
                    uint32_t loc = 0;
                    for (auto& m : v.Members)
                    {
                        bool isMat = GLSLTypeUtil::IsMatrixType(m.Type);
                        uint32_t cols = isMat ? GLSLTypeUtil::LocationSlots(m.Type) : 1;
                        uint32_t slots = cols * m.ArraySize;

                        nlohmann::json jm;
                        jm["name"] = m.Name;
                        jm["type"] = GLSLTypeUtil::ToString(m.Type);
                        if (m.ArraySize > 1) jm["arraySize"] = m.ArraySize;
                        jm["location"] = loc;
                        jm["slots"] = slots;
                        if (isMat) jm["columnType"] = GLSLTypeUtil::ToString(GLSLTypeUtil::ColumnType(m.Type));
                        loc += slots;
                        jmembers.push_back(std::move(jm));
                    }
                }

                auto& fragOutputs = glsl.FragmentOutputs;
                if (!fragOutputs.empty())
                {
                    auto& jouts = jp["outputs"] = nlohmann::json::array();
                    for (auto& fo : fragOutputs)
                    {
                        nlohmann::json jo;
                        jo["name"]     = fo.Name;
                        jo["location"] = fo.Location;
                        jo["type"]     = GLSLTypeUtil::ToString(fo.Type);
                        jouts.push_back(std::move(jo));
                    }
                }
            }

            jpasses.push_back(std::move(jp));
        }

        auto& jkws = j["keywords"] = nlohmann::json::array();
        for (auto& kw : shader.Keywords)
        {
            nlohmann::json jkw;
            jkw["name"]          = kw.Name;
            jkw["multi_compile"] = kw.IsMultiCompile;
            jkws.push_back(std::move(jkw));
        }

        return j.dump(2);
    }

    static std::string_view ResourceKindToString(CSL::ResourceKind k)
    {
        switch (k)
        {
        case CSL::ResourceKind::StorageBuffer:        return "StorageBuffer";
        case CSL::ResourceKind::UniformBuffer:        return "UniformBuffer";
        case CSL::ResourceKind::Sampler2D:            return "Sampler2D";
        case CSL::ResourceKind::Sampler2DMS:          return "Sampler2DMS";
        case CSL::ResourceKind::Sampler2DShadow:      return "Sampler2DShadow";
        case CSL::ResourceKind::Sampler2DArray:       return "Sampler2DArray";
        case CSL::ResourceKind::Sampler2DArrayShadow: return "Sampler2DArrayShadow";
        case CSL::ResourceKind::Sampler3D:            return "Sampler3D";
        case CSL::ResourceKind::SamplerCube:          return "SamplerCube";
        case CSL::ResourceKind::SamplerCubeShadow:    return "SamplerCubeShadow";
        case CSL::ResourceKind::Image2D:              return "Image2D";
        case CSL::ResourceKind::Image3D:              return "Image3D";
        case CSL::ResourceKind::ImageCube:            return "ImageCube";
        }
        return "Unknown";
    }

    static std::string_view ImageFormatToString(CSL::ImageFormat f)
    {
        switch (f)
        {
        case CSL::ImageFormat::rgba32f:        return "rgba32f";
        case CSL::ImageFormat::rgba16f:        return "rgba16f";
        case CSL::ImageFormat::rg32f:          return "rg32f";
        case CSL::ImageFormat::rg16f:          return "rg16f";
        case CSL::ImageFormat::r32f:           return "r32f";
        case CSL::ImageFormat::r16f:           return "r16f";
        case CSL::ImageFormat::rgba32i:        return "rgba32i";
        case CSL::ImageFormat::rgba16i:        return "rgba16i";
        case CSL::ImageFormat::rgba8i:         return "rgba8i";
        case CSL::ImageFormat::rg32i:          return "rg32i";
        case CSL::ImageFormat::rg16i:          return "rg16i";
        case CSL::ImageFormat::rg8i:           return "rg8i";
        case CSL::ImageFormat::r32i:           return "r32i";
        case CSL::ImageFormat::r16i:           return "r16i";
        case CSL::ImageFormat::r8i:            return "r8i";
        case CSL::ImageFormat::rgba32ui:        return "rgba32ui";
        case CSL::ImageFormat::rgba16ui:       return "rgba16ui";
        case CSL::ImageFormat::rgba8ui:        return "rgba8ui";
        case CSL::ImageFormat::rg32ui:         return "rg32ui";
        case CSL::ImageFormat::rg16ui:         return "rg16ui";
        case CSL::ImageFormat::rg8ui:          return "rg8ui";
        case CSL::ImageFormat::r32ui:          return "r32ui";
        case CSL::ImageFormat::r16ui:          return "r16ui";
        case CSL::ImageFormat::r8ui:           return "r8ui";
        case CSL::ImageFormat::rgba16:         return "rgba16";
        case CSL::ImageFormat::rgb10_a2:       return "rgb10_a2";
        case CSL::ImageFormat::rgba8:          return "rgba8";
        case CSL::ImageFormat::rgba8_snorm:    return "rgba8_snorm";
        case CSL::ImageFormat::rg16:           return "rg16";
        case CSL::ImageFormat::rg8:            return "rg8";
        case CSL::ImageFormat::rg8_snorm:      return "rg8_snorm";
        case CSL::ImageFormat::r16:            return "r16";
        case CSL::ImageFormat::r8:             return "r8";
        case CSL::ImageFormat::r8_snorm:       return "r8_snorm";
        case CSL::ImageFormat::r16f_depth:     return "r16f_depth";
        case CSL::ImageFormat::r32f_depth:    return "r32f_depth";
        case CSL::ImageFormat::Unknown:        return "unknown";
        }
        return "unknown";
    }

    std::string ToJson(const CompiledComputeShader& shader)
    {
        nlohmann::json j;

        j["name"] = shader.ShaderName;
        j["glslVersion"] = shader.GlslVersion;

        auto& jkernels = j["kernels"] = nlohmann::json::array();
        for (auto& k : shader.Kernels)
        {
            nlohmann::json jk;
            jk["name"] = k.Name;
            jk["groupSize"] = std::vector<uint32_t>{ k.GroupSizeX, k.GroupSizeY, k.GroupSizeZ };
            if (!k.VariantDefines.empty())
                jk["variants"] = k.VariantDefines;
            jkernels.push_back(std::move(jk));
        }

        auto& jres = j["resources"] = nlohmann::json::array();
        for (auto& r : shader.Resources)
        {
            nlohmann::json jr;
            jr["kind"] = std::string(ResourceKindToString(r.Kind));
            jr["type"] = GLSLTypeUtil::ToString(r.Type);
            jr["name"] = r.Name;
            jr["set"] = r.Set;
            jr["binding"] = r.Binding;
            if (r.Format != CSL::ImageFormat::Unknown)
                jr["format"] = std::string(ImageFormatToString(r.Format));
            if (!r.BlockName.empty())
            {
                jr["blockName"] = r.BlockName;
                jr["instanceName"] = r.InstanceName;
            }
            jres.push_back(std::move(jr));
        }

        auto& junis = j["uniforms"] = nlohmann::json::array();
        for (auto& u : shader.Uniforms)
        {
            nlohmann::json ju;
            ju["name"] = u.Name;
            ju["type"] = GLSLTypeUtil::ToString(u.Type);
            ju["location"] = u.Location;
            junis.push_back(std::move(ju));
        }

        auto& jbindings = j["bindings"] = nlohmann::json::array();
        for (auto& b : shader.Bindings)
        {
            nlohmann::json jb;
            jb["set"] = b.Set;
            jb["binding"] = b.Binding;
            jb["name"] = b.Name;
            jb["kind"] = std::string(ResourceKindToString(b.Kind));
            jbindings.push_back(std::move(jb));
        }

        return j.dump(2);
    }

    static std::string_view DescriptorTypeToString(DescriptorType t)
    {
        switch (t)
        {
        case DescriptorType::UniformBuffer:        return "UniformBuffer";
        case DescriptorType::StorageBuffer:        return "StorageBuffer";
        case DescriptorType::UniformBufferDynamic: return "UniformBufferDynamic";
        case DescriptorType::StorageBufferDynamic: return "StorageBufferDynamic";
        case DescriptorType::CombinedImageSampler: return "CombinedImageSampler";
        case DescriptorType::SampledImage:         return "SampledImage";
        case DescriptorType::StorageImage:        return "StorageImage";
        case DescriptorType::Sampler:              return "Sampler";
        case DescriptorType::InputAttachment:     return "InputAttachment";
        }
        return "Unknown";
    }

    std::string ToJson(const ShaderReflection& reflection)
    {
        nlohmann::json j;

        auto& jb = j["bindings"] = nlohmann::json::array();
        for (auto& b : reflection.Bindings)
        {
            nlohmann::json o;
            o["set"]     = b.Set;
            o["binding"] = b.Binding;
            o["type"]    = std::string(DescriptorTypeToString(b.Type));
            o["stages"]  = b.StageFlags;
            o["count"]   = b.Count;
            if (!b.Name.empty())      o["name"]      = b.Name;
            if (!b.BlockName.empty()) o["blockName"] = b.BlockName;
            if (b.BufferSize)         o["size"]      = b.BufferSize;
            jb.push_back(std::move(o));
        }

        auto& jp = j["pushConstants"] = nlohmann::json::array();
        for (auto& pc : reflection.PushConstants)
        {
            nlohmann::json o;
            o["offset"] = pc.Offset;
            o["size"]    = pc.Size;
            o["stages"]  = pc.StageFlags;
            if (!pc.Name.empty()) o["name"] = pc.Name;
            jp.push_back(std::move(o));
        }

        return j.dump(2);
    }

} // namespace PrismShaderCompiler
