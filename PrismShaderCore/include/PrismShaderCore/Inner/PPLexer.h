#pragma once

#include "PPToken.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace PrismShaderCompiler
{

class DiagnosticCollector;

// PP-token 词法：续行拼接、注释保留、行首标志、物理位置。
// 只做词法 —— 不展开宏、不求值条件、不解释指令。
class PPLexer
{
public:
    PPLexer(std::string_view source, PPFileId file);

    // 扫描全部 token，末尾追加一个 EndOfFile
    std::vector<PPToken> Tokenize(DiagnosticCollector* diag = nullptr);

    // 由 token 序列还原源文本：逐 token 拼接「前导原文 + 拼写」。
    // 仅当续行出现在 token 内部时该处会被拼接掉，其余字节逐字还原。
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
