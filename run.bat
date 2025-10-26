@echo off
REM MCP Manager - Windows Run Script

cd /d "%~dp0"

echo ================================================================================
echo MCP Manager - Starting...
echo ================================================================================
echo.

REM Check if built
if not exist "build\Release\mcp-manager.exe" (
    if not exist "build\Debug\mcp-manager.exe" (
        echo ❌ MCP Manager not built yet!
        echo.
        echo Please build first:
        echo   build.bat
        echo.
        echo Or manually:
        echo   mkdir build ^&^& cd build
        echo   cmake .. -G "Visual Studio 17 2022"
        echo   cmake --build . --config Release
        echo.
        pause
        exit /b 1
    )
)

REM Determine which build to use
if exist "build\Release\mcp-manager.exe" (
    set BUILD_TYPE=Release
) else (
    set BUILD_TYPE=Debug
)

echo ℹ️  Using %BUILD_TYPE% build
echo.

REM Check if Qt DLLs are available
if defined CMAKE_PREFIX_PATH (
    echo ✅ Qt path: %CMAKE_PREFIX_PATH%
    set "PATH=%CMAKE_PREFIX_PATH%\bin;%PATH%"
) else (
    echo ⚠️  CMAKE_PREFIX_PATH not set
    echo    If you get DLL errors, set Qt path:
    echo    set CMAKE_PREFIX_PATH=C:\Qt\6.5.0\msvc2022_64
    echo.
)

echo 🚀 Starting MCP Manager...
echo    Gateway will listen on port 8700
echo.

REM Run MCP Manager
start "" "build\%BUILD_TYPE%\mcp-manager.exe"

if errorlevel 1 (
    echo.
    echo ❌ Failed to start MCP Manager
    echo.
    echo Common issues:
    echo   1. Qt DLLs not found - Set CMAKE_PREFIX_PATH
    echo   2. Visual C++ Runtime missing - Install VC++ Redistributable
    echo   3. Port 8700 already in use - Kill conflicting process
    echo.
    pause
    exit /b 1
)

echo.
echo ✅ MCP Manager started
echo.
