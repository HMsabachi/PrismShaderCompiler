#pragma once

#include "../Base.h"
#include <string>
#include <vector>
#include <cstdint>

namespace PrismShaderCompiler
{

enum class DescriptorKind : uint8_t
{
    UniformBuffer,
    StorageBuffer,
    Sampler,
    StorageImage,
    UniformTexelBuffer,
    StorageTexelBuffer
};

struct DescriptorInfo
{
    uint32_t Set = 0;
    uint32_t Binding = 0;
    uint32_t StageFlags = 0;
    uint32_t Size = 0;
    DescriptorKind Kind = DescriptorKind::UniformBuffer;
    char Name[47] = { 0 };
};

struct PassReflection
{
    std::vector<DescriptorInfo> Descriptors;
    uint32_t PushConstantSize = 0;
};

PassReflection PSC_API ReflectDescriptors(const std::vector<uint32_t>& vsSpirv,
    const std::vector<uint32_t>& fsSpirv);

PassReflection PSC_API ReflectCompute(const std::vector<uint32_t>& computeSpirv);

} // namespace PrismShaderCompiler
