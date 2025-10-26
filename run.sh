#!/bin/bash
# Azure DevOps MCP Server Launcher

cd "$(dirname "$0")"

# Check if built
if [ ! -f "build/azuredevops-mcp-server" ]; then
    echo "❌ Application not built yet!"
    echo "Run: cd build && cmake .. && make"
    exit 1
fi

# Check for DISPLAY (X11)
if [ -z "$DISPLAY" ]; then
    echo "⚠️  Warning: No DISPLAY variable set"
    echo "Make sure you're running in a graphical environment"
fi

# Export environment variables if available
if [ -n "$AZDO_PAT" ]; then
    echo "✅ Using AZDO_PAT from environment"
fi

if [ -n "$AZDO_ORG" ]; then
    echo "✅ Using AZDO_ORG from environment"
fi

echo ""
echo "🚀 Starting Azure DevOps MCP Server..."
echo ""

# Run the application
./build/azuredevops-mcp-server "$@"
