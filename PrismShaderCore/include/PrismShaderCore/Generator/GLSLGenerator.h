#pragma once

#include "../Base.h"

#include "../PSL/Common.h"
#include <string>
#include <vector>

namespace PrismShaderCompiler
{

struct SpirvEntry
{
    std::vector<uint32_t> Vertex;
    std::vector<uint32_t> Fragment;
};

struct GlslOutput
{
    std::string Vertex;
    std::string Fragment;
};

GlslOutput PSC_API DecompileSPIRV(const SpirvEntry& spirv);

} // namespace PrismShaderCompiler
