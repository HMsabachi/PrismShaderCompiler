#include "Generator/ComputeIRGenerator.h"
#include "Compiler.h"

namespace PrismShaderCompiler
{
namespace ComputeIRGen
{

    static CompilerConfig s_Config = {};

    void PSC_API SetConfig(const CompilerConfig& config)
    {
        s_Config = config;
    }

    static void ReplaceInsert(std::string& source, const std::string& replacement, uint32_t id)
    {
        std::string marker = "[Prism::Insert:" + std::to_string(id) + "]";
        size_t pos = source.find(marker);
        if (pos == std::string::npos) return;
        source.erase(pos, marker.size());
        source.insert(pos, replacement);
    }

    static std::string LineDirective(const SourceLocation& loc)
    {
        return "#line " + std::to_string(loc.Line) + " \"" + loc.FilePath + "\"\n";
    }

    Output PSC_API Generate(const CompiledComputeShader& shader, uint32_t kernelIndex)
    {
        Output out;

        std::string source = shader.SharedSource;

        for (auto& k : shader.Kernels)
        {
            ReplaceInsert(source, LineDirective(k.DeclAfterLoc), k.DeclInsertID);
        }

        for (uint32_t i = 0; i < shader.Kernels.size(); i++)
        {
            auto& k = shader.Kernels[i];
            if (i == kernelIndex)
            {
                std::string repl;
                repl += "layout(local_size_x = " + std::to_string(k.GroupSizeX)
                     + ", local_size_y = " + std::to_string(k.GroupSizeY)
                     + ", local_size_z = " + std::to_string(k.GroupSizeZ) + ") in;\n";
                repl += LineDirective(k.DefLoc);
                repl += "void main()\n{\n";
                repl += k.FunctionSource;
                repl += "}\n";
                repl += LineDirective(k.DefAfterLoc);
                ReplaceInsert(source, repl, k.DefInsertID);
            }
            else
            {
                ReplaceInsert(source, LineDirective(k.DefAfterLoc), k.DefInsertID);
            }
        }

        out.Source = "#version " + std::to_string(shader.GlslVersion) + " core\n";
        out.Source += LineDirective(shader.SharedStartLoc);
        out.Source += source;

        return out;
    }

    } // namespace ComputeIRGen
} // namespace PrismShaderCompiler
