#include "Generator/MSLGenerator.h"
#include <spirv_msl.hpp>

namespace PrismShaderCompiler
{

std::string PSC_API DecompileMSL(const std::vector<uint32_t>& spirv)
{
    if (spirv.empty()) return {};
    spirv_cross::CompilerMSL compiler(spirv);

    return compiler.compile();
}

} // namespace PrismShaderCompiler
