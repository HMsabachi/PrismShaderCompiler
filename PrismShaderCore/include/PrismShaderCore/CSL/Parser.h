#pragma once

#include "../PSL/TokenStream.h"
#include "AST.h"
#include "../PSL/Diagnostics.h"

#include <string>
#include <vector>

namespace PrismShaderCompiler::CSL
{

class Parser
{
public:
    Parser(TokenStream& stream, DiagnosticCollector* diag = nullptr);

    ComputeDocument ParseComputeShader();

private:
    Token& Current();
    Token PeekToken(int offset = 1);
    Token Advance();
    bool Check(TokenType type);
    bool Match(TokenType type);
    Token Consume(TokenType type, const std::string& errMsg);
    Token ConsumeNumber(const std::string& errMsg);
    bool IsAtEnd();
    SourceLocation CurrentLoc();
    void Error(const std::string& msg);

    Token ConsumeType(const std::string& errMsg);

    std::string_view TokenText(const Token& t) const;
    std::string TokenStr(const Token& t) const;
    float TokenFloat(const Token& t) const;
    int TokenInt(const Token& t) const;

    void SkipTo(TokenType type);
    void FlushSharedChunk(std::string& out, uint32_t& start);

    void ParseVersion(ComputeDocument& doc);
    void ParseBody(ComputeDocument& doc);
    void ParseLayout(ComputeDocument& doc, uint32_t& sharedStart);
    void ParsePreprocess(ComputeDocument& doc, uint32_t& sharedStart);
    void ParseLayoutResource(ComputeDocument& doc, uint32_t& sharedStart);
    void ParseUniform(ComputeDocument& doc, uint32_t& sharedStart);
    void ParseKernel(ComputeDocument& doc, uint32_t& sharedStart);

    TokenStream& m_Stream;
    DiagnosticCollector* m_Diag = nullptr;
    uint32_t m_NextInsertID = 0;
};

} // namespace PrismShaderCompiler::CSL
