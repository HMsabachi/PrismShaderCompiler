#include "Inner/GLSLRewrite.h"

#include "Inner/PPFile.h"
#include "PSL/Diagnostics.h"

#include <cstdlib>

namespace PrismShaderCompiler
{

namespace
{

constexpr size_t kNoIndex = static_cast<size_t>(-1);

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

    const GLSLType glslType = GLSLTypeUtil::FromName(typeName);

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
        member.Type = GLSLTypeUtil::FromName(cursor.Tok().Spelling);

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

    const GLSLType glslType = GLSLTypeUtil::FromName(cursor.Tok().Spelling);

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
    SigCursor cursor(items, begin + 2);

    if (!cursor.Valid() || !cursor.Tok().IsPunct("("))
        return false;

    if (!cursor.Step() || !cursor.Tok().IsPunct(")"))
        return false;

    if (!cursor.Step())
        return false;

    if (cursor.Tok().IsIdent("void") && !cursor.Step())
        return false;

    if (!cursor.Tok().IsPunct("{"))
        return false;

    const size_t braceIndex = cursor.Index();
    int depth = 0;
    size_t end = items.size();

    for (size_t i = braceIndex; i < items.size(); ++i)
    {
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
