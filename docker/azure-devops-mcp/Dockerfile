# Dockerfile for Microsoft Azure DevOps MCP Server
# Uses the official Node.js 22 LTS image
FROM node:22-alpine

# Install dependencies for better compatibility
RUN apk add --no-cache \
    ca-certificates \
    git

# Set working directory
WORKDIR /app

# Install the Azure DevOps MCP server globally
RUN npm install -g @azure-devops/mcp

# Create a non-root user for security
RUN addgroup -g 1001 mcpuser && \
    adduser -D -u 1001 -G mcpuser mcpuser && \
    chown -R mcpuser:mcpuser /app

# Switch to non-root user
USER mcpuser

# Set environment variables
ENV NODE_ENV=production

# The entrypoint will be configured via docker run with the organization name
# and authentication type
ENTRYPOINT ["npx", "@azure-devops/mcp"]
