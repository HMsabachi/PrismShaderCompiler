project "PrismShaderCore"
    kind "StaticLib"
    language "C++"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include/PrismShaderCore",
        "src",
        "..\\vendor\\json\\include",
        "..\\vendor\\glslang",
        "..\\vendor\\SPIRV-Cross",
    }

    links { "glslang", "SPIRV", "OSDependent", "glslang-default-resource-limits", "spirv-cross-core", "spirv-cross-glsl", "spirv-cross-hlsl", "spirv-cross-msl" }

    defines { "PSC_BUILD" }

    files {
        "include/**.h",
        "src/**.h",
        "src/**.cpp",
    }

    filter "configurations:Debug"
        runtime "Debug"
        libdirs { "..\\vendor\\lib\\Debug" }

    filter "configurations:Release"
        runtime "Release"
        libdirs { "..\\vendor\\lib\\Release" }

    filter "configurations:Dist"
        runtime "Release"
        libdirs { "..\\vendor\\lib\\Release" }

    filter "system:windows"
        cppdialect "C++20"
        systemversion "latest"
        buildoptions { "/utf-8" }
