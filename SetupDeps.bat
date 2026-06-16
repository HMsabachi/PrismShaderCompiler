@echo off
setlocal
echo ============================================================
echo PrismShaderCompiler — Generate & Build
echo ============================================================

set ROOT=%~dp0
cd /d "%ROOT%"

:: ============================================================
:: 1. Find premake5
:: ============================================================
set "PREMAKE=%ROOT%vendor\Premake\Bin\premake5.exe"
if not exist "%PREMAKE%" (
    echo [ERROR] premake5.exe not found at: vendor\Premake\Bin\premake5.exe
    pause
    exit /b 1
)
echo [1/3] premake5 found: %PREMAKE%

:: ============================================================
:: 2. Generate VS2022 solution
:: ============================================================
echo [2/3] Generating VS2022 solution...
call "%PREMAKE%" vs2022
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] premake5 failed
    pause
    exit /b 1
)

:: ============================================================
:: 3. Build (optional, skip if msbuild not in PATH)
:: ============================================================
where msbuild >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [3/3] msbuild not found in PATH. Solution generated.
    echo   Open PrismShaderCompiler.sln in Visual Studio to build.
    goto :done
)

echo [3/3] Building Debug x64...
msbuild PrismShaderCompiler.sln /p:Configuration=Debug /p:Platform=x64 /m
if %ERRORLEVEL% NEQ 0 (
    echo [WARNING] Build had errors. Check output above.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo Build completed successfully!
echo ============================================================

:done
endlocal
