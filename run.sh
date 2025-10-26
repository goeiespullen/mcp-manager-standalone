#!/bin/bash
# MCP Manager - Build and Run Script

cd "$(dirname "$0")"

# Check for DISPLAY (X11)
if [ -z "$DISPLAY" ]; then
    echo "⚠️  Warning: No DISPLAY variable set"
    echo "Make sure you're running in a graphical environment"
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "📁 Creating build directory..."
    mkdir build
fi

# Build if necessary
if [ ! -f "build/mcp-manager" ]; then
    echo "🔨 Building MCP Manager..."
    cd build
    cmake .. || { echo "❌ CMake failed"; exit 1; }
    make -j$(nproc) || { echo "❌ Build failed"; exit 1; }
    cd ..
    echo "✅ Build complete"
fi

echo ""
echo "🚀 Starting MCP Manager..."
echo "   Gateway will listen on port 8700"
echo ""

# Run the application
./build/mcp-manager "$@"
