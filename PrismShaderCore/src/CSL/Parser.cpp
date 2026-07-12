#include "CSL/Parser.h"
#include "PSL/GLSLType.h"
#include "Log.h"

namespace PrismShaderCompiler::CSL
{

    static ResourceKind SamplerTokenToResourceKind(TokenType t)
    {
        switch (t)
        {
        case TokenType::Sampler2DGLSLKw:          return ResourceKind::Sampler2D;
        case TokenType::Sampler2DMSGLSLKw:         return ResourceKind::Sampler2DMS;
        case TokenType::Sampler2DShadowGLSLKw:     return ResourceKind::Sampler2DShadow;
        case TokenType::Sampler2DArrayGLSLKw:      return ResourceKind::Sampler2DArray;
        case TokenType::Sampler2DArrayShadowGLSLKw:return ResourceKind::Sampler2DArrayShadow;
        case TokenType::Sampler3DGLSLKw:           return ResourceKind::Sampler3D;
        case TokenType::SamplerCubeGLSLKw:         return ResourceKind::SamplerCube;
        case TokenType::SamplerCubeShadowGLSLKw:   return ResourceKind::SamplerCubeShadow;
        default:                                   return ResourceKind::Sampler2D;
        }
    }

    static ResourceKind ImageTokenToResourceKind(TokenType t)
    {
        switch (t)
        {
        case TokenType::Image2DGLSLKw:   return ResourceKind::Image2D;
        case TokenType::Image3DGLSLKw:   return ResourceKind::Image3D;
        case TokenType::ImageCubeGLSLKw: return ResourceKind::ImageCube;
        default:                         return ResourceKind::Image2D;
        }
    }

    static ImageFormat ParseImageFormat(const std::string& fmt)
    {
        static const std::unordered_map<std::string, ImageFormat> kMap = {
            {"rgba32f", ImageFormat::rgba32f}, {"rgba16f", ImageFormat::rgba16f},
            {"rgba8",   ImageFormat::rgba8},   {"rg32f",   ImageFormat::rg32f},
            {"rg16f",   ImageFormat::rg16f},   {"rg8",     ImageFormat::rg8},
            {"r32f",    ImageFormat::r32f},    {"r16f",    ImageFormat::r16f},
            {"r8",      ImageFormat::r8},
            {"rgba32i", ImageFormat::rgba32i}, {"rgba16i", ImageFormat::rgba16i},
            {"rgba8i",  ImageFormat::rgba8i},  {"rg32i",   ImageFormat::rg32i},
            {"rg16i",   ImageFormat::rg16i},   {"rg8i",    ImageFormat::rg8i},
            {"r32i",    ImageFormat::r32i},    {"r16i",    ImageFormat::r16i},
            {"r8i",     ImageFormat::r8i},
            {"rgba32ui",ImageFormat::rgba32ui},{"rgba16ui",ImageFormat::rgba16ui},
            {"rgba8ui", ImageFormat::rgba8ui}, {"rg32ui",  ImageFormat::rg32ui},
            {"rg16ui",  ImageFormat::rg16ui},  {"rg8ui",   ImageFormat::rg8ui},
            {"r32ui",   ImageFormat::r32ui},   {"r16ui",   ImageFormat::r16ui},
            {"r8ui",    ImageFormat::r8ui},
            {"rgba16",  ImageFormat::rgba16},  {"rgb10_a2",ImageFormat::rgb10_a2},
            {"rgba8_snorm", ImageFormat::rgba8_snorm},
            {"rg16",    ImageFormat::rg16},    {"rg8",     ImageFormat::rg8},
            {"rg8_snorm",   ImageFormat::rg8_snorm},
            {"r16",     ImageFormat::r16},     {"r8",      ImageFormat::r8},
            {"r8_snorm",    ImageFormat::r8_snorm},
            {"r16f_depth",  ImageFormat::r16f_depth},
            {"r32f_depth",  ImageFormat::r32f_depth},
        };
        auto it = kMap.find(fmt);
        return it != kMap.end() ? it->second : ImageFormat::Unknown;
    }


    Parser::Parser(TokenStream& stream, DiagnosticCollector* diag)
        : m_Stream(stream), m_Diag(diag)
    {
    }

    Token& Parser::Current()        { return m_Stream.Current(); }
    Token Parser::PeekToken(int off) { return m_Stream.PeekToken(off); }
    Token Parser::Advance()         { return m_Stream.Advance(); }
    bool Parser::Check(TokenType t)  { return m_Stream.Check(t); }
    bool Parser::Match(TokenType t)  { return m_Stream.Match(t); }

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

    bool Parser::IsAtEnd() { return m_Stream.IsAtEnd(); }

    SourceLocation Parser::CurrentLoc()
    {
        return m_Stream.GetSM().GetLocation(Current().Offset);
    }

    void Parser::Error(const std::string& msg)
    {
        auto loc = CurrentLoc();
        std::string_view code = m_Stream.GetSM().GetView(Current().Offset, Current().Length);
        if (m_Diag) m_Diag->Error(msg, loc, std::string(code));
        auto& log = PrismShaderCompiler::Log::Instance();
        log.Error("{}:{} {}", loc.Column, loc.Line, msg);
        log.Error("    --> '{}'", code);
    }

