#pragma once

#include "../PSL/Common.h"
#include "../PSL/GLSLType.h"

#include <string>
#include <vector>
#include <cstdint>

namespace PrismShaderCompiler::CSL
{

enum class ResourceKind : uint8_t
{
	StorageBuffer,        // layout(...) buffer { } name;
	UniformBuffer,        // layout(...) uniform { } name;

	Sampler2D,            // uniform sampler2D
	Sampler2DMS,          // uniform sampler2DMS
	Sampler2DShadow,      // uniform sampler2DShadow
	Sampler2DArray,       // uniform sampler2DArray
	Sampler2DArrayShadow, // uniform sampler2DArrayShadow
	Sampler3D,            // uniform sampler3D
	SamplerCube,          // uniform samplerCube
	SamplerCubeShadow,    // uniform samplerCubeShadow

	Image2D,              // uniform image2D
	Image3D,              // uniform image3D
	ImageCube,            // uniform imageCube
};

enum class ImageFormat : uint8_t
{
	Unknown,

	// float
	rgba32f, rgba16f,
	rg32f, rg16f,
	r32f, r16f,

	// signed int
	rgba32i, rgba16i, rgba8i,
	rg32i, rg16i, rg8i,
	r32i, r16i, r8i,

	// unsigned int
	rgba32ui, rgba16ui, rgba8ui,
	rg32ui, rg16ui, rg8ui,
	r32ui, r16ui, r8ui,

	// unorm / snorm
	rgba16, rgb10_a2, rgba8, rgba8_snorm,
	rg16, rg8, rg8_snorm,
	r16, r8, r8_snorm,

	// depth/stencil
	r16f_depth, r32f_depth,
};

struct ComputeResource
{
	ResourceKind Kind;
	GLSLType Type;
	ImageFormat Format = ImageFormat::Unknown;
	std::string Name;
	std::string BlockName;
	std::string InstanceName;
	uint32_t Set = 0;
	uint32_t Binding;
	bool ReadOnly = false;
	bool WriteOnly = false;
	SourceLocation Loc;
};

struct ComputeUniform
{
    GLSLType Type;
	std::string Name;
	SourceLocation Loc;
};

struct KernelDecl
{
	std::string Name;
	std::vector<std::string> VariantDefines;
	uint32_t InsertID = 0;
	SourceLocation Loc;
};

struct KernelDef
{
	std::string Name;
	uint32_t GroupSizeX, GroupSizeY, GroupSizeZ;
	std::string FunctionSource;
	uint32_t InsertID = 0;
	SourceLocation Loc;
};

struct ComputeDocument
{
	std::string ShaderName;
	int GlslVersion = 450;
	std::vector<KernelDecl> KernelDecls;
	std::vector<KernelDef> Kernels;
	std::vector<ComputeResource> Resources;
	std::vector<ComputeUniform> Uniforms;
	std::string SharedSource;
};

} // namespace PrismShaderCompiler::CSL
