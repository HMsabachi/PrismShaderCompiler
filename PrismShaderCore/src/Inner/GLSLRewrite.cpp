#include "Inner/GLSLRewrite.h"

#include "Inner/PPFile.h"
#include "PSL/Diagnostics.h"

#include <cstdlib>

namespace PrismShaderCompiler
{

namespace
{

// 不可能与真实源偏移相撞的哨兵：合成内容之后必须重新锚定 #line
constexpr uint32_t kNoOffset = 0xFFFFFFFFu;

std::string NormalizePath(const std::string& path)
{
    std::string out = path;

    for (char& c : out)
    {
        if (c == '\\')
            c = '/';
    }

    return out;
}

// 拼写 -> GLSLType。走 ToString 反查，避免再抄一份类型名表。
GLSLType ParseGLSLType(std::string_view text)
{
    static const GLSLType kTypes[] =
    {
        GLSLType::Void, GLSLType::Bool, GLSLType::Int, GLSLType::UInt, GLSLType::Float, GLSLType::Double,
        GLSLType::BVec2, GLSLType::BVec3, GLSLType::BVec4,
        GLSLType::IVec2, GLSLType::IVec3, GLSLType::IVec4,
        GLSLType::UVec2, GLSLType::UVec3, GLSLType::UVec4,
        GLSLType::Vec2, GLSLType::Vec3, GLSLType::Vec4,
        GLSLType::DVec2, GLSLType::DVec3, GLSLType::DVec4,
        GLSLType::Mat2, GLSLType::Mat3, GLSLType::Mat4,
        GLSLType::Mat2x2, GLSLType::Mat2x3, GLSLType::Mat2x4,
        GLSLType::Mat3x2, GLSLType::Mat3x3, GLSLType::Mat3x4,
        GLSLType::Mat4x2, GLSLType::Mat4x3, GLSLType::Mat4x4,
        GLSLType::Sampler2D, GLSLType::Sampler2DMS, GLSLType::SamplerCube,
        GLSLType::Sampler2DShadow, GLSLType::SamplerCubeShadow,
        GLSLType::Sampler2DArray, GLSLType::Sampler2DArrayShadow, GLSLType::Sampler3D,
        GLSLType::Image2D, GLSLType::Image3D, GLSLType::ImageCube,
        GLSLType::AtomicUInt,
    };

    for (const GLSLType type : kTypes)
    {
        if (text == GLSLTypeUtil::ToString(type))
            return type;
    }

    return GLSLType::None;
}

// 条件标记不是声明的一部分：向前看时撞上标记即视为认不出
const PPToken* TokenAt(const std::vector<PPItem>& items, size_t index)
{
    if (index >= items.size() || items[index].K != PPItem::Kind::Token)
        return nullptr;

    return &items[index].Tok;
}

constexpr size_t kNoIndex = static_cast<size_t>(-1);

// 声明解析游标。
//
// PPItem 流里换行是货真价实的 token（逐字回写需要它），但声明可以跨行写 ——
// `varying V\n{\n ... }` 与 `void vert()\n{` 都是资产里的实际写法。
// 所以解析一律跳换行前进；撞上条件标记则中止，因为标记两侧是另一份声明。
class SigCursor
{
public:
    SigCursor(const std::vector<PPItem>& items, size_t from)
        : m_Items(items), m_Index(Advance(from))
    {
    }

    bool Valid() const { return m_Index != kNoIndex; }
    size_t Index() const { return m_Index; }
    const PPToken& Tok() const { return m_Items[m_Index].Tok; }

    bool Step()
    {
        m_Index = Valid() ? Advance(m_Index + 1) : kNoIndex;
        return Valid();
    }

private:
    size_t Advance(size_t from) const
    {
        for (size_t i = from; i < m_Items.size(); ++i)
        {
            if (m_Items[i].K != PPItem::Kind::Token)
                return kNoIndex;

            if (m_Items[i].Tok.Is(PPType::NewLine))
                continue;

            return i;
        }

        return kNoIndex;
    }

