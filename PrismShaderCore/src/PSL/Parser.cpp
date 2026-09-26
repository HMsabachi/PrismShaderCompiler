#include "PSL/Parser.h"
#include "Log.h"

namespace PrismShaderCompiler
{

Parser::Parser(TokenStream& stream, DiagnosticCollector* diag)
    : m_Stream(stream), m_Diag(diag)
{
}

AST::ShaderDocument Parser::ParseShader()
{
    AST::ShaderDocument doc;

    Consume(TokenType::ShaderKw, "期望 'Shader' 关键字");
    doc.ShaderName = TokenStr(Consume(TokenType::StringLiteral, "期望 Shader 名称字符串"));
    Consume(TokenType::LeftBrace, "期望 '{'");

    while (!IsAtEnd() && !Check(TokenType::RightBrace))
    {
        if (Check(TokenType::PropertiesKw))
        {
            Advance();
            Consume(TokenType::LeftBrace, "期望 '{'");
            ParseProperties(doc.Uniforms);
            Consume(TokenType::RightBrace, "期望 '}'");
        }
        else if (Check(TokenType::RenderCommandKw))
        {
            Advance();
            Consume(TokenType::LeftBrace, "期望 '{'");
            doc.RenderState = ParseRenderCommand();
            Consume(TokenType::RightBrace, "期望 '}'");
        }
        else if (Check(TokenType::SubShaderKw))
        {
            Advance();
            Consume(TokenType::LeftBrace, "期望 '{'");

            while (!IsAtEnd() && !Check(TokenType::RightBrace))
            {
                if (Check(TokenType::LODKw))
                {
                    Advance();
                    doc.LOD = TokenInt(ConsumeNumber("期望 LOD 数值"));
                }
                else if (Check(TokenType::TagsKw))
                {
                    Advance();
                    Consume(TokenType::LeftBrace, "期望 '{'");
                    ParseTags(doc.Tags);
                    Consume(TokenType::RightBrace, "期望 '}'");
                }
                else if (Check(TokenType::PassKw))
                {
                    Advance();
                    Consume(TokenType::LeftBrace, "期望 '{'");
                    AST::PassDef pass;
                    ParsePass(pass);
                    doc.Passes.push_back(std::move(pass));
                    Consume(TokenType::RightBrace, "期望 '}'");
                }
                else if (Check(TokenType::UsePassKw))
                {
                    Advance();
                    AST::UsePassDef usePass;
                    usePass.Loc = CurrentLoc();
                    usePass.ShaderName = TokenStr(Consume(TokenType::StringLiteral, "期望 Shader 名称"));
                    usePass.PassName = TokenStr(Consume(TokenType::StringLiteral, "期望 Pass 名称"));
                    doc.UsePasses.push_back(std::move(usePass));
                }
                else if (Check(TokenType::RenderCommandKw))
                {
                    // SubShader
                    Advance();
                    Consume(TokenType::LeftBrace, "期望 '{'");
                    doc.RenderState = ParseRenderCommand();
                    Consume(TokenType::RightBrace, "期望 '}'");
                }
                else
                {
                    Error("SubShader 内期望 'LOD', 'Tags', 'Pass', 'UsePass' 或 'RenderCommand'");
                    Advance();
                }
            }
            Consume(TokenType::RightBrace, "期望 '}'");
        }
        else
        {
            Error("期望 'Properties', 'RenderCommand' 或 'SubShader'");
            Advance();
        }
    }

    Consume(TokenType::RightBrace, "期望 '}'");

    ParseMaterialLayout(doc);
    return doc;
}


Token& Parser::Current()           { return m_Stream.Current(); }
Token Parser::PeekToken(int off)   { return m_Stream.PeekToken(off); }
Token Parser::Advance()            { return m_Stream.Advance(); }
bool Parser::Check(TokenType t)    { return m_Stream.Check(t); }
bool Parser::Match(TokenType t)    { return m_Stream.Match(t); }
Token Parser::Consume(TokenType type, const std::string& errMsg)
{
    if (Check(type)) return Advance();
    Error(errMsg);
    return Current();
}

Token Parser::ConsumeNumber(const std::string& errMsg)
{
    if (Check(TokenType::FloatLiteral) || Check(TokenType::IntegerLiteral))
        return Advance();
    Error(errMsg);
    return Current();
}

bool Parser::IsAtEnd()             { return m_Stream.IsAtEnd(); }

SourceLocation Parser::CurrentLoc()
{
    return m_Stream.GetSM().GetLocation(Current().Offset);
}

void Parser::Error(const std::string& msg)
{
    auto loc = CurrentLoc();
    uint32_t tokLen = Current().Length;
    std::string lineText(m_Stream.GetSM().GetLineText(Current().Offset));
    if (m_Diag) m_Diag->Error(msg, loc, lineText);
    auto& log = PrismShaderCompiler::Log::Instance();
    log.Error("{}", FormatDiagnostic(Severity::Error, loc, msg, lineText, tokLen));
}

// Token 文本取值
std::string_view Parser::TokenText(const Token& t) const
{
    return m_Stream.GetSM().GetView(t.Offset, t.Length);
}

std::string Parser::TokenStr(const Token& t) const
{
    return std::string(TokenText(t));
}

float Parser::TokenFloat(const Token& t) const
{
    auto sv = TokenText(t);
    char buf[64];
    uint32_t n = sv.size() < 63 ? (uint32_t)sv.size() : 63;
    std::memcpy(buf, sv.data(), n);
    buf[n] = '\0';
    return (float)std::atof(buf);
}

int Parser::TokenInt(const Token& t) const
{
    auto sv = TokenText(t);
    char buf[64];
    uint32_t n = sv.size() < 63 ? (uint32_t)sv.size() : 63;
    std::memcpy(buf, sv.data(), n);
    buf[n] = '\0';
    int base = 10;
    const char* p = buf;
    if (*p == '-' || *p == '+') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) base = 16;
    return (int)std::strtol(buf, nullptr, base);
}

