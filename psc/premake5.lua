project "psc"
    kind "ConsoleApp"
    language "C++"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "src",
        "../PrismShaderCore/include",
        "../vendor/CLI11/Include",
        "../vendor/spdlog/include",
    }

    links {
        "PrismShaderCore",
    }

    files {
        "src/**.h",
        "src/**.cpp",
    }

    filter "configurations:Debug"
        runtime "Debug"

    filter "configurations:Release"
        runtime "Release"

    filter "configurations:Dist"
        runtime "Release"

    filter "system:windows"
        cppdialect "C++20"
        systemversion "latest"
        buildoptions { "/utf-8" }
