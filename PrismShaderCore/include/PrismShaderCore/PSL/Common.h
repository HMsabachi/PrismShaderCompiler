#pragma once

#include <cstdint>
#include <string>

namespace PrismShaderCompiler
{

    struct SourceLocation
    {
        uint32_t Line = 0;
        uint32_t Column = 0;
        uint32_t Offset = 0;
        std::string FilePath;
    };

    enum class ShaderStageType : uint8_t
    {
        Vertex,
        Fragment,
        Compute,
    };

} // namespace PrismShaderCompiler
