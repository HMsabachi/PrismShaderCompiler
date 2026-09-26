#pragma once

#include "PPToken.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PrismShaderCompiler
{

class MacroTable;
class DiagnosticCollector;

struct PPExprNode
{
    enum class Kind : uint8_t { Const, Symbol, Unary, Binary, Conditional };

    Kind K = Kind::Const;
    int64_t Value = 0;
    std::string Symbol;
    std::string Op;
    std::vector<PPExprNode> Kids;
    PPSourceLoc Loc;
};

class PPExpr
{
public:
    PPExpr(const MacroTable& macros, DiagnosticCollector* diag);

    PPExprNode Parse(const std::vector<PPToken>& tokens, const PPSourceLoc& loc);

    bool Failed() const { return m_Failed; }

private:
    bool AtEnd() const { return m_Index >= m_Tokens.size(); }

    PPExprNode ParseConditional();
    PPExprNode ParseBinary(int minPrec);
    PPExprNode ParseUnary();
    PPExprNode ParsePrimary();
    PPExprNode ParseDefined();

    void Fail(const std::string& message, const PPSourceLoc& loc);

    const MacroTable& m_Macros;
    DiagnosticCollector* m_Diag = nullptr;
    std::vector<PPToken> m_Tokens;
    size_t m_Index = 0;
    bool m_Failed = false;
};

bool PPExprIsConst(const PPExprNode& node);
int64_t PPExprConstValue(const PPExprNode& node);
std::string PPExprToText(const PPExprNode& node);

PPExprNode PPExprMakeConst(int64_t value, const PPSourceLoc& loc);
PPExprNode PPExprMakeSymbol(std::string symbol, const PPSourceLoc& loc);
PPExprNode PPExprNegate(const PPExprNode& node);

} // namespace PrismShaderCompiler
