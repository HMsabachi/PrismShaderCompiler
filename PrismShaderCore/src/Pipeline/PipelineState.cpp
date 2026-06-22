#include "Pipeline/PipelineState.h"

namespace PrismShaderCompiler
{

PipelineState PipelineState::Default()
{
    return PipelineState{};
}

void PipelineState::Merge(const PipelineState& passState)
{
    if (passState.IsSet(Field::BlendEnabled))    BlendEnabled = passState.BlendEnabled;
    if (passState.IsSet(Field::SrcFactor))       SrcFactor = passState.SrcFactor;
    if (passState.IsSet(Field::DstFactor))       DstFactor = passState.DstFactor;
    if (passState.IsSet(Field::SrcAlpha))        SrcAlpha = passState.SrcAlpha;
    if (passState.IsSet(Field::DstAlpha))        DstAlpha = passState.DstAlpha;
    if (passState.IsSet(Field::DepthTest))       DepthTest = passState.DepthTest;
    if (passState.IsSet(Field::DepthWrite))      DepthWrite = passState.DepthWrite;
    if (passState.IsSet(Field::DepthCompare))    DepthCompare = passState.DepthCompare;
    if (passState.IsSet(Field::WriteMask))       WriteMask = passState.WriteMask;
    if (passState.IsSet(Field::DepthBiasFactor)) DepthBiasFactor = passState.DepthBiasFactor;
    if (passState.IsSet(Field::DepthBiasUnits))  DepthBiasUnits = passState.DepthBiasUnits;
    if (passState.IsSet(Field::Cull))            Cull = passState.Cull;

    SetFlags |= passState.SetFlags;
}

} // namespace PrismShaderCompiler
