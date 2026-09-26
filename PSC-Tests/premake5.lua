project "PSC-Tests"
    kind "ConsoleApp"
    language "C++"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs {
        "src",
        "../PrismShaderCore/include",
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
        libdirs { "../vendor/lib/Debug" }

    filter "configurations:Release"
        runtime "Release"
        libdirs { "../vendor/lib/Release" }

    filter "configurations:Dist"
        runtime "Release"
        libdirs { "../vendor/lib/Release" }

    filter "system:windows"
        cppdialect "C++20"
        systemversion "latest"
        buildoptions { "/utf-8" }
