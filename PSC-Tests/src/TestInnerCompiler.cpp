#include "TestFramework.h"

#include <PrismShaderCore/Inner/InnerCompiler.h>
#include <PrismShaderCore/PSL/Diagnostics.h>

#include <map>
#include <string>
#include <vector>

namespace
{

using PrismShaderCompiler::DiagnosticCollector;
using PrismShaderCompiler::InnerCompiler;
using PrismShaderCompiler::InnerParams;
using PrismShaderCompiler::InnerProperty;
using PrismShaderCompiler::InnerResult;
using PrismShaderCompiler::PropertyType;

const char* const kMinimalBlock =
    "attribute vec3 a_Position : POSITION;\n"
    "attribute vec2 a_TexCoord : TEXCOORD0;\n"
    "varying VertexOutput\n"
    "{\n"
    "    vec3 WorldPos;\n"
    "    vec2 UV;\n"
    "} vs_Output;\n"
    "layout(location = 0) out vec4 FragColor;\n"
    "void vert()\n"
    "{\n"
    "    vs_Output.WorldPos = a_Position;\n"
    "    vs_Output.UV = a_TexCoord;\n"
    "    gl_Position = vec4(a_Position, 1.0);\n"
    "}\n"
    "void frag()\n"
    "{\n"
    "    FragColor = vec4(vs_Output.WorldPos, vs_Output.UV.x);\n"
    "}\n";

struct Harness
{
    DiagnosticCollector Diag;
    std::map<std::string, std::string> Vfs;

    InnerResult Compile(const std::string& glslBlock,
                        std::vector<InnerProperty> properties = {},
                        const std::string& path = "Assets/Test.Shader",
                        uint32_t glslVersion = 450)
    {
        InnerParams params;
        params.GlslBlock = glslBlock;
        params.Properties = std::move(properties);
        params.Config.SourcePath = path;
        params.Config.IncludeRoot = "Assets/Include";
        params.Config.GlslVersion = glslVersion;
        params.ReadFile = [this](const std::string& p, std::string& out)
        {
            const auto it = Vfs.find(p);

            if (it == Vfs.end())
                return false;

            out = it->second;
            return true;
        };

        InnerCompiler compiler(Diag);
        return compiler.Compile(params);
    }
};

bool Contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

int CountOccurrences(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
        return 0;

    int count = 0;
    size_t pos = haystack.find(needle);

    while (pos != std::string::npos)
    {
        ++count;
        pos = haystack.find(needle, pos + needle.size());
    }

    return count;
}

InnerProperty MakeProperty(const std::string& name, PropertyType type, uint32_t textureSlot = 0)
{
    InnerProperty property;
    property.Name = name;
    property.Type = type;
    property.TextureSlot = textureSlot;
    return property;
}

} // namespace

PSC_TEST(MinimalBlockCompilesBothStages)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock);

    PSC_CHECK(result.Success);
    PSC_CHECK(!result.VertexGlsl.empty());
    PSC_CHECK(!result.FragmentGlsl.empty());
}

PSC_TEST(VersionLineComesFirstAndCarriesConfig)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock, {}, "Assets/Test.Shader", 460);

    PSC_CHECK(result.Success);

    const std::string& vertex = result.VertexGlsl;
    const size_t version = vertex.find("#version 460 core");

    PSC_CHECK(version != std::string::npos);
    PSC_CHECK(vertex.find("#line ") == std::string::npos || version < vertex.find("#line "));
}

PSC_TEST(VertexAttributeBecomesLayoutLocation)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock);

    PSC_CHECK(result.Success);
    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 0) in vec3 a_Position;"));
    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 4) in vec2 a_TexCoord;"));
    PSC_CHECK(!Contains(result.VertexGlsl, "attribute "));

    PSC_CHECK(!Contains(result.FragmentGlsl, "layout(location = 0) in vec3 a_Position;"));
    PSC_CHECK(!Contains(result.FragmentGlsl, "attribute "));
}

