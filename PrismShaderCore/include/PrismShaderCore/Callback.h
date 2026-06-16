#pragma once

#include <string>
#include <functional>
#include <fstream>

namespace PrismShaderCompiler
{

using ReadFileCallback = std::function<std::string(const std::string& path)>;

namespace Callbacks
{
    inline std::string ReadFileFromDisk(const std::string& path)
    {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return {};
        std::string s(static_cast<size_t>(f.tellg()), '\0');
        f.seekg(0);
        f.read(s.data(), s.size());
        return s;
    }
}

} // namespace PrismShaderCompiler
