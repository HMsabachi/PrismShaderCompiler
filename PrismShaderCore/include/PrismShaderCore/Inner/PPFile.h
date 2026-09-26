#pragma once

#include "PPToken.h"
#include "../PSL/Common.h"

#include <string>
#include <vector>

namespace PrismShaderCompiler
{

inline SourceLocation ToDiagnosticLocation(const PPSourceLoc& loc, const std::string& path = std::string())
{
    SourceLocation out;
    out.Line = loc.Line;
    out.Column = loc.Column;
    out.FilePath = path;
    return out;
}


struct PPFile
{
    std::string Path;
    std::string CanonicalPath;
    std::string Source;
};

class PPFileTable
{
public:
    PPFileId Add(std::string path, std::string canonicalPath, std::string source);

    const PPFile& Get(PPFileId id) const { return m_Files[id]; }
    const std::string& GetSource(PPFileId id) const { return m_Files[id].Source; }
    const std::string& GetPath(PPFileId id) const { return m_Files[id].Path; }

    bool IsValid(PPFileId id) const { return id < m_Files.size(); }

private:
    std::vector<PPFile> m_Files;
};

} // namespace PrismShaderCompiler