void Parser::ParseProperties(std::vector<AST::ShaderUniform>& uniforms)
{
    while (!IsAtEnd() && !Check(TokenType::RightBrace))
    {
        if (Check(TokenType::Identifier))
        {
            uniforms.push_back(ParseProperty());
        }
        else
        {
            Error("期望属性名");
            Advance();
        }
    }
}

AST::ShaderUniform Parser::ParseProperty()
{
    AST::ShaderUniform uniform;
    uniform.Name = TokenStr(Consume(TokenType::Identifier, "期望属性名"));

    Consume(TokenType::LeftParen, "期望 '('");
    uniform.DisplayName = TokenStr(Consume(TokenType::StringLiteral, "期望显示名称"));
    Consume(TokenType::Comma, "期望 ','");
    ParsePropertyType(uniform);
    Consume(TokenType::RightParen, "期望 ')'");

    Consume(TokenType::Equals, "期望 '='");
    uniform.DefaultValue = ParseDefaultValue(uniform.Type);

    return uniform;
}

void Parser::ParsePropertyType(AST::ShaderUniform& uniform)
{
    static const std::unordered_map<TokenType, PropertyType> kTypeMap = {
        {TokenType::BoolKw,        PropertyType::Bool},
        {TokenType::FloatKw,       PropertyType::Float},
        {TokenType::IntKw,         PropertyType::Int},
        {TokenType::ColorKw,       PropertyType::Color},
        {TokenType::Color3Kw,      PropertyType::Color3},
        {TokenType::Vector2Kw,     PropertyType::Vector2},
        {TokenType::Vector3Kw,     PropertyType::Vector3},
        {TokenType::Vector4Kw,     PropertyType::Vector4},
        {TokenType::Matrix3Kw,     PropertyType::Matrix3},
        {TokenType::Matrix4Kw,     PropertyType::Matrix4},
        {TokenType::Texture2DKw,   PropertyType::Texture2D},
        {TokenType::Texture2DMSKw, PropertyType::Texture2DMS},
        {TokenType::TextureCubeKw,  PropertyType::TextureCube},
    };

    auto it = kTypeMap.find(Current().Type);
    if (it != kTypeMap.end())
    {
        Advance();
        uniform.Type = it->second;
        return;
    }

    if (Check(TokenType::RangeKw))
    {
        Advance();
        Consume(TokenType::LeftParen, "Range 期望 '('");
        uniform.RangeMin = TokenFloat(ConsumeNumber("期望数值"));
        Consume(TokenType::Comma, "期望 ','");
        uniform.RangeMax = TokenFloat(ConsumeNumber("期望数值"));
        Consume(TokenType::RightParen, "期望 ')'");
        uniform.Type = PropertyType::Range;
        return;
    }

    if (Check(TokenType::EnumKw))
    {
        Advance();
        Consume(TokenType::LeftParen, "Enum 期望 '('");
        uniform.EnumOptions.push_back(TokenStr(Consume(TokenType::Identifier, "期望选项")));
        while (Check(TokenType::Comma))
        {
            Advance();
            uniform.EnumOptions.push_back(TokenStr(Consume(TokenType::Identifier, "期望选项")));
        }
        Consume(TokenType::RightParen, "期望 ')'");
        uniform.Type = PropertyType::Enum;
        return;
    }

    Error("未知属性类型");
}

