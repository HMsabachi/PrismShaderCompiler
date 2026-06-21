#pragma once

#include <cstdint>
#include "Token.h"

namespace PrismShaderCompiler
{

enum class GLSLType : uint8_t
{
    None,

    Void,

    Bool,
    Int,
    UInt,
    Float,
    Double,

    BVec2,
    BVec3,
    BVec4,

    IVec2,
    IVec3,
    IVec4,

    UVec2,
    UVec3,
    UVec4,

    Vec2,
    Vec3,
    Vec4,

    DVec2,
    DVec3,
    DVec4,

    Mat2,
    Mat3,
    Mat4,

    Mat2x2,
    Mat2x3,
    Mat2x4,
    Mat3x2,
    Mat3x3,
    Mat3x4,
    Mat4x2,
    Mat4x3,
    Mat4x4,

    Sampler2D,
    Sampler2DMS,
    SamplerCube,
    Sampler2DShadow,
    SamplerCubeShadow,
    Sampler2DArray,
    Sampler2DArrayShadow,
    Sampler3D,

    Image2D,
    Image3D,
    ImageCube,

    AtomicUInt,
};

namespace GLSLTypeUtil
{

inline const char* ToString(GLSLType type)
{
    switch (type)
    {
    case GLSLType::Void:                return "void";
    case GLSLType::Bool:                return "bool";
    case GLSLType::Int:                 return "int";
    case GLSLType::UInt:                return "uint";
    case GLSLType::Float:               return "float";
    case GLSLType::Double:              return "double";
    case GLSLType::BVec2:               return "bvec2";
    case GLSLType::BVec3:               return "bvec3";
    case GLSLType::BVec4:               return "bvec4";
    case GLSLType::IVec2:               return "ivec2";
    case GLSLType::IVec3:               return "ivec3";
    case GLSLType::IVec4:               return "ivec4";
    case GLSLType::UVec2:               return "uvec2";
    case GLSLType::UVec3:               return "uvec3";
    case GLSLType::UVec4:               return "uvec4";
    case GLSLType::Vec2:                return "vec2";
    case GLSLType::Vec3:                return "vec3";
    case GLSLType::Vec4:                return "vec4";
    case GLSLType::DVec2:               return "dvec2";
    case GLSLType::DVec3:               return "dvec3";
    case GLSLType::DVec4:               return "dvec4";
    case GLSLType::Mat2:                return "mat2";
    case GLSLType::Mat3:                return "mat3";
    case GLSLType::Mat4:                return "mat4";
    case GLSLType::Mat2x2:              return "mat2x2";
    case GLSLType::Mat2x3:              return "mat2x3";
    case GLSLType::Mat2x4:              return "mat2x4";
    case GLSLType::Mat3x2:              return "mat3x2";
    case GLSLType::Mat3x3:              return "mat3x3";
    case GLSLType::Mat3x4:              return "mat3x4";
    case GLSLType::Mat4x2:              return "mat4x2";
    case GLSLType::Mat4x3:              return "mat4x3";
    case GLSLType::Mat4x4:              return "mat4x4";
    case GLSLType::Sampler2D:           return "sampler2D";
    case GLSLType::Sampler2DMS:         return "sampler2DMS";
    case GLSLType::SamplerCube:         return "samplerCube";
    case GLSLType::Sampler2DShadow:     return "sampler2DShadow";
    case GLSLType::SamplerCubeShadow:   return "samplerCubeShadow";
    case GLSLType::Sampler2DArray:      return "sampler2DArray";
    case GLSLType::Sampler2DArrayShadow:return "sampler2DArrayShadow";
    case GLSLType::Sampler3D:           return "sampler3D";
    case GLSLType::Image2D:             return "image2D";
    case GLSLType::Image3D:             return "image3D";
    case GLSLType::ImageCube:           return "imageCube";
    case GLSLType::AtomicUInt:          return "atomic_uint";
    default:                            return "unknown";
    }
}

inline bool IsSamplerType(GLSLType type)
{
    return type == GLSLType::Sampler2D
        || type == GLSLType::Sampler2DMS
        || type == GLSLType::SamplerCube
        || type == GLSLType::Sampler2DShadow
        || type == GLSLType::SamplerCubeShadow
        || type == GLSLType::Sampler2DArray
        || type == GLSLType::Sampler2DArrayShadow
        || type == GLSLType::Sampler3D;
}

inline bool IsMatrixType(GLSLType type)
{
    return type >= GLSLType::Mat2 && type <= GLSLType::Mat4x4;
}

inline bool IsImageType(GLSLType type)
{
    return type == GLSLType::Image2D
        || type == GLSLType::Image3D
        || type == GLSLType::ImageCube;
}

inline uint32_t LocationSlots(GLSLType type)
{
    switch (type)
    {
    case GLSLType::Mat2:    case GLSLType::Mat2x2:
    case GLSLType::Mat2x3:  case GLSLType::Mat2x4:   return 2;
    case GLSLType::Mat3:    case GLSLType::Mat3x2:
    case GLSLType::Mat3x3:  case GLSLType::Mat3x4:   return 3;
    case GLSLType::Mat4:    case GLSLType::Mat4x2:
    case GLSLType::Mat4x3:  case GLSLType::Mat4x4:   return 4;
    default:                                          return 1;
    }
}

inline GLSLType FromTokenType(TokenType t)
{
    switch (t)
    {
    case TokenType::VoidGLSLKw:              return GLSLType::Void;
    case TokenType::BoolGLSLKw:              return GLSLType::Bool;
    case TokenType::IntGLSLKw:               return GLSLType::Int;
    case TokenType::UIntGLSLKw:              return GLSLType::UInt;
    case TokenType::FloatGLSLKw:             return GLSLType::Float;
    case TokenType::DoubleGLSLKw:            return GLSLType::Double;
    case TokenType::BVec2GLSLKw:             return GLSLType::BVec2;
    case TokenType::BVec3GLSLKw:             return GLSLType::BVec3;
    case TokenType::BVec4GLSLKw:             return GLSLType::BVec4;
    case TokenType::IVec2GLSLKw:             return GLSLType::IVec2;
    case TokenType::IVec3GLSLKw:             return GLSLType::IVec3;
    case TokenType::IVec4GLSLKw:             return GLSLType::IVec4;
    case TokenType::UVec2GLSLKw:             return GLSLType::UVec2;
    case TokenType::UVec3GLSLKw:             return GLSLType::UVec3;
    case TokenType::UVec4GLSLKw:             return GLSLType::UVec4;
    case TokenType::Vec2GLSLKw:              return GLSLType::Vec2;
    case TokenType::Vec3GLSLKw:              return GLSLType::Vec3;
    case TokenType::Vec4GLSLKw:              return GLSLType::Vec4;
    case TokenType::DVec2GLSLKw:             return GLSLType::DVec2;
    case TokenType::DVec3GLSLKw:             return GLSLType::DVec3;
    case TokenType::DVec4GLSLKw:             return GLSLType::DVec4;
    case TokenType::Mat2GLSLKw:              return GLSLType::Mat2;
    case TokenType::Mat3GLSLKw:              return GLSLType::Mat3;
    case TokenType::Mat4GLSLKw:              return GLSLType::Mat4;
    case TokenType::Mat2x2GLSLKw:            return GLSLType::Mat2x2;
    case TokenType::Mat2x3GLSLKw:            return GLSLType::Mat2x3;
    case TokenType::Mat2x4GLSLKw:            return GLSLType::Mat2x4;
    case TokenType::Mat3x2GLSLKw:            return GLSLType::Mat3x2;
    case TokenType::Mat3x3GLSLKw:            return GLSLType::Mat3x3;
    case TokenType::Mat3x4GLSLKw:            return GLSLType::Mat3x4;
    case TokenType::Mat4x2GLSLKw:            return GLSLType::Mat4x2;
    case TokenType::Mat4x3GLSLKw:            return GLSLType::Mat4x3;
    case TokenType::Mat4x4GLSLKw:            return GLSLType::Mat4x4;
    case TokenType::Sampler2DGLSLKw:          return GLSLType::Sampler2D;
    case TokenType::Sampler3DGLSLKw:          return GLSLType::Sampler3D;
    case TokenType::SamplerCubeGLSLKw:         return GLSLType::SamplerCube;
    case TokenType::Sampler2DShadowGLSLKw:     return GLSLType::Sampler2DShadow;
    case TokenType::SamplerCubeShadowGLSLKw:   return GLSLType::SamplerCubeShadow;
    case TokenType::Sampler2DMSGLSLKw:          return GLSLType::Sampler2DMS;
    case TokenType::Sampler2DArrayGLSLKw:       return GLSLType::Sampler2DArray;
    case TokenType::Sampler2DArrayShadowGLSLKw: return GLSLType::Sampler2DArrayShadow;
    case TokenType::Image2DGLSLKw:             return GLSLType::Image2D;
    case TokenType::Image3DGLSLKw:             return GLSLType::Image3D;
    case TokenType::ImageCubeGLSLKw:           return GLSLType::ImageCube;
    case TokenType::AtomicUIntGLSLKw:          return GLSLType::AtomicUInt;
    default:                                   return GLSLType::None;
    }
}

inline bool IsTypeToken(TokenType t)
{
    return t == TokenType::Identifier
        || FromTokenType(t) != GLSLType::None;
}

} // namespace GLSLTypeUtil

} // namespace PrismShaderCompiler
