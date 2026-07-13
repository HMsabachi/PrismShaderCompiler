#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

#include <PrismShaderCore/Compiler.h>
#include <PrismShaderCore/Metadata.h>
#include <CLI/CLI11.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fstream>
#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace psc = PrismShaderCompiler;

enum class Target : uint32_t
{
    GLSL  = 1,
    HLSL  = 2,
    MSL   = 4,
    SPIRV = 8,
    IR    = 16,
    JSON  = 32,
    All   = 63,
};

static void WriteFile(const std::string& path, const std::string& content)
{
    std::ofstream out(path, std::ios::binary);
    if (out) out.write(content.data(), content.size());
}

static void WriteBinaryFile(const std::string& path, const std::vector<uint32_t>& data)
{
    std::ofstream out(path, std::ios::binary);
    if (out)
        out.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint32_t));
}

static std::string SanitizeName(std::string name)
{
    for (auto& c : name)
        if (c == '/' || c == '\\') c = '_';
    return name;
}

template<typename T>
static bool ReportDiag(const T& out, std::string_view target)
{
    for (const auto& e : out.Errors)   spdlog::error("[{}] {}", target, e);
    for (const auto& w : out.Warnings) spdlog::warn("[{}] {}", target, w);
    return out.Errors.empty();
}

struct ComputeTargetSpec
{
    Target flag;
    const char* name;
    const char* ext;
    bool binary;
    psc::ComputeKernelOutput (psc::ShaderCompiler::*gen)(const psc::CompiledComputeShader&, uint32_t);
};

static const ComputeTargetSpec kComputeTargets[] = {
    { Target::IR,    "IR",    ".comp.ir",    false, &psc::ShaderCompiler::GenerateComputeIR    },
    { Target::SPIRV, "SPIRV", ".comp.spv",   true,  &psc::ShaderCompiler::GenerateComputeSPIRV },
    { Target::GLSL,  "GLSL",  ".comp.glsl",  false, &psc::ShaderCompiler::GenerateComputeGLSL  },
    { Target::HLSL,  "HLSL",  ".comp.hlsl",  false, &psc::ShaderCompiler::GenerateComputeHLSL  },
    { Target::MSL,   "MSL",   ".comp.metal", false, &psc::ShaderCompiler::GenerateComputeMSL   },
};

struct ShaderTargetSpec
{
    Target flag;
    const char* name;
    const char* vertExt;
    const char* fragExt;
    bool binary;
    psc::PassOutput (psc::ShaderCompiler::*gen)(const psc::CompiledShader&, uint32_t, const std::vector<std::string>&);
};

static const ShaderTargetSpec kShaderTargets[] = {
    { Target::GLSL,  "GLSL",  ".vert.glsl",  ".frag.glsl",  false, &psc::ShaderCompiler::GenerateGLSL  },
    { Target::HLSL,  "HLSL",  ".vert.hlsl",  ".frag.hlsl",  false, &psc::ShaderCompiler::GenerateHLSL  },
    { Target::MSL,   "MSL",   ".vert.metal", ".frag.metal", false, &psc::ShaderCompiler::GenerateMSL   },
    { Target::SPIRV, "SPIRV", ".vert.spv",   ".frag.spv",   true,  &psc::ShaderCompiler::GenerateSPIRV },
    { Target::IR,    "IR",    ".vert.ir",    ".frag.ir",    false, &psc::ShaderCompiler::GenerateIR    },
};

static int EmitCompute(psc::ShaderCompiler& compiler, const std::string& input,
                       uint32_t targets, const std::string& outputDir)
{
    auto compute = compiler.CompileComputeFile(input);
    if (compute.Kernels.empty())
    {
        spdlog::error("compilation failed: no kernels generated");
        return 1;
    }

    std::filesystem::create_directories(outputDir);
    if (targets == 0) targets |= (uint32_t)Target::IR;

    for (uint32_t i = 0; i < compute.Kernels.size(); ++i)
    {
        std::string base = SanitizeName(compute.ShaderName + "." + compute.Kernels[i].Name);
        for (const auto& spec : kComputeTargets)
        {
            if (!(targets & (uint32_t)spec.flag)) continue;
            auto out = (compiler.*spec.gen)(compute, i);
            if (!ReportDiag(out, spec.name)) continue;
            if (spec.binary ? out.Spirv.empty() : out.Source.empty()) continue;
            auto path = std::filesystem::path(outputDir) / (base + spec.ext);
            if (spec.binary)
                WriteBinaryFile(path.string(), out.Spirv);
            else
                WriteFile(path.string(), out.Source);
            spdlog::info("{}{}", base, spec.ext);
        }
    }

    if (targets & (uint32_t)Target::JSON)
    {
        std::string base = SanitizeName(compute.ShaderName);
        auto path = std::filesystem::path(outputDir) / (base + ".meta.json");
        WriteFile(path.string(), psc::ToJson(compute));
        spdlog::info("{}.meta.json", base);
    }

    spdlog::info("done: {} kernel(s)", compute.Kernels.size());
    return 0;
}