std::vector<Scalar> Parser::ParseDefaultValue(PropertyType type)
{
    // 元组: (x, y, z, w) — 用于 Color/Vector/Color3
    auto ParseTuple = [this]() -> std::vector<Scalar> {
        std::vector<Scalar> scalars;
        Consume(TokenType::LeftParen, "期望 '('");
        scalars.push_back(Scalar::FromFloat(TokenFloat(ConsumeNumber("期望数值"))));
        while (Check(TokenType::Comma))
        {
            Advance();
            scalars.push_back(Scalar::FromFloat(TokenFloat(ConsumeNumber("期望数值"))));
        }
        Consume(TokenType::RightParen, "期望 ')'");
        return scalars;
    };

    switch (type)
    {
    case PropertyType::Bool:
        return {Scalar::FromBool(Consume(TokenType::TrueKw, "期望 true/false").Is(TokenType::TrueKw))};
    case PropertyType::Float:
    case PropertyType::Range:
        return {Scalar::FromFloat(TokenFloat(Advance()))};
    case PropertyType::Int:
    case PropertyType::Enum:
        return {Scalar::FromInt(TokenInt(Advance()))};
    case PropertyType::Color:
    case PropertyType::Color3:
    case PropertyType::Vector2:
    case PropertyType::Vector3:
    case PropertyType::Vector4:
        return ParseTuple();
    case PropertyType::Matrix3:
    case PropertyType::Matrix4:
        return {Scalar::FromFloat(TokenFloat(Advance()))};
    case PropertyType::Texture2D:
    case PropertyType::Texture2DMS:
    case PropertyType::TextureCube:
        if (Check(TokenType::StringLiteral))
        {
            Advance(); Advance(); Advance();
            return {};
        }
        Consume(TokenType::LeftBrace, "期望 '{}'");
        Consume(TokenType::RightBrace, "期望 '}'");
        return {};
    default:
        return {};
    }
}

void Parser::ParseMaterialLayout(AST::ShaderDocument& doc)
{
    doc.MaterialLayout = PropertyLayout{};
    uint32_t nextTexSlot = 0;

    for (auto& uniform : doc.Uniforms)
    {
        if (PropertyTypeUtil::IsTextureType(uniform.Type))
        {
            uniform.TextureSlot = nextTexSlot++;
        }
        else
        {
            doc.MaterialLayout.Add(uniform.Name, uniform.Type);
            const auto* member = doc.MaterialLayout.Find(uniform.Name);
            uniform.BufferOffset = member->Offset;
            uniform.BufferSize = member->Size;
        }
    }
}

