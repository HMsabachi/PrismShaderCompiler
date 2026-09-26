#pragma once

#include "PPToken.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PrismShaderCompiler
{

class MacroTable;
class DiagnosticCollector;

// 条件表达式节点。
//
// Const  —— 已折叠为常量
// Symbol —— 含 deferred 标识符，内层无法定值；Symbol 存其规范化文本
//
// 折叠是部分求值：deferred 标识符留作符号，immediate 标识符代入常量，
// 能定死的分支当场定死（`0 && X` → 0），定不死的原样保留。
struct PPExprNode
{
    enum class Kind : uint8_t { Const, Symbol, Unary, Binary, Conditional };

    Kind K = Kind::Const;
    int64_t Value = 0;
    std::string Symbol;
    std::string Op;
    std::vector<PPExprNode> Kids;   // Unary 1 项 / Binary 2 项 / Conditional 3 项
    PPSourceLoc Loc;
};

class PPExpr
{
public:
    PPExpr(const MacroTable& macros, DiagnosticCollector* diag);

    // tokens 为已展开的条件 token（不含 #if / #elif 本身）
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