PSC_TEST(VaryingBecomesFlattenedInterfaceWithSyncCode)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock);

    PSC_CHECK(result.Success);

    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 0) out vec3 VertexOutput_WorldPos;"));
    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 1) out vec2 VertexOutput_UV;"));
    PSC_CHECK(Contains(result.VertexGlsl, "VertexOutput_WorldPos = vs_Output.WorldPos;"));
    PSC_CHECK(Contains(result.VertexGlsl, "VertexOutput_UV = vs_Output.UV;"));

    PSC_CHECK(Contains(result.FragmentGlsl, "layout(location = 0) in vec3 VertexOutput_WorldPos;"));
    PSC_CHECK(Contains(result.FragmentGlsl, "layout(location = 1) in vec2 VertexOutput_UV;"));
    PSC_CHECK(Contains(result.FragmentGlsl, "vs_Output.WorldPos = VertexOutput_WorldPos;"));
    PSC_CHECK(Contains(result.FragmentGlsl, "vs_Output.UV = VertexOutput_UV;"));

    PSC_CHECK(Contains(result.VertexGlsl, "struct VertexOutput"));
    PSC_CHECK(Contains(result.VertexGlsl, "} vs_Output;"));
    PSC_CHECK(Contains(result.FragmentGlsl, "struct VertexOutput"));
}

PSC_TEST(MatrixVaryingSplitsIntoColumns)
{
    const std::string block =
        "attribute vec3 a_Position : POSITION;\n"
        "varying Varying\n"
        "{\n"
        "    mat3 Basis;\n"
        "} vs_In;\n"
        "void vert()\n"
        "{\n"
        "    gl_Position = vec4(a_Position, 1.0);\n"
        "}\n"
        "void frag()\n"
        "{\n"
        "    gl_FragColor = vec4(vs_In.Basis[0], 1.0);\n"
        "}\n";

    Harness harness;
    const InnerResult result = harness.Compile(block);

    PSC_CHECK(result.Success);
    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 0) out vec3 Varying_Basis_col0;"));
    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 2) out vec3 Varying_Basis_col2;"));
    PSC_CHECK(Contains(result.VertexGlsl, "Varying_Basis_col1 = vs_In.Basis[1];"));
}

PSC_TEST(ArrayVaryingAppendsElementIndex)
{
    const std::string block =
        "attribute vec3 a_Position : POSITION;\n"
        "varying Varying\n"
        "{\n"
        "    float Weights[2];\n"
        "} vs_In;\n"
        "void vert()\n"
        "{\n"
        "    gl_Position = vec4(a_Position, 1.0);\n"
        "}\n"
        "void frag()\n"
        "{\n"
        "    gl_FragColor = vec4(vs_In.Weights[0]);\n"
        "}\n";

    Harness harness;
    const InnerResult result = harness.Compile(block);

    PSC_CHECK(result.Success);
    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 0) out float Varying_Weights_0;"));
    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 1) out float Varying_Weights_1;"));
    PSC_CHECK(Contains(result.VertexGlsl, "Varying_Weights_1 = vs_In.Weights[1];"));
    PSC_CHECK(Contains(result.VertexGlsl, "float Weights[2];"));
}

PSC_TEST(EntryPointsAreRebuiltAsMain)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock);

    PSC_CHECK(result.Success);

    PSC_CHECK(!Contains(result.VertexGlsl, "void vert"));
    PSC_CHECK(!Contains(result.FragmentGlsl, "void frag"));

    PSC_CHECK_EQ(CountOccurrences(result.VertexGlsl, "void main()"), 1);
    PSC_CHECK_EQ(CountOccurrences(result.FragmentGlsl, "void main()"), 1);

    PSC_CHECK(Contains(result.VertexGlsl, "gl_Position = vec4(a_Position, 1.0);"));
    PSC_CHECK(!Contains(result.FragmentGlsl, "gl_Position = vec4(a_Position, 1.0);"));

    PSC_CHECK(Contains(result.FragmentGlsl, "FragColor = vec4(vs_Output.WorldPos, vs_Output.UV.x);"));
    PSC_CHECK(!Contains(result.VertexGlsl, "FragColor = vec4(vs_Output.WorldPos, vs_Output.UV.x);"));
}

