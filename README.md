# MCP Manager Gateway

**Cross-Platform Support:** Linux, Windows, macOS


**Enterprise-grade, multi-tenant MCP server management and session-based gateway**

Multi-server manager for Model Context Protocol (MCP) servers with built-in session-based gateway for multi-tenant access. Manage multiple MCP integrations (Azure DevOps, Confluence, etc.) from a single Qt-based application.

## Overview

MCP Manager provides two modes of operation:

1. **Server Management Mode**: Traditional multi-server process management
2. **Gateway Mode**: Session-based proxy with credential injection (NEW!)

## Architecture

### Gateway Mode (Recommended for Production)

```
┌─────────────┐
│  Dashboard  │ (Client 1 with credentials A)
└──────┬──────┘
       │
       │ TCP Socket
       ▼
┌─────────────────────────────────────────┐
│   MCP Manager Gateway (port 8700)       │
│                                         │
│  ┌──────────┐  ┌──────────┐           │
│  │ Session 1│  │ Session 2│           │
│  │ (Creds A)│  │ (Creds B)│           │
│  └────┬─────┘  └────┬─────┘           │
│       │             │                  │
│       ▼             ▼                  │
│  ┌─────────┐  ┌─────────┐            │
│  │  MCP    │  │  MCP    │            │
│  │ Server 1│  │ Server 2│            │
│  │Subprocess  │Subprocess            │
│  └─────────┘  └─────────┘            │
└─────────────────────────────────────────┘
       ▲
       │
┌──────┴──────┐
│  Dashboard  │ (Client 2 with credentials B)
└─────────────┘
```

## Features

### Server Management
- 🖥️ **Multi-Server Management** - Start, stop, and monitor multiple MCP servers
- 📊 **Visual Dashboard** - See status, ports, PIDs at a glance
- 📝 **Real-time Logs** - Monitor output from all servers with filtering
- 🔧 **Configuration Management** - JSON-based server configuration
- 💪 **Process Management** - Health monitoring, auto-restart on crash
- ⚙️ **CLI Support** - Command-line options for automation

### Gateway Features (NEW!)
- 🔒 **Session-based Isolation** - Each client gets dedicated MCP server instances
- 🔑 **Credential Injection** - Client credentials injected per session
- 👥 **Multi-Tenant Support** - Multiple clients with independent sessions
- 🚪 **Auto Cleanup** - Automatic session destruction on disconnect
- 📡 **JSON-RPC 2.0** - Standard protocol over TCP
- 🔍 **Traffic Monitoring** - Built-in protocol inspector
- 🎯 **Zero Server Modification** - Works with any MCP server
- 🔐 **Credential Inheritance** - Automatic fallback to environment variables

### Permission Management (NEW!)
- 🛡️ **Fine-grained Access Control** - Control tool access per server and globally
- 🔍 **Universal Auto-detection** - Automatic permission categorization for all MCP servers
- ⚡ **Real-time Enforcement** - Sessions destroyed automatically on permission changes
- 📊 **Unified Interface** - Combined tools browser and permissions in single tab
- 📝 **Change Tracking** - Audit log for all permission modifications
- 🌍 **Global Defaults** - Set baseline permissions for all servers with per-server overrides

## Quick Start

**📖 For detailed installation instructions, see [INSTALL.md](INSTALL.md)**

### 1. Build

**Linux/macOS:**
```bash
./build.sh
# Or manually:
mkdir -p build && cd build
cmake .. && make -j$(nproc)
```

**Windows:**
```cmd
build.bat
REM Or manually:
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### 2. Configure

Edit `configs/servers.json`:

```json
{
  "version": "1.0",
  "servers": [
    {
      "name": "Confluence",
      "type": "python",
      "command": "/path/to/venv/bin/python",
      "arguments": ["-m", "mcp_atlassian"],
      "port": 8766,
      "workingDir": "/path/to/mcp-servers/atlassian",
      "env": {
        "CONFLUENCE_URL": "https://your-instance.atlassian.net/wiki"
      }
    }
  ]
}
```

**Important:** Do NOT put credentials in this file! They're provided per-session.

### 3. Run

**Linux/macOS:**
```bash
./run.sh
```

**Windows:**
```cmd
run.bat
# Or:
build\Release\mcp-manager.exe
```

**Manual start:**
```bash
# GUI mode (recommended)
./build/mcp-manager

