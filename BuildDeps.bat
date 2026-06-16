@echo off
setlocal
echo ============================================================
echo PrismShaderCompiler — Build Dependencies
echo ============================================================

set ROOT=%~dp0
set VENDOR=%ROOT%vendor
set LIB=%VENDOR%lib

if not exist "%LIB%" mkdir "%LIB%"

:: ============================================================
:: glslang
:: ============================================================
if exist "%LIB%\glslang.lib" if exist "%LIB%\SPIRV.lib" if exist "%LIB%\OSDependent.lib" (
    echo glslang libs found, skipping.
    goto :spirv_cross
)

echo Building glslang...
cd /d "%VENDOR%\glslang"
if exist build rmdir /s /q build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DENABLE_GLSLANG_INSTALL=OFF ^
    -DBUILD_TESTING=OFF ^
    -DENABLE_SPVREMAPPER=OFF ^
    -DENABLE_HLSL=OFF ^
    -DENABLE_OPT=OFF
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] glslang cmake failed & pause & exit /b 1 )
cmake --build . --config Release --parallel
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] glslang build failed & pause & exit /b 1 )
copy glslang\Release\glslang.lib "%LIB%\" >nul
copy glslang\Release\glslang-default-resource-limits.lib "%LIB%\" >nul
copy SPIRV\Release\SPIRV.lib "%LIB%\" >nul
if exist OSDependent\Windows\Release (
    copy OSDependent\Windows\Release\OSDependent.lib "%LIB%\" >nul
) else (
    copy OSDependent\Release\OSDependent.lib "%LIB%\" >nul
)
echo glslang done.

:: ============================================================
:: SPIRV-Cross
:: ============================================================
:spirv_cross
if exist "%LIB%\spirv-cross-glsl.lib" (
    echo spirv-cross-glsl.lib found, skipping.
    goto :done
)

echo Building SPIRV-Cross...
cd /d "%VENDOR%\SPIRV-Cross"
if exist build rmdir /s /q build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DSPIRV_CROSS_CLI=OFF ^
    -DSPIRV_CROSS_ENABLE_TESTS=OFF
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] SPIRV-Cross cmake failed & pause & exit /b 1 )
cmake --build . --config Release --parallel
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] SPIRV-Cross build failed & pause & exit /b 1 )
copy Release\spirv-cross-glsl.lib "%LIB%\" >nul
copy Release\spirv-cross-core.lib "%LIB%\" >nul
echo SPIRV-Cross done.

:done
echo.
echo Dependencies ready.
endlocal
