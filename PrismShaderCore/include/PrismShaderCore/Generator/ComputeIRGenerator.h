#pragma once

#include "../Base.h"
#include "../CompilerCompute.h"

#include <string>

namespace PrismShaderCompiler
{

struct CompilerConfig;

namespace ComputeIRGen
{

struct Output
{
    std::string Source;
};

void PSC_API SetConfig(const CompilerConfig& config);

Output PSC_API Generate(const CompiledComputeShader& shader, uint32_t kernelIndex);

} // namespace ComputeIRGen
} // namespace PrismShaderCompiler
