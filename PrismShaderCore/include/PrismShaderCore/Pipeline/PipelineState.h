#pragma once

#include <cstdint>
#include <string>

namespace PrismShaderCompiler
{

enum class ColorMask : uint8_t
{
    RGBA,
    RGB,
    R,
    G,
    B,
    A,
    None,
};

enum class CullMode : uint8_t
{
    Off,
    Back,
    Front,
};

enum class DepthFunc : uint8_t
{
    Never,
    Less,
    Equal,
    LEqual,
    Greater,
    NotEqual,
    GEqual,
    Always,
};

enum class BlendFactor : uint8_t
{
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
};

struct PipelineState
{
    enum class Field : uint16_t
    {
        BlendEnabled    = 1u << 0,
        SrcFactor       = 1u << 1,
        DstFactor       = 1u << 2,
        SrcAlpha        = 1u << 3,
        DstAlpha        = 1u << 4,
        DepthTest       = 1u << 5,
        DepthWrite      = 1u << 6,
        DepthCompare    = 1u << 7,
        WriteMask       = 1u << 8,
        DepthBiasFactor = 1u << 9,
        DepthBiasUnits  = 1u << 10,
        Cull            = 1u << 11,
    };

    bool BlendEnabled = true;
    BlendFactor SrcFactor = BlendFactor::SrcAlpha;
    BlendFactor DstFactor = BlendFactor::OneMinusSrcAlpha;
    BlendFactor SrcAlpha = BlendFactor::One;
    BlendFactor DstAlpha = BlendFactor::OneMinusSrcAlpha;

    bool DepthTest = true;
    bool DepthWrite = true;
    DepthFunc DepthCompare = DepthFunc::LEqual;

    ColorMask WriteMask = ColorMask::RGBA;

    float DepthBiasFactor = 0;
    float DepthBiasUnits = 0;

    CullMode Cull = CullMode::Back;

    uint16_t SetFlags = 0;

    void Mark(Field f) { SetFlags |= static_cast<uint16_t>(f); }
    bool IsSet(Field f) const { return (SetFlags & static_cast<uint16_t>(f)) != 0; }

    void Merge(const PipelineState& passState);
    static PipelineState Default();
};

} // namespace PrismShaderCompiler
