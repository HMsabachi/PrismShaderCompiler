#include <PrismShaderCore/Compiler.h>
#include <PrismShaderCore/Metadata.h>
#include <CLI/CLI11.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fstream>
#include <filesystem>

namespace psc = PrismShaderCompiler;

static void WriteFile(const std::string& path, const std::string& content)
{
    std::ofstream out(path, std::ios::binary);
    if (out) out.write(content.data(), content.size());
}

int main(int argc, char* argv[])
{
    CLI::App app{"psc — Prism Shader Compiler"};

    std::string input;
    std::string outputDir = ".";
    std::string includeDir = "Assets/Include";
    std::string engineDir  = "Assets/Engine";
    std::vector<std::string> defines;
    bool verbose = false;
    bool jsonOutput = false;

    app.add_option("input", input, "Input .Shader file")->required();
    app.add_option("-o,--output", outputDir, "Output directory");
    app.add_option("-I", includeDir, "Include search path");
    app.add_option("-E", engineDir, "Engine header path");
    app.add_option("-D", defines, "Define shader keyword");
    app.add_flag("--json", jsonOutput, "Output metadata as JSON");
    app.add_flag("-v,--verbose", verbose, "Verbose output");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    // Logging
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
    auto shader = compiler.CompileFile(input);

    if (shader.Passes.empty())
    {
        spdlog::error("compilation failed: no passes generated");
        return 1;
    }

    std::filesystem::create_directories(outputDir);

    for (uint32_t i = 0; i < shader.Passes.size(); ++i)
    {
        auto out = compiler.GenerateGLSL(shader, i, defines);
        std::string base = shader.Passes.size() > 1
            ? shader.ShaderName + "." + shader.Passes[i].Name
            : shader.ShaderName;
        for (auto& c : base)
            if (c == '/' || c == '\\') c = '_';

        auto vertPath = std::filesystem::path(outputDir) / (base + ".vert");
        auto fragPath = std::filesystem::path(outputDir) / (base + ".frag");

        WriteFile(vertPath.string(), out.VertexShader);
        WriteFile(fragPath.string(), out.FragmentShader);
        spdlog::info("{}.vert / {}.frag", base, base);
    }

    if (jsonOutput)
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
