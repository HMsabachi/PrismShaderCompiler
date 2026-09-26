#pragma once

#include "PPPreprocessor.h"
#include "../Property/PropertyType.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace PrismShaderCompiler
{

struct InnerConfig
{
    std::string SourcePath;
    std::string IncludeRoot = "Assets/Include";
    uint32_t GlslVersion = 450;

    std::string MaterialBlockName = "PrismMaterial";
    uint32_t OpenGLMaterialUniformBufferBinding = 20;
    uint32_t OpenGLTextureBeginBinding = 12;
    uint32_t VulkanMaterialUniformBufferSet = 3;
    uint32_t VulkanMaterialUniformBufferBinding = 0;
    uint32_t VulkanTextureBeginSet = 3;
    uint32_t VulkanTextureBeginBinding = 1;
};

struct InnerProperty
{
    std::string Name;
    PropertyType Type = PropertyType::Float;
    uint32_t TextureSlot = 0;
};

using InnerReadFileFn = std::function<bool(const std::string& path, std::string& out)>;

struct InnerParams
{
    std::string GlslBlock;
    std::vector<InnerProperty> Properties;
    InnerConfig Config;
    InnerReadFileFn ReadFile;
};

struct InnerResult
{
    bool Success = false;
    std::string VertexGlsl;
    std::string FragmentGlsl;
    std::vector<PPVariantDecl> Variants;
};

struct ComputeInnerParams
{
    std::string Source;
    InnerConfig Config;
    InnerReadFileFn ReadFile;
};

} // namespace PrismShaderCompiler
