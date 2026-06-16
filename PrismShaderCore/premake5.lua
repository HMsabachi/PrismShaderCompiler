project "PrismShaderCore"
    kind "StaticLib"
    language "C++"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "include/PrismShaderCore",
        "src",
        -- TODO Phase 2: "..\\vendor\\glslang",
        -- TODO Phase 3: "..\\vendor\\SPIRV-Cross",
    }

    -- TODO Phase 2+: uncomment when vendor libs are built
    -- libdirs { "..\\vendor\\lib" }
    -- links { "glslang", "SPIRV", "OGLCompiler", "OSDependent", "spirv-cross-glsl" }

    defines { "PSC_BUILD" }

    files {
        "include/**.h",
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