    const std::vector<PPItem>& m_Items;
    size_t m_Index = kNoIndex;
};

void MarkSkipped(const std::vector<PPItem>& items, std::vector<bool>& skip, size_t begin, size_t end)
{
    for (size_t i = begin + 1; i <= end && i < skip.size(); ++i)
        skip[i] = true;

    // 声明收尾的换行一并吃掉 —— 替身文本自带换行，留着会平白多出空行
    const size_t after = end + 1;

    if (after < skip.size() && items[after].K == PPItem::Kind::Token
        && items[after].Tok.Is(PPType::NewLine))
    {
        skip[after] = true;
    }
}

const PPSourceLoc& FirstTokenLoc(const std::vector<PPItem>& items)
{
    static const PPSourceLoc kUnknown;

    for (const PPItem& item : items)
    {
        if (item.K == PPItem::Kind::Token)
            return item.Tok.Loc;
    }

    return kUnknown;
}

} // namespace

// 输出缓冲。两项职责：
//   1. 源 token 按原文范围逐字回写 —— deferred 区里的条件分支一字不能改；
//   2. 位置不连续处补 #line，让 glslang 的报错落回 .Shader 与 .glsl 的原始行。
class GLSLWriter
{
public:
    explicit GLSLWriter(const PPFileTable& files)
        : m_Files(files)
    {
    }

    void Raw(std::string_view text)
    {
        m_Out.append(text);
        Invalidate();
    }

    void Token(const PPToken& token)
    {
        if (!token.HasRaw())
        {
            // 宏展开产物没有原文可回写，只能按拼写输出
            if (token.LeadingSpace && NeedSpaceBefore())
                m_Out.push_back(' ');

            m_Out += token.Spelling;
            Invalidate();
            return;
        }

        if (!m_Files.IsValid(token.Loc.File))
            return;

        const std::string& source = m_Files.GetSource(token.Loc.File);
        const uint32_t end = token.RawOffset + token.RawLength;

        if (end > source.size() || token.TriviaOffset >= end)
            return;

        const bool contiguous = (token.Loc.File == m_LastFile) && (token.TriviaOffset == m_LastEnd);

        if (contiguous)
        {
            m_Out.append(source, token.TriviaOffset, end - token.TriviaOffset);
        }
        else
        {
            // 先落前导空白与注释，再把 #line 顶到 token 正前方，token 才会落在它原本的行上
            if (token.RawOffset > token.TriviaOffset)
                m_Out.append(source, token.TriviaOffset, token.RawOffset - token.TriviaOffset);

            LineDirective(token.Loc);
            m_Out.append(source, token.RawOffset, token.RawLength);
        }

        m_LastFile = token.Loc.File;
        m_LastEnd = end;
    }

    void LineDirective(const PPSourceLoc& loc)
    {
        if (!m_Files.IsValid(loc.File))
            return;

        Directive("#line " + std::to_string(loc.Line) + " \"" + NormalizePath(m_Files.GetPath(loc.File)) + "\"");
    }

    // #if / #elif / #else / #endif 必须落在行首
    void Directive(const std::string& text)
    {
        if (!AtCleanLineStart())
            m_Out.push_back('\n');

        m_Out += text;

        if (m_Out.empty() || m_Out.back() != '\n')
            m_Out.push_back('\n');

        Invalidate();
    }

    std::string Take() { return std::move(m_Out); }

private:
    void Invalidate()
    {
        m_LastFile = PP_NO_FILE;
        m_LastEnd = kNoOffset;
    }

    bool AtCleanLineStart() const
    {
        for (size_t i = m_Out.size(); i > 0; --i)
        {
            const char c = m_Out[i - 1];

            if (c == '\n')
                return true;

            if (c != ' ' && c != '\t' && c != '\r')
                return false;
        }

        return true;
    }

    bool NeedSpaceBefore() const
    {
        if (m_Out.empty())
            return false;

        const char c = m_Out.back();
        return c != '\n' && c != ' ' && c != '\t';
    }