# Or with auto-start
./build/mcp-manager --start-all
```

The gateway automatically starts on port 8700.

### 4. Connect from Dashboard

```python
from mcp_client.gateway_dashboard_client import GatewayDashboardClient

with GatewayDashboardClient() as client:
    # Credentials loaded from environment automatically
    projects = await client.list_projects()
    spaces = await client.list_confluence_spaces()
```

## Gateway Protocol

### Create Session

```json
Request:
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "mcp-manager/create-session",
  "params": {
    "serverType": "Confluence",
    "credentials": {
      "CONFLUENCE_API_TOKEN": "your-token",
      "CONFLUENCE_USERNAME": "your-email@example.com"
    }
  }
}

Response:
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "sessionId": "abc123def456",
    "serverType": "Confluence",
    "created": "2025-10-24T14:35:09"
  }
}
```

### Call Tool

```json
Request:
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "sessionId": "abc123def456",
    "name": "confluence-search",
    "arguments": {"query": "documentation"}
  }
}

Response:
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "content": [{"type": "text", "text": "Found 10 pages..."}]
  }
}
```

### List Sessions

```json
Request:
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "mcp-manager/list-sessions",
  "params": {}
}

Response:
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "count": 2,
    "sessions": [
      {
        "sessionId": "abc123",
        "serverType": "Confluence",
        "created": "2025-10-24T14:35:09",
        "requestCount": 5,
        "active": true
      }
    ]
  }
}
```

### Destroy Session

```json
Request:
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "mcp-manager/destroy-session",
  "params": {"sessionId": "abc123def456"}
}

Response:
{
  "jsonrpc": "2.0",
  "id": 4,
  "result": {"sessionId": "abc123def456", "destroyed": true}
}
```

## Python Client Library

### Low-Level Client

```python
from mcp_client.mcp_manager_client import MCPManagerClient

with MCPManagerClient() as client:
    # Create session
    session = client.create_session('Confluence', {
        'CONFLUENCE_API_TOKEN': 'your-token',
        'CONFLUENCE_USERNAME': 'your-email@example.com'
    })

    # Call tools
    result = client.call_tool(
        session.session_id,
        'confluence-search',
        {'query': 'documentation'}
    )

    # Cleanup
    client.destroy_session(session.session_id)
```

### Dashboard Integration

```python
from mcp_client.gateway_dashboard_client import GatewayDashboardClient

# Drop-in replacement for DashboardMCPClient
with GatewayDashboardClient() as client:
    # Credentials loaded from environment automatically
    projects = await client.list_projects()
    teams = await client.list_teams(project)

    # All existing dashboard methods work!
```

## Environment Variables

Credentials are loaded automatically from environment:

```bash
# Confluence
export ATLASSIAN_EMAIL="your-email@example.com"
export ATLASSIAN_API_TOKEN="your-api-token"

# Azure DevOps
export AZDO_PAT="your-personal-access-token"

# ChatNS
export CHAT_BEARER="your-bearer-token"
export CHAT_APIM="your-apim-subscription-key"
```

## Permission Management

### Permission Categories

MCP Manager implements five permission categories to control tool access:

| Permission | Description | Default | Use Cases |
|-----------|-------------|---------|-----------|
| `READ_REMOTE` | Read from remote services | ✅ Enabled | Search, list, view operations |
| `WRITE_REMOTE` | Write/modify remote data | ❌ Disabled | Create, update, delete operations |
| `WRITE_LOCAL` | Save/export to filesystem | ❌ Disabled | Download, export, save operations |
| `EXECUTE_AI` | Use AI/LLM capabilities | ❌ Disabled | Chat, completion, generation |
| `EXECUTE_CODE` | Execute code or scripts | ❌ Disabled | Run, build, compile operations |

### Permission Hierarchy

1. **Global Defaults** - Baseline permissions for all servers
2. **Per-Server Overrides** - Explicit overrides for specific servers
3. **Tool Requirements** - Each tool declares required permissions
4. **Runtime Check** - Gateway validates permissions on every tool call

### Auto-detection Logic

Tools are automatically assigned permissions based on keywords in their name and description:

```
Write Operations:
  Keywords: create, update, delete, add, modify, set, post, put, transition, link
  → WRITE_REMOTE