PSC_TEST(FragmentOutputIsHoistedToHeadOnce)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock);

    PSC_CHECK(result.Success);

    const std::string declaration = "layout(location = 0) out vec4 FragColor;";

    PSC_CHECK_EQ(CountOccurrences(result.FragmentGlsl, declaration), 1);
    PSC_CHECK(result.FragmentGlsl.find(declaration) < result.FragmentGlsl.find("void main()"));
    PSC_CHECK_EQ(CountOccurrences(result.VertexGlsl, declaration), 0);
}

PSC_TEST(DeferredConditionSurvivesWithRewrittenAttribute)
{
    const std::string block =
        "#pragma shader_feature SKINNED\n"
        "attribute vec3 a_Position : POSITION;\n"
        "#ifdef SKINNED\n"
        "attribute ivec4 a_BoneIndices : BONEINDICES;\n"
        "#endif\n"
        "void vert()\n"
        "{\n"
        "    gl_Position = vec4(a_Position, 1.0);\n"
        "}\n"
        "void frag()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    Harness harness;
    const InnerResult result = harness.Compile(block);

    PSC_CHECK(result.Success);
    PSC_CHECK(Contains(result.VertexGlsl, "#if defined(SKINNED)"));
    PSC_CHECK(Contains(result.VertexGlsl, "#endif"));
    PSC_CHECK(Contains(result.VertexGlsl, "layout(location = 6) in ivec4 a_BoneIndices;"));

    // 片元阶段属性声明整体丢弃，但条件标记保留，两侧结构必须对齐
    PSC_CHECK(Contains(result.FragmentGlsl, "#if defined(SKINNED)"));
    PSC_CHECK(!Contains(result.FragmentGlsl, "a_BoneIndices"));

    if (result.Variants.size() != 1)
    {
        PSC_CHECK_EQ(result.Variants.size(), size_t(1));
        return;
    }

    PSC_CHECK_EQ(result.Variants[0].Name, std::string("shader_feature"));

    if (result.Variants[0].Keywords.size() != 1)
    {
        PSC_CHECK_EQ(result.Variants[0].Keywords.size(), size_t(1));
        return;
    }

    PSC_CHECK_EQ(result.Variants[0].Keywords[0], std::string("SKINNED"));
}

PSC_TEST(PropertiesCarryBothBackendBindings)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock,
        { MakeProperty("_Color", PropertyType::Color),
          MakeProperty("_MainTex", PropertyType::Texture2D, 0) });

    PSC_CHECK(result.Success);
    PSC_CHECK(Contains(result.VertexGlsl, "#if defined(PRISM_BACKEND_VULKAN)"));
    PSC_CHECK(Contains(result.VertexGlsl, "layout(std140, binding = 20)"));
    PSC_CHECK(Contains(result.VertexGlsl, "layout(std140, set = 3, binding = 0)"));
    PSC_CHECK(Contains(result.VertexGlsl, "uniform PrismMaterial"));
    PSC_CHECK(Contains(result.VertexGlsl, "vec4 _Color;"));
    PSC_CHECK(Contains(result.VertexGlsl, "PRISM_MATERIAL_TEXTURE_LAYOUT(12, 1) uniform sampler2D _MainTex;"));

    PSC_CHECK(Contains(result.FragmentGlsl, "uniform PrismMaterial"));
}

PSC_TEST(NoPropertiesMeansNoUniformBlock)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock);

    PSC_CHECK(result.Success);
    PSC_CHECK(!Contains(result.VertexGlsl, "uniform PrismMaterial"));
}