    const PPFileTable& m_Files;
    std::string m_Out;
    PPFileId m_LastFile = PP_NO_FILE;
    uint32_t m_LastEnd = kNoOffset;
};

namespace
{

void WriteMarker(GLSLWriter& writer, const PPItem& item)
{
    switch (item.K)
    {
    case PPItem::Kind::CondBegin:
        writer.Directive("#if " + item.Condition);
        break;

    case PPItem::Kind::CondElse:
        writer.Directive("#" + item.Condition);
        break;

    case PPItem::Kind::CondEnd:
        writer.Directive("#endif");
        break;

    default:
        break;
    }
}

} // namespace

GLSLRewriter::GLSLRewriter(const PPFileTable& files, const InnerConfig& config, DiagnosticCollector& diag)
    : m_Files(files), m_Config(config), m_Diag(diag)
{
}

std::string GLSLRewriter::PathOf(const PPSourceLoc& loc) const
{
    return m_Files.IsValid(loc.File) ? m_Files.GetPath(loc.File) : m_Config.SourcePath;
}

bool GLSLRewriter::ParseAttribute(const std::vector<PPItem>& items, size_t begin, AttributeDecl& out) const
{
    SigCursor cursor(items, begin + 1);

    if (!cursor.Valid() || cursor.Tok().IsNot(PPType::Identifier))
        return false;

    const size_t typeIndex = cursor.Index();
    const std::string typeName = cursor.Tok().Spelling;

    if (!cursor.Step() || cursor.Tok().IsNot(PPType::Identifier))
        return false;

    const std::string name = cursor.Tok().Spelling;

    if (!cursor.Step() || !cursor.Tok().IsPunct(":"))
        return false;

    if (!cursor.Step() || cursor.Tok().IsNot(PPType::Identifier))
        return false;

    const size_t semanticIndex = cursor.Index();
    const std::string semanticName = cursor.Tok().Spelling;

    if (!cursor.Step() || !cursor.Tok().IsPunct(";"))
        return false;

    const GLSLType glslType = ParseGLSLType(typeName);

    if (glslType == GLSLType::None)
    {
        m_Diag.Error("attribute 类型无法识别: " + typeName,
                     ToDiagnosticLocation(items[typeIndex].Tok.Loc, PathOf(items[typeIndex].Tok.Loc)));
        return false;
    }

    const VertexSemantic parsed = ParseVertexSemantic(semanticName);

    if (parsed == VertexSemantic::Unknown)
    {
        m_Diag.Error("attribute 语义无法识别: " + semanticName,
                     ToDiagnosticLocation(items[semanticIndex].Tok.Loc, PathOf(items[semanticIndex].Tok.Loc)));
        return false;
    }

    out.Begin = begin;
    out.End = cursor.Index();
    out.Type = glslType;
    out.Name = name;
    out.Semantic = parsed;
    out.Loc = items[begin].Tok.Loc;
    return true;
}

bool GLSLRewriter::ParseVarying(const std::vector<PPItem>& items, size_t begin, VaryingDecl& out) const
{
    SigCursor cursor(items, begin + 1);

    if (!cursor.Valid() || cursor.Tok().IsNot(PPType::Identifier))
        return false;

    out.StructName = cursor.Tok().Spelling;
    out.Loc = items[begin].Tok.Loc;

    if (!cursor.Step() || !cursor.Tok().IsPunct("{"))
        return false;

    bool closed = false;

    while (cursor.Step())
    {
        if (cursor.Tok().IsPunct("}"))
        {
            closed = true;
            break;
        }

        if (cursor.Tok().IsNot(PPType::Identifier))
            return false;

        VaryingMember member;
        member.Type = ParseGLSLType(cursor.Tok().Spelling);

        if (member.Type == GLSLType::None)
        {
            m_Diag.Error("varying 成员类型无法识别: " + cursor.Tok().Spelling,
                         ToDiagnosticLocation(cursor.Tok().Loc, PathOf(cursor.Tok().Loc)));
            return false;
        }

        if (!cursor.Step() || cursor.Tok().IsNot(PPType::Identifier))
            return false;

        member.Name = cursor.Tok().Spelling;

        if (!cursor.Step())
            return false;

        if (cursor.Tok().IsPunct("["))
        {
            if (!cursor.Step() || cursor.Tok().IsNot(PPType::Number))
                return false;

            const unsigned long parsed = std::strtoul(cursor.Tok().Spelling.c_str(), nullptr, 10);
            member.ArraySize = parsed == 0 ? 1u : static_cast<uint32_t>(parsed);

            if (!cursor.Step() || !cursor.Tok().IsPunct("]"))
                return false;

            if (!cursor.Step())
                return false;
        }

        if (!cursor.Tok().IsPunct(";"))
            return false;

        out.Members.push_back(std::move(member));
    }

    if (!closed)
        return false;

    // '}' 之后是实例名与分号
    if (!cursor.Step() || cursor.Tok().IsNot(PPType::Identifier))
        return false;

    out.InstanceName = cursor.Tok().Spelling;

    if (!cursor.Step() || !cursor.Tok().IsPunct(";"))
        return false;

    out.Begin = begin;
    out.End = cursor.Index();
    return true;
}

bool GLSLRewriter::ParseFragOutput(const std::vector<PPItem>& items, size_t begin, FragOutputDecl& out) const
{
    SigCursor cursor(items, begin + 1);

    if (!cursor.Valid() || !cursor.Tok().IsPunct("("))
        return false;

    if (!cursor.Step() || !cursor.Tok().IsIdent("location"))
        return false;

    if (!cursor.Step() || !cursor.Tok().IsPunct("="))
        return false;

    if (!cursor.Step() || cursor.Tok().IsNot(PPType::Number))
        return false;

    const uint32_t location = static_cast<uint32_t>(std::strtoul(cursor.Tok().Spelling.c_str(), nullptr, 10));

    if (!cursor.Step() || !cursor.Tok().IsPunct(")"))
        return false;

    if (!cursor.Step() || !cursor.Tok().IsIdent("out"))
        return false;

    if (!cursor.Step() || cursor.Tok().IsNot(PPType::Identifier))
        return false;

    const GLSLType glslType = ParseGLSLType(cursor.Tok().Spelling);

    if (glslType == GLSLType::None)
        return false;

    if (!cursor.Step() || cursor.Tok().IsNot(PPType::Identifier))
        return false;

    const std::string name = cursor.Tok().Spelling;

    if (!cursor.Step() || !cursor.Tok().IsPunct(";"))
        return false;

    out.Begin = begin;
    out.End = cursor.Index();
    out.Type = glslType;
    out.Name = name;
    out.Location = location;
    out.Loc = items[begin].Tok.Loc;
    return true;
}

bool GLSLRewriter::ParseEntry(const std::vector<PPItem>& items, size_t begin, EntryDecl& out) const
{
    // begin 是 'void'，begin+1 是入口名；形参表与左花括号之间允许换行
    SigCursor cursor(items, begin + 2);

    if (!cursor.Valid() || !cursor.Tok().IsPunct("("))
        return false;

    if (!cursor.Step() || !cursor.Tok().IsPunct(")"))
        return false;

    if (!cursor.Step())
        return false;

    // 兼容 void vert(void) 的写法
    if (cursor.Tok().IsIdent("void") && !cursor.Step())
        return false;

    if (!cursor.Tok().IsPunct("{"))
        return false;

    const size_t braceIndex = cursor.Index();
    int depth = 0;
    size_t end = items.size();

    for (size_t i = braceIndex; i < items.size(); ++i)
    {
        // 条件标记可以夹在函数体内部，跳过即可
        if (items[i].K != PPItem::Kind::Token)
            continue;

        const PPToken& token = items[i].Tok;

        if (token.IsPunct("{"))
        {
            ++depth;
        }
        else if (token.IsPunct("}"))
        {
            --depth;

            if (depth == 0)
            {
                end = i;
                break;
            }
        }
    }

    if (end >= items.size())
    {
        m_Diag.Error("函数体未闭合", ToDiagnosticLocation(items[begin].Tok.Loc, PathOf(items[begin].Tok.Loc)));
        return false;
    }

    out.Valid = true;
    out.Begin = begin;
    out.BodyBegin = braceIndex + 1;
    out.BodyEnd = end - 1;
    out.End = end;
    out.Loc = items[begin].Tok.Loc;
    return true;
}

bool GLSLRewriter::Collect(const std::vector<PPItem>& items, Plan& plan, bool vertexStage) const
{
    plan.Skip.assign(items.size(), false);

    bool ok = true;
    int condDepth = 0;

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (items[i].K == PPItem::Kind::CondBegin)
        {
            ++condDepth;
            continue;
        }

        if (items[i].K == PPItem::Kind::CondEnd)
        {
            --condDepth;
            continue;
        }

        const PPToken* token = TokenAt(items, i);

        if (token == nullptr)
            continue;

        if (token->IsIdent("attribute"))
        {
            AttributeDecl decl;

            if (!ParseAttribute(items, i, decl))
            {
                ok = false;
                continue;
            }

            MarkSkipped(items, plan.Skip, decl.Begin, decl.End);

            // 顶点阶段就地改写；片元阶段只保留位置锚点，声明本身丢弃
            plan.Replacements[decl.Begin] = vertexStage
                ? "layout(location = " + std::to_string(SemanticToLocation(decl.Semantic)) + ") in "
                  + std::string(GLSLTypeUtil::ToString(decl.Type)) + " " + decl.Name + ";\n"
                : std::string();

            i = decl.End;
            continue;
        }

        if (token->IsIdent("varying"))
        {
            VaryingDecl decl;

            if (!ParseVarying(items, i, decl))
            {
                ok = false;
                continue;
            }

            if (condDepth > 0)
            {
                m_Diag.Error("varying 块不能出现在条件块内 —— 接口位置在编译期分配，条件化会让两侧对不上",
                             ToDiagnosticLocation(decl.Loc, PathOf(decl.Loc)));
                ok = false;
            }

            MarkSkipped(items, plan.Skip, decl.Begin, decl.End);
            plan.Replacements[decl.Begin] = BuildVaryingStruct(decl);

            if (plan.HasVarying)
            {
                m_Diag.Warning("一个 GLSL 块内出现多个 varying 块，仅第一个参与接口分配",
                               ToDiagnosticLocation(decl.Loc, PathOf(decl.Loc)));
            }
            else
            {
                plan.HasVarying = true;
                plan.Varying = decl;
            }

            i = decl.End;
            continue;
        }

        if (token->IsIdent("void"))
        {
            SigCursor cursor(items, i + 1);

            const bool isVertex = cursor.Valid() && cursor.Tok().IsIdent("vert");
            const bool isFragment = cursor.Valid() && cursor.Tok().IsIdent("frag");

            if (isVertex || isFragment)
            {
                EntryDecl decl;

                if (!ParseEntry(items, i, decl))
                {
                    ok = false;
                    continue;
                }

                if (condDepth > 0)
                {
                    m_Diag.Error("void vert()/void frag() 不能出现在条件块内",
                                 ToDiagnosticLocation(decl.Loc, PathOf(decl.Loc)));
                    ok = false;
                }

                EntryDecl& slot = isVertex ? plan.Vertex : plan.Fragment;

                if (slot.Valid)
                {
                    m_Diag.Error(std::string("重复定义 void ") + (isVertex ? "vert" : "frag") + "()",
                                 ToDiagnosticLocation(decl.Loc, PathOf(decl.Loc)));
                    ok = false;
                }
                else
                {
                    slot = decl;
                }

                MarkSkipped(items, plan.Skip, decl.Begin, decl.End);
                plan.Replacements[decl.Begin] = std::string();
                i = decl.End;
                continue;
            }
        }

        if (token->IsIdent("layout"))
        {
            FragOutputDecl decl;

            if (ParseFragOutput(items, i, decl))
            {
                if (condDepth > 0)
                {
                    m_Diag.Error("片元输出声明不能出现在条件块内",
                                 ToDiagnosticLocation(decl.Loc, PathOf(decl.Loc)));
                    ok = false;
                }

                MarkSkipped(items, plan.Skip, decl.Begin, decl.End);
                plan.Replacements[decl.Begin] = std::string();
                plan.FragOutputs.push_back(std::move(decl));
                i = decl.End;
            }

            continue;
        }
    }

