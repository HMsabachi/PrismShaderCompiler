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

enum class StencilFunc : uint8_t
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

enum class StencilOp : uint8_t
{
    Keep,
    Zero,
    Replace,
    Incr,
    IncrWrap,
    Decr,
    DecrWrap,
    Invert,
};

enum class PolygonMode : uint8_t
{
    Fill,
    Line,
    Point,
};

struct PipelineState
{
    enum class Field : uint32_t
    {
        BlendEnabled       = 1u << 0,
        SrcFactor          = 1u << 1,
        DstFactor          = 1u << 2,
        SrcAlpha           = 1u << 3,
        DstAlpha           = 1u << 4,
        DepthTest          = 1u << 5,
        DepthWrite         = 1u << 6,
        DepthCompare       = 1u << 7,
        WriteMask          = 1u << 8,
        DepthBiasFactor    = 1u << 9,
        DepthBiasUnits     = 1u << 10,
        Cull               = 1u << 11,
        StencilTest        = 1u << 12,
        StencilCompare     = 1u << 13,
        StencilRef         = 1u << 14,
        StencilReadMask    = 1u << 15,
        StencilWriteMask   = 1u << 16,
        StencilFailOp      = 1u << 17,
        StencilDepthFailOp = 1u << 18,
        StencilPassOp      = 1u << 19,
        FillMode           = 1u << 20,
        LineWidth          = 1u << 21,
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

    bool StencilTest = false;
    StencilFunc StencilCompare = StencilFunc::Always;
    int32_t StencilRef = 0;
    uint32_t StencilReadMask = 0xFFFFFFFFu;
    uint32_t StencilWriteMask = 0xFFFFFFFFu;
    StencilOp StencilFailOp = StencilOp::Keep;
    StencilOp StencilDepthFailOp = StencilOp::Keep;
    StencilOp StencilPassOp = StencilOp::Keep;

    PolygonMode FillMode = PolygonMode::Fill;
    float LineWidth = 1.0f;

    uint32_t SetFlags = 0;

    void Mark(Field f) { SetFlags |= static_cast<uint32_t>(f); }
    bool IsSet(Field f) const { return (SetFlags & static_cast<uint32_t>(f)) != 0; }

    void Merge(const PipelineState& passState);
    static PipelineState Default();
};

} // namespace PrismShaderCompiler
