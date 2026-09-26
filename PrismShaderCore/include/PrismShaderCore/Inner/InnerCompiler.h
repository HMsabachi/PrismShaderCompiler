#pragma once

#include "InnerTypes.h"
#include "PPFile.h"

namespace PrismShaderCompiler
{

class DiagnosticCollector;

class InnerCompiler
{
public:
    explicit InnerCompiler(DiagnosticCollector& diag);

    InnerResult Compile(const InnerParams& params);

private:
    PPFileTable m_Files;
    DiagnosticCollector& m_Diag;
};

} // namespace PrismShaderCompiler
