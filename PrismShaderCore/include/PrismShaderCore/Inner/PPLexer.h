#pragma once

#include "PPToken.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace PrismShaderCompiler
{

class DiagnosticCollector;

class PPLexer
{
public:
    PPLexer(std::string_view source, PPFileId file);

    std::vector<PPToken> Tokenize(DiagnosticCollector* diag = nullptr);

    std::string Detokenize(const std::vector<PPToken>& tokens) const;

    std::string_view GetSource() const { return m_Source; }

private:
    bool AtEnd() const;
    char Cur() const { return Peek(0); }
    char Peek(size_t ahead = 0) const;
    void Advance();
    uint32_t SkipSplices(uint32_t pos) const;

    void ScanTrivia(DiagnosticCollector* diag);
    void SkipLineComment();
    void SkipBlockComment(DiagnosticCollector* diag);
    void ScanIdentifier(std::string& out);
    void ScanNumber(std::string& out);
    void ScanQuoted(std::string& out, char quote, DiagnosticCollector* diag);

    const char* MatchPunctuator() const;
    void EmitToken(std::vector<PPToken>& out, PPType type, std::string spelling,
                   uint32_t tokenStart, const PPSourceLoc& loc);

    std::string_view m_Source;
    PPFileId m_File = PP_NO_FILE;
    uint32_t m_Pos = 0;
    uint32_t m_Line = 1;
    uint32_t m_Column = 1;
    uint32_t m_TriviaStart = 0;
    bool m_AtLineStart = true;
};

} // namespace PrismShaderCompiler
