#pragma once

#include "InnerTypes.h"
#include "../PSL/Common.h"

#include <cstdint>
#include <string>

namespace PrismShaderCompiler
{

struct CompiledComputeShader;
class DiagnosticCollector;

class ComputeRewriter
{
public:
    explicit ComputeRewriter(DiagnosticCollector& diag);

    bool Analyze(const ComputeInnerParams& params, CompiledComputeShader& out);

    std::string Emit(const ComputeInnerParams& params, uint32_t kernelIndex, TargetBackend backend);

private:
    DiagnosticCollector& m_Diag;
};

} // namespace PrismShaderCompiler
