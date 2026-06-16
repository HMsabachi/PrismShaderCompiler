#include "Generator/GLSLGenerator.h"
#include <spirv_glsl.hpp>

namespace PrismShaderCompiler
{

static std::string Decompile(const std::vector<uint32_t>& spirv)
{
    if (spirv.empty()) return {};
    spirv_cross::CompilerGLSL compiler(spirv);

    spirv_cross::CompilerGLSL::Options opts;
    opts.version = 450;
    opts.es = false;
    compiler.set_common_options(opts);

    return compiler.compile();
}

GlslOutput PSC_API DecompileSPIRV(const SpirvEntry& spirv)
{
    GlslOutput out;
    out.Vertex   = Decompile(spirv.Vertex);
    out.Fragment = Decompile(spirv.Fragment);
    return out;
}

} // namespace PrismShaderCompiler
