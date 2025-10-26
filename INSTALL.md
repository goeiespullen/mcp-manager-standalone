# MCP Manager - Installation Guide

Complete installation instructions for building and running MCP Manager on Linux, Windows, and macOS.

**Cross-Platform Support:** Linux, Windows, macOS

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation by Platform](#installation-by-platform)
  - [Linux](#linux)
  - [Windows](#windows)
  - [macOS](#macos)
- [Quick Start](#quick-start)
- [MCP Server Installation](#mcp-server-installation)
  - [Available MCP Servers](#available-mcp-servers)
  - [Adding a New MCP Server](#adding-a-new-mcp-server)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### All Platforms

1. **C++ Compiler** with C++17 support
2. **CMake** 3.14 or higher
3. **Qt** 5.12+ or Qt 6.x
4. **Git** (for cloning the repository)

### Platform-Specific Requirements

**Linux:**
- GCC 7+ or Clang 6+
- Qt development packages
- X11/Wayland (for GUI)

**Windows:**
- Visual Studio 2019+ OR MinGW-w64
- Qt installer from qt.io
- Windows SDK

**macOS:**
- Xcode Command Line Tools
- Qt via Homebrew or qt.io installer
- macOS 10.13+ (High Sierra or later)

---

## Installation by Platform

### Linux

#### Ubuntu/Debian

**1. Install Build Tools**

```bash
sudo apt update
sudo apt install build-essential cmake git
```

**2. Install Qt (Choose Qt 5 OR Qt 6)**

**Option A - Qt 5 (Stable):**
```bash
sudo apt install qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools
```

**Option B - Qt 6 (Modern):**
```bash
sudo apt install qt6-base-dev qt6-base-dev-tools
```

**3. Download MCP Manager**

```bash
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone
```

**4. Build**

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**5. Run**

```bash
cd ..
./run.sh
# Or directly:
./build/mcp-manager
```

---

#### Fedora/RHEL/CentOS

**1. Install Build Tools**

```bash
sudo dnf install gcc-c++ cmake git
```

**2. Install Qt**

**Qt 5:**
```bash
sudo dnf install qt5-qtbase-devel
```

**Qt 6:**
```bash
sudo dnf install qt6-qtbase-devel
```

**3. Build and Run**

```bash
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone
mkdir build && cd build
cmake ..
make -j$(nproc)
cd ..
./run.sh
```

---

#### Arch Linux

**1. Install Build Tools and Qt**

```bash
sudo pacman -S base-devel cmake git qt5-base
# Or for Qt 6:
sudo pacman -S base-devel cmake git qt6-base
```

**2. Build and Run**

```bash
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone
mkdir build && cd build
cmake ..
make -j$(nproc)
cd ..
./run.sh
```

---

### Windows

#### Prerequisites Installation

**1. Install Visual Studio 2022 (Community Edition - Free)**

1. Download from: https://visualstudio.microsoft.com/downloads/
2. Run installer
3. Select workload: **"Desktop development with C++"**
4. Install

**2. Install CMake**

1. Download from: https://cmake.org/download/
2. Run installer
3. ✅ **IMPORTANT:** Check "Add CMake to system PATH"
4. Install

**3. Install Qt**

**Option A - Qt Online Installer (Recommended):**

1. Download from: https://www.qt.io/download-qt-installer
2. Run installer
3. Create free Qt account
4. Select Qt version:
   - ✅ Qt 6.5+ (recommended) OR Qt 5.15+
   - ✅ MinGW 64-bit (if not using Visual Studio)
5. Remember installation path (e.g., `C:\Qt\6.5.0\msvc2022_64`)

**Option B - Chocolatey (Advanced):**
```cmd
choco install qt6 cmake visualstudio2022-workload-vctools
```

---

#### Building MCP Manager

**Using Command Prompt:**

```cmd
REM 1. Download source
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone

REM 2. Set Qt path (adjust to your installation)
set CMAKE_PREFIX_PATH=C:\Qt\6.5.0\msvc2022_64

REM 3. Create build directory
mkdir build
cd build

REM 4. Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64

REM 5. Build
cmake --build . --config Release

REM 6. Run
cd ..
build\Release\mcp-manager.exe
```

**Using build.bat Script:**

```cmd
cd mcp-manager-standalone
build.bat
```

---

#### Alternative: MinGW Build

```cmd
REM Set Qt MinGW path
set CMAKE_PREFIX_PATH=C:\Qt\6.5.0\mingw_64
set PATH=C:\Qt\6.5.0\mingw_64\bin;C:\Qt\Tools\mingw1120_64\bin;%PATH%

REM Build
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j4

REM Run
cd ..
build\mcp-manager.exe
```

---

### macOS

#### Prerequisites Installation

**1. Install Xcode Command Line Tools**

```bash
xcode-select --install
```

**2. Install Homebrew (if not installed)**

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**3. Install Build Tools and Qt**

**Option A - Qt 5 (via Homebrew):**
```bash
brew install cmake qt@5
```

**Option B - Qt 6 (via Homebrew, recommended):**
```bash
brew install cmake qt@6
```

**Option C - Qt Installer:**
1. Download from: https://www.qt.io/download-qt-installer
2. Install Qt 6.5+ or Qt 5.15+
3. Select macOS components

---

#### Building MCP Manager

**Using Homebrew Qt:**

```bash
# Clone repository
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone

# For Qt 6:
export CMAKE_PREFIX_PATH=$(brew --prefix qt@6)

# For Qt 5:
# export CMAKE_PREFIX_PATH=$(brew --prefix qt@5)

# Build
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)

# Run
cd ..
./run.sh
```

**Using Qt Installer:**

```bash
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone

# Set Qt path (adjust to your installation)
export CMAKE_PREFIX_PATH=~/Qt/6.5.0/macos

mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)

cd ..
./run.sh
```

---

## Quick Start

After building, follow these steps to run MCP Manager:

### 1. Start MCP Manager

**Linux/macOS:**
```bash
cd mcp-manager-standalone
./run.sh
```

**Windows:**
```cmd
cd mcp-manager-standalone
build\Release\mcp-manager.exe
```

### 2. Configure MCP Servers

1. Open MCP Manager GUI
2. Click "Add Server" or edit `configs/servers.json`
3. Add your MCP server configurations:

```json
{
  "version": "1.0",
  "servers": [
    {
      "name": "My MCP Server",
      "type": "python",
      "command": "/path/to/python",
      "arguments": ["server.py"],
      "port": 8765,
      "workingDir": "/path/to/mcp-server",
      "env": {
        "API_KEY": "your_key"
      }
    }
  ]
}
```

### 3. Start Gateway

The gateway starts automatically on port 8700 when you launch MCP Manager.

### 4. Test Connection

**Linux/macOS:**
```bash
nc -zv localhost 8700
```

**Windows:**
```cmd
telnet localhost 8700
```

Or use the ChatNSbot:
```bash
git clone https://github.com/goeiespullen/chatnsbot-standalone.git
cd chatnsbot-standalone
./run.sh
```

---

## MCP Server Installation

MCP Manager is a gateway that connects to MCP (Model Context Protocol) servers. You need to install at least one MCP server to use the gateway.

### Available MCP Servers

#### Official MCP Servers

**Azure DevOps MCP Server**
- **Repository:** https://github.com/microsoft/azure-devops-mcp
- **Language:** TypeScript/Node.js
- **Features:** Work items, sprints, repositories, pull requests
- **Installation:**

```bash
# Clone the repository
git clone https://github.com/microsoft/azure-devops-mcp.git
cd azure-devops-mcp

# Install dependencies
npm install

# Build
npm run build

# Test (optional)
npm test
```

**Configuration in MCP Manager:**
```json
{
  "name": "Azure DevOps",
  "type": "nodejs",
  "command": "node",
  "arguments": ["dist/index.js"],
  "port": 8765,
  "workingDir": "/path/to/azure-devops-mcp",
  "env": {
    "AZDO_ORG": "your-organization",
    "AZDO_PROJECT": "your-project"
  }
}
```

**Environment variables needed:**
- `AZDO_PAT` - Azure DevOps Personal Access Token

---

**Confluence MCP Server (Atlassian)**
- **Repository:** https://github.com/sooperset/mcp-atlassian
- **Language:** Python
- **Features:** Pages, spaces, search, content management
- **Installation:**

```bash
# Clone the repository
git clone https://github.com/sooperset/mcp-atlassian.git
cd mcp-atlassian

# Create virtual environment
python3 -m venv venv
source venv/bin/activate  # Linux/macOS
# Or: venv\Scripts\activate  # Windows

# Install dependencies
pip install -r requirements.txt
```

**Configuration in MCP Manager:**
```json
{
  "name": "Confluence",
  "type": "python",
  "command": "/path/to/mcp-atlassian/venv/bin/python",
  "arguments": ["-m", "mcp_atlassian"],
  "port": 8766,
  "workingDir": "/path/to/mcp-atlassian",
  "env": {
    "CONFLUENCE_URL": "https://your-instance.atlassian.net/wiki"
  }
}
```

**Environment variables needed:**
- `CONFLUENCE_API_TOKEN` - Atlassian API token
- `CONFLUENCE_USERNAME` - Your Atlassian email

---

### Community MCP Servers

Browse more MCP servers at:
- **Awesome MCP Servers:** https://github.com/punkpeye/awesome-mcp-servers
- **Official MCP Docs:** https://modelcontextprotocol.io/

Popular community servers:
- **GitHub MCP:** https://github.com/modelcontextprotocol/servers/tree/main/src/github
- **Google Drive MCP:** https://github.com/modelcontextprotocol/servers/tree/main/src/gdrive
- **Slack MCP:** https://github.com/modelcontextprotocol/servers/tree/main/src/slack
- **PostgreSQL MCP:** https://github.com/modelcontextprotocol/servers/tree/main/src/postgres

---

### Adding a New MCP Server

When you want to add a new MCP server to MCP Manager, follow these steps:

#### Step 1: Install the MCP Server

**For Python servers:**
```bash
# Clone from GitHub
git clone <github-url>
cd <server-directory>

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt
```

**For Node.js servers:**
```bash
# Clone from GitHub
git clone <github-url>
cd <server-directory>

# Install dependencies
npm install

# Build (if TypeScript)
npm run build
```

#### Step 2: Test the Server

Before adding to MCP Manager, test the server manually:

**Python:**
```bash
python -m server_module
# Or:
python server.py
```

**Node.js:**
```bash
node dist/index.js
# Or:
npm start
```

#### Step 3: Add to MCP Manager

1. Open MCP Manager GUI
2. Click "Add Server" (or edit `configs/servers.json`)
3. Fill in the details:

```json
{
  "name": "My MCP Server",
  "type": "python",  // or "nodejs"
  "command": "/path/to/python",  // or "node"
  "arguments": ["server.py"],    // or ["dist/index.js"]
  "port": 8767,                  // unique port number
  "workingDir": "/path/to/server",
  "env": {
    "API_KEY": "your_key",
    "SERVER_URL": "https://api.example.com"
  },
  "autostart": false,
  "healthCheckInterval": 30000
}
```

**Required fields:**
- `name` - Display name in MCP Manager
- `type` - `"python"` or `"nodejs"`
- `command` - Full path to python/node executable
- `arguments` - Arguments to pass (file to run)
- `port` - Unique port number (8765+)
- `workingDir` - Server's root directory

**Optional fields:**
- `env` - Environment variables (non-sensitive only!)
- `autostart` - Auto-start on MCP Manager launch
- `healthCheckInterval` - Health check frequency (ms)

#### Step 4: Configure Credentials

**IMPORTANT:** Never put credentials in `servers.json`!

Set credentials via environment variables:

**Linux/macOS:**
```bash
export API_TOKEN="your_secret_token"
export API_KEY="your_api_key"
```

**Windows:**
```cmd
set API_TOKEN=your_secret_token
set API_KEY=your_api_key
```

Or create a `.env` file (add to `.gitignore`):
```bash
API_TOKEN=your_secret_token
API_KEY=your_api_key
```

#### Step 5: Start the Server

1. In MCP Manager GUI, find your server
2. Click "Start"
3. Check logs for any errors
4. Verify status shows "Running"
5. Test via ChatNSbot or client

---

### Example: Complete Setup Flow

**Scenario:** Adding GitHub MCP server

```bash
# 1. Install the server
git clone https://github.com/modelcontextprotocol/servers.git
cd servers/src/github
npm install
npm run build

# 2. Set credentials
export GITHUB_TOKEN="ghp_your_token_here"

# 3. Add to MCP Manager config
# Edit configs/servers.json:
{
  "name": "GitHub",
  "type": "nodejs",
  "command": "node",
  "arguments": ["build/index.js"],
  "port": 8768,
  "workingDir": "/path/to/servers/src/github",
  "env": {
    "GITHUB_OWNER": "your-username"
  }
}

# 4. Start MCP Manager
./run.sh

# 5. Start GitHub server in GUI
# Click "Start" button next to "GitHub"

# 6. Test with ChatNSbot
cd /path/to/chatnsbot-standalone
./run.sh
# Ask: "List my GitHub repositories"
```

---

### MCP Server Development

Want to create your own MCP server?

**Resources:**
- **MCP Specification:** https://modelcontextprotocol.io/specification
- **Python SDK:** https://github.com/modelcontextprotocol/python-sdk
- **TypeScript SDK:** https://github.com/modelcontextprotocol/typescript-sdk
- **Example Servers:** https://github.com/modelcontextprotocol/servers

**Minimum server requirements:**
- Implement MCP protocol (JSON-RPC 2.0)
- Support `initialize` method
- Support `tools/list` method
- Support `tools/call` method
- Handle stdin/stdout communication

**Template structure:**
```python
# Python MCP Server Template
from mcp import Server

server = Server("my-server")

@server.list_tools()
async def list_tools():
    return [{"name": "my_tool", "description": "...", "inputSchema": {...}}]

@server.call_tool()
async def call_tool(name, arguments):
    if name == "my_tool":
        return {"result": "..."}

if __name__ == "__main__":
    server.run()
```

---


---

## Configuration

### Server Configuration File

Location: `configs/servers.json`

**Example:**

```json
{
  "version": "1.0",
  "servers": [
    {
      "name": "Azure DevOps",
      "type": "python",
      "command": "/usr/bin/python3",
      "arguments": ["mcp_servers/devops_server.py"],
      "port": 8765,
      "workingDir": "/path/to/chatns",
      "env": {
        "AZDO_ORG": "your-org"
      },
      "autostart": false,
      "healthCheckInterval": 30000
    },
    {
      "name": "Confluence",
      "type": "python",
      "command": "/usr/bin/python3",
      "arguments": ["mcp_servers/confluence_server.py"],
      "port": 8766,
      "workingDir": "/path/to/chatns",
      "env": {
        "CONFLUENCE_URL": "https://your-instance.atlassian.net/wiki"
      },
      "autostart": false
    }
  ]
}
```

### Environment Variables

**For Python MCP servers:**
```bash
export AZDO_PAT="your_azure_devops_token"
export CONFLUENCE_API_TOKEN="your_confluence_token"
```

**For the MCP Manager:**
```bash
export QT_QPA_PLATFORM=xcb  # Linux X11
export QT_DEBUG_PLUGINS=1   # Debug Qt plugins
```

---

## Troubleshooting

### Linux Issues

**Problem:** "cannot find -lQt5Core"

**Solution:**
```bash
# Qt 5
sudo apt install qtbase5-dev

# Qt 6
sudo apt install qt6-base-dev
```

---

**Problem:** "Could not find a package configuration file provided by Qt"

**Solution:**
```bash
# Find Qt installation
dpkg -L qtbase5-dev | grep cmake

# Set CMAKE_PREFIX_PATH
export CMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt5
cmake ..
```

---

**Problem:** "No DISPLAY environment variable"

**Solution:**
```bash
# For SSH sessions, enable X11 forwarding
ssh -X user@host

# Or use VNC/RDP for remote GUI
```

---

### Windows Issues

**Problem:** "CMake could not find Qt"

**Solution:**
```cmd
REM Set Qt path explicitly
set CMAKE_PREFIX_PATH=C:\Qt\6.5.0\msvc2022_64
cmake .. -G "Visual Studio 17 2022" -A x64
```

---

**Problem:** "Qt DLLs not found when running"

**Solution:**
```cmd
REM Add Qt bin directory to PATH
set PATH=C:\Qt\6.5.0\msvc2022_64\bin;%PATH%

REM Or copy DLLs to build directory
copy C:\Qt\6.5.0\msvc2022_64\bin\Qt6Core.dll build\Release\
copy C:\Qt\6.5.0\msvc2022_64\bin\Qt6Gui.dll build\Release\
copy C:\Qt\6.5.0\msvc2022_64\bin\Qt6Widgets.dll build\Release\
```

---

**Problem:** "moc/uic not found"

**Solution:**
Install Qt development tools, not just runtime libraries.

---

### macOS Issues

**Problem:** "CMake could not find Qt"

**Solution:**
```bash
# Using Homebrew Qt
export CMAKE_PREFIX_PATH=$(brew --prefix qt@6)

# Using Qt Installer
export CMAKE_PREFIX_PATH=~/Qt/6.5.0/macos
```

---

**Problem:** "Code signing error"

**Solution:**
```bash
# Disable code signing for development
cmake .. -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
```

---

**Problem:** "Application damaged and can't be opened"

**Solution:**
```bash
# Remove quarantine attribute
xattr -cr build/mcp-manager
```

---

### Common Issues (All Platforms)

**Problem:** Gateway not listening on port 8700

**Solution:**
1. Check if port is already in use:
   - Linux/macOS: `lsof -i :8700`
   - Windows: `netstat -ano | findstr 8700`
2. Kill conflicting process
3. Restart MCP Manager

---

**Problem:** MCP servers fail to start

**Solution:**
1. Check server configuration in `configs/servers.json`
2. Verify Python/Node.js paths are correct
3. Check server logs in MCP Manager
4. Test server manually:
   ```bash
   python3 mcp_servers/server.py
   ```

---

**Problem:** Build fails with "C++17 required"

**Solution:**
Update your compiler:
- Linux: GCC 7+ or Clang 6+
- Windows: Visual Studio 2019+
- macOS: Xcode 10+

---

## Building from Scratch

### Minimal Example

**Linux:**
```bash
# Install dependencies
sudo apt install build-essential cmake qtbase5-dev git

# Clone, build, run
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone
mkdir build && cd build
cmake .. && make -j$(nproc)
cd .. && ./run.sh
```

**Windows:**
```cmd
REM After installing Qt, Visual Studio, CMake
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
cd .. && build\Release\mcp-manager.exe
```

**macOS:**
```bash
# Install dependencies
brew install cmake qt@6

# Clone, build, run
git clone https://github.com/goeiespullen/mcp-manager-standalone.git
cd mcp-manager-standalone
export CMAKE_PREFIX_PATH=$(brew --prefix qt@6)
mkdir build && cd build
cmake .. && make -j$(sysctl -n hw.ncpu)
cd .. && ./run.sh
```

---

## Support

- **Issues:** https://github.com/goeiespullen/mcp-manager-standalone/issues
- **Documentation:** [README.md](README.md)
- **ChatNSbot:** https://github.com/goeiespullen/chatnsbot-standalone

---

## License

Part of the ChatNS Summer School project.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>
