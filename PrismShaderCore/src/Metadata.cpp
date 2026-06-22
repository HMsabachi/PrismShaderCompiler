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

            if (i < shader.PassGLSL.size())
            {
                auto& glsl = shader.PassGLSL[i];

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

} // namespace PrismShaderCompiler
