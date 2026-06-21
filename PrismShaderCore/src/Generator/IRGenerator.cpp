#include "Generator/IRGenerator.h"
#include "Compiler.h"
#include "PSL/IncludeExpander.h"
#include "PSL/GLSLType.h"
#include "Property/PropertyLayout.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>

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

    uint32_t VaryingLocationSlots(const AST::VaryingBlock& varying)
    {
        if (varying.IsStruct)
        {
            uint32_t slots = 0;
            for (const auto& member : varying.Members)
                slots += GLSLTypeUtil::LocationSlots(member.Type) * member.ArraySize;
            return slots * varying.ArraySize;
        }
        return GLSLTypeUtil::LocationSlots(varying.Type) * varying.ArraySize;
    }

    static void GenerateVarying(std::string& source, const AST::VaryingBlock& varying,
                                 const bool isVertex, uint32_t location)
    {
        std::string prefix = "layout(location = ";
        prefix += std::to_string(location) + (isVertex ? ") out " : ") in ");

        std::string line;
        if (varying.IsStruct)
        {
            line += prefix + varying.StructName + "\n{\n";
            for (const auto& member : varying.Members)
            {
                line += "    " + std::string(GLSLTypeUtil::ToString(member.Type)) + " " + member.Name;
                if (member.ArraySize > 1)
                    line += "[" + std::to_string(member.ArraySize) + "]";
                line += ";\n";
            }
            line += "}" + varying.InstanceName;
            if (varying.ArraySize > 1)
                line += "[" + std::to_string(varying.ArraySize) + "]";
            line += ";\n";
        }
        else
        {
            line += prefix + std::string(GLSLTypeUtil::ToString(varying.Type)) + " " + varying.InstanceName;
            if (varying.ArraySize > 1)
                line += "[" + std::to_string(varying.ArraySize) + "]";
            line += ";\n";
        }
        line += "#line " + std::to_string(varying.Loc.Line) + " \"" + std::string(varying.Loc.FilePath) + "\"\n";
        replaceInsert(source, line, varying.InsertID);
    }

    static void GenerateVertexCode(std::string& source, const AST::GLSLCode& glsl)
    {
        std::string head = "// " + std::string(glsl.Loc.FilePath) + "\n";
        head += "#version " + std::to_string(s_Config.GlslVersion) + " core\n";
        source = head + source;
        for (const auto& attr : glsl.Attributes)
            GenerateAttribute(source, attr);
        uint32_t vtxLoc = 0;
        for (const auto& varying : glsl.Varyings)
        {
            GenerateVarying(source, varying, true, vtxLoc);
            vtxLoc += VaryingLocationSlots(varying);
        }
        std::string line = "#line " + std::to_string(glsl.Vertex.Loc.Line - 1) + " \"" + std::string(glsl.Vertex.Loc.FilePath) + "\"\n";
        for (const auto& fragOut : glsl.FragmentOutputs)
        {
            std::string foLine = "#line " + std::to_string(fragOut.Loc.Line) + " \"" + std::string(fragOut.Loc.FilePath) + "\"\n";
            replaceInsert(source, foLine, fragOut.InsertID);
        }
        line += "void main()\n";
        line += glsl.Vertex.Source + "\n";
        source += line;
    }

    static void GenerateFragmentCode(std::string& source, const AST::GLSLCode& glsl)
    {
        std::string head = "// " + std::string(glsl.Loc.FilePath) +"\n";
        head += "#version " + std::to_string(s_Config.GlslVersion) + " core\n";
        for (const auto& fragOut : glsl.FragmentOutputs)
            head += "layout(location = " + std::to_string(fragOut.Location) + ") out "
                  + std::string(GLSLTypeUtil::ToString(fragOut.Type)) + " " + fragOut.Name + ";\n";
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
        uint32_t fragLoc = 0;
        for (const auto& varying : glsl.Varyings)
        {
            GenerateVarying(source, varying, false, fragLoc);
            fragLoc += VaryingLocationSlots(varying);
        }
        std::string line = "#line " + std::to_string(glsl.Fragment.Loc.Line - 1) + " \"" + std::string(glsl.Fragment.Loc.FilePath) + "\"\n";
        line += "void main()\n";
        line += glsl.Fragment.Source + "\n";
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
