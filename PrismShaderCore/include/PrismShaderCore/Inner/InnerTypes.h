#pragma once

#include "PPPreprocessor.h"
#include "../Property/PropertyType.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace PrismShaderCompiler
{

// 内层编译器的公开数据契约。
//
// 原料：Properties 声明 + GLSL 块代码 + 文件路径。
// 产出：顶点 GLSL + 片元 GLSL + 变体声明。
// 边界之外的东西（SubShader / Pass / Tags / 材质资产）内层一概不碰。
struct InnerConfig
{
    std::string SourcePath;                                     // .Shader 路径，供 #line 与 include 相对解析
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
    uint32_t TextureSlot = 0;                                   // 纹理类的槽位，非纹理类忽略
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
    std::vector<PPVariantDecl> Variants;                        // 每行 multi_compile / shader_feature
};

} // namespace PrismShaderCompiler
