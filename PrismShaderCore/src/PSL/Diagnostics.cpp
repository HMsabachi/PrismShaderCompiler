#include "PSL/Diagnostics.h"
#include "Log.h"

#include <format>

namespace PrismShaderCompiler
{

void DiagnosticCollector::Warning(const std::string& msg, const SourceLocation& loc,
                                   const std::string& snippet)
{
    m_Diagnostics.push_back({Severity::Warning, msg, snippet, loc});
}

void DiagnosticCollector::Error(const std::string& msg, const SourceLocation& loc,
                                 const std::string& snippet)
{
    m_Diagnostics.push_back({Severity::Error, msg, snippet, loc});
}

void DiagnosticCollector::Fatal(const std::string& msg, const SourceLocation& loc,
                                 const std::string& snippet)
{
    m_HasFatal = true;
    m_Diagnostics.push_back({Severity::Fatal, msg, snippet, loc});
}

bool DiagnosticCollector::HasErrors() const
{
    for (auto& d : m_Diagnostics)
        if (d.Level == Severity::Error || d.Level == Severity::Fatal)
            return true;
    return false;
}

bool DiagnosticCollector::HasWarnings() const
{
    for (auto& d : m_Diagnostics)
        if (d.Level == Severity::Warning)
            return true;
    return false;
}

void DiagnosticCollector::Clear()
{
    m_Diagnostics.clear();
    m_HasFatal = false;
}

void DiagnosticCollector::PrintAll() const
{
    auto& log = PrismShaderCompiler::Log::Instance();

    for (auto& d : m_Diagnostics)
    {
        std::string prefix;
        if (!d.Loc.FilePath.empty())
            prefix = std::format("{}:{}:{}: ", d.Loc.FilePath, d.Loc.Line, d.Loc.Column);
        else if (d.Loc.Line > 0)
            prefix = std::format("Line {}:{}: ", d.Loc.Line, d.Loc.Column);

        std::string msg = prefix + d.Message;
        if (!d.CodeSnippet.empty())
            msg += std::format("\n    --> '{}'", d.CodeSnippet);

        if (d.Level == Severity::Error || d.Level == Severity::Fatal)
            log.Error("{}", msg);
        else
            log.Warn("{}", msg);
    }
}

} // namespace PrismShaderCompiler