    const PPSourceLoc blockLoc = FirstTokenLoc(items);

    if (!plan.Vertex.Valid)
    {
        m_Diag.Error("GLSL 块缺少 void vert()", ToDiagnosticLocation(blockLoc, PathOf(blockLoc)));
        ok = false;
    }

    if (!plan.Fragment.Valid)
    {
        m_Diag.Error("GLSL 块缺少 void frag()", ToDiagnosticLocation(blockLoc, PathOf(blockLoc)));
        ok = false;
    }

    return ok;
}

void GLSLRewriter::FlattenVarying(const VaryingDecl& varying, std::vector<FlatVar>& out) const
{
    uint32_t location = 0;
    const std::string prefix = varying.StructName + "_";

    for (const VaryingMember& member : varying.Members)
    {
        const bool isMatrix = GLSLTypeUtil::IsMatrixType(member.Type);
        const GLSLType columnType = GLSLTypeUtil::ColumnType(member.Type);
        const uint32_t columns = isMatrix ? GLSLTypeUtil::LocationSlots(member.Type) : 1;

        for (uint32_t element = 0; element < member.ArraySize; ++element)
        {
            for (uint32_t column = 0; column < columns; ++column)
            {
                FlatVar flat;
                flat.Type = columnType;
                flat.Location = location++;
                flat.Name = prefix + member.Name;

                if (isMatrix)
                    flat.Name += "_col" + std::to_string(column);

                if (member.ArraySize > 1)
                    flat.Name += "_" + std::to_string(element);

                flat.UserExpr = varying.InstanceName + "." + member.Name;

                if (member.ArraySize > 1)
                    flat.UserExpr += "[" + std::to_string(element) + "]";

                if (isMatrix)
                    flat.UserExpr += "[" + std::to_string(column) + "]";

                out.push_back(std::move(flat));
            }
        }
    }
}

