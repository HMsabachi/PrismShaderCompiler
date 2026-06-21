#include "Generator/IRGenerator.h"
#include "Compiler.h"
#include "PSL/IncludeExpander.h"
#include "PSL/GLSLType.h"
#include "Property/PropertyLayout.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>

namespace PrismShaderCompiler::IRGen
{
    using namespace PrismShaderCompiler;
    static CompilerConfig s_Config = {};

    void PSC_API SetConfig(const CompilerConfig& config)
    {
        s_Config = config;
    }

    static void replaceInsert(std::string& source, const std::string& replacement, uint32_t id)
    {
        std::string marker = "[Prism::Insert:" + std::to_string(id) + "]";
        size_t pos = source.find(marker);
        if (pos == std::string::npos) return;
        source.erase(pos, marker.size());
        source.insert(pos, replacement);
    }

    static void GenerateAttribute(std::string& source, const AST::VertexAttribute& attr)
    {
        std::string line = "layout(location = " + std::to_string(PrismShaderCompiler::SemanticToLocation(attr.Semantic)) + ") in " + std::string(GLSLTypeUtil::ToString(attr.Type)) + " " + attr.Name + ";\n";
        line += "#line " + std::to_string(attr.Loc.Line) + " \"" + std::string(attr.Loc.FilePath) + "\"\n";
        replaceInsert(source, line, attr.InsertID);
    }

    static void GenerateInclude(std::string& source, const AST::IncludeDef& include)
    {
        std::string line = "#line 1 \"" + include.Path + "\"\n";
        line += ExpandIncludesRecursive(include.Path, s_Config.IncludeRoot);
        line += "#line " + std::to_string(include.Loc.Line) + " \"" + std::string(include.Loc.FilePath) + "\"\n";
        replaceInsert(source, line, include.InsertID);
    }

    static void GenerateEngineHeader(std::string& source)
    {
        for (auto& header : s_Config.EngineHeaders)
        {
            source += "#line 1 \"" + header + "\"\n";
            source += ExpandIncludesRecursive(header, s_Config.EngineRoot);
        }
    }

    static void GenerateDefines(std::string& source, const std::vector<std::string>& keywords)
    {
        for (const auto& kw : keywords)
            source += "#define " + kw + "\n";
    }

    static void GeneratePragma(std::string& source, const AST::PragmaDef& pragma)
    {
        std::string line = "#line " + std::to_string(pragma.Loc.Line) + " \"" + std::string(pragma.Loc.FilePath) + "\"\n";
        replaceInsert(source, line, pragma.InsertID);
    }

    struct FlatVar
    {
        std::string Name{};
        GLSLType Type{};
        uint32_t Location{};
        std::string UserExpr{};
    };

    static std::vector<FlatVar> FlattenVarying(const AST::VaryingBlock& v, uint32_t baseLoc)
    {
        std::vector<FlatVar> result;
        uint32_t loc = baseLoc;
        const std::string prefix = v.StructName + "_";

        for (const auto& m : v.Members)
        {
            bool isMat = GLSLTypeUtil::IsMatrixType(m.Type);
            GLSLType colType = GLSLTypeUtil::ColumnType(m.Type);
            uint32_t cols = isMat ? GLSLTypeUtil::LocationSlots(m.Type) : 1;
            uint32_t elems = m.ArraySize;
            for (uint32_t i = 0; i < elems; i++)
            {
                for (uint32_t c = 0; c < cols; c++)
                {
                    FlatVar fv;
                    fv.Type = colType;
                    fv.Location = loc++;
                    fv.Name = prefix + m.Name;
                    if (isMat)       fv.Name += "_col" + std::to_string(c);
                    if (elems > 1)   fv.Name += "_" + std::to_string(i);
                    fv.UserExpr = v.InstanceName + "." + m.Name;
                    if (elems > 1) fv.UserExpr += "[" + std::to_string(i) + "]";
                    if (isMat)     fv.UserExpr += "[" + std::to_string(c) + "]";
                    result.push_back(fv);
                }
            }
        }
        return result;
    }

    struct VaryingSync
    {
        std::string Declarations;   // layout(location=N) in/out ... 声明块
        std::string SyncCode;       // 赋值代码
    };
    static VaryingSync GenerateVaryingSync(const AST::VaryingBlock& v, bool isVertex, uint32_t baseLoc)
    {
        VaryingSync out;
        auto flats = FlattenVarying(v, baseLoc);
        const char* io = isVertex ? "out" : "in";

        for (const auto& fv : flats)
        {
            out.Declarations += "layout(location = " + std::to_string(fv.Location) + ") "
                              + io + " " + std::string(GLSLTypeUtil::ToString(fv.Type))
                              + " " + fv.Name + ";\n";
            if (isVertex)
                out.SyncCode += "    " + fv.Name + " = " + fv.UserExpr + ";\n";
            else
                out.SyncCode += "    " + fv.UserExpr + " = " + fv.Name + ";\n";
        }
        return out;
    }

    static void GenerateVarying(std::string& source, const AST::VaryingBlock& v)
    {
        std::string line;
        line += "struct " + v.StructName + "\n";
        line += "{\n";
        for (const auto& m : v.Members)
        {
            line += "    " + std::string(GLSLTypeUtil::ToString(m.Type)) + " " + m.Name;
            if (m.ArraySize > 1)
                line += "[" + std::to_string(m.ArraySize) + "]";
            line += ";\n";
        }
        line += "} " + v.InstanceName + ";\n";
        line += "#line " + std::to_string(v.Loc.Line) + " \"" + std::string(v.Loc.FilePath) + "\"\n";
        replaceInsert(source, line, v.InsertID);
    }

