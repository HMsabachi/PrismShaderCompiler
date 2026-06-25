#pragma once

#include "Token.h"
#include "Diagnostics.h"
#include "SourceManager.h"

#include <vector>
#include <cstring>

namespace PrismShaderCompiler
{

class TokenStream
{
public:
    TokenStream(const SourceManager& sm, DiagnosticCollector* diag = nullptr);

    const SourceManager& GetSM() const { return m_SM; }

    // Parser 接口（兼容旧 vector<Token> 模式）
    Token& Current();
    Token Advance();
    Token& PeekToken(int ahead = 0);
    bool Check(TokenType t);
    bool Match(TokenType t);
    bool IsAtEnd();

private:
    void EnsureAvailable(size_t ahead);
    Token LexSingle();
    void SkipWhitespaceAndComments();

    // 子词法
    Token LexString();
    Token LexNumber();
    Token LexIdent();
    Token LexOperator(char c);
    Token LexPreprocessDirective();

    // 关键字查找
    static TokenType LookupKeyword(const char* str, uint32_t len);

    const SourceManager& m_SM;
    DiagnosticCollector* m_Diag;

    const char* m_Ptr;          // 当前位置
    uint32_t m_ByteOffset;      // 当前字节偏移

    std::vector<Token> m_Buffer;
    size_t m_Pos = 0;
};

inline bool CmpN(const char* s, const char* lit, uint32_t n) {
    return std::memcmp(s, lit, n) == 0;
}
inline bool CmpN(const char* s, std::string_view lit) {
    return std::memcmp(s, lit.data(), lit.size()) == 0;
}

} // namespace PrismShaderCompiler