std::string GLSLRewriter::BuildVaryingStruct(const VaryingDecl& varying) const
{
    std::string text = "struct " + varying.StructName + "\n{\n";

    for (const VaryingMember& member : varying.Members)
    {
        text += "    " + std::string(GLSLTypeUtil::ToString(member.Type)) + " " + member.Name;

        if (member.ArraySize > 1)
            text += "[" + std::to_string(member.ArraySize) + "]";

        text += ";\n";
    }

    text += "} " + varying.InstanceName + ";\n";
    return text;
}

void GLSLRewriter::WritePropertyBlock(GLSLWriter& writer, const std::vector<InnerProperty>& properties) const
{
    std::string uboBody;
    std::string textures;

    for (const InnerProperty& property : properties)
    {
        if (PropertyTypeUtil::IsTextureType(property.Type))
        {
            textures += "PRISM_MATERIAL_TEXTURE_LAYOUT("
                      + std::to_string(property.TextureSlot + m_Config.OpenGLTextureBeginBinding) + ", "
                      + std::to_string(property.TextureSlot + m_Config.VulkanTextureBeginBinding) + ") "
                      + PropertyTypeUtil::ToGLSLUniform(property.Type) + " " + property.Name + ";\n";
            continue;
        }

        uboBody += "    " + std::string(PropertyTypeUtil::ToGLSLType(property.Type)) + " " + property.Name + ";\n";
    }

    if (uboBody.empty() && textures.empty())
        return;

    // 后端是 deferred 的，绑定分支原样留在输出里，由 glslang 按 -D 选边
    writer.Raw("#if defined(PRISM_BACKEND_VULKAN)\n");
    writer.Raw("#define PRISM_MATERIAL_LAYOUT layout(std140, set = " + std::to_string(m_Config.VulkanMaterialUniformBufferSet)
             + ", binding = " + std::to_string(m_Config.VulkanMaterialUniformBufferBinding) + ")\n");
    writer.Raw("#define PRISM_MATERIAL_TEXTURE_LAYOUT(OPENGL_BINDING, VULKAN_BINDING) layout(set = "
             + std::to_string(m_Config.VulkanTextureBeginSet) + ", binding = VULKAN_BINDING)\n");
    writer.Raw("#else\n");
    writer.Raw("#define PRISM_MATERIAL_LAYOUT layout(std140, binding = "
             + std::to_string(m_Config.OpenGLMaterialUniformBufferBinding) + ")\n");
    writer.Raw("#define PRISM_MATERIAL_TEXTURE_LAYOUT(OPENGL_BINDING, VULKAN_BINDING) layout(binding = OPENGL_BINDING)\n");
    writer.Raw("#endif\n");

    if (!uboBody.empty())
        writer.Raw("PRISM_MATERIAL_LAYOUT uniform " + m_Config.MaterialBlockName + "\n{\n" + uboBody + "};\n");

    writer.Raw(textures);
}