PipelineState Parser::ParseRenderCommand()
{
    PipelineState state = PipelineState::Default();

    while (!IsAtEnd() && !Check(TokenType::RightBrace))
    {
        if (Check(TokenType::BlendKw))
        {
            Advance();
            if (Check(TokenType::OffKw))
            {
                Advance(); state.BlendEnabled = false;
                state.Mark(PipelineState::Field::BlendEnabled);
            }
            else
            {
                state.BlendEnabled = true;
                state.Mark(PipelineState::Field::BlendEnabled);
                // SrcFactor DstFactor [SrcAlpha DstAlpha]
                if (Check(TokenType::SrcAlphaKw))     { Advance(); state.SrcFactor = BlendFactor::SrcAlpha; }
                else if (Check(TokenType::OneKw))      { Advance(); state.SrcFactor = BlendFactor::One; }
                else if (Check(TokenType::ZeroKw))     { Advance(); state.SrcFactor = BlendFactor::Zero; }
                else if (Check(TokenType::DstAlphaKw)) { Advance(); state.SrcFactor = BlendFactor::DstAlpha; }
                else if (Check(TokenType::OneMinusSrcAlphaKw)) { Advance(); state.SrcFactor = BlendFactor::OneMinusSrcAlpha; }
                else if (Check(TokenType::OneMinusDstAlphaKw)) { Advance(); state.SrcFactor = BlendFactor::OneMinusDstAlpha; }
                state.Mark(PipelineState::Field::SrcFactor);

                if (Check(TokenType::SrcAlphaKw))     { Advance(); state.DstFactor = BlendFactor::SrcAlpha; }
                else if (Check(TokenType::OneKw))      { Advance(); state.DstFactor = BlendFactor::One; }
                else if (Check(TokenType::ZeroKw))     { Advance(); state.DstFactor = BlendFactor::Zero; }
                else if (Check(TokenType::DstAlphaKw)) { Advance(); state.DstFactor = BlendFactor::DstAlpha; }
                else if (Check(TokenType::OneMinusSrcAlphaKw)) { Advance(); state.DstFactor = BlendFactor::OneMinusSrcAlpha; }
                else if (Check(TokenType::OneMinusDstAlphaKw)) { Advance(); state.DstFactor = BlendFactor::OneMinusDstAlpha; }
                state.Mark(PipelineState::Field::DstFactor);

                // 可选的独立 alpha blend 参数
                if (!Check(TokenType::RightBrace) && !Check(TokenType::CullKw)
                    && !Check(TokenType::ZTestKw) && !Check(TokenType::ZWriteKw) && !Check(TokenType::BlendKw)
                    && !Check(TokenType::ColorMaskKw) && !Check(TokenType::OffsetKw)
                    && !Check(TokenType::StencilKw) && !Check(TokenType::PolygonModeKw) && !Check(TokenType::LineWidthKw))
                {
                    if (Check(TokenType::SrcAlphaKw))     { Advance(); state.SrcAlpha = BlendFactor::SrcAlpha; }
                    else if (Check(TokenType::OneKw))      { Advance(); state.SrcAlpha = BlendFactor::One; }
                    else if (Check(TokenType::ZeroKw))     { Advance(); state.SrcAlpha = BlendFactor::Zero; }
                    else if (Check(TokenType::DstAlphaKw)) { Advance(); state.SrcAlpha = BlendFactor::DstAlpha; }
                    else if (Check(TokenType::OneMinusSrcAlphaKw)) { Advance(); state.SrcAlpha = BlendFactor::OneMinusSrcAlpha; }
                    else if (Check(TokenType::OneMinusDstAlphaKw)) { Advance(); state.SrcAlpha = BlendFactor::OneMinusDstAlpha; }
                    state.Mark(PipelineState::Field::SrcAlpha);

                    if (Check(TokenType::SrcAlphaKw))     { Advance(); state.DstAlpha = BlendFactor::SrcAlpha; }
                    else if (Check(TokenType::OneKw))      { Advance(); state.DstAlpha = BlendFactor::One; }
                    else if (Check(TokenType::ZeroKw))     { Advance(); state.DstAlpha = BlendFactor::Zero; }
                    else if (Check(TokenType::DstAlphaKw)) { Advance(); state.DstAlpha = BlendFactor::DstAlpha; }
                    else if (Check(TokenType::OneMinusSrcAlphaKw)) { Advance(); state.DstAlpha = BlendFactor::OneMinusSrcAlpha; }
                    else if (Check(TokenType::OneMinusDstAlphaKw)) { Advance(); state.DstAlpha = BlendFactor::OneMinusDstAlpha; }
                    state.Mark(PipelineState::Field::DstAlpha);
                }
            }
        }
        else if (Check(TokenType::CullKw))
        {
            Advance();
            if (Check(TokenType::OffKw))    { Advance(); state.Cull = CullMode::Off; }
            else if (Check(TokenType::BackKw))  { Advance(); state.Cull = CullMode::Back; }
            else if (Check(TokenType::FrontKw)) { Advance(); state.Cull = CullMode::Front; }
            state.Mark(PipelineState::Field::Cull);
        }
        else if (Check(TokenType::ZTestKw))
        {
            Advance();
            if (Check(TokenType::OffKw))
            {
                Advance(); state.DepthTest = false;
                state.Mark(PipelineState::Field::DepthTest);
            }
            else
            {
                state.DepthTest = true;
                if (Check(TokenType::NeverKw))     { Advance(); state.DepthCompare = DepthFunc::Never; }
                else if (Check(TokenType::LessKw))      { Advance(); state.DepthCompare = DepthFunc::Less; }
                else if (Check(TokenType::EqualKw))     { Advance(); state.DepthCompare = DepthFunc::Equal; }
                else if (Check(TokenType::LEqualKw))    { Advance(); state.DepthCompare = DepthFunc::LEqual; }
                else if (Check(TokenType::GreaterKw))   { Advance(); state.DepthCompare = DepthFunc::Greater; }
                else if (Check(TokenType::NotEqualKw))  { Advance(); state.DepthCompare = DepthFunc::NotEqual; }
                else if (Check(TokenType::GEqualKw))   { Advance(); state.DepthCompare = DepthFunc::GEqual; }
                else if (Check(TokenType::AlwaysKw))    { Advance(); state.DepthCompare = DepthFunc::Always; }
                state.Mark(PipelineState::Field::DepthTest);
                state.Mark(PipelineState::Field::DepthCompare);
            }
        }
        else if (Check(TokenType::ZWriteKw))
        {
            Advance();
            if (Check(TokenType::OnKw)) { Advance(); state.DepthWrite = true; }
            else { Advance(); state.DepthWrite = false; }
            state.Mark(PipelineState::Field::DepthWrite);
        }
        else if (Check(TokenType::ColorMaskKw))
        {
            Advance();
            Token t = Advance();
            std::string v = TokenStr(t);
            if (v == "RGBA")           state.WriteMask = ColorMask::RGBA;
            else if (v == "RGB")       state.WriteMask = ColorMask::RGB;
            else if (v == "R")         state.WriteMask = ColorMask::R;
            else if (v == "G")         state.WriteMask = ColorMask::G;
            else if (v == "B")         state.WriteMask = ColorMask::B;
            else if (v == "A")         state.WriteMask = ColorMask::A;
            else if (v == "0")         state.WriteMask = ColorMask::None;
            else                       Error("非法的 ColorMask 值: " + v);
            state.Mark(PipelineState::Field::WriteMask);
        }
        else if (Check(TokenType::OffsetKw))
        {
            Advance();
            state.DepthBiasFactor = TokenFloat(ConsumeNumber("期望 factor"));
            Match(TokenType::Comma);
            state.DepthBiasUnits = TokenFloat(ConsumeNumber("期望 units"));
            state.Mark(PipelineState::Field::DepthBiasFactor);
            state.Mark(PipelineState::Field::DepthBiasUnits);
        }
        else if (Check(TokenType::StencilKw))
        {
            Advance();
            Consume(TokenType::LeftBrace, "期望 '{' 开始 Stencil 块");
            while (!IsAtEnd() && !Check(TokenType::RightBrace))
            {
                std::string key = TokenStr(Advance());
                if (key == "Ref")
                {
                    state.StencilRef = TokenInt(ConsumeNumber("期望 Stencil Ref 值"));
                    state.Mark(PipelineState::Field::StencilRef);
                }
                else if (key == "ReadMask")
                {
                    state.StencilReadMask = (uint32_t)TokenInt(ConsumeNumber("期望 Stencil ReadMask 值"));
                    state.Mark(PipelineState::Field::StencilReadMask);
                }
                else if (key == "WriteMask")
                {
                    state.StencilWriteMask = (uint32_t)TokenInt(ConsumeNumber("期望 Stencil WriteMask 值"));
                    state.Mark(PipelineState::Field::StencilWriteMask);
                }
                else if (key == "Comp")
                {
                    state.StencilCompare = ParseStencilFunc();
                    state.StencilTest = true;
                    state.Mark(PipelineState::Field::StencilTest);
                    state.Mark(PipelineState::Field::StencilCompare);
                }
                else if (key == "Pass")
                {
                    state.StencilPassOp = ParseStencilOp();
                    state.StencilTest = true;
                    state.Mark(PipelineState::Field::StencilTest);
                    state.Mark(PipelineState::Field::StencilPassOp);
                }
                else if (key == "Fail")
                {
                    state.StencilFailOp = ParseStencilOp();
                    state.StencilTest = true;
                    state.Mark(PipelineState::Field::StencilTest);
                    state.Mark(PipelineState::Field::StencilFailOp);
                }
                else if (key == "ZFail")
                {
                    state.StencilDepthFailOp = ParseStencilOp();
                    state.StencilTest = true;
                    state.Mark(PipelineState::Field::StencilTest);
                    state.Mark(PipelineState::Field::StencilDepthFailOp);
                }
                else
                    Error("未知的 Stencil 子命令: " + key);
            }
            Consume(TokenType::RightBrace, "期望 '}' 结束 Stencil 块");
        }
        else if (Check(TokenType::PolygonModeKw))
        {
            Advance();
            std::string v = TokenStr(Advance());
            if (v == "Fill")       state.FillMode = PolygonMode::Fill;
            else if (v == "Line")   state.FillMode = PolygonMode::Line;
            else if (v == "Point") state.FillMode = PolygonMode::Point;
            else                   Error("非法的 PolygonMode 值: " + v);
            state.Mark(PipelineState::Field::FillMode);
        }
        else if (Check(TokenType::LineWidthKw))
        {
            Advance();
            state.LineWidth = TokenFloat(ConsumeNumber("期望 LineWidth 值"));
            state.Mark(PipelineState::Field::LineWidth);
        }
        else
            Advance();
    }

    return state;
}

