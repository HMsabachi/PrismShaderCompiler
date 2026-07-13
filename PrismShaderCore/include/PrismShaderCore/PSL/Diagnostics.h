#pragma once

#include "Common.h"

#include <string>
#include <vector>

namespace PrismShaderCompiler
{

enum class Severity
{
    Warning,
    Error,
    Fatal,
};

struct Diagnostic
{
    Severity Level = Severity::Error;
    std::string Message;
    std::string CodeSnippet;
    SourceLocation Loc;
};

class DiagnosticCollector
{
public:
    void Warning(const std::string& msg, const SourceLocation& loc = {},
                 const std::string& snippet = "");
    void Error(const std::string& msg, const SourceLocation& loc = {},
               const std::string& snippet = "");
    void Fatal(const std::string& msg, const SourceLocation& loc = {},
               const std::string& snippet = "");

    bool HasErrors() const;
    bool HasWarnings() const;

    const std::vector<Diagnostic>& GetDiagnostics() const { return m_Diagnostics; }
    void Clear();

    void PrintAll() const;

private:
    std::vector<Diagnostic> m_Diagnostics;
    bool m_HasFatal = false;
};

std::string FormatDiagnostic(Severity level, const SourceLocation& loc,
                             const std::string& msg, const std::string& lineText,
                             uint32_t indicatorLen = 0);

} // namespace PrismShaderCompiler