static int EmitShader(psc::ShaderCompiler& compiler, const std::string& input,
                      uint32_t targets, const std::string& outputDir,
                      const std::vector<std::string>& defines)
{
    auto shader = compiler.CompileFile(input);
    if (shader.Passes.empty())
    {
        spdlog::error("compilation failed: no passes generated");
        return 1;
    }

    std::filesystem::create_directories(outputDir);

    const uint32_t kAnyBody = (uint32_t)Target::GLSL | (uint32_t)Target::HLSL
                            | (uint32_t)Target::MSL  | (uint32_t)Target::SPIRV
                            | (uint32_t)Target::IR;
    if (!(targets & kAnyBody)) targets |= (uint32_t)Target::GLSL;

    for (uint32_t i = 0; i < shader.Passes.size(); ++i)
    {
        std::string base = shader.Passes.size() > 1
            ? SanitizeName(shader.ShaderName + "." + shader.Passes[i].Name)
            : SanitizeName(shader.ShaderName);

        for (const auto& spec : kShaderTargets)
        {
            if (!(targets & (uint32_t)spec.flag)) continue;
            auto out = (compiler.*spec.gen)(shader, i, defines);
            if (!ReportDiag(out, spec.name)) continue;
            if (spec.binary ? (out.SpirvVertex.empty() || out.SpirvFragment.empty()) : (out.VertexShader.empty() || out.FragmentShader.empty())) continue;
            auto dir = std::filesystem::path(outputDir);
            if (spec.binary)
            {
                WriteBinaryFile((dir / (base + spec.vertExt)).string(), out.SpirvVertex);
                WriteBinaryFile((dir / (base + spec.fragExt)).string(), out.SpirvFragment);
            }
            else
            {
                WriteFile((dir / (base + spec.vertExt)).string(), out.VertexShader);
                WriteFile((dir / (base + spec.fragExt)).string(), out.FragmentShader);
            }
            spdlog::info("{}{} / {}{}", base, spec.vertExt, base, spec.fragExt);
        }
    }

    if (targets & (uint32_t)Target::JSON)
    {
        std::string base = SanitizeName(shader.ShaderName);
        auto path = std::filesystem::path(outputDir) / (base + ".meta.json");
        WriteFile(path.string(), psc::ToJson(shader));
        spdlog::info("{}.meta.json", base);
    }

    spdlog::info("done: {} pass(es)", shader.Passes.size());
    return 0;
}

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    CLI::App app{"psc - Prism Shader Compiler / Prism着色器编译器"};

    std::string input;
    std::string outputDir = ".";
    std::string includeDir = "Assets/Include";
    std::string engineDir  = "Assets/Engine";
    std::vector<std::string> defines;
    bool verbose = false;

    uint32_t targets = 0;
    auto addTarget = [&](std::string flags, Target t, std::string desc) {
        app.add_flag(flags, [&targets, t](int64_t) { targets |= (uint32_t)t; }, desc);
    };

    app.add_option("input", input, "Input .Shader file / 输入文件")->required();
    app.add_option("-o,--output", outputDir, "Output directory / 输出目录");
    app.add_option("-I", includeDir, "Include search path / Include搜索路径");
    app.add_option("-E", engineDir, "Engine header path / 引擎头文件路径");
    app.add_option("-D", defines, "Define shader keyword / 定义关键字");
    addTarget("-g,--glsl",  Target::GLSL,  "Generate GLSL / 生成GLSL");
    addTarget("-l,--hlsl",  Target::HLSL,  "Generate HLSL / 生成HLSL");
    addTarget("-m,--msl",   Target::MSL,   "Generate MSL / 生成MSL");
    addTarget("-s,--spirv", Target::SPIRV, "Generate SPIR-V / 生成SPIR-V");
    addTarget("-r,--ir",    Target::IR,    "Generate IR / 生成IR");
    addTarget("-j,--json",  Target::JSON,  "Output metadata JSON / 输出元数据JSON");
    addTarget("-a,--all",   Target::All,   "Generate all targets + JSON / 生成全部目标+JSON");
    app.add_flag("-v,--verbose", verbose, "Verbose output / 详细输出");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("psc", sink);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%^%l%$] %v");
    if (verbose) spdlog::set_level(spdlog::level::debug);

    psc::CompilerConfig config;
    config.IncludeRoot = includeDir;
    config.EngineRoot  = engineDir;
    config.OnLog = [](psc::LogLevel lv, const std::string& msg) {
        switch (lv) {
        case psc::LogLevel::Debug:   spdlog::debug(msg);   break;
        case psc::LogLevel::Info:    spdlog::info(msg);    break;
        case psc::LogLevel::Warning: spdlog::warn(msg);    break;
        case psc::LogLevel::Error:   spdlog::error(msg);   break;
        case psc::LogLevel::Fatal:   spdlog::critical(msg); break;
        }
    };
    config.ReadFile = psc::Callbacks::ReadFileFromDisk;
    psc::ShaderCompiler compiler(config);

    std::filesystem::path inputPath(input);
    bool isCompute = (inputPath.extension() == ".ComputeShader");

    if (!isCompute)
    {
        std::filesystem::path scanRoot = std::filesystem::absolute(input).parent_path();
        auto shaderMap = compiler.ScanShaderDirectory(scanRoot.string());
        config.ResolveUsePass = [&compiler, &shaderMap](const std::string& shaderName) -> psc::CompiledShader {
            auto it = shaderMap.find(shaderName);
            if (it != shaderMap.end())
            {
                spdlog::info("UsePass: resolving '{}' -> '{}'", shaderName, it->second);
                return compiler.CompileFile(it->second);
            }
            spdlog::error("UsePass: Shader '{}' not found", shaderName);
            return {};
        };
        compiler.SetConfig(config);
    }

    if (isCompute)
        return EmitCompute(compiler, input, targets, outputDir);
    return EmitShader(compiler, input, targets, outputDir, defines);
}
