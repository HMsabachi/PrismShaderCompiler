#include "Inner/PPExpr.h"

#include "Inner/MacroTable.h"
#include "Inner/PPFile.h"
#include "PSL/Diagnostics.h"

#include <cstdlib>

namespace PrismShaderCompiler
{

namespace
{

struct BinaryOpInfo
{
    const char* Op;
    int Prec;
};

const BinaryOpInfo kBinaryOps[] =
{
    { "||", 1 }, { "&&", 2 }, { "|", 3 }, { "^", 4 }, { "&", 5 },
    { "==", 6 }, { "!=", 6 },
    { "<", 7 }, { "<=", 7 }, { ">", 7 }, { ">=", 7 },
    { "<<", 8 }, { ">>", 8 },
    { "+", 9 }, { "-", 9 },
    { "*", 10 }, { "/", 10 }, { "%", 10 },
};

int BinaryPrecedence(const std::string& op)
{
    for (const BinaryOpInfo& info : kBinaryOps)
    {
        if (op == info.Op)
            return info.Prec;
    }

    return 0;
}

int NodePrecedence(const PPExprNode& node)
{
    switch (node.K)
    {
    case PPExprNode::Kind::Const:       return 12;
    case PPExprNode::Kind::Symbol:      return 12;
    case PPExprNode::Kind::Unary:       return 11;
    case PPExprNode::Kind::Conditional: return 0;
    case PPExprNode::Kind::Binary:      return BinaryPrecedence(node.Op);
    }

    return 0;
}

PPExprNode MakeConst(int64_t value, const PPSourceLoc& loc)
{
    PPExprNode node;
    node.K = PPExprNode::Kind::Const;
    node.Value = value;
    node.Loc = loc;
    return node;
}

PPExprNode MakeSymbol(std::string symbol, const PPSourceLoc& loc)
{
    PPExprNode node;
    node.K = PPExprNode::Kind::Symbol;
    node.Symbol = std::move(symbol);
    node.Loc = loc;
    return node;
}

int64_t EvalBinary(const std::string& op, int64_t a, int64_t b,
                   DiagnosticCollector* diag, const PPSourceLoc& loc)
{
    if (op == "*") return a * b;

    if (op == "/")
    {
        if (b == 0)
        {
            if (diag) diag->Warning("条件表达式除以零", ToDiagnosticLocation(loc));
            return 0;
        }

        return a / b;
    }

    if (op == "%")
    {
        if (b == 0)
        {
            if (diag) diag->Warning("条件表达式对零取模", ToDiagnosticLocation(loc));
            return 0;
        }

        return a % b;
    }

    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "<<") return (b >= 0 && b < 64) ? static_cast<int64_t>(static_cast<uint64_t>(a) << b) : 0;
    if (op == ">>") return (b >= 0 && b < 64) ? (a >> b) : 0;
    if (op == "<") return a < b ? 1 : 0;
    if (op == "<=") return a <= b ? 1 : 0;
    if (op == ">") return a > b ? 1 : 0;
    if (op == ">=") return a >= b ? 1 : 0;
    if (op == "==") return a == b ? 1 : 0;
    if (op == "!=") return a != b ? 1 : 0;
    if (op == "&") return a & b;
    if (op == "^") return a ^ b;
    if (op == "|") return a | b;

