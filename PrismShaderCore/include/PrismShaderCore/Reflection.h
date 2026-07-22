#pragma once

#include "Base.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PrismShaderCompiler
{

enum class DescriptorType : uint8_t
{
    UniformBuffer,
    StorageBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    CombinedImageSampler,
    SampledImage,
    StorageImage,
    Sampler,
    InputAttachment,
};

enum ShaderStageFlagBits : uint32_t
{
    ShaderStageVertexBit   = PSC_BIT(0),
    ShaderStageFragmentBit = PSC_BIT(1),
    ShaderStageComputeBit  = PSC_BIT(2),
};

struct ReflectedBinding
{
    uint32_t Set = 0;
    uint32_t Binding = 0;
    DescriptorType Type = DescriptorType::UniformBuffer;
    uint32_t StageFlags = 0;
    uint32_t Count = 1;
    std::string Name;
    std::string BlockName;
    uint32_t BufferSize = 0;
};

struct ReflectedPushConstant
{
    uint32_t Offset = 0;
    uint32_t Size = 0;
    uint32_t StageFlags = 0;
    std::string Name;
};

struct ShaderReflection
{
    std::vector<ReflectedBinding> Bindings;
    std::vector<ReflectedPushConstant> PushConstants;
};

} // namespace PrismShaderCompiler
