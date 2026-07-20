#include "Pipeline/PipelineState.h"

namespace PrismShaderCompiler
{

PipelineState PipelineState::Default()
{
    return PipelineState{};
}

void PipelineState::Merge(const PipelineState& passState)
{
    if (passState.IsSet(Field::BlendEnabled))        BlendEnabled = passState.BlendEnabled;
    if (passState.IsSet(Field::SrcFactor))           SrcFactor = passState.SrcFactor;
    if (passState.IsSet(Field::DstFactor))           DstFactor = passState.DstFactor;
    if (passState.IsSet(Field::SrcAlpha))            SrcAlpha = passState.SrcAlpha;
    if (passState.IsSet(Field::DstAlpha))            DstAlpha = passState.DstAlpha;
    if (passState.IsSet(Field::DepthTest))           DepthTest = passState.DepthTest;
    if (passState.IsSet(Field::DepthWrite))          DepthWrite = passState.DepthWrite;
    if (passState.IsSet(Field::DepthCompare))        DepthCompare = passState.DepthCompare;
    if (passState.IsSet(Field::WriteMask))           WriteMask = passState.WriteMask;
    if (passState.IsSet(Field::DepthBiasFactor))     DepthBiasFactor = passState.DepthBiasFactor;
    if (passState.IsSet(Field::DepthBiasUnits))      DepthBiasUnits = passState.DepthBiasUnits;
    if (passState.IsSet(Field::Cull))                Cull = passState.Cull;
    if (passState.IsSet(Field::StencilTest))         StencilTest = passState.StencilTest;
    if (passState.IsSet(Field::StencilCompare))     StencilCompare = passState.StencilCompare;
    if (passState.IsSet(Field::StencilRef))          StencilRef = passState.StencilRef;
    if (passState.IsSet(Field::StencilReadMask))    StencilReadMask = passState.StencilReadMask;
    if (passState.IsSet(Field::StencilWriteMask))    StencilWriteMask = passState.StencilWriteMask;
    if (passState.IsSet(Field::StencilFailOp))      StencilFailOp = passState.StencilFailOp;
    if (passState.IsSet(Field::StencilDepthFailOp)) StencilDepthFailOp = passState.StencilDepthFailOp;
    if (passState.IsSet(Field::StencilPassOp))       StencilPassOp = passState.StencilPassOp;
    if (passState.IsSet(Field::FillMode))            FillMode = passState.FillMode;
    if (passState.IsSet(Field::LineWidth))           LineWidth = passState.LineWidth;

    SetFlags |= passState.SetFlags;
}

} // namespace PrismShaderCompiler
