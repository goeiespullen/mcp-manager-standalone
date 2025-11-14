#!/bin/bash
# Build script for Azure DevOps MCP Server Docker image

set -e

echo "🐳 Building Azure DevOps MCP Server Docker image..."

# Build the image
docker build -t azuredevops-mcp:latest .

echo "✅ Docker image built successfully!"
echo ""
echo "📋 Next steps:"
echo "1. Copy azure-devops-mcp.env.example to azure-devops-mcp.env"
echo "2. Edit azure-devops-mcp.env and add your Azure DevOps PAT"
echo "3. Test the image:"
echo "   docker run --rm -i --env-file azure-devops-mcp.env azuredevops-mcp:latest YOUR_ORG --authentication envvar"
echo ""
echo "💡 Tip: You can also push to a registry:"
echo "   docker tag azuredevops-mcp:latest ghcr.io/YOUR_USERNAME/azuredevops-mcp:latest"
echo "   docker push ghcr.io/YOUR_USERNAME/azuredevops-mcp:latest"
