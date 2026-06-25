#pragma once

#include <cstdint>
#include <string_view>
#include <string>

namespace PrismShaderCompiler
{

class SourceManager;

enum class TokenType : uint8_t
{
    EndOfFile,
    Invalid,

    // Literals
    Identifier,
    StringLiteral,
    IntegerLiteral,
    FloatLiteral,

    // Punctuation
    LeftBrace,          // '{'
    RightBrace,         // '}'
    LeftParen,          // '('
    RightParen,         // ')'
    LeftBracket,        // '['
    RightBracket,       // ']'
    Comma,              // ','
    Dot,                // '.'
    Equals,             // '='
    Colon,              // ':'
    Semicolon,          // ';'

    PreprocessDirective, // '#include' / '#pragma' 等预处理指令

    // GLSL operators — 单字符
    Plus,               // '+'
    Minus,              // '-'
    Star,               // '*'
    Slash,              // '/'
    Percent,            // '%'
    Less,               // '<'
    Greater,            // '>'
    Not,                // '!'
    Question,           // '?'
    Ampersand,          // '&'
    Pipe,               // '|'
    Caret,              // '^'
    Tilde,              // '~'

    // GLSL operators — 复合
    PlusPlus,           // '++'
    MinusMinus,         // '--'
    PlusEqual,          // '+='
    MinusEqual,         // '-='
    StarEqual,          // '*='
    SlashEqual,         // '/='
    PercentEqual,       // '%='
    LessEqual,          // '<='
    GreaterEqual,       // '>='
    EqualEqual,         // '=='
    NotEqual,           // '!='
    AndAnd,             // '&&'
    OrOr,               // '||'
    LeftShift,          // '<<'
    RightShift,         // '>>'
    AmpersandEqual,     // '&='
    PipeEqual,          // '|='
    CaretEqual,         // '^='

    // PSL structural keywords
    ShaderKw,
    PropertiesKw,
    RenderCommandKw,
    LODKw,
    SubShaderKw,
    PassKw,
    TagsKw,
    NameKw,
    GLSLKw,
    UsePassKw,

    // Property type keywords
    BoolKw,
    FloatKw,
    IntKw,
    ColorKw,
    Color3Kw,
    Vector2Kw,
    Vector3Kw,
    Vector4Kw,
    Matrix3Kw,
    Matrix4Kw,
    Texture2DKw,
    Texture2DMSKw,
    TextureCubeKw,
    RangeKw,
    EnumKw,

    // Render command keywords
    BlendKw,
    ColorMaskKw,
    CullKw,
    OffsetKw,
    ZTestKw,
    ZWriteKw,
    OnKw,
    OffKw,
    BackKw,
    FrontKw,
    NeverKw,
    LessKw,
    EqualKw,
    LEqualKw,
    GreaterKw,
    NotEqualKw,
    GEqualKw,
    AlwaysKw,
    SrcAlphaKw,
    OneMinusSrcAlphaKw,
    DstAlphaKw,
    OneMinusDstAlphaKw,
    ZeroKw,
    OneKw,

    // Shader entry points
    VertKw,
    FragKw,

    // GLSL keywords
    AttributeKw,
    VaryingKw,
    LayoutKw,
    LocationKw,
    InKw,
    OutKw,
    InOutKw,
    UniformKw,

    // GLSL types — scalars
    VoidGLSLKw,
    BoolGLSLKw,
    IntGLSLKw,
    UIntGLSLKw,
    FloatGLSLKw,
    DoubleGLSLKw,

    // GLSL types — vectors
    BVec2GLSLKw,
    BVec3GLSLKw,
    BVec4GLSLKw,
    IVec2GLSLKw,
    IVec3GLSLKw,
    IVec4GLSLKw,
    UVec2GLSLKw,
    UVec3GLSLKw,
    UVec4GLSLKw,
    Vec2GLSLKw,
    Vec3GLSLKw,
    Vec4GLSLKw,
    DVec2GLSLKw,
    DVec3GLSLKw,
    DVec4GLSLKw,

    // GLSL types — matrices
    Mat2GLSLKw,
    Mat3GLSLKw,
    Mat4GLSLKw,
    Mat2x2GLSLKw,
    Mat2x3GLSLKw,
    Mat2x4GLSLKw,
    Mat3x2GLSLKw,
    Mat3x3GLSLKw,
    Mat3x4GLSLKw,
    Mat4x2GLSLKw,
    Mat4x3GLSLKw,
    Mat4x4GLSLKw,

    // GLSL types — samplers
    Sampler2DGLSLKw,
    Sampler3DGLSLKw,
    SamplerCubeGLSLKw,
    Sampler2DShadowGLSLKw,
    SamplerCubeShadowGLSLKw,
    Sampler2DMSGLSLKw,
    Sampler2DArrayGLSLKw,
    Sampler2DArrayShadowGLSLKw,

    // GLSL types — images
    Image2DGLSLKw,
    Image3DGLSLKw,
    ImageCubeGLSLKw,

    // GLSL types — atomic
    AtomicUIntGLSLKw,

    ShaderFeatureKw,
    MultiCompileKw,

    TrueKw,
    FalseKw,
};

struct Token
{
    TokenType Type = TokenType::Invalid;
    uint32_t Offset = 0;   
    uint32_t Length = 0;   

    bool Is(TokenType t) const { return Type == t; }
    bool IsNot(TokenType t) const { return Type != t; }

    // 取值（需要 SourceManager 提供源文本）
    std::string_view View(const SourceManager& sm) const;
    std::string ToString(const SourceManager& sm) const { return std::string(View(sm)); }
};

const char* TokenTypeToString(TokenType type);

} // namespace PrismShaderCompiler
