#include "Inner/PPFile.h"

namespace PrismShaderCompiler
{

PPFileId PPFileTable::Add(std::string path, std::string canonicalPath, std::string source)
{
    // 不去重：一次内层编译最多读进十来个文件，而两个输出阶段各跑一遍预处理，
    // 各自的 token 携带各自的 FileId，共用一张表即可。
    const PPFileId id = static_cast<PPFileId>(m_Files.size());

    PPFile file;
    file.Path = std::move(path);
    file.CanonicalPath = std::move(canonicalPath);
    file.Source = std::move(source);

    m_Files.push_back(std::move(file));
    return id;
}

} // namespace PrismShaderCompiler