StencilFunc Parser::ParseStencilFunc()
{
    std::string v = TokenStr(Advance());
    if (v == "Never")    return StencilFunc::Never;
    if (v == "Less")     return StencilFunc::Less;
    if (v == "Equal")    return StencilFunc::Equal;
    if (v == "LEqual")   return StencilFunc::LEqual;
    if (v == "Greater")  return StencilFunc::Greater;
    if (v == "NotEqual") return StencilFunc::NotEqual;
    if (v == "GEqual")   return StencilFunc::GEqual;
    if (v == "Always")   return StencilFunc::Always;
    Error("非法的 Stencil Comp 值: " + v);
    return StencilFunc::Always;
}

StencilOp Parser::ParseStencilOp()
{
    std::string v = TokenStr(Advance());
    if (v == "Keep")      return StencilOp::Keep;
    if (v == "Zero")      return StencilOp::Zero;
    if (v == "Replace")   return StencilOp::Replace;
    if (v == "Incr")      return StencilOp::Incr;
    if (v == "IncrWrap")  return StencilOp::IncrWrap;
    if (v == "Decr")      return StencilOp::Decr;
    if (v == "DecrWrap")  return StencilOp::DecrWrap;
    if (v == "Invert")    return StencilOp::Invert;
    Error("非法的 Stencil Op 值: " + v);
    return StencilOp::Keep;
}