Download/Export:
  Keywords: download, export, save, dump
  → WRITE_LOCAL

AI Operations:
  Keywords: chat, completion, generate, ai, gpt, llm
  → EXECUTE_AI

Code Execution:
  Keywords: execute, run, script, build, compile
  → EXECUTE_CODE

Default:
  If no write/download/ai/code keywords detected
  → READ_REMOTE
```

### Real-time Enforcement

When permissions are changed:

1. **Signal Emission** - `MCPServerInstance` emits `permissionsChanged()`
2. **Manager Relay** - `MCPServerManager` relays signal with server name
3. **Gateway Action** - `MCPGateway` destroys affected sessions
4. **Session Cleanup** - All subprocess instances terminated
5. **Next Request** - New session created with updated permissions
6. **Logging** - All actions logged to Traffic Monitor

### Permission Workflow Example

```
1. User disables WRITE_REMOTE for ChatNS
   ↓
2. Signal chain: MCPServerInstance → MCPServerManager → MCPGateway
   ↓
3. Gateway finds all ChatNS sessions (e.g., session abc123)
   ↓
4. Gateway destroys session abc123
   ↓
5. Log: "Permissions changed for server ChatNS, destroying all related sessions"
   ↓
6. Next tool call creates new session with WRITE_REMOTE disabled
   ↓
7. Tool requiring WRITE_REMOTE is blocked: "insufficient permissions"
```

### Configuration File Format

Permissions are saved in `configs/servers.json`:

```json
{
  "permissions": {
    "global_defaults": {
      "READ_REMOTE": true,
      "WRITE_REMOTE": false,
      "WRITE_LOCAL": false,
      "EXECUTE_AI": false,
      "EXECUTE_CODE": false
    }
  },
  "servers": [
    {
      "name": "ChatNS",
      "permissions": {
        "EXECUTE_AI": true,
        "READ_REMOTE": true
      }
    }
  ]
}
```

### Credential Management

**Environment Variable Inheritance:**

When no credentials are provided by the client, MCP Manager automatically inherits credentials from its environment:

```bash
# ChatNS
export CHAT_APIM="your-apim-key"
export OCP_APIM_SUBSCRIPTION_KEY="your-subscription-key"
export CHAT_BEARER="your-bearer-token"

# Azure DevOps
export AZDO_PAT="your-personal-access-token"
export AZDO_ORG="your-organization"

# Atlassian
export ATLASSIAN_EMAIL="your-email@example.com"
export ATLASSIAN_API_TOKEN="your-api-token"
export CONFLUENCE_URL="https://your-instance.atlassian.net/wiki"
export JIRA_URL="https://your-instance.atlassian.net"

# TeamCentraal
export TEAMCENTRAAL_URL="https://teamcentraal.ns.nl/odata/POS_Odata_v4"
export TEAMCENTRAAL_USERNAME="your-username"
export TEAMCENTRAAL_PASSWORD="your-password"
```

**Credential Priority:**

1. Client-provided credentials (highest priority)
2. Environment variables (fallback)
3. Config file `env` section (base environment)

This enables seamless operation for both:
- **Dashboard clients** with explicit credential management
- **CLI tools** relying on environment variables

## Configuration

### Server Configuration

Location: `mcp-manager/configs/servers.json`

```json
{
  "version": "1.0",
  "servers": [
    {
      "name": "Server Name",
      "type": "python",
      "command": "/path/to/executable",
      "arguments": ["-m", "module"],
      "port": 8765,
      "workingDir": "/path/to/workdir",
      "autoStart": false,
      "env": {
        "VAR_NAME": "value"
      }
    }
  ]
}
```

### Adding New MCP Servers

1. Add configuration to `servers.json`
2. Reload config (File → Reload Config or restart)
3. Server is immediately available to all clients
4. No code changes needed!

## Supported MCP Servers

- **Azure DevOps** - Work items, sprints, teams, iterations, repositories
- **Confluence** - Pages, spaces, search, content management
- **ChatNS** - NS internal chat/AI integration
- **Custom** - Add any MCP server via configuration

## Testing

### Test Gateway Protocol

```bash
python3 test_gateway.py
```

Tests:
✅ Basic connection
✅ Session management
✅ Multiple concurrent sessions
✅ Error handling

### Test Dashboard Integration

```bash
python3 examples/gateway_integration_example.py
```

Examples:
- Azure DevOps workflow
- Confluence workflow
- Multi-server usage
- Dashboard integration patterns

## Command Line Options

```bash
# Custom config file
./build/mcp-manager --config /path/to/config.json

