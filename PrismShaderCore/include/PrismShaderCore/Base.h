#pragma once

#ifdef _WIN32
    #ifdef _WIN64
        #define PSC_PLATFORM_WINDOWS
    #else
        #error PrismShaderCompiler only supports 64-bit Windows!
    #endif
#elif defined(__linux__)
    #define PSC_PLATFORM_LINUX
#elif defined(__APPLE__) || defined(__MACH__)
    #define PSC_PLATFORM_MACOS
#endif

// PSC_SHARED + PSC_BUILD → dllexport
// PSC_SHARED only         → dllimport
// neither                 → static lib, empty
#ifdef PSC_SHARED
    #ifdef _WIN32
        #ifdef PSC_BUILD
            #define PSC_API __declspec(dllexport)
        #else
            #define PSC_API __declspec(dllimport)
        #endif
    #else
        #ifdef PSC_BUILD
            #define PSC_API __attribute__((visibility("default")))
        #else
            #define PSC_API
        #endif
    #endif
#else
    #define PSC_API
#endif

#define PSC_BIT(x) (1 << (x))