    Token Parser::ConsumeType(const std::string& errMsg)
    {
        if (GLSLTypeUtil::IsTypeToken(Current().Type)) return Advance();
        Error(errMsg);
        return Current();
    }

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
        return std::atoi(buf);
    }

    void Parser::SkipTo(TokenType type)
    {
        while (!IsAtEnd() && !Check(type))
            Advance();
    }

    ComputeDocument Parser::ParseComputeShader()
    {
        ComputeDocument doc;
        m_NextInsertID = 0;

        ParseVersion(doc);
        ParseBody(doc);

        return doc;
    }


    void Parser::FlushSharedChunk(std::string& out, uint32_t& start)
    {
        uint32_t current = Current().Offset;
        if (current > start)
        {
            auto view = m_Stream.GetSM().GetView(start, current - start);
            out += std::string(view);
            start = current;
        }
    }

    void Parser::ParseVersion(ComputeDocument& doc)
    {
        if (!Check(TokenType::PreprocessDirective))
        {
            Error("期望 '#version' 指令");
            return;
        }

        std::string dir = TokenStr(Current());
        if (dir != "#version")
        {
            Error("期望 '#version' 指令，实际为 '" + dir + "'");
            return;
        }
        Advance();

        doc.GlslVersion = TokenInt(ConsumeNumber("期望 GLSL 版本号"));
    }

    void Parser::ParseBody(ComputeDocument& doc)
    {
        uint32_t sharedStart = Current().Offset;

        while (!IsAtEnd())
        {
            if (Check(TokenType::LayoutKw))
            {
                ParseLayoutResource(doc, sharedStart);
            }
            else if (Check(TokenType::UniformKw))
            {
                ParseUniform(doc, sharedStart);
            }
            else if (Check(TokenType::LeftBracket))
            {
                Token next = PeekToken(1);
                if (next.Is(TokenType::NumThreadsKw))
                {
                    ParseKernel(doc, sharedStart);
                }
                else
                {
                    Advance();
                }
            }
            else if (Check(TokenType::PreprocessDirective) && TokenStr(Current()) == "#pragma")
            {
                ParsePreprocess(doc, sharedStart);
            }
            else
            {
                Advance();
            }
        }

        FlushSharedChunk(doc.SharedSource, sharedStart);
    }


    void Parser::ParsePreprocess(ComputeDocument& doc, uint32_t& sharedStart)
    {
        if (TokenStr(Current()) != "#pragma")
            return;
        FlushSharedChunk(doc.SharedSource, sharedStart);
        uint32_t id = m_NextInsertID++;
        doc.SharedSource += "[Prism::Insert:" + std::to_string(id) + "]";
        Advance(); // #pragma
        if (Check(TokenType::Identifier) && TokenStr(Current()) == "kernel")
        {
            Advance(); // kernel

            KernelDecl decl;
            decl.InsertID = id;
            decl.Loc = CurrentLoc();
            decl.Name = TokenStr(Consume(TokenType::Identifier, "期望 kernel 名称"));

            uint32_t declLine = CurrentLoc().Line;
            while (Check(TokenType::Identifier) && CurrentLoc().Line == declLine)
                decl.VariantDefines.push_back(TokenStr(Advance()));

            doc.KernelDecls.push_back(std::move(decl));
        }
        else
        {
            uint32_t line = CurrentLoc().Line;
            while (!IsAtEnd() && CurrentLoc().Line == line)
                Advance();
        }

        sharedStart = Current().Offset;
    }

    void Parser::ParseLayoutResource(ComputeDocument& doc, uint32_t& sharedStart)
    {
        ComputeResource res;
        res.Loc = CurrentLoc();

        Advance(); // layout
        Consume(TokenType::LeftParen, "layout 期望 '('");

        while (!Check(TokenType::RightParen) && !IsAtEnd())
        {
            if (Check(TokenType::Identifier))
            {
                std::string qual = TokenStr(Current());
                if (qual == "std140" || qual == "std430")
                {
                    Advance();
                }
                else if (qual == "binding")
                {
                    Advance();
                    Consume(TokenType::Equals, "期望 '='");
                    res.Binding = (uint32_t)TokenInt(ConsumeNumber("期望 binding 值"));
                }
                else if (qual == "set")
                {
                    Advance();
                    Consume(TokenType::Equals, "期望 '='");
                    res.Set = (uint32_t)TokenInt(ConsumeNumber("期望 set 值"));
                }
                else
                {
                    res.Format = ParseImageFormat(qual);
                    Advance();
                }
            }
            else if (Check(TokenType::Comma))
            {
                Advance();
            }
            else
            {
                Advance();
            }
        }

        Consume(TokenType::RightParen, "期望 ')'");

        while (Check(TokenType::Identifier))
        {
            std::string word = TokenStr(Current());
            if (word == "readonly")  { res.ReadOnly = true;  Advance(); }
            else if (word == "writeonly") { res.WriteOnly = true; Advance(); }
            else if (word == "restrict")  { Advance(); }
            else break;
        }

        if (Check(TokenType::UniformKw))
        {
            Advance();

            Token typeTok = ConsumeType("期望 sampler 或 image 类型");
            GLSLType glslType = GLSLTypeUtil::FromTokenType(typeTok.Type);

            if (GLSLTypeUtil::IsSamplerType(glslType))
            {
                res.Kind = SamplerTokenToResourceKind(typeTok.Type);
                res.Type = glslType;
            }
            else if (GLSLTypeUtil::IsImageType(glslType))
            {
                res.Kind = ImageTokenToResourceKind(typeTok.Type);
                res.Type = glslType;
            }
            else
            {
                Error("layout 后期望 sampler 或 image 类型");
                return;
            }

            res.Name = TokenStr(Consume(TokenType::Identifier, "期望资源名称"));
            Consume(TokenType::Semicolon, "期望 ';'");
        }
        else if (Check(TokenType::Identifier) && TokenStr(Current()) == "buffer")
        {
            Advance();

            res.Kind = ResourceKind::StorageBuffer;
            res.BlockName = TokenStr(Consume(TokenType::Identifier, "期望 buffer 块名"));
            res.Type = GLSLType::None;

            Consume(TokenType::LeftBrace, "期望 '{'");
            int braceDepth = 1;
            while (!IsAtEnd() && braceDepth > 0)
            {
                if (Check(TokenType::LeftBrace))  { Advance(); braceDepth++; }
                else if (Check(TokenType::RightBrace)) { Advance(); braceDepth--; }
                else Advance();
            }

            res.InstanceName = TokenStr(Consume(TokenType::Identifier, "期望 buffer 实例名"));
            Consume(TokenType::Semicolon, "期望 ';'");
        }
        else
        {
            Error("layout 后期望 'uniform' 或 'buffer'");
        }

        doc.Resources.push_back(std::move(res));
    }

    void Parser::ParseUniform(ComputeDocument& doc, uint32_t& sharedStart)
    {
        ComputeUniform uniform;
        uniform.Loc = CurrentLoc();

        Advance(); // uniform

        uniform.Type = GLSLTypeUtil::FromTokenType(ConsumeType("期望 uniform 类型").Type);
        uniform.Name = TokenStr(Consume(TokenType::Identifier, "期望变量名"));
        Consume(TokenType::Semicolon, "期望 ';'");

        doc.Uniforms.push_back(std::move(uniform));
    }

    void Parser::ParseKernel(ComputeDocument& doc, uint32_t& sharedStart)
    {
        FlushSharedChunk(doc.SharedSource, sharedStart);
        uint32_t id = m_NextInsertID++;
        doc.SharedSource += "[Prism::Insert:" + std::to_string(id) + "]";

        Consume(TokenType::LeftBracket, "期望 '['");
        Consume(TokenType::NumThreadsKw, "期望 'numthreads'");
        Consume(TokenType::LeftParen, "期望 '('");

        uint32_t x = (uint32_t)TokenInt(ConsumeNumber("期望 X"));
        Consume(TokenType::Comma, "期望 ','");
        uint32_t y = (uint32_t)TokenInt(ConsumeNumber("期望 Y"));
        Consume(TokenType::Comma, "期望 ','");
        uint32_t z = (uint32_t)TokenInt(ConsumeNumber("期望 Z"));

        Consume(TokenType::RightParen, "期望 ')'");
        Consume(TokenType::RightBracket, "期望 ']'");

        Consume(TokenType::VoidGLSLKw, "期望 'void'");
        std::string kernelName = TokenStr(Consume(TokenType::Identifier, "期望 kernel 函数名"));
        Consume(TokenType::LeftParen, "期望 '('");
        Consume(TokenType::RightParen, "期望 ')'");

        Token openBrace = Consume(TokenType::LeftBrace, "期望 '{'");

        auto declIt = std::find_if(doc.KernelDecls.begin(), doc.KernelDecls.end(),
            [&](const KernelDecl& d) { return d.Name == kernelName; });

        if (declIt == doc.KernelDecls.end())
        {
            Error("kernel '" + kernelName + "' 未在 #pragma kernel 中声明");
        }

        KernelDef def;
        def.InsertID = id;
        def.Name = kernelName;
        def.GroupSizeX = x;
        def.GroupSizeY = y;
        def.GroupSizeZ = z;
        def.Loc = CurrentLoc();

        int funcDepth = 1;
        while (!IsAtEnd() && funcDepth > 0)
        {
            Token ft = Advance();
            if (ft.Is(TokenType::LeftBrace))
            {
                funcDepth++;
            }
            else if (ft.Is(TokenType::RightBrace))
            {
                funcDepth--;
                if (funcDepth == 0)
                {
                    uint32_t bodyStart = openBrace.Offset + openBrace.Length;
                    uint32_t bodyLen = ft.Offset - bodyStart;
                    def.FunctionSource = std::string(m_Stream.GetSM().GetView(bodyStart, bodyLen));
                    break;
                }
            }
        }

        doc.Kernels.push_back(std::move(def));
        sharedStart = Current().Offset;
    }

} // namespace PrismShaderCompiler::CSL
