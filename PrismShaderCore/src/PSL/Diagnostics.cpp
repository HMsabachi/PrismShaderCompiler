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
        std::string msg = FormatDiagnostic(d.Level, d.Loc, d.Message, d.CodeSnippet, 0);
        if (d.Level == Severity::Error || d.Level == Severity::Fatal)
            log.Error("{}", msg);
        else
            log.Warn("{}", msg);
    }
}

std::string FormatDiagnostic(Severity level, const SourceLocation& loc,
                             const std::string& msg, const std::string& lineText,
                             uint32_t indicatorLen)
{
    static constexpr std::string_view kSev[] = { "warning", "error", "fatal" };
    auto sevStr = kSev[static_cast<size_t>(level)];

    std::string out;
    if (!loc.FilePath.empty())
        out = std::format("{}:{}:{}: {}: {}", loc.FilePath, loc.Line, loc.Column, sevStr, msg);
    else if (loc.Line > 0)
        out = std::format("Line {}:{}: {}: {}", loc.Line, loc.Column, sevStr, msg);
    else
        out = std::format("{}: {}", sevStr, msg);

    if (!lineText.empty())
    {
        std::string lineNum = std::to_string(loc.Line);
        std::string pad(lineNum.size(), ' ');
        out += std::format("\n  {} | {}", lineNum, lineText);
        uint32_t carets = indicatorLen > 0 ? indicatorLen : 1;
        std::string pointer(loc.Column > 0 ? loc.Column - 1 : 0, ' ');
        pointer += std::string(carets, '^');
        out += std::format("\n  {} | {}", pad, pointer);
    }
    return out;
}

} // namespace PrismShaderCompiler
