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
    DescriptorKind Kind = DescriptorKind::UniformBuffer;
    uint32_t StageFlags = 0;
    std::string Name;
    uint32_t Size = 0;
};

struct PassReflection
{
    std::vector<DescriptorInfo> Descriptors;
};

PassReflection PSC_API ReflectDescriptors(const std::vector<uint32_t>& vsSpirv,
    const std::vector<uint32_t>& fsSpirv);

} // namespace PrismShaderCompiler
