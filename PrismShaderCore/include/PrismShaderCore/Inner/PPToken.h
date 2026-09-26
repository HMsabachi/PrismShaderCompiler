#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace PrismShaderCompiler
{

using PPFileId = uint32_t;
inline constexpr PPFileId PP_NO_FILE = 0xFFFFFFFFu;

struct PPSourceLoc
{
    PPFileId File = PP_NO_FILE;
    uint32_t Line = 1;
    uint32_t Column = 1;
};

enum class PPType : uint8_t
{
    EndOfFile,
    NewLine,
    Identifier,
    Number,
    StringLiteral,
    CharLiteral,
    Punctuator,
    Other,
};

struct PPToken
{
    PPType Type = PPType::EndOfFile;
    PPSourceLoc Loc;

    // 拼写。源 token 为续行拼接后的文本；合成 token（宏展开产物）为新建的串。
    std::string Spelling;

    // 前导空白与注释在源缓冲中的范围；合成 token 为 0
    uint32_t TriviaOffset = 0;
    uint32_t TriviaLength = 0;

    // 本 token 自身在源缓冲中的原始范围（含内部续行）；合成 token 为 0。
    // 逐字回写 deferred 区与无损 detokenize 都靠它。
    uint32_t RawOffset = 0;
    uint32_t RawLength = 0;

    bool AtLineStart = false;   // 本行首个非空白、非注释 token
    bool LeadingSpace = false;  // 与前一 token 之间存在空白或注释

    bool HasRaw() const { return RawLength > 0; }

    bool Is(PPType t) const { return Type == t; }
    bool IsNot(PPType t) const { return Type != t; }

    bool IsIdent(std::string_view name) const
    {
        return Type == PPType::Identifier && Spelling == name;
    }

    bool IsPunct(std::string_view p) const
    {
        return Type == PPType::Punctuator && Spelling == p;
    }
};

const char* PPTypeToString(PPType type);

} // namespace PrismShaderCompiler