PSC_TEST(PropertyTextureSlotsOffsetTheBinding)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock,
        { MakeProperty("_A", PropertyType::Texture2D, 0),
          MakeProperty("_B", PropertyType::Texture2D, 1) });

    PSC_CHECK(result.Success);
    PSC_CHECK(Contains(result.VertexGlsl, "PRISM_MATERIAL_TEXTURE_LAYOUT(12, 1) uniform sampler2D _A;"));
    PSC_CHECK(Contains(result.VertexGlsl, "PRISM_MATERIAL_TEXTURE_LAYOUT(13, 2) uniform sampler2D _B;"));
}

PSC_TEST(SourcePositionsAreAnchoredWithLineDirectives)
{
    Harness harness;
    const InnerResult result = harness.Compile(kMinimalBlock);

    PSC_CHECK(result.Success);
    PSC_CHECK(Contains(result.VertexGlsl, "#line "));
    PSC_CHECK(Contains(result.VertexGlsl, "\"Assets/Test.Shader\""));
    PSC_CHECK(Contains(result.FragmentGlsl, "\"Assets/Test.Shader\""));
}

PSC_TEST(IncludeIsExpandedIntoBothStages)
{
    Harness harness;
    harness.Vfs["Assets/Include/Bits.glsl"] = "#pragma once\nfloat helper_value;\n";

    const std::string block =
        "#include \"Bits.glsl\"\n"
        "attribute vec3 a_Position : POSITION;\n"
        "void vert()\n"
        "{\n"
        "    gl_Position = vec4(a_Position, 1.0);\n"
        "}\n"
        "void frag()\n"
        "{\n"
        "    gl_FragColor = vec4(helper_value);\n"
        "}\n";

    const InnerResult result = harness.Compile(block);

    PSC_CHECK(result.Success);
    PSC_CHECK_EQ(CountOccurrences(result.VertexGlsl, "float helper_value;"), 1);
    PSC_CHECK_EQ(CountOccurrences(result.FragmentGlsl, "float helper_value;"), 1);
    PSC_CHECK(Contains(result.VertexGlsl, "\"Assets/Include/Bits.glsl\""));
}

PSC_TEST(MissingVertEntryReportsError)
{
    const std::string block =
        "attribute vec3 a_Position : POSITION;\n"
        "void frag()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    Harness harness;
    const InnerResult result = harness.Compile(block);

    PSC_CHECK(!result.Success);
    PSC_CHECK(harness.Diag.HasErrors());
}

PSC_TEST(UnknownAttributeSemanticReportsError)
{
    const std::string block =
        "attribute vec3 a_Position : NOT_A_SEMANTIC;\n"
        "void vert()\n"
        "{\n"
        "    gl_Position = vec4(a_Position, 1.0);\n"
        "}\n"
        "void frag()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    Harness harness;
    const InnerResult result = harness.Compile(block);

    PSC_CHECK(!result.Success);
    PSC_CHECK(harness.Diag.HasErrors());
}

PSC_TEST(ConditionalEntryPointReportsError)
{
    const std::string block =
        "#pragma shader_feature SKINNED\n"
        "attribute vec3 a_Position : POSITION;\n"
        "#ifdef SKINNED\n"
        "void vert()\n"
        "{\n"
        "    gl_Position = vec4(a_Position, 1.0);\n"
        "}\n"
        "#endif\n"
        "void frag()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    Harness harness;
    const InnerResult result = harness.Compile(block);

    PSC_CHECK(!result.Success);
    PSC_CHECK(harness.Diag.HasErrors());
}

PSC_TEST(EmptyGlslBlockReportsError)
{
    Harness harness;
    const InnerResult result = harness.Compile("");

    PSC_CHECK(!result.Success);
    PSC_CHECK(harness.Diag.HasErrors());
}