void Parser::ParsePass(AST::PassDef& pass)
{
    while (!IsAtEnd() && !Check(TokenType::RightBrace))
    {
        if (Check(TokenType::NameKw))
        {
            Advance();
            pass.Name = TokenStr(Consume(TokenType::StringLiteral, "期望 Pass 名称"));
        }
        else if (Check(TokenType::TagsKw))
        {
            Advance();
            Consume(TokenType::LeftBrace, "期望 '{'");
            ParseTags(pass.Tags);
            Consume(TokenType::RightBrace, "期望 '}'");
        }
        else if (Check(TokenType::RenderCommandKw))
        {
            Advance();
            Consume(TokenType::LeftBrace, "期望 '{'");
            pass.RenderState = ParseRenderCommand();
            Consume(TokenType::RightBrace, "期望 '}'");
        }
        else if (Check(TokenType::GLSLKw))
        {
            Advance();
            Consume(TokenType::LeftBrace, "期望 '{'");
            ParseGLSLBlock(pass.Glsl);
            Consume(TokenType::RightBrace, "期望 '}'");
        }
        else
        {
            Error("Pass 内期望 'Name', 'Tags', 'RenderCommand' 或 'GLSL'");
            Advance();
        }
    }
}

void Parser::ParseTags(std::unordered_map<std::string, std::string>& tags)
{
    while (!IsAtEnd() && !Check(TokenType::RightBrace))
    {
        std::string key = TokenStr(Consume(TokenType::StringLiteral, "期望 Tag 键"));
        Consume(TokenType::Equals, "期望 '='");
        std::string value = TokenStr(Consume(TokenType::StringLiteral, "期望 Tag 值"));
        tags[key] = value;
    }
}