# Auto-start all servers
./build/mcp-manager --autostart

# Both together
./build/mcp-manager -c configs/servers.json -a

# Help
./build/mcp-manager --help
```

## GUI Interface

### 1. Servers Tab
- View all configured servers
- Start/stop individual servers or all at once
- Real-time status, port, and PID display

### 2. Gateway Tab
- Gateway status and port
- Active session count
- Python client usage examples
- Session management tips

### 3. Tools & Permissions Tab (NEW!)
Unified interface for tool management and access control:

**📋 Tools Browser Section:**
- Browse available tools per server
- See tool descriptions and required permissions
- Check real-time access status
- Enable/disable individual tools

**🌍 Global Permissions Section:**
- Set default permissions for all servers:
  - `READ_REMOTE` - Read from remote services (default: enabled)
  - `WRITE_REMOTE` - Write/modify remote data (default: disabled)
  - `WRITE_LOCAL` - Save/export to local filesystem (default: disabled)
  - `EXECUTE_AI` - Use AI/LLM capabilities (default: disabled)
  - `EXECUTE_CODE` - Execute code or scripts (default: disabled)

**🖥️ Per-Server Overrides Section:**
- Override global defaults for specific servers
- Clear overrides to use global defaults
- Visual indication of effective permissions

**📝 Change Logging Section (Collapsible):**
- Track all permission modifications
- See timestamp, action, and changes
- Discard unsaved changes
- View complete change history in separate window

**Permission Auto-detection:**
- Tools are automatically categorized based on name and description
- Keywords detected: create, update, delete, download, export, execute, chat, etc.
- Manual adjustments possible via per-server overrides

**Real-time Enforcement:**
- Permission changes take effect immediately
- Active sessions are automatically destroyed when permissions change
- Next tool call creates new session with updated permissions
- All enforcement actions logged in Traffic Monitor

### 4. Logs Tab
- View stdout/stderr from all servers
- Filter by specific server
- Clear logs

### 5. Traffic Monitor Tab
- Inspect MCP protocol messages
- Request/response tracking
- Client connection monitoring
- Permission enforcement events
- Session lifecycle tracking

## Architecture Components

### MCPServerManager
- Load/save server configurations
- Start/stop/restart servers
- Monitor server health
- Auto-restart on crashes
- **Global permission management**
- **Permission change signal relay**

### MCPGateway
- Master TCP server (port 8700)
- Session creation/destruction
- Request routing
- Multi-client handling
- **Permission enforcement**
- **Automatic session cleanup on permission changes**

### MCPSession
- Dedicated MCP server subprocess
- Client-specific credential injection
- Bidirectional communication
- Automatic lifecycle management
- **Environment variable inheritance**
- **Server-specific credential fallback**

### MCPServerInstance
- Single server process management
- Health monitoring
- Output capture
- Port management
- **Tool permission auto-detection**
- **Per-server permission overrides**
- **Tool access validation**

## Performance

- **Connections:** 10+ simultaneous clients
- **Sessions:** 100+ concurrent sessions
- **Latency:** <50ms routing overhead
- **Memory:** ~50MB per subprocess
- **Startup:** <2s initialization

## Troubleshooting

### Gateway not starting
```bash
# Check port availability
netstat -tuln | grep 8700

