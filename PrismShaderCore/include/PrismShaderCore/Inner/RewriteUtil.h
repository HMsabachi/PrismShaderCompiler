#pragma once

#include "PPPreprocessor.h"
#include "PPToken.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace PrismShaderCompiler
{

inline constexpr uint32_t kNoRawOffset = 0xFFFFFFFFu;

std::string NormalizePath(const std::string& path);

const PPToken* TokenAt(const std::vector<PPItem>& items, size_t index);
const PPSourceLoc& FirstTokenLoc(const std::vector<PPItem>& items);

void MarkSkipped(const std::vector<PPItem>& items, std::vector<bool>& skip, size_t begin, size_t end);

class GLSLWriter
{
public:
    explicit GLSLWriter(const PPFileTable& files);

    void Raw(std::string_view text);
    void Token(const PPToken& token);
    void LineDirective(const PPSourceLoc& loc);
    void Directive(const std::string& text);

    std::string Take();

private:
    void Invalidate();
    bool AtCleanLineStart() const;
    bool NeedSpaceBefore() const;

    const PPFileTable& m_Files;
    std::string m_Out;
    PPFileId m_LastFile = PP_NO_FILE;
    uint32_t m_LastEnd = kNoRawOffset;
};

void WriteMarker(GLSLWriter& writer, const PPItem& item);

} // namespace PrismShaderCompiler
