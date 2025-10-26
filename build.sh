#!/bin/bash
# MCP Manager - Linux/macOS Build Script
#
# This script builds MCP Manager with proper dependency checking

set -e  # Exit on error

echo "================================================================================"
echo "MCP Manager - Build Script"
echo "================================================================================"
echo ""

cd "$(dirname "$0")"

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
    NPROC=$(nproc)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
    NPROC=$(sysctl -n hw.ncpu)
else
    OS="Unknown"
    NPROC=4
fi

echo "🖥️  Platform: $OS"
echo ""

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found"
    echo ""
    if [ "$OS" = "Linux" ]; then
        echo "Install with:"
        echo "  sudo apt install cmake        # Ubuntu/Debian"
        echo "  sudo dnf install cmake        # Fedora/RHEL"
        echo "  sudo pacman -S cmake          # Arch Linux"
    elif [ "$OS" = "macOS" ]; then
        echo "Install with:"
        echo "  brew install cmake"
    fi
    echo ""
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
echo "✅ CMake version: $CMAKE_VERSION"

# Check for C++ compiler
if command -v g++ &> /dev/null; then
    CXX_COMPILER="g++"
    CXX_VERSION=$(g++ --version | head -n1)
    echo "✅ C++ compiler: $CXX_VERSION"
elif command -v clang++ &> /dev/null; then
    CXX_COMPILER="clang++"
    CXX_VERSION=$(clang++ --version | head -n1)
    echo "✅ C++ compiler: $CXX_VERSION"
else
    echo "❌ No C++ compiler found"
    echo ""
    if [ "$OS" = "Linux" ]; then
        echo "Install with:"
        echo "  sudo apt install build-essential    # Ubuntu/Debian"
        echo "  sudo dnf install gcc-c++            # Fedora/RHEL"
        echo "  sudo pacman -S base-devel           # Arch Linux"
    elif [ "$OS" = "macOS" ]; then
        echo "Install with:"
        echo "  xcode-select --install"
    fi
    echo ""
    exit 1
fi

# Check for Qt
echo ""
echo "🔍 Checking for Qt..."
if [ -n "$CMAKE_PREFIX_PATH" ]; then
    echo "✅ CMAKE_PREFIX_PATH set: $CMAKE_PREFIX_PATH"
elif command -v qmake &> /dev/null; then
    QT_PATH=$(qmake -query QT_INSTALL_PREFIX)
    echo "✅ Found Qt at: $QT_PATH"
    export CMAKE_PREFIX_PATH="$QT_PATH"
elif [ "$OS" = "macOS" ]; then
    # Try Homebrew Qt locations
    if [ -d "$(brew --prefix qt@6 2>/dev/null)" ]; then
        export CMAKE_PREFIX_PATH=$(brew --prefix qt@6)
        echo "✅ Found Qt 6 via Homebrew: $CMAKE_PREFIX_PATH"
    elif [ -d "$(brew --prefix qt@5 2>/dev/null)" ]; then
        export CMAKE_PREFIX_PATH=$(brew --prefix qt@5)
        echo "✅ Found Qt 5 via Homebrew: $CMAKE_PREFIX_PATH"
    fi
else
    echo "⚠️  Qt not found automatically"
    echo ""
    if [ "$OS" = "Linux" ]; then
        echo "Install with:"
        echo "  sudo apt install qtbase5-dev        # Ubuntu/Debian Qt 5"
        echo "  sudo apt install qt6-base-dev       # Ubuntu/Debian Qt 6"
        echo "  sudo dnf install qt5-qtbase-devel   # Fedora/RHEL Qt 5"
        echo "  sudo dnf install qt6-qtbase-devel   # Fedora/RHEL Qt 6"
        echo "  sudo pacman -S qt5-base             # Arch Linux Qt 5"
        echo "  sudo pacman -S qt6-base             # Arch Linux Qt 6"
    elif [ "$OS" = "macOS" ]; then
        echo "Install with:"
        echo "  brew install qt@6"
        echo ""
        echo "Then set:"
        echo "  export CMAKE_PREFIX_PATH=\$(brew --prefix qt@6)"
    fi
    echo ""
    echo "Or download from: https://www.qt.io/download-qt-installer"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ""
echo "================================================================================"
echo "Starting build process..."
echo "================================================================================"
echo ""

# Create build directory
if [ ! -d "build" ]; then
    echo "📁 Creating build directory..."
    mkdir build
else
    echo "ℹ️  Using existing build directory"
fi

cd build

# Configure with CMake
echo ""
echo "🔧 Configuring with CMake..."
echo ""

cmake .. || {
    echo ""
    echo "❌ CMake configuration failed"
    echo ""
    echo "Common fixes:"
    echo "  1. Set Qt path: export CMAKE_PREFIX_PATH=/path/to/qt"
    echo "  2. Install Qt development packages"
    echo "  3. Check INSTALL.md for detailed instructions"
    echo ""
    exit 1
}

# Build
echo ""
echo "🔨 Building MCP Manager (using $NPROC cores)..."
echo ""

make -j$NPROC || {
    echo ""
    echo "❌ Build failed"
    echo ""
    echo "Check the error messages above and consult INSTALL.md"
    echo ""
    exit 1
}

cd ..

echo ""
echo "================================================================================"
echo "✅ Build successful!"
echo "================================================================================"
echo ""
echo "Binary location:"
echo "  build/mcp-manager"
echo ""
echo "To run MCP Manager:"
echo "  ./run.sh"
echo ""
echo "Or manually:"
echo "  ./build/mcp-manager"
echo ""
echo "================================================================================"