# Check logs
./build/mcp-manager --verbose
```

### Session creation fails
- Verify server config in `servers.json`
- Check executable path is correct
- Ensure working directory exists
- Review gateway logs for errors

### Server crashes immediately
- Check dependencies are installed
- Verify Python venv is activated
- Check module path is correct
- Review server stderr output

### Tool calls timeout
- Verify MCP server is responding
- Check server logs for errors
- Increase client timeout
- Verify credentials are correct

## Project Structure

```
mcp-manager/
├── include/
│   ├── MainWindow.h          # Main GUI
│   ├── MCPServerManager.h    # Server lifecycle
│   ├── MCPServerInstance.h   # Single server
│   ├── MCPGateway.h          # TCP gateway
│   ├── MCPSession.h          # Client session
│   └── TrafficMonitor.h      # Traffic monitoring
├── src/
│   ├── main.cpp
│   ├── MainWindow.cpp
│   ├── MCPServerManager.cpp
│   ├── MCPServerInstance.cpp
│   ├── MCPGateway.cpp
│   ├── MCPSession.cpp
│   └── TrafficMonitor.cpp
├── configs/
│   └── servers.json          # Server config
└── CMakeLists.txt

mcp_client/
├── mcp_manager_client.py           # Low-level client
├── gateway_dashboard_client.py     # Dashboard integration
└── dashboard_client.py             # Legacy client

examples/
└── gateway_integration_example.py  # Integration examples

tests/
└── test_gateway.py                 # Protocol tests
```

## Requirements

### Build Requirements
- CMake 3.16+
- Qt6 (Core, Widgets, Network)
- C++17 compiler

### Runtime Requirements
- Qt6 libraries
- Python 3.8+ (for Python client)
- Network access to localhost:8700

## Benefits

### For Developers
✅ Single configuration point
✅ Easy to add new servers
✅ No modifications to third-party code
✅ Built-in monitoring and logging
✅ Automatic permission detection
✅ Environment variable fallback

### For Operations
✅ Centralized management
✅ Process health monitoring
✅ Auto-restart on crashes
✅ Easy credential rotation
✅ Real-time permission enforcement
✅ Comprehensive audit logging

### For Security
✅ Credential isolation per tenant
✅ No credentials in config files
✅ Session-based access control
✅ Automatic cleanup
✅ Fine-grained tool permissions
✅ Immediate permission changes
✅ Complete change tracking

## Roadmap

- ✅ Multi-server management
- ✅ Process lifecycle management
- ✅ Health monitoring
- ✅ Session-based gateway
- ✅ Credential injection
- ✅ Multi-tenant support
- ✅ Python client library
- ✅ Fine-grained permission system
- ✅ Universal tool permission auto-detection
- ✅ Real-time permission enforcement
- ✅ Environment variable credential inheritance
- ✅ Unified tools & permissions interface
- ✅ Change tracking and audit logging
- ⏳ GUI server add/edit dialogs
- ⏳ REST API for external control
- ⏳ Docker support
- ⏳ Server templates library

## License

Internal NS Development - ChatNS Summer School Initiative

## Contact

For questions or issues, contact the NS AI/Data development team.

---

## Related Projects

### Clients

- **ChatNSbot** - Terminal chatbot powered by ChatNS LLM
  - Connects to MCP Manager Gateway for interactive AI conversations
  - Zero dependencies (Python stdlib only)
  - Cross-platform: Linux, Windows, macOS
  - Repository: https://github.com/goeiespullen/chatnsbot-standalone

### MCP Servers

- **Azure DevOps MCP** - Official Microsoft MCP server
  - Work items, sprints, repositories, pull requests
  - Repository: https://github.com/microsoft/azure-devops-mcp

- **Atlassian MCP** - Confluence and Jira integration
  - Pages, spaces, search, content management
  - Repository: https://github.com/sooperset/mcp-atlassian

- **More MCP Servers** - Community servers collection
  - GitHub, Google Drive, Slack, PostgreSQL, and more
  - Repository: https://github.com/modelcontextprotocol/servers

### Resources

- **Model Context Protocol** - Official MCP specification
  - https://modelcontextprotocol.io/

- **Awesome MCP Servers** - Curated list of MCP servers
  - https://github.com/punkpeye/awesome-mcp-servers

---

## Installation

**📖 For complete installation instructions, see [INSTALL.md](INSTALL.md)**

Quick start:

**Linux/macOS:**
```bash
./build.sh
./run.sh
```

**Windows:**
```cmd
build.bat
run.bat
```

---

## Support

- **Issues:** https://github.com/goeiespullen/mcp-manager-standalone/issues
- **Installation Guide:** [INSTALL.md](INSTALL.md)
- **ChatNSbot:** https://github.com/goeiespullen/chatnsbot-standalone

