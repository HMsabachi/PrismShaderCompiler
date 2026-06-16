#include "Generator/HLSLGenerator.h"
#include <spirv_hlsl.hpp>

namespace PrismShaderCompiler
{

std::string PSC_API DecompileHLSL(const std::vector<uint32_t>& spirv)
{
    if (spirv.empty()) return {};
    spirv_cross::CompilerHLSL compiler(spirv);

    spirv_cross::CompilerHLSL::Options opts;
    opts.shader_model = 60;
    compiler.set_hlsl_options(opts);

    return compiler.compile();
}

} // namespace PrismShaderCompiler
