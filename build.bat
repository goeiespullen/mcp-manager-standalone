@echo off
REM MCP Manager - Windows Build Script
REM
REM This script builds MCP Manager on Windows using CMake and Visual Studio

setlocal EnableDelayedExpansion

echo ================================================================================
echo MCP Manager - Windows Build Script
echo ================================================================================
echo.

REM Check if CMake is installed
where cmake >nul 2>&1
if errorlevel 1 (
    echo ❌ CMake is not found in PATH
    echo.
    echo Please install CMake from: https://cmake.org/download/
    echo Make sure to check "Add CMake to system PATH" during installation
    echo.
    pause
    exit /b 1
)

REM Get CMake version
for /f "tokens=3" %%i in ('cmake --version 2^>^&1 ^| findstr /C:"version"') do set CMAKE_VERSION=%%i
echo ✅ Found CMake %CMAKE_VERSION%

REM Check for Visual Studio
where cl >nul 2>&1
if errorlevel 1 (
    echo.
    echo ⚠️  Visual Studio C++ compiler not found in PATH
    echo.
    echo You need to either:
    echo   1. Run this script from "Developer Command Prompt for VS"
    echo   2. Or install Visual Studio 2019+ with C++ workload
    echo   3. Or use MinGW (see INSTALL.md)
    echo.
    set /p CONTINUE="Continue anyway? (y/N): "
    if /i not "!CONTINUE!"=="y" (
        exit /b 1
    )
)

REM Check for Qt (optional check - CMake will do the real check)
if defined CMAKE_PREFIX_PATH (
    echo ✅ Qt path set: %CMAKE_PREFIX_PATH%
) else (
    echo.
    echo ⚠️  CMAKE_PREFIX_PATH not set
    echo.
    echo Please set Qt path before building:
    echo   set CMAKE_PREFIX_PATH=C:\Qt\6.5.0\msvc2022_64
    echo.
    echo Or install Qt from: https://www.qt.io/download-qt-installer
    echo.
    set /p QT_PATH="Enter Qt path (or press Enter to skip): "
    if not "!QT_PATH!"=="" (
        set CMAKE_PREFIX_PATH=!QT_PATH!
        echo ✅ Set CMAKE_PREFIX_PATH=!QT_PATH!
    )
)

echo.
echo ================================================================================
echo Starting build process...
echo ================================================================================
echo.

REM Create build directory
if not exist build (
    echo 📁 Creating build directory...
    mkdir build
) else (
    echo ℹ️  Using existing build directory
)

cd build

REM Configure with CMake
echo.
echo 🔧 Configuring with CMake...
echo.

cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo.
    echo ❌ CMake configuration failed
    echo.
    echo Common fixes:
    echo   1. Set Qt path: set CMAKE_PREFIX_PATH=C:\Qt\6.5.0\msvc2022_64
    echo   2. Install Visual Studio 2022 with C++ workload
    echo   3. Check INSTALL.md for detailed instructions
    echo.
    cd ..
    pause
    exit /b 1
)

REM Build
echo.
echo 🔨 Building MCP Manager (Release)...
echo.

cmake --build . --config Release --parallel
if errorlevel 1 (
    echo.
    echo ❌ Build failed
    echo.
    echo Check the error messages above and consult INSTALL.md
    echo.
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ================================================================================
echo ✅ Build successful!
echo ================================================================================
echo.
echo Binary location:
echo   build\Release\mcp-manager.exe
echo.
echo To run MCP Manager:
echo   run.bat
echo.
echo Or manually:
echo   build\Release\mcp-manager.exe
echo.
echo ================================================================================
pause