void GLSLRewriter::WriteVaryingInterface(GLSLWriter& writer, const std::vector<FlatVar>& flats, bool vertexStage) const
{
    const char* qualifier = vertexStage ? "out" : "in";

    for (const FlatVar& flat : flats)
    {
        writer.Raw("layout(location = " + std::to_string(flat.Location) + ") " + qualifier + " "
                 + GLSLTypeUtil::ToString(flat.Type) + " " + flat.Name + ";\n");
    }
}

void GLSLRewriter::WriteFragmentOutputs(GLSLWriter& writer, const Plan& plan) const
{
    for (const FragOutputDecl& decl : plan.FragOutputs)
    {
        writer.Raw("layout(location = " + std::to_string(decl.Location) + ") out "
                 + GLSLTypeUtil::ToString(decl.Type) + " " + decl.Name + ";\n");
    }
}

void GLSLRewriter::WriteMain(GLSLWriter& writer, const std::vector<PPItem>& items, const Plan& plan,
                             const std::vector<FlatVar>& flats, bool vertexStage) const
{
    const EntryDecl& entry = vertexStage ? plan.Vertex : plan.Fragment;

    writer.Raw("void main()\n{\n");

    if (!vertexStage)
    {
        for (const FlatVar& flat : flats)
            writer.Raw("    " + flat.UserExpr + " = " + flat.Name + ";\n");
    }

    for (size_t i = entry.BodyBegin; i <= entry.BodyEnd && i < items.size(); ++i)
    {
        if (items[i].K == PPItem::Kind::Token)
        {
            writer.Token(items[i].Tok);
        }
        else
        {
            WriteMarker(writer, items[i]);
        }
    }

    if (vertexStage)
    {
        for (const FlatVar& flat : flats)
            writer.Raw("    " + flat.Name + " = " + flat.UserExpr + ";\n");
    }

    writer.Raw("}\n");
}

std::string GLSLRewriter::Emit(const std::vector<PPItem>& items,
                               const std::vector<InnerProperty>& properties,
                               bool vertexStage)
{
    Plan plan;

    if (!Collect(items, plan, vertexStage))
        return std::string();

    GLSLWriter writer(m_Files);

    writer.Raw("// " + NormalizePath(m_Config.SourcePath) + "\n");
    writer.Raw("#version " + std::to_string(m_Config.GlslVersion) + " core\n");
    WritePropertyBlock(writer, properties);

    if (!vertexStage)
        WriteFragmentOutputs(writer, plan);

    std::vector<FlatVar> flats;

    if (plan.HasVarying)
        FlattenVarying(plan.Varying, flats);

    WriteVaryingInterface(writer, flats, vertexStage);

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (plan.Skip[i])
            continue;

        const auto replacement = plan.Replacements.find(i);

        if (replacement != plan.Replacements.end())
        {
            writer.Raw(replacement->second);
            continue;
        }

        if (items[i].K == PPItem::Kind::Token)
            writer.Token(items[i].Tok);
        else
            WriteMarker(writer, items[i]);
    }

    WriteMain(writer, items, plan, flats, vertexStage);

    return writer.Take();
}

} // namespace PrismShaderCompiler
