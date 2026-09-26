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

    std::string Spelling;

    uint32_t TriviaOffset = 0;
    uint32_t TriviaLength = 0;

    uint32_t RawOffset = 0;
    uint32_t RawLength = 0;

    bool AtLineStart = false;
    bool LeadingSpace = false;

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
