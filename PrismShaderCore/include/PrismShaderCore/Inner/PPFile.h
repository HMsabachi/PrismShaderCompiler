#pragma once

#include "PPToken.h"
#include "../PSL/Common.h"

#include <string>
#include <vector>

namespace PrismShaderCompiler
{

// 内层位置 -> 诊断位置。文件路径由调用方在需要时补上 —— PPFileTable 在阶段 4 起才可用。
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
    std::string Path;           // 书写路径，进 #line 与诊断
    std::string CanonicalPath;  // 规范路径，供 include 去重
    std::string Source;
};

class PPFileTable
{
public:
    PPFileId Add(std::string path, std::string canonicalPath, std::string source);

    const PPFile& Get(PPFileId id) const { return m_Files[id]; }
    const std::string& GetSource(PPFileId id) const { return m_Files[id].Source; }
    const std::string& GetPath(PPFileId id) const { return m_Files[id].Path; }

    // 源缓冲在整次编译期间必须稳定，逐字回写依赖它
    bool IsValid(PPFileId id) const { return id < m_Files.size(); }

private:
    std::vector<PPFile> m_Files;
};

} // namespace PrismShaderCompiler
