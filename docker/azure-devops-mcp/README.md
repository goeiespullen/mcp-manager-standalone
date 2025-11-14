# Azure DevOps MCP Server - Docker Setup

Docker setup voor de **officiële Microsoft Azure DevOps MCP Server** met 80+ tools.

## 🎯 Features

- ✅ **Stdio transport** - Compatibel met MCP Manager gateway
- ✅ **PAT authentication** - Personal Access Token via environment variable (geen browser nodig!)
- ✅ **TypeScript/Node.js** - Gebouwd op de officiële Microsoft SDK
- ✅ **80+ tools** - Volledige Azure DevOps integratie:
  - **Boards**: Work items, epics, features, stories, tasks, comments, attachments
  - **Repos**: Pull requests, branches, repositories, commits
  - **Pipelines**: Builds, logs, pipeline CRUD, queuing
  - **Artifacts**: Feeds, packages, permissions
  - **Test Plans**: Test plans, suites, cases
  - **Wiki**: Pages, create, update, search
  - **Search**: Work items en wiki zoeken

## 📦 Prerequisites

- Docker
- Azure DevOps Personal Access Token (PAT)
- Azure DevOps organization naam (bijv. `ns-topaas`)

## 🚀 Quick Start

### 1. Build het Docker image

```bash
cd /home/laptop/MEGA/development/chatns/mcp-manager/docker/azure-devops-mcp
./build.sh
```

### 2. Configureer credentials

Bewerk het env file:
```bash
# Edit het bestand
nano /home/laptop/MEGA/development/chatns/chatnsbot-standalone/azure-devops-mcp.env
```

Vul je Personal Access Token in:
```env
ADO_MCP_AUTH_TOKEN=your-actual-pat-token-here
```

**PAT aanmaken:**
1. Ga naar: `https://dev.azure.com/ns-topaas/_usersSettings/tokens`
2. Klik op "New Token"
3. Selecteer scopes:
   - ✅ Code (Read, Write)
   - ✅ Work Items (Read, Write)
   - ✅ Build (Read, Execute)
   - ✅ Wiki (Read, Write)
4. Kopieer de token en plak in het env file

### 3. Test het Docker image

```bash
docker run --rm -i \
  --env-file /home/laptop/MEGA/development/chatns/chatnsbot-standalone/azure-devops-mcp.env \
  azuredevops-mcp:latest \
  ns-topaas \
  --authentication envvar
```

Dit start de MCP server die via stdio communiceert.

### 4. Integratie met MCP Manager

De server is al geconfigureerd in `configs/servers.json`:

```json
{
  "name": "Azure DevOps",
  "command": "docker",
  "type": "docker",
  "arguments": [
    "run",
    "--rm",
    "-i",
    "--env-file",
    "/home/laptop/MEGA/development/chatns/chatnsbot-standalone/azure-devops-mcp.env",
    "azuredevops-mcp:latest",
    "ns-topaas",
    "--authentication",
    "envvar"
  ]
}
```

Start de server via de MCP Manager GUI of:
```bash
./mcp-manager
# Ga naar "Servers" tab
# Klik "Start" bij Azure DevOps
```

## 🔐 Authentication Types

De Microsoft server ondersteunt meerdere auth methodes:

| Type | Beschrijving | Docker Compatible |
|------|--------------|-------------------|
| `envvar` | PAT via environment variable | ✅ **Aanbevolen** |
| `interactive` | Browser-based OAuth | ❌ Vereist GUI |
| `azcli` | Azure CLI credentials | ⚠️ Vereist `az login` |
| `env` | DefaultAzureCredential | ⚠️ Complex |

**Voor Docker gebruik ALTIJD `envvar`!**

## 📊 Available Tools (80+)

### Boards Domain
- Work item operations (create, update, query, bulk)
- Comments, attachments, links
- Boards, columns, iterations
- Area paths en iterations management

### Repos Domain
- Pull request CRUD
- Comments, labels, iterations
- Branch operations
- Repository management
- Commit search

### Pipelines Domain
- Build queue, cancel, retry
- Logs download
- Pipeline definitions CRUD
- Build reports

### Artifacts Domain
- Feed management
- Package operations
- Permissions and views
- Retention policies

### Test Plans Domain
- Test plan CRUD
- Test suite management
- Test case creation

### Wiki Domain
- Page creation and updates
- Wiki management
- Search functionality

## 🐛 Troubleshooting

### PAT token niet werkt
```bash
# Test direct
docker run --rm -i \
  -e ADO_MCP_AUTH_TOKEN="your-token" \
  azuredevops-mcp:latest \
  ns-topaas \
  --authentication envvar
```

### Docker image niet gevonden
```bash
# Rebuild
cd /home/laptop/MEGA/development/chatns/mcp-manager/docker/azure-devops-mcp
./build.sh
```

### Server start niet in MCP Manager
1. Check logs in MCP Manager "Logs" tab
2. Verificeer dat `azure-devops-mcp.env` bestaat en PAT bevat
3. Check dat Docker draait: `docker ps`

## 📚 Resources

- **GitHub**: https://github.com/microsoft/azure-devops-mcp
- **NPM Package**: https://www.npmjs.com/package/@azure-devops/mcp
- **Azure DevOps API**: https://learn.microsoft.com/en-us/rest/api/azure/devops/

## 🔄 Updates

Om de laatste versie te krijgen:

```bash
# Rebuild image (pulls latest version from npm)
cd /home/laptop/MEGA/development/chatns/mcp-manager/docker/azure-devops-mcp
./build.sh
```

De server wordt automatisch geüpdatet via npm tijdens de Docker build.

## ⚖️ License

Dit is de officiële Microsoft Azure DevOps MCP Server.
Licensed under MIT License.
