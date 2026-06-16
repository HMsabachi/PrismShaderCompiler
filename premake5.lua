workspace "PrismShaderCompiler"
    architecture "x86_64"
    startproject "psc"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    -- 子项目
    include "PrismShaderCore"
    include "psc"


    -- 公共过滤配置
    filter "system:windows"
        cppdialect "C++20"
        systemversion "latest"
        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        defines "PSC_DEBUG"
        optimize "Off"
        symbols "On"

    filter "configurations:Release"
        defines { "PSC_RELEASE", "NDEBUG" }
        optimize "On"

    filter "configurations:Dist"
        defines { "PSC_DIST", "NDEBUG" }
        optimize "On"
        symbols "Off"
