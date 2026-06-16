@echo off
setlocal
echo ============================================================
echo PrismShaderCompiler — Generate Projects
echo ============================================================

set ROOT=%~dp0
set PREMAKE=%ROOT%vendor\Premake\Bin\premake5.exe

if not exist "%PREMAKE%" (
    echo [ERROR] premake5.exe not found at: %PREMAKE%
    pause
    exit /b 1
)

cd /d "%ROOT%"
call "%PREMAKE%" vs2022
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] premake5 failed
    pause
    exit /b 1
)

echo All done.

:done
endlocal