    static void GenerateVertexCode(std::string& source, const AST::GLSLCode& glsl)
    {
        std::string head = "// " + std::string(glsl.Loc.FilePath) + "\n";
        head += "#version " + std::to_string(s_Config.GlslVersion) + " core\n";

        std::optional<VaryingSync> vSync;
        if (glsl.Varying)
            vSync = GenerateVaryingSync(*glsl.Varying, true, 0);
        if (vSync) head += vSync->Declarations;

        source = head + source;
        for (const auto& attr : glsl.Attributes)
            GenerateAttribute(source, attr);
        if (glsl.Varying)
            GenerateVarying(source, *glsl.Varying);
        for (const auto& fragOut : glsl.FragmentOutputs)
        {
            std::string foLine = "#line " + std::to_string(fragOut.Loc.Line) + " \"" + std::string(fragOut.Loc.FilePath) + "\"\n";
            replaceInsert(source, foLine, fragOut.InsertID);
        }
        std::string line = "void main()\n";
        line += "{\n";
        line += "#line " + std::to_string(glsl.Vertex.LocBegin.Line) + " \"" + std::string(glsl.Vertex.LocBegin.FilePath) + "\"\n";
        line += glsl.Vertex.Source;
        line += "#line " + std::to_string(glsl.Vertex.LocEnd.Line) + " \"" + std::string(glsl.Vertex.LocEnd.FilePath) + "\"\n";
        if (vSync) line += vSync->SyncCode;
        line += "}\n";
        source += line;
    }

    static void GenerateFragmentCode(std::string& source, const AST::GLSLCode& glsl)
    {
        std::string head = "// " + std::string(glsl.Loc.FilePath) +"\n";
        head += "#version " + std::to_string(s_Config.GlslVersion) + " core\n";
        for (const auto& fragOut : glsl.FragmentOutputs)
            head += "layout(location = " + std::to_string(fragOut.Location) + ") out "
                  + std::string(GLSLTypeUtil::ToString(fragOut.Type)) + " " + fragOut.Name + ";\n";

        std::optional<VaryingSync> vSync;
        if (glsl.Varying)
            vSync = GenerateVaryingSync(*glsl.Varying, false, 0);
        if (vSync) head += vSync->Declarations;

        source = head + source;
        for (const auto& attr : glsl.Attributes)
        {
            std::string line = "#line " + std::to_string(attr.Loc.Line) + " \"" + std::string(attr.Loc.FilePath) + "\"\n";
            replaceInsert(source, line, attr.InsertID);
        }
        for (const auto& fragOut : glsl.FragmentOutputs)
        {
            std::string line = "#line " + std::to_string(fragOut.Loc.Line) + " \"" + std::string(fragOut.Loc.FilePath) + "\"\n";
            replaceInsert(source, line, fragOut.InsertID);
        }
        if (glsl.Varying)
            GenerateVarying(source, *glsl.Varying);
        std::string line = "void main()\n";
        line += "{\n";
        if (vSync) line += vSync->SyncCode;
        line += "#line " + std::to_string(glsl.Fragment.LocBegin.Line) + " \"" + std::string(glsl.Fragment.LocBegin.FilePath) + "\"\n";
        line += glsl.Fragment.Source;
        line += "#line " + std::to_string(glsl.Fragment.LocEnd.Line) + " \"" + std::string(glsl.Fragment.LocEnd.FilePath) + "\"\n";
        line += "}\n";
        source += line;
    }

    static void GeneratePropertyUniforms(std::string& source, const std::vector<AST::ShaderUniform>& uniforms)
    {
        std::string uboBody;
        for (const auto& uniform : uniforms)
        {
            if (PropertyTypeUtil::IsTextureType(uniform.Type))
            {
                source += "layout(binding = ";
                source += std::to_string(uniform.TextureSlot);
                source += ") ";
                source += PropertyTypeUtil::ToGLSLUniform(uniform.Type);
                source += " " + uniform.Name + ";\n";
            }
            else
            {
                uboBody += "    ";
                uboBody += PropertyTypeUtil::ToGLSLType(uniform.Type);
                uboBody += " " + uniform.Name + ";\n";
            }
        }
        if (!uboBody.empty())
        {
            source += "layout(std140, binding = " + std::to_string(s_Config.BindingMaterial)
                   + ") uniform " + s_Config.MaterialBlockName + "\n{\n";
            source += uboBody;
            source += "};\n\n";
        }
    }

    Output PSC_API Generate(const AST::GLSLCode& glsl, const std::vector<AST::ShaderUniform>& uniforms, const std::string& filePath, const std::vector<std::string>& activeKeywords)
    {
        Output output{};

        std::string sharedSource;
        GenerateEngineHeader(sharedSource);
        GenerateDefines(sharedSource, activeKeywords);
        GeneratePropertyUniforms(sharedSource, uniforms);
        sharedSource += "#line " + std::to_string(glsl.Loc.Line) + " \"" + std::string(glsl.Loc.FilePath) + "\"\n";
        sharedSource += glsl.SharedSource;
        for (const auto& pragma : glsl.Pragmas)
            GeneratePragma(sharedSource, pragma);
        for (const auto& include : glsl.Includes)
            GenerateInclude(sharedSource, include);

        output.Vertex = sharedSource;
        output.Fragment = sharedSource;
        GenerateVertexCode(output.Vertex, glsl);
        GenerateFragmentCode(output.Fragment, glsl);
        return output;
    }

} // namespace PrismShaderCompiler