void Parser::ParseGLSLBlock(AST::GLSLCode& glsl)
{
    glsl.Loc = CurrentLoc();
    const uint32_t rawStart = Current().Offset;

    int depth = 1;

    while (!IsAtEnd() && depth > 0)
    {
        if (Check(TokenType::PreprocessDirective))
        {
            ParseGLSLDirective(glsl);
            continue;
        }

        if (Check(TokenType::LeftBrace))
        {
            Advance();
            ++depth;
        }
        else if (Check(TokenType::RightBrace))
        {
            --depth;

            if (depth == 0)
            {
                const uint32_t rawEnd = Current().Offset;

                if (rawEnd > rawStart)
                    glsl.RawSource = std::string(m_Stream.GetSM().GetView(rawStart, rawEnd - rawStart));

                break;
            }

            Advance();
        }
        else
        {
            Advance();
        }
    }
}

void Parser::ParseGLSLDirective(AST::GLSLCode& glsl)
{
    const std::string dir = TokenStr(Advance());

    if (dir != "#pragma")
        return;

    AST::PragmaDef pragma;
    pragma.Loc = CurrentLoc();

    if (Check(TokenType::ShaderFeatureKw))
    {
        Advance();
        pragma.IsShaderFeature = true;
    }
    else if (Check(TokenType::MultiCompileKw))
    {
        Advance();
        pragma.IsMultiCompile = true;
    }
    else
    {
        return;
    }

    const uint32_t pragmaLine = CurrentLoc().Line;

    while (Check(TokenType::Identifier) && CurrentLoc().Line == pragmaLine)
        pragma.Keywords.push_back(TokenStr(Advance()));

    if (!pragma.Keywords.empty())
        glsl.Pragmas.push_back(std::move(pragma));
}

} // namespace PrismShaderCompiler
