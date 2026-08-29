#include "Generator/ReflectionGenerator.h"

#include <spirv_cross.hpp>
#include <algorithm>
#include <unordered_map>

namespace PrismShaderCompiler
{

    namespace
    {
        uint32_t ExecutionModelToStageFlag(spv::ExecutionModel model)
        {
            switch (model)
            {
            case spv::ExecutionModelVertex:    return 0x00000001;
            case spv::ExecutionModelFragment:  return 0x00000010;
            case spv::ExecutionModelGLCompute: return 0x00000020;
            default:                           return 0;
            }
        }

        void ReflectStage(const std::vector<uint32_t>& spirv, uint32_t stageFlag,
            std::unordered_map<uint64_t, DescriptorInfo>& merged)
        {
            if (spirv.empty())
                return;

            spirv_cross::Compiler compiler(spirv);
            auto stageFlagFinal = stageFlag;
            if (stageFlagFinal == 0)
            {
                auto entryPoints = compiler.get_entry_points_and_stages();
                if (!entryPoints.empty())
                    stageFlagFinal = ExecutionModelToStageFlag(entryPoints.front().execution_model);
            }

            auto addResources = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                DescriptorKind kind)
            {
                for (const auto& res : resources)
                {
                    uint32_t set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                    uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                    uint64_t key = (uint64_t(set) << 32) | uint32_t(binding);

                    auto it = merged.find(key);
                    if (it == merged.end())
                    {
                        DescriptorInfo info;
                        info.Set = set;
                        info.Binding = binding;
                        info.Kind = kind;
                        info.StageFlags = stageFlagFinal;
                        strncpy(info.Name, res.name.c_str(), sizeof(info.Name) - 1);
                        if (info.Kind == DescriptorKind::UniformBuffer ||
                            info.Kind == DescriptorKind::StorageBuffer)
                        {
                            const auto& type = compiler.get_type_from_variable(res.id);
                            info.Size = (uint32_t)compiler.get_declared_struct_size(type);
                        }
                        merged.emplace(key, info);
                    }
                    else
                    {
                        it->second.StageFlags |= stageFlagFinal;
                    }
                }
            };

            auto resources = compiler.get_shader_resources();
            addResources(resources.uniform_buffers, DescriptorKind::UniformBuffer);
            addResources(resources.storage_buffers, DescriptorKind::StorageBuffer);
            addResources(resources.separate_images, DescriptorKind::Sampler);
            addResources(resources.separate_samplers, DescriptorKind::Sampler);
            addResources(resources.storage_images, DescriptorKind::StorageImage);
            addResources(resources.sampled_images, DescriptorKind::Sampler);
        }
    }

    static PassReflection FinalizeReflection(std::unordered_map<uint64_t, DescriptorInfo>& merged)
    {
        PassReflection result;
        result.Descriptors.reserve(merged.size());
        for (auto& [_, info] : merged)
            result.Descriptors.push_back(std::move(info));

        std::sort(result.Descriptors.begin(), result.Descriptors.end(),
            [](const DescriptorInfo& a, const DescriptorInfo& b)
            {
                if (a.Set != b.Set) return a.Set < b.Set;
                return a.Binding < b.Binding;
            });

        return result;
    }

    PassReflection PSC_API ReflectDescriptors(const std::vector<uint32_t>& vsSpirv,
        const std::vector<uint32_t>& fsSpirv)
    {
        std::unordered_map<uint64_t, DescriptorInfo> merged;

        ReflectStage(vsSpirv, 0x00000001, merged);
        ReflectStage(fsSpirv, 0x00000004, merged);

        return FinalizeReflection(merged);
    }

    PassReflection PSC_API ReflectCompute(const std::vector<uint32_t>& computeSpirv)
    {
        std::unordered_map<uint64_t, DescriptorInfo> merged;

        ReflectStage(computeSpirv, 0x00000020, merged);

        return FinalizeReflection(merged);
    }

} // namespace PrismShaderCompiler
