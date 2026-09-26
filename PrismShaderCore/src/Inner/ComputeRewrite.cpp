#include "Inner/ComputeRewrite.h"

#include "Inner/PPFile.h"
#include "Inner/PPPreprocessor.h"
#include "Inner/RewriteUtil.h"
#include "CSL/AST.h"
#include "CompilerCompute.h"
#include "PSL/Diagnostics.h"
#include "PSL/GLSLType.h"

#include <cstdlib>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace PrismShaderCompiler
{

namespace
{

constexpr size_t kNoIndex = static_cast<size_t>(-1);

size_t NextToken(const std::vector<PPItem>& items, size_t index)
{
    while (index < items.size()
        && (items[index].K != PPItem::Kind::Token || items[index].Tok.Is(PPType::NewLine)))
    {
        ++index;
    }

    return index;
}

class TokenCursor
{
public:
    TokenCursor(const std::vector<PPItem>& items, size_t index)
        : m_Items(items), m_Index(NextToken(items, index))
    {
    }

    const PPToken* Peek() const { return TokenAt(m_Items, m_Index); }
    const PPToken* PeekNext() const { return TokenAt(m_Items, NextToken(m_Items, m_Index + 1)); }

    size_t Raw() const { return m_Index; }

    void Step() { m_Index = NextToken(m_Items, m_Index + 1); }

    bool Accept(std::string_view punct)
    {
        const PPToken* token = Peek();

        if (!token || !token->IsPunct(punct))
            return false;

        Step();
        return true;
    }

    bool AcceptIdent(std::string_view name)
    {
        const PPToken* token = Peek();

        if (!token || !token->IsIdent(name))
            return false;

        Step();
        return true;
    }

private:
    const std::vector<PPItem>& m_Items;
    size_t m_Index;
};

struct Range
{
    size_t Begin = 0;
    size_t End = 0;
};

struct KernelInfo
{
    std::string Name;
    std::vector<std::string> Variants;

    uint32_t GroupSizeX = 1;
    uint32_t GroupSizeY = 1;
    uint32_t GroupSizeZ = 1;

    Range Def;
    Range Body;
    bool Defined = false;
};

struct ScanResult
{
    int GlslVersion = 450;
    std::vector<Range> Drops;
    std::vector<KernelInfo> Kernels;
    std::vector<CSL::ComputeResource> Resources;
    std::vector<CSL::ComputeUniform> Uniforms;
};

CSL::ResourceKind SamplerKind(GLSLType type)
{
    switch (type)
    {
    case GLSLType::Sampler2D:            return CSL::ResourceKind::Sampler2D;
    case GLSLType::Sampler2DMS:          return CSL::ResourceKind::Sampler2DMS;
    case GLSLType::Sampler2DShadow:      return CSL::ResourceKind::Sampler2DShadow;
    case GLSLType::Sampler2DArray:       return CSL::ResourceKind::Sampler2DArray;
    case GLSLType::Sampler2DArrayShadow: return CSL::ResourceKind::Sampler2DArrayShadow;
    case GLSLType::Sampler3D:            return CSL::ResourceKind::Sampler3D;
    case GLSLType::SamplerCube:          return CSL::ResourceKind::SamplerCube;
    case GLSLType::SamplerCubeShadow:    return CSL::ResourceKind::SamplerCubeShadow;
    default:                             return CSL::ResourceKind::Sampler2D;
    }
}

CSL::ResourceKind ImageKind(GLSLType type)
{
    switch (type)
    {
    case GLSLType::Image2D:   return CSL::ResourceKind::Image2D;
    case GLSLType::Image3D:   return CSL::ResourceKind::Image3D;
    case GLSLType::ImageCube: return CSL::ResourceKind::ImageCube;
    default:                  return CSL::ResourceKind::Image2D;
    }
}

CSL::ImageFormat ParseImageFormat(const std::string& format)
{
    static const std::unordered_map<std::string, CSL::ImageFormat> kFormats = {
        {"rgba32f", CSL::ImageFormat::rgba32f}, {"rgba16f", CSL::ImageFormat::rgba16f},
        {"rg32f",   CSL::ImageFormat::rg32f},   {"rg16f",   CSL::ImageFormat::rg16f},
        {"r32f",    CSL::ImageFormat::r32f},    {"r16f",    CSL::ImageFormat::r16f},
        {"rgba32i", CSL::ImageFormat::rgba32i}, {"rgba16i", CSL::ImageFormat::rgba16i},
        {"rgba8i",  CSL::ImageFormat::rgba8i},  {"rg32i",   CSL::ImageFormat::rg32i},
        {"rg16i",   CSL::ImageFormat::rg16i},   {"rg8i",    CSL::ImageFormat::rg8i},
        {"r32i",    CSL::ImageFormat::r32i},    {"r16i",    CSL::ImageFormat::r16i},
        {"r8i",     CSL::ImageFormat::r8i},
        {"rgba32ui",CSL::ImageFormat::rgba32ui},{"rgba16ui",CSL::ImageFormat::rgba16ui},
        {"rgba8ui", CSL::ImageFormat::rgba8ui}, {"rg32ui",  CSL::ImageFormat::rg32ui},
        {"rg16ui",  CSL::ImageFormat::rg16ui},  {"rg8ui",   CSL::ImageFormat::rg8ui},
        {"r32ui",   CSL::ImageFormat::r32ui},   {"r16ui",   CSL::ImageFormat::r16ui},
        {"r8ui",    CSL::ImageFormat::r8ui},
        {"rgba16",  CSL::ImageFormat::rgba16},  {"rgb10_a2",CSL::ImageFormat::rgb10_a2},
        {"rgba8",   CSL::ImageFormat::rgba8},   {"rgba8_snorm", CSL::ImageFormat::rgba8_snorm},
        {"rg16",    CSL::ImageFormat::rg16},    {"rg8",     CSL::ImageFormat::rg8},
        {"rg8_snorm",   CSL::ImageFormat::rg8_snorm},
        {"r16",     CSL::ImageFormat::r16},     {"r8",      CSL::ImageFormat::r8},
        {"r8_snorm",    CSL::ImageFormat::r8_snorm},
        {"r16f_depth",  CSL::ImageFormat::r16f_depth},
        {"r32f_depth",  CSL::ImageFormat::r32f_depth},
    };

    const auto it = kFormats.find(format);
    return it != kFormats.end() ? it->second : CSL::ImageFormat::Unknown;
}

class Scanner
{
public:
    Scanner(const PPFileTable& files, const std::vector<PPItem>& items, DiagnosticCollector& diag)
        : m_Files(files), m_Items(items), m_Diag(diag)
    {
    }

    void Run(ScanResult& out);

private:
    size_t LineEnd(size_t begin) const;
    size_t MatchBrace(size_t open) const;
    void DropLine(size_t begin, ScanResult& out) const;
    void SkipBlock(TokenCursor& cursor, size_t close) const;

    void ScanKernelPragma(size_t begin, ScanResult& out);
    size_t ScanKernelDef(size_t begin, ScanResult& out);
    size_t ScanLayout(size_t begin, ScanResult& out);

    bool ReadInstanceName(TokenCursor& cursor, CSL::ComputeResource& resource,
                          const PPSourceLoc& loc, bool required) const;

    std::string PathOf(const PPSourceLoc& loc) const;
    SourceLocation LocOf(const PPSourceLoc& loc) const;
    void Error(const std::string& msg, const PPSourceLoc& loc) const;

    const PPFileTable& m_Files;
    const std::vector<PPItem>& m_Items;
    DiagnosticCollector& m_Diag;
};

void Scanner::Run(ScanResult& out)
{
    size_t i = 0;

    while (i < m_Items.size())
    {
        const PPToken* token = TokenAt(m_Items, i);

        if (!token)
        {
            ++i;
            continue;
        }

        if (token->IsPunct("#"))
        {
            const size_t word = NextToken(m_Items, i + 1);
            const PPToken* keyword = TokenAt(m_Items, word);

            if (keyword && keyword->IsIdent("version"))
            {
                const size_t number = NextToken(m_Items, word + 1);

                if (const PPToken* value = TokenAt(m_Items, number); value && value->Is(PPType::Number))
                    out.GlslVersion = static_cast<int>(std::strtol(value->Spelling.c_str(), nullptr, 10));

                DropLine(i, out);
                i = LineEnd(i) + 1;
                continue;
            }

            if (keyword && keyword->IsIdent("pragma"))
            {
                const size_t kind = NextToken(m_Items, word + 1);

                if (const PPToken* name = TokenAt(m_Items, kind); name && name->IsIdent("kernel"))
                {
                    ScanKernelPragma(i, out);
                    i = LineEnd(i) + 1;
                    continue;
                }
            }

            ++i;
            continue;
        }

        if (token->IsIdent("layout"))
        {
            i = ScanLayout(i, out);
            continue;
        }

        if (token->IsIdent("uniform"))
        {
            Error("普通 uniform 必须带 layout(location=N)，如: layout(location=0) uniform float u_Param;",
                  token->Loc);
            ++i;
            continue;
        }

        if (token->IsPunct("["))
        {
            const size_t word = NextToken(m_Items, i + 1);

            if (const PPToken* keyword = TokenAt(m_Items, word); keyword && keyword->IsIdent("numthreads"))
            {
                i = ScanKernelDef(i, out);
                continue;
            }
        }

        ++i;
    }
}

size_t Scanner::LineEnd(size_t begin) const
{
    const PPToken* first = TokenAt(m_Items, begin);

    if (!first)
        return begin;

    size_t end = begin;

    while (end + 1 < m_Items.size())
    {
        const PPToken* next = TokenAt(m_Items, end + 1);

        if (!next || next->Loc.File != first->Loc.File || next->Loc.Line != first->Loc.Line)
            break;

        ++end;
    }

    return end;
}

size_t Scanner::MatchBrace(size_t open) const
{
    int depth = 0;

    for (size_t i = open; i < m_Items.size(); ++i)
    {
        const PPToken* token = TokenAt(m_Items, i);

        if (!token)
            continue;

        if (token->IsPunct("{"))
        {
            ++depth;
        }
        else if (token->IsPunct("}"))
        {
            --depth;

            if (depth == 0)
                return i;
        }
    }

    return kNoIndex;
}

void Scanner::DropLine(size_t begin, ScanResult& out) const
{
    out.Drops.push_back(Range{ begin, LineEnd(begin) });
}

void Scanner::SkipBlock(TokenCursor& cursor, size_t close) const
{
    while (cursor.Raw() <= close)
        cursor.Step();
}

void Scanner::ScanKernelPragma(size_t begin, ScanResult& out)
{
    const size_t word = NextToken(m_Items, begin + 1);
    const size_t kind = NextToken(m_Items, word + 1);
    const size_t nameIndex = NextToken(m_Items, kind + 1);
    const PPToken* name = TokenAt(m_Items, nameIndex);

    DropLine(begin, out);

    if (!name || name->IsNot(PPType::Identifier))
    {
        Error("#pragma kernel 缺少 kernel 名称", m_Items[begin].Tok.Loc);
        return;
    }

    KernelInfo info;
    info.Name = name->Spelling;

    for (size_t i = nameIndex + 1; i < m_Items.size(); ++i)
    {
        const PPToken* token = TokenAt(m_Items, i);

        if (!token || token->Loc.File != name->Loc.File || token->Loc.Line != name->Loc.Line)
            break;

        if (token->Is(PPType::Identifier))
            info.Variants.push_back(token->Spelling);
    }

    out.Kernels.push_back(std::move(info));
}

size_t Scanner::ScanKernelDef(size_t begin, ScanResult& out)
{
    const PPSourceLoc loc = m_Items[begin].Tok.Loc;
    TokenCursor cursor(m_Items, begin);

    cursor.Step();

    if (!cursor.AcceptIdent("numthreads") || !cursor.Accept("("))
    {
        Error("[numthreads] 语法错误，期望 '[numthreads(X, Y, Z)]'", loc);
        return cursor.Raw();
    }

    uint32_t group[3] = { 1, 1, 1 };

    for (int axis = 0; axis < 3; ++axis)
    {
        const PPToken* token = cursor.Peek();

        if (!token || token->IsNot(PPType::Number))
        {
            Error("[numthreads] 期望整型线程组尺寸", token ? token->Loc : loc);
            return cursor.Raw();
        }

        group[axis] = static_cast<uint32_t>(std::strtoul(token->Spelling.c_str(), nullptr, 0));
        cursor.Step();

        if (axis < 2 && !cursor.Accept(","))
        {
            Error("[numthreads] 期望 ','", token->Loc);
            return cursor.Raw();
        }
    }

    if (!cursor.Accept(")") || !cursor.Accept("]"))
    {
        Error("[numthreads(X, Y, Z)] 期望 ')]'", loc);
        return cursor.Raw();
    }

    if (!cursor.AcceptIdent("void"))
    {
        Error("compute kernel 必须返回 void", loc);
        return cursor.Raw();
    }

    const PPToken* name = cursor.Peek();

    if (!name || name->IsNot(PPType::Identifier))
    {
        Error("期望 compute kernel 函数名", loc);
        return cursor.Raw();
    }

    cursor.Step();

    if (!cursor.Accept("(") || !cursor.Accept(")"))
    {
        Error("compute kernel 不能有参数", name->Loc);
        return cursor.Raw();
    }

    const size_t brace = cursor.Raw();

    if (!cursor.Accept("{"))
    {
        Error("compute kernel 缺少 '{'", name->Loc);
        return cursor.Raw();
    }

    const size_t close = MatchBrace(brace);

    if (close == kNoIndex)
    {
        Error("compute kernel 缺少 '}'", name->Loc);
        return cursor.Raw();
    }

    KernelInfo* info = nullptr;

    for (KernelInfo& existing : out.Kernels)
    {
        if (existing.Name == name->Spelling)
        {
            info = &existing;
            break;
        }
    }

    if (!info)
    {
        Error("compute kernel '" + name->Spelling + "' 未在 #pragma kernel 中声明", name->Loc);
        return close + 1;
    }

    info->GroupSizeX = group[0];
    info->GroupSizeY = group[1];
    info->GroupSizeZ = group[2];
    info->Def = Range{ begin, close };
    info->Body = Range{ brace + 1, close - 1 };
    info->Defined = true;

    return close + 1;
}

size_t Scanner::ScanLayout(size_t begin, ScanResult& out)
{
    const PPSourceLoc loc = m_Items[begin].Tok.Loc;
    TokenCursor cursor(m_Items, begin);

    cursor.Step();

    if (!cursor.Accept("("))
    {
        Error("layout 期望 '('", loc);
        return cursor.Raw();
    }

    CSL::ComputeResource resource;
    resource.Loc = LocOf(loc);

    uint32_t location = 0;
    bool hasLocation = false;

    while (const PPToken* token = cursor.Peek())
    {
        if (cursor.Accept(")"))
            break;

        if (cursor.Accept(","))
            continue;

        if (token->IsNot(PPType::Identifier))
        {
            cursor.Step();
            continue;
        }

        const std::string qualifier = token->Spelling;
        cursor.Step();

        if (qualifier == "std140" || qualifier == "std430" || qualifier == "push_constant")
            continue;

        if (qualifier == "binding" || qualifier == "set" || qualifier == "location")
        {
            const PPToken* value = nullptr;

            if (!cursor.Accept("=") || (value = cursor.Peek()) == nullptr || value->IsNot(PPType::Number))
            {
                Error("layout(" + qualifier + ") 期望 '= 数值'", token->Loc);
                return cursor.Raw();
            }

            const uint32_t number = static_cast<uint32_t>(std::strtoul(value->Spelling.c_str(), nullptr, 0));
            cursor.Step();

            if (qualifier == "binding")
                resource.Binding = number;
            else if (qualifier == "set")
                resource.Set = number;
            else
            {
                location = number;
                hasLocation = true;
            }

            continue;
        }

        resource.Format = ParseImageFormat(qualifier);

        if (resource.Format == CSL::ImageFormat::Unknown)
            Error("未知的 image format: " + qualifier, token->Loc);
    }

    while (const PPToken* token = cursor.Peek())
    {
        if (token->IsIdent("readonly"))
            resource.ReadOnly = true;
        else if (token->IsIdent("writeonly"))
            resource.WriteOnly = true;
        else if (!token->IsIdent("restrict"))
            break;

        cursor.Step();
    }

    if (cursor.AcceptIdent("uniform"))
    {
        const PPToken* next = cursor.Peek();
        const bool block = next && (next->IsPunct("{")
            || (next->Is(PPType::Identifier) && cursor.PeekNext() && cursor.PeekNext()->IsPunct("{")));

        if (block)
        {
            resource.Kind = CSL::ResourceKind::UniformBuffer;
            resource.Type = GLSLType::None;

            if (next->Is(PPType::Identifier))
            {
                resource.BlockName = next->Spelling;
                cursor.Step();
            }

            const size_t open = cursor.Raw();

            if (!cursor.Accept("{"))
            {
                Error("uniform 块缺少 '{'", loc);
                return cursor.Raw();
            }

            const size_t close = MatchBrace(open);

            if (close == kNoIndex)
            {
                Error("uniform 块缺少 '}'", loc);
                return cursor.Raw();
            }

            SkipBlock(cursor, close);

            if (!ReadInstanceName(cursor, resource, loc, false))
                return cursor.Raw();

            out.Resources.push_back(std::move(resource));
            return cursor.Raw();
        }

        const PPToken* type = cursor.Peek();

        if (!type || type->IsNot(PPType::Identifier))
        {
            Error("uniform 后期望 GLSL 类型", loc);
            return cursor.Raw();
        }

        const GLSLType glslType = GLSLTypeUtil::FromName(type->Spelling);
        cursor.Step();

        if (glslType == GLSLType::None)
        {
            Error("layout 后期望 sampler、image 或普通 GLSL 类型", type->Loc);
            return cursor.Raw();
        }

        const PPToken* name = cursor.Peek();

        if (!name || name->IsNot(PPType::Identifier))
        {
            Error("期望资源名称", type->Loc);
            return cursor.Raw();
        }

        cursor.Step();
        cursor.Accept(";");

        if (GLSLTypeUtil::IsSamplerType(glslType) || GLSLTypeUtil::IsImageType(glslType))
        {
            resource.Kind = GLSLTypeUtil::IsSamplerType(glslType) ? SamplerKind(glslType) : ImageKind(glslType);
            resource.Type = glslType;
            resource.Name = name->Spelling;
            out.Resources.push_back(std::move(resource));
            return cursor.Raw();
        }

        if (!hasLocation)
            Error("普通 uniform 必须指定 layout(location=N)", type->Loc);

        CSL::ComputeUniform uniform;
        uniform.Type = glslType;
        uniform.Location = location;
        uniform.Name = name->Spelling;
        uniform.Loc = LocOf(loc);
        out.Uniforms.push_back(std::move(uniform));

        return cursor.Raw();
    }

    if (cursor.AcceptIdent("buffer"))
    {
        resource.Kind = CSL::ResourceKind::StorageBuffer;
        resource.Type = GLSLType::None;

        if (const PPToken* block = cursor.Peek(); block && block->Is(PPType::Identifier))
        {
            resource.BlockName = block->Spelling;
            cursor.Step();
        }

        const size_t open = cursor.Raw();

        if (!cursor.Accept("{"))
        {
            Error("buffer 块缺少 '{'", loc);
            return cursor.Raw();
        }

        const size_t close = MatchBrace(open);

        if (close == kNoIndex)
        {
            Error("buffer 块缺少 '}'", loc);
            return cursor.Raw();
        }

        SkipBlock(cursor, close);

        if (!ReadInstanceName(cursor, resource, loc, true))
            return cursor.Raw();

        out.Resources.push_back(std::move(resource));
        return cursor.Raw();
    }

    Error("layout 后期望 'uniform' 或 'buffer'", loc);
    return cursor.Raw();
}

bool Scanner::ReadInstanceName(TokenCursor& cursor, CSL::ComputeResource& resource,
                               const PPSourceLoc& loc, bool required) const
{
    const PPToken* instance = cursor.Peek();

    if (!instance || instance->IsNot(PPType::Identifier))
    {
        if (required)
        {
            Error("期望块实例名", loc);
            return false;
        }

        cursor.Accept(";");
        return true;
    }

    resource.InstanceName = instance->Spelling;
    cursor.Step();
    cursor.Accept(";");

    return true;
}

std::string Scanner::PathOf(const PPSourceLoc& loc) const
{
    return m_Files.IsValid(loc.File) ? m_Files.GetPath(loc.File) : std::string();
}

SourceLocation Scanner::LocOf(const PPSourceLoc& loc) const
{
    return ToDiagnosticLocation(loc, PathOf(loc));
}

void Scanner::Error(const std::string& msg, const PPSourceLoc& loc) const
{
    m_Diag.Error(msg, LocOf(loc));
}

bool PrepareCompute(const ComputeInnerParams& params, DiagnosticCollector& diag,
                    PPFileTable& files, std::vector<PPItem>& items)
{
    PPParams pp;
    pp.Path = params.Config.SourcePath;
    pp.Source = params.Source;
    pp.IncludeRoot = params.Config.IncludeRoot;
    pp.ReadFile = params.ReadFile;
    pp.Defines.push_back("PRISM_COMPUTE_SHADER");

    PPPreprocessor preprocessor(files, diag);
    PPResult result = preprocessor.Run(pp);

    items = std::move(result.Items);
    return result.Success;
}

void MarkRange(const std::vector<PPItem>& items, std::vector<bool>& skip, const Range& range)
{
    for (size_t i = range.Begin; i <= range.End && i < skip.size(); ++i)
    {
        if (items[i].K == PPItem::Kind::Token)
            skip[i] = true;
    }
}

} // namespace

ComputeRewriter::ComputeRewriter(DiagnosticCollector& diag)
    : m_Diag(diag)
{
}

bool ComputeRewriter::Analyze(const ComputeInnerParams& params, CompiledComputeShader& out)
{
    PPFileTable files;
    std::vector<PPItem> items;

    if (!PrepareCompute(params, m_Diag, files, items))
        return false;

    ScanResult scan;
    Scanner scanner(files, items, m_Diag);
    scanner.Run(scan);

    if (m_Diag.HasErrors())
        return false;

    out.GlslVersion = scan.GlslVersion;
    out.Source = params.Source;
    out.SourcePath = params.Config.SourcePath;
    out.Resources = std::move(scan.Resources);
    out.Uniforms = std::move(scan.Uniforms);

    for (KernelInfo& kernel : scan.Kernels)
    {
        if (!kernel.Defined)
            continue;

        CompiledComputeShader::KernelInfo info;
        info.Name = std::move(kernel.Name);
        info.VariantDefines = std::move(kernel.Variants);
        info.GroupSizeX = kernel.GroupSizeX;
        info.GroupSizeY = kernel.GroupSizeY;
        info.GroupSizeZ = kernel.GroupSizeZ;

        out.Kernels.push_back(std::move(info));
    }

    return true;
}

std::string ComputeRewriter::Emit(const ComputeInnerParams& params, uint32_t kernelIndex)
{
    PPFileTable files;
    std::vector<PPItem> items;

    if (!PrepareCompute(params, m_Diag, files, items))
        return std::string();

    ScanResult scan;
    Scanner scanner(files, items, m_Diag);
    scanner.Run(scan);

    if (m_Diag.HasErrors())
        return std::string();

    if (kernelIndex >= scan.Kernels.size() || !scan.Kernels[kernelIndex].Defined)
    {
        m_Diag.Error("compute kernel 序号 " + std::to_string(kernelIndex) + " 超出范围",
                     ToDiagnosticLocation(PPSourceLoc{}, params.Config.SourcePath));
        return std::string();
    }

    const KernelInfo& kernel = scan.Kernels[kernelIndex];

    std::vector<bool> skip(items.size(), false);

    for (const Range& drop : scan.Drops)
        MarkRange(items, skip, drop);

    for (const KernelInfo& other : scan.Kernels)
    {
        if (other.Defined && other.Name != kernel.Name)
            MarkRange(items, skip, other.Def);
    }

    GLSLWriter writer(files);

    writer.Raw("// " + NormalizePath(params.Config.SourcePath) + "\n");
    writer.Raw("#version " + std::to_string(scan.GlslVersion) + " core\n");

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (skip[i])
            continue;

        if (i == kernel.Def.Begin)
        {
            writer.Raw("layout(local_size_x = " + std::to_string(kernel.GroupSizeX)
                     + ", local_size_y = " + std::to_string(kernel.GroupSizeY)
                     + ", local_size_z = " + std::to_string(kernel.GroupSizeZ) + ") in;\n");
            writer.Raw("void main()\n{\n");

            for (size_t k = kernel.Body.Begin; k <= kernel.Body.End && k < items.size(); ++k)
            {
                if (items[k].K == PPItem::Kind::Token)
                    writer.Token(items[k].Tok);
                else
                    WriteMarker(writer, items[k]);
            }

            writer.Raw("}\n");
            i = kernel.Def.End;
            continue;
        }

        if (items[i].K == PPItem::Kind::Token)
            writer.Token(items[i].Tok);
        else
            WriteMarker(writer, items[i]);
    }

    return writer.Take();
}

} // namespace PrismShaderCompiler
