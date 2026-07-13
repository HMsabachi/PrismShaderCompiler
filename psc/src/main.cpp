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
#include <unordered_map>

namespace psc = PrismShaderCompiler;

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

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    CLI::App app{"psc — Prism Shader Compiler / Prism着色器编译器"};

    std::string input;
    std::string outputDir = ".";
    std::string includeDir = "Assets/Include";
    std::string engineDir  = "Assets/Engine";
    std::vector<std::string> defines;
    bool verbose = false;

    enum class Target : uint32_t { GLSL = 1, HLSL = 2, MSL = 4, SPIRV = 8, IR = 16, JSON = 32, All = 63 };
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

    std::filesystem::path inputPath(input);
    if (inputPath.extension() == ".ComputeShader")
    {
        auto compute = compiler.CompileComputeFile(input);
        if (compute.Kernels.empty())
        {
            spdlog::error("compilation failed: no kernels generated");
            return 1;
        }
        std::filesystem::create_directories(outputDir);
        if (targets == 0) targets |= (uint32_t)Target::IR;

        auto report = [](const psc::ComputeKernelOutput& out, const std::string& target) {
            for (auto& e : out.Errors)   spdlog::error("[{}] {}", target, e);
            for (auto& w : out.Warnings) spdlog::warn("[{}] {}", target, w);
            return out.Errors.empty();
        };

        for (uint32_t i = 0; i < compute.Kernels.size(); ++i)
        {
            std::string base = compute.ShaderName + "." + compute.Kernels[i].Name;
            for (auto& c : base)
                if (c == '/' || c == '\\') c = '_';

            if (targets & (uint32_t)Target::IR)
            {
                auto out = compiler.GenerateComputeIR(compute, i);
                if (report(out, "IR"))
                {
                    WriteFile((std::filesystem::path(outputDir) / (base + ".comp.ir")).string(), out.Source);
                    spdlog::info("{}.comp.ir", base);
                }
            }
            if (targets & (uint32_t)Target::SPIRV)
            {
                auto out = compiler.GenerateComputeSPIRV(compute, i);
                if (report(out, "SPIRV"))
                {
                    WriteBinaryFile((std::filesystem::path(outputDir) / (base + ".comp.spv")).string(), out.Spirv);
                    spdlog::info("{}.comp.spv", base);
                }
            }
            if (targets & (uint32_t)Target::GLSL)
            {
                auto out = compiler.GenerateComputeGLSL(compute, i);
                if (report(out, "GLSL"))
                {
                    WriteFile((std::filesystem::path(outputDir) / (base + ".comp.glsl")).string(), out.Source);
                    spdlog::info("{}.comp.glsl", base);
                }
            }
            if (targets & (uint32_t)Target::HLSL)
            {
                auto out = compiler.GenerateComputeHLSL(compute, i);
                if (report(out, "HLSL"))
                {
                    WriteFile((std::filesystem::path(outputDir) / (base + ".comp.hlsl")).string(), out.Source);
                    spdlog::info("{}.comp.hlsl", base);
                }
            }
            if (targets & (uint32_t)Target::MSL)
            {
                auto out = compiler.GenerateComputeMSL(compute, i);
                if (report(out, "MSL"))
                {
                    WriteFile((std::filesystem::path(outputDir) / (base + ".comp.metal")).string(), out.Source);
                    spdlog::info("{}.comp.metal", base);
                }
            }
        }

        if (targets & (uint32_t)Target::JSON)
        {
            std::string base = compute.ShaderName;
            for (auto& c : base)
                if (c == '/' || c == '\\') c = '_';
            auto jsonPath = std::filesystem::path(outputDir) / (base + ".meta.json");
            WriteFile(jsonPath.string(), psc::ToJson(compute));
            spdlog::info("{}.meta.json", base);
        }

        spdlog::info("done: {} kernel(s)", compute.Kernels.size());
        return 0;
    }

    auto shader = compiler.CompileFile(input);

    if (shader.Passes.empty())
    {
        spdlog::error("compilation failed: no passes generated");
        return 1;
    }

    std::filesystem::create_directories(outputDir);

    if (!(targets & 15)) targets |= (uint8_t)Target::GLSL; // 默认 glsl

    auto report = [](const psc::PassOutput& out, const std::string& target) {
        for (auto& e : out.Errors)   spdlog::error("[{}] {}", target, e);
        for (auto& w : out.Warnings) spdlog::warn("[{}] {}", target, w);
        return out.Errors.empty();
    };

    for (uint32_t i = 0; i < shader.Passes.size(); ++i)
    {
        std::string base = shader.Passes.size() > 1
            ? shader.ShaderName + "." + shader.Passes[i].Name
            : shader.ShaderName;
        for (auto& c : base)
            if (c == '/' || c == '\\') c = '_';

        if (targets & (uint32_t)Target::GLSL)
        {
            auto out = compiler.GenerateGLSL(shader, i, defines);
            if (report(out, "GLSL"))
            {
                WriteFile((std::filesystem::path(outputDir) / (base + ".vert.glsl")).string(), out.VertexShader);
                WriteFile((std::filesystem::path(outputDir) / (base + ".frag.glsl")).string(), out.FragmentShader);
                spdlog::info("{}.vert.glsl / {}.frag.glsl", base, base);
            }
        }
        if (targets & (uint32_t)Target::HLSL)
        {
            auto out = compiler.GenerateHLSL(shader, i, defines);
            if (report(out, "HLSL"))
            {
                WriteFile((std::filesystem::path(outputDir) / (base + ".vert.hlsl")).string(), out.VertexShader);
                WriteFile((std::filesystem::path(outputDir) / (base + ".frag.hlsl")).string(), out.FragmentShader);
                spdlog::info("{}.vert.hlsl / {}.frag.hlsl", base, base);
            }
        }
        if (targets & (uint32_t)Target::MSL)
        {
            auto out = compiler.GenerateMSL(shader, i, defines);
            if (report(out, "MSL"))
            {
                WriteFile((std::filesystem::path(outputDir) / (base + ".vert.metal")).string(), out.VertexShader);
                WriteFile((std::filesystem::path(outputDir) / (base + ".frag.metal")).string(), out.FragmentShader);
                spdlog::info("{}.vert.metal / {}.frag.metal", base, base);
            }
        }
        if (targets & (uint32_t)Target::SPIRV)
        {
            auto out = compiler.GenerateSPIRV(shader, i, defines);
            if (report(out, "SPIRV"))
            {
                WriteBinaryFile((std::filesystem::path(outputDir) / (base + ".vert.spv")).string(), out.SpirvVertex);
                WriteBinaryFile((std::filesystem::path(outputDir) / (base + ".frag.spv")).string(), out.SpirvFragment);
                spdlog::info("{}.vert.spv / {}.frag.spv", base, base);
            }
        }
        if (targets & (uint32_t)Target::IR)
        {
            auto out = compiler.GenerateIR(shader, i, defines);
            if (report(out, "IR"))
            {
                WriteFile((std::filesystem::path(outputDir) / (base + ".vert.ir")).string(), out.VertexShader);
                WriteFile((std::filesystem::path(outputDir) / (base + ".frag.ir")).string(), out.FragmentShader);
                spdlog::info("{}.vert.ir / {}.frag.ir", base, base);
            }
        }
    }

    if (targets & (uint32_t)Target::JSON)
    {
        std::string base = shader.ShaderName;
        for (auto& c : base)
            if (c == '/' || c == '\\') c = '_';
        auto jsonPath = std::filesystem::path(outputDir) / (base + ".meta.json");
        WriteFile(jsonPath.string(), psc::ToJson(shader));
        spdlog::info("{}.meta.json", base);
    }

    spdlog::info("done: {} pass(es)", shader.Passes.size());
    return 0;
}
