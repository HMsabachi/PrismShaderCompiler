#include "Inner/PPFile.h"

namespace PrismShaderCompiler
{

PPFileId PPFileTable::Add(std::string path, std::string canonicalPath, std::string source)
{
    const PPFileId id = static_cast<PPFileId>(m_Files.size());

    PPFile file;
    file.Path = std::move(path);
    file.CanonicalPath = std::move(canonicalPath);
    file.Source = std::move(source);

    m_Files.push_back(std::move(file));
    return id;
}

} // namespace PrismShaderCompiler
