#include "PSL/Token.h"
#include "PSL/SourceManager.h"

namespace PrismShaderCompiler
{

std::string_view Token::View(const SourceManager& sm) const
{
    return sm.GetView(Offset, Length);
}

const char* TokenTypeToString(TokenType type)
{
    switch (type)
    {
    case TokenType::EndOfFile:          return "EOF";
    case TokenType::Invalid:            return "Invalid";
    case TokenType::Identifier:         return "Identifier";
    case TokenType::StringLiteral:      return "StringLiteral";
    case TokenType::IntegerLiteral:     return "IntegerLiteral";
    case TokenType::FloatLiteral:       return "FloatLiteral";
    case TokenType::LeftBrace:          return "'{'";
    case TokenType::RightBrace:         return "'}'";
    case TokenType::LeftParen:          return "'('";
    case TokenType::RightParen:         return "')'";
    case TokenType::LeftBracket:        return "'['";
    case TokenType::RightBracket:       return "']'";
    case TokenType::Comma:              return "','";
    case TokenType::Dot:                return "'.'";
    case TokenType::Equals:             return "'='";
    case TokenType::Colon:              return "':'";
    case TokenType::Semicolon:           return "';'";
    case TokenType::Plus:               return "'+'";
    case TokenType::Minus:              return "'-'";
    case TokenType::Star:               return "'*'";
    case TokenType::Slash:              return "'/'";
    case TokenType::Percent:            return "'%'";
    case TokenType::Less:               return "'<'";
    case TokenType::Greater:            return "'>'";
    case TokenType::Not:                return "'!'";
    case TokenType::Question:           return "'?'";
    case TokenType::Ampersand:          return "'&'";
    case TokenType::Pipe:               return "'|'";
    case TokenType::Caret:              return "'^'";
    case TokenType::Tilde:              return "'~'";
    case TokenType::PlusPlus:           return "'++'";
    case TokenType::MinusMinus:         return "'--'";
    case TokenType::PlusEqual:          return "'+='";
    case TokenType::MinusEqual:         return "'-='";
    case TokenType::StarEqual:          return "'*='";
    case TokenType::SlashEqual:         return "'/='";
    case TokenType::PercentEqual:       return "'%='";
    case TokenType::LessEqual:          return "'<='";
    case TokenType::GreaterEqual:       return "'>='";
    case TokenType::EqualEqual:         return "'=='";
    case TokenType::NotEqual:           return "'!='";
    case TokenType::AndAnd:             return "'&&'";
    case TokenType::OrOr:               return "'||'";
    case TokenType::LeftShift:          return "'<<'";
    case TokenType::RightShift:         return "'>>'";
    case TokenType::AmpersandEqual:     return "'&='";
    case TokenType::PipeEqual:          return "'|='";
    case TokenType::CaretEqual:         return "'^='";
    case TokenType::ShaderKw:           return "'Shader'";
    case TokenType::PropertiesKw:       return "'Properties'";
    case TokenType::RenderCommandKw:    return "'RenderCommand'";
    case TokenType::LODKw:             return "'LOD'";
    case TokenType::SubShaderKw:        return "'SubShader'";
    case TokenType::PassKw:             return "'Pass'";
    case TokenType::UsePassKw:          return "'UsePass'";
    case TokenType::TagsKw:             return "'Tags'";
    case TokenType::NameKw:             return "'Name'";
    case TokenType::GLSLKw:             return "'GLSL'";
    case TokenType::BoolKw:             return "'Bool'";
    case TokenType::FloatKw:            return "'Float'";
    case TokenType::IntKw:              return "'Int'";
    case TokenType::ColorKw:            return "'Color'";
    case TokenType::Color3Kw:           return "'Color3'";
    case TokenType::Vector2Kw:          return "'Vector2'";
    case TokenType::Vector3Kw:          return "'Vector3'";
    case TokenType::Vector4Kw:          return "'Vector4'";
    case TokenType::Matrix3Kw:          return "'Matrix3'";
    case TokenType::Matrix4Kw:          return "'Matrix4'";
    case TokenType::Texture2DKw:        return "'Texture2D'";
    case TokenType::Texture2DMSKw:      return "'Texture2DMS'";
    case TokenType::TextureCubeKw:       return "'TextureCube'";
    case TokenType::RangeKw:            return "'Range'";
    case TokenType::EnumKw:             return "'Enum'";
    case TokenType::BlendKw:            return "'Blend'";
    case TokenType::ColorMaskKw:        return "'ColorMask'";
    case TokenType::CullKw:             return "'Cull'";
    case TokenType::OffsetKw:           return "'Offset'";
    case TokenType::ZTestKw:            return "'ZTest'";
    case TokenType::ZWriteKw:           return "'ZWrite'";
    case TokenType::OnKw:               return "'On'";
    case TokenType::OffKw:              return "'Off'";
    case TokenType::BackKw:             return "'Back'";
    case TokenType::FrontKw:            return "'Front'";
    case TokenType::NeverKw:            return "'Never'";
    case TokenType::LessKw:             return "'Less'";
    case TokenType::EqualKw:            return "'Equal'";
    case TokenType::LEqualKw:           return "'LEqual'";
    case TokenType::GreaterKw:          return "'Greater'";
    case TokenType::NotEqualKw:         return "'NotEqual'";
    case TokenType::GEqualKw:           return "'GEqual'";
    case TokenType::AlwaysKw:           return "'Always'";
    case TokenType::SrcAlphaKw:         return "'SrcAlpha'";
    case TokenType::OneMinusSrcAlphaKw: return "'OneMinusSrcAlpha'";
    case TokenType::DstAlphaKw:         return "'DstAlpha'";
    case TokenType::OneMinusDstAlphaKw: return "'OneMinusDstAlpha'";
    case TokenType::ZeroKw:             return "'Zero'";
    case TokenType::OneKw:              return "'One'";
    case TokenType::StencilKw:         return "'Stencil'";
    case TokenType::PolygonModeKw:      return "'PolygonMode'";
    case TokenType::LineWidthKw:         return "'LineWidth'";
    case TokenType::VertKw:             return "'vert'";
    case TokenType::FragKw:             return "'frag'";
    case TokenType::AttributeKw:        return "'attribute'";
    case TokenType::VaryingKw:          return "'VARYING'";
    case TokenType::LayoutKw:           return "'layout'";
    case TokenType::LocationKw:        return "'location'";
    case TokenType::InKw:               return "'in'";
    case TokenType::OutKw:              return "'out'";
    case TokenType::InOutKw:            return "'inout'";
    case TokenType::UniformKw:          return "'uniform'";
    case TokenType::VoidGLSLKw:               return "'void'";
    case TokenType::BoolGLSLKw:               return "'bool'";
    case TokenType::IntGLSLKw:                return "'int'";
    case TokenType::UIntGLSLKw:               return "'uint'";
    case TokenType::FloatGLSLKw:              return "'float'";
    case TokenType::DoubleGLSLKw:             return "'double'";
    case TokenType::BVec2GLSLKw:              return "'bvec2'";
    case TokenType::BVec3GLSLKw:              return "'bvec3'";
    case TokenType::BVec4GLSLKw:              return "'bvec4'";
    case TokenType::IVec2GLSLKw:              return "'ivec2'";
    case TokenType::IVec3GLSLKw:              return "'ivec3'";
    case TokenType::IVec4GLSLKw:              return "'ivec4'";
    case TokenType::UVec2GLSLKw:              return "'uvec2'";
    case TokenType::UVec3GLSLKw:              return "'uvec3'";
    case TokenType::UVec4GLSLKw:              return "'uvec4'";
    case TokenType::Vec2GLSLKw:               return "'vec2'";
    case TokenType::Vec3GLSLKw:               return "'vec3'";
    case TokenType::Vec4GLSLKw:               return "'vec4'";
    case TokenType::DVec2GLSLKw:              return "'dvec2'";
    case TokenType::DVec3GLSLKw:              return "'dvec3'";
    case TokenType::DVec4GLSLKw:              return "'dvec4'";
    case TokenType::Mat2GLSLKw:               return "'mat2'";
    case TokenType::Mat3GLSLKw:               return "'mat3'";
    case TokenType::Mat4GLSLKw:               return "'mat4'";
    case TokenType::Mat2x2GLSLKw:             return "'mat2x2'";
    case TokenType::Mat2x3GLSLKw:             return "'mat2x3'";
    case TokenType::Mat2x4GLSLKw:             return "'mat2x4'";
    case TokenType::Mat3x2GLSLKw:             return "'mat3x2'";
    case TokenType::Mat3x3GLSLKw:             return "'mat3x3'";
    case TokenType::Mat3x4GLSLKw:             return "'mat3x4'";
    case TokenType::Mat4x2GLSLKw:             return "'mat4x2'";
    case TokenType::Mat4x3GLSLKw:             return "'mat4x3'";
    case TokenType::Mat4x4GLSLKw:             return "'mat4x4'";
    case TokenType::Sampler2DGLSLKw:           return "'sampler2D'";
    case TokenType::Sampler3DGLSLKw:           return "'sampler3D'";
    case TokenType::SamplerCubeGLSLKw:          return "'samplerCube'";
    case TokenType::Sampler2DShadowGLSLKw:      return "'sampler2DShadow'";
    case TokenType::SamplerCubeShadowGLSLKw:    return "'samplerCubeShadow'";
    case TokenType::Sampler2DMSGLSLKw:           return "'sampler2DMS'";
    case TokenType::Sampler2DArrayGLSLKw:        return "'sampler2DArray'";
    case TokenType::Sampler2DArrayShadowGLSLKw:  return "'sampler2DArrayShadow'";
    case TokenType::Image2DGLSLKw:              return "'image2D'";
    case TokenType::Image3DGLSLKw:              return "'image3D'";
    case TokenType::ImageCubeGLSLKw:            return "'imageCube'";
    case TokenType::AtomicUIntGLSLKw:           return "'atomic_uint'";
    case TokenType::ShaderFeatureKw:    return "'shader_feature'";
    case TokenType::MultiCompileKw:     return "'multi_compile'";
    case TokenType::NumThreadsKw:       return "'numthreads'";
    case TokenType::PreprocessDirective: return "'#directive'";
    case TokenType::TrueKw:             return "'true'";
    case TokenType::FalseKw:            return "'false'";
    }
    return "Unknown";
}

} // namespace PrismShaderCompiler
