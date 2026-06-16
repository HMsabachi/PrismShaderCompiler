#pragma once

#include "../Callback.h"
#include <string>

namespace PrismShaderCompiler
{

std::string ExpandIncludesRecursive(const std::string& filePath,
                                    const std::string& includeRoot,
                                    PrismShaderCompiler::ReadFileCallback readFile = PrismShaderCompiler::Callbacks::ReadFileFromDisk);

} // namespace PrismShaderCompiler
