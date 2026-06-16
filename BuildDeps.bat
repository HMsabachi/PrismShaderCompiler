@echo off
setlocal
echo ============================================================
echo PrismShaderCompiler — Build Dependencies
echo ============================================================

set ROOT=%~dp0
set VENDOR=%ROOT%vendor
set LIB=%VENDOR%\lib

if not exist "%LIB%\Release" mkdir "%LIB%\Release"
if not exist "%LIB%\Debug"   mkdir "%LIB%\Debug"

:: ============================================================
:: glslang
:: ============================================================
if not exist "%LIB%\Release\glslang.lib"    goto :build_glslang
if not exist "%LIB%\Release\OSDependent.lib" goto :build_glslang
if not exist "%LIB%\Debug\glslang.lib"       goto :build_glslang
if not exist "%LIB%\Debug\OSDependent.lib"   goto :build_glslang
echo glslang libs found, skipping.
goto :spirv_cross

:build_glslang

echo Building glslang (Release)...
call :glslang_build Release
echo Building glslang (Debug)...
call :glslang_build Debug
echo glslang done.
goto :spirv_cross

:glslang_build
cd /d "%VENDOR%\glslang"
if exist build rmdir /s /q build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%1 ^
    -DENABLE_GLSLANG_INSTALL=OFF ^
    -DBUILD_TESTING=OFF ^
    -DENABLE_SPVREMAPPER=OFF ^
    -DENABLE_HLSL=OFF ^
    -DENABLE_OPT=OFF
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] glslang cmake failed & pause & exit /b 1 )
cmake --build . --config %1 --parallel
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] glslang build failed & pause & exit /b 1 )
if "%1"=="Debug" (set SUFFIX=d) else (set SUFFIX=)
copy glslang\%1\glslang%SUFFIX%.lib "%LIB%\%1\glslang.lib" || echo [WARN] glslang%SUFFIX%.lib missing
copy glslang\%1\glslang-default-resource-limits%SUFFIX%.lib "%LIB%\%1\glslang-default-resource-limits.lib" || echo [WARN] resource-limits missing
copy SPIRV\%1\SPIRV%SUFFIX%.lib "%LIB%\%1\SPIRV.lib" || echo [WARN] SPIRV.lib missing
if exist glslang\OSDependent\Windows\%1 (
    copy glslang\OSDependent\Windows\%1\OSDependent%SUFFIX%.lib "%LIB%\%1\OSDependent.lib" || echo [WARN] OSDependent.lib missing
) else (
    copy glslang\OSDependent\%1\OSDependent%SUFFIX%.lib "%LIB%\%1\OSDependent.lib" || echo [WARN] OSDependent.lib missing
)
exit /b

:: ============================================================
:: SPIRV-Cross
:: ============================================================
:spirv_cross
if not exist "%LIB%\Release\spirv-cross-glsl.lib"  goto :build_spirv
if not exist "%LIB%\Release\spirv-cross-core.lib"  goto :build_spirv
if not exist "%LIB%\Debug\spirv-cross-glsl.lib"    goto :build_spirv
if not exist "%LIB%\Debug\spirv-cross-core.lib"    goto :build_spirv
echo spirv-cross libs found, skipping.
goto :done

:build_spirv

echo Building SPIRV-Cross (Release)...
call :spirv_build Release
echo Building SPIRV-Cross (Debug)...
call :spirv_build Debug
echo SPIRV-Cross done.
goto :done

:spirv_build
if "%1"=="Debug" (set SUFFIX=d) else (set SUFFIX=)
cd /d "%VENDOR%\SPIRV-Cross"
if exist build rmdir /s /q build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%1 ^
    -DSPIRV_CROSS_CLI=OFF ^
    -DSPIRV_CROSS_ENABLE_TESTS=OFF
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] SPIRV-Cross cmake failed & pause & exit /b 1 )
cmake --build . --config %1 --parallel
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] SPIRV-Cross build failed & pause & exit /b 1 )
for %%f in (spirv-cross-glsl spirv-cross-core spirv-cross-hlsl spirv-cross-msl) do (
    copy %1\%%f%SUFFIX%.lib "%LIB%\%1\%%f.lib" || echo [WARN] %%f%SUFFIX%.lib missing
)
exit /b

:done
echo.
echo Dependencies ready.
endlocal