    return 0;
}

PPExprNode FoldBinary(const std::string& op, PPExprNode left, PPExprNode right,
                      DiagnosticCollector* diag, const PPSourceLoc& loc)
{
    const bool leftConst = (left.K == PPExprNode::Kind::Const);
    const bool rightConst = (right.K == PPExprNode::Kind::Const);

    if (op == "&&")
    {
        if (leftConst && left.Value == 0) return MakeConst(0, loc);
        if (rightConst && right.Value == 0) return MakeConst(0, loc);
        if (leftConst && rightConst) return MakeConst(1, loc);
        if (leftConst) return right;
        if (rightConst) return left;
    }
    else if (op == "||")
    {
        if (leftConst && left.Value != 0) return MakeConst(1, loc);
        if (rightConst && right.Value != 0) return MakeConst(1, loc);
        if (leftConst && rightConst) return MakeConst(0, loc);
        if (leftConst) return right;
        if (rightConst) return left;
    }
    else if (leftConst && rightConst)
    {
        return MakeConst(EvalBinary(op, left.Value, right.Value, diag, loc), loc);
    }

    PPExprNode node;
    node.K = PPExprNode::Kind::Binary;
    node.Op = op;
    node.Loc = loc;
    node.Kids.push_back(std::move(left));
    node.Kids.push_back(std::move(right));
    return node;
}

PPExprNode FoldUnary(const std::string& op, PPExprNode kid, const PPSourceLoc& loc)
{
    if (kid.K == PPExprNode::Kind::Const)
    {
        if (op == "!") return MakeConst(kid.Value == 0 ? 1 : 0, loc);
        if (op == "-") return MakeConst(-kid.Value, loc);
        if (op == "+") return MakeConst(kid.Value, loc);
        if (op == "~") return MakeConst(~kid.Value, loc);
    }

    PPExprNode node;
    node.K = PPExprNode::Kind::Unary;
    node.Op = op;
    node.Loc = loc;
    node.Kids.push_back(std::move(kid));
    return node;
}

int64_t ParseInteger(const std::string& spelling, DiagnosticCollector* diag, const PPSourceLoc& loc)
{
    std::string text = spelling;

    while (!text.empty() && (text.back() == 'u' || text.back() == 'U' || text.back() == 'l'
                             || text.back() == 'L' || text.back() == 'z' || text.back() == 'Z'))
    {
        text.pop_back();
    }

    if (text.empty())
    {
        if (diag) diag->Error("非法的整数字面量: " + spelling, ToDiagnosticLocation(loc));
        return 0;
    }

    int base = 10;
    size_t start = 0;

    if (text.size() > 1 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        base = 16;
        start = 2;
    }
    else if (text.size() > 1 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B'))
    {
        base = 2;
        start = 2;
    }
    else if (text.size() > 1 && text[0] == '0')
    {
        base = 8;
        start = 1;
    }

    if (start >= text.size())
    {
        if (diag) diag->Error("非法的整数字面量: " + spelling, ToDiagnosticLocation(loc));
        return 0;
    }

    char* end = nullptr;
    const long long value = std::strtoll(text.c_str() + start, &end, base);

    if (end == text.c_str() + start || *end != '\0')
    {
        if (diag) diag->Error("条件表达式中的常量不是整数: " + spelling, ToDiagnosticLocation(loc));
        return 0;
    }

    return static_cast<int64_t>(value);
}

int64_t ParseCharLiteral(const std::string& spelling)
{
    if (spelling.size() < 3 || spelling.front() != '\'' || spelling.back() != '\'')
        return 0;

    const std::string body = spelling.substr(1, spelling.size() - 2);

    if (body.empty())
        return 0;

    if (body[0] == '\\' && body.size() >= 2)
    {
        switch (body[1])
        {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case '0':  return 0;
        case '\\': return '\\';
        case '\'': return '\'';
        case '"':  return '"';
        default:   return static_cast<unsigned char>(body[1]);
        }
    }

    return static_cast<unsigned char>(body[0]);
}

std::string Serialize(const PPExprNode& node, int parentPrecedence)
{
    std::string text;

    switch (node.K)
    {
    case PPExprNode::Kind::Const:
        text = std::to_string(node.Value);
        break;

    case PPExprNode::Kind::Symbol:
        text = node.Symbol;
        break;

    case PPExprNode::Kind::Unary:
        text = node.Op + Serialize(node.Kids[0], 11);
        break;

    case PPExprNode::Kind::Binary:
        text = Serialize(node.Kids[0], BinaryPrecedence(node.Op))
             + " " + node.Op + " "
             + Serialize(node.Kids[1], BinaryPrecedence(node.Op) + 1);
        break;

    case PPExprNode::Kind::Conditional:
        text = Serialize(node.Kids[0], 1)
             + " ? " + Serialize(node.Kids[1], 0)
             + " : " + Serialize(node.Kids[2], 0);
        break;
    }

    if (NodePrecedence(node) < parentPrecedence)
        return "(" + text + ")";

    return text;
}

} // namespace

PPExpr::PPExpr(const MacroTable& macros, DiagnosticCollector* diag)
    : m_Macros(macros), m_Diag(diag)
{
}

void PPExpr::Fail(const std::string& message, const PPSourceLoc& loc)
{
    if (!m_Failed && m_Diag)
        m_Diag->Error(message, ToDiagnosticLocation(loc));

    m_Failed = true;
}

PPExprNode PPExpr::Parse(const std::vector<PPToken>& tokens, const PPSourceLoc& loc)
{
    m_Tokens = tokens;
    m_Index = 0;
    m_Failed = false;

    if (m_Tokens.empty())
    {
        Fail("条件表达式为空", loc);
        return MakeConst(0, loc);
    }

    PPExprNode node = ParseConditional();

    if (!m_Failed && !AtEnd())
        Fail("条件表达式尾部有多余 token: " + m_Tokens[m_Index].Spelling, m_Tokens[m_Index].Loc);

    return node;
}

PPExprNode PPExpr::ParseConditional()
{
    PPExprNode condition = ParseBinary(1);

    if (m_Failed || AtEnd() || !m_Tokens[m_Index].IsPunct("?"))
        return condition;

    const PPSourceLoc loc = m_Tokens[m_Index].Loc;
    ++m_Index;

    PPExprNode thenNode = ParseConditional();

    if (m_Failed || AtEnd() || !m_Tokens[m_Index].IsPunct(":"))
    {
        Fail("三目运算符缺少 ':'", loc);
        return condition;
    }

    ++m_Index;
    PPExprNode elseNode = ParseConditional();

    if (condition.K == PPExprNode::Kind::Const)
        return condition.Value != 0 ? std::move(thenNode) : std::move(elseNode);

    PPExprNode node;
    node.K = PPExprNode::Kind::Conditional;
    node.Op = "?:";
    node.Loc = loc;
    node.Kids.push_back(std::move(condition));
    node.Kids.push_back(std::move(thenNode));
    node.Kids.push_back(std::move(elseNode));
    return node;
}

PPExprNode PPExpr::ParseBinary(int minPrecedence)
{
    PPExprNode left = ParseUnary();

    while (!m_Failed && !AtEnd())
    {
        const PPToken& token = m_Tokens[m_Index];

        if (token.IsNot(PPType::Punctuator))
            return left;

        const int precedence = BinaryPrecedence(token.Spelling);

        if (precedence == 0 || precedence < minPrecedence)
            return left;

        const std::string op = token.Spelling;
        const PPSourceLoc loc = token.Loc;
        ++m_Index;

        PPExprNode right = ParseBinary(precedence + 1);
        left = FoldBinary(op, std::move(left), std::move(right), m_Diag, loc);
    }

    return left;
}

PPExprNode PPExpr::ParseUnary()
{
    if (AtEnd())
    {
        Fail("表达式意外结束", m_Tokens.empty() ? PPSourceLoc{} : m_Tokens.back().Loc);
        return MakeConst(0, PPSourceLoc{});
    }

    const PPToken& token = m_Tokens[m_Index];

    if (token.Is(PPType::Punctuator) && (token.Spelling == "!" || token.Spelling == "-"
                                         || token.Spelling == "+" || token.Spelling == "~"))
    {
        const std::string op = token.Spelling;
        const PPSourceLoc loc = token.Loc;
        ++m_Index;

        return FoldUnary(op, ParseUnary(), loc);
    }

    return ParsePrimary();
}

PPExprNode PPExpr::ParsePrimary()
{
    if (AtEnd())
    {
        Fail("表达式意外结束", PPSourceLoc{});
        return MakeConst(0, PPSourceLoc{});
    }

    const PPToken& token = m_Tokens[m_Index];

    if (token.IsPunct("("))
    {
        const PPSourceLoc loc = token.Loc;
        ++m_Index;

        PPExprNode inner = ParseConditional();

        if (m_Failed)
            return inner;

        if (AtEnd() || !m_Tokens[m_Index].IsPunct(")"))
        {
            Fail("缺少 ')'", loc);
            return inner;
        }

        ++m_Index;
        return inner;
    }

    if (token.Is(PPType::CharLiteral))
    {
        const int64_t value = ParseCharLiteral(token.Spelling);
        const PPSourceLoc loc = token.Loc;
        ++m_Index;
        return MakeConst(value, loc);
    }

    if (token.Is(PPType::Number))
    {
        const std::string spelling = token.Spelling;
        const PPSourceLoc loc = token.Loc;
        ++m_Index;
        return MakeConst(ParseInteger(spelling, m_Diag, loc), loc);
    }

    if (token.Is(PPType::Identifier))
    {
        if (token.IsIdent("defined"))
            return ParseDefined();

        const std::string name = token.Spelling;
        const PPSourceLoc loc = token.Loc;
        ++m_Index;

        if (name == "true") return MakeConst(1, loc);
        if (name == "false") return MakeConst(0, loc);

        if (m_Macros.IsDeferred(name))
            return MakeSymbol(name, loc);

        return MakeConst(0, loc);
    }

    Fail("条件表达式出现非法 token: " + token.Spelling, token.Loc);
    ++m_Index;
    return MakeConst(0, token.Loc);
}

PPExprNode PPExpr::ParseDefined()
{
    const PPSourceLoc loc = m_Tokens[m_Index].Loc;
    ++m_Index;

    bool parenthesized = false;

    if (!AtEnd() && m_Tokens[m_Index].IsPunct("("))
    {
        parenthesized = true;
        ++m_Index;
    }

    if (AtEnd() || m_Tokens[m_Index].IsNot(PPType::Identifier))
    {
        Fail("defined 缺少操作数", loc);
        return MakeConst(0, loc);
    }

    const std::string name = m_Tokens[m_Index].Spelling;
    ++m_Index;

    if (parenthesized)
    {
        if (AtEnd() || !m_Tokens[m_Index].IsPunct(")"))
        {
            Fail("defined 缺少 ')'", loc);
            return MakeConst(0, loc);
        }

        ++m_Index;
    }

    if (m_Macros.IsDeferred(name))
        return MakeSymbol("defined(" + name + ")", loc);

    return MakeConst(m_Macros.IsDefined(name) ? 1 : 0, loc);
}

bool PPExprIsConst(const PPExprNode& node)
{
    return node.K == PPExprNode::Kind::Const;
}

int64_t PPExprConstValue(const PPExprNode& node)
{
    return node.K == PPExprNode::Kind::Const ? node.Value : 0;
}

std::string PPExprToText(const PPExprNode& node)
{
    return Serialize(node, 0);
}

PPExprNode PPExprMakeConst(int64_t value, const PPSourceLoc& loc)
{
    return MakeConst(value, loc);
}

PPExprNode PPExprMakeSymbol(std::string symbol, const PPSourceLoc& loc)
{
    return MakeSymbol(std::move(symbol), loc);
}

PPExprNode PPExprNegate(const PPExprNode& node)
{
    return FoldUnary("!", node, node.Loc);
}

} // namespace PrismShaderCompiler
