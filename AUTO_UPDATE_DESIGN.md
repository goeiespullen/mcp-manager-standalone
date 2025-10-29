# MCP Manager Auto-Update Feature - Design Document

## Overview

Add automatic update functionality to MCP Manager that checks GitHub for new releases and allows one-click updates.

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    MCP Manager GUI                            │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Help Menu                                           │   │
│  │  └─ Check for Updates... [New]                      │   │
│  └─────────────────────────────────────────────────────┘   │
│                           │                                   │
│                           ▼                                   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Update Dialog                                       │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │ Current Version: 1.0.0                       │   │   │
│  │  │ Latest Version:  1.1.0                       │   │   │
│  │  │                                               │   │   │
│  │  │ [Release Notes]                              │   │   │
│  │  │ - Feature: Auto-update support               │   │   │
│  │  │ - Fix: Gateway stability improvements        │   │   │
│  │  │                                               │   │   │
│  │  │ [Download] [Cancel]                          │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                  Update Manager (C++)                         │
│                                                               │
│  1. Check GitHub API for latest release                      │
│     GET https://api.github.com/repos/USER/REPO/releases/latest│
│                                                               │
│  2. Compare versions (semver)                                 │
│     Current: 1.0.0  vs  Latest: 1.1.0                        │
│                                                               │
│  3. Download release ZIP                                      │
│     https://github.com/USER/REPO/archive/refs/tags/v1.1.0.zip│
│                                                               │
│  4. Extract to temp directory                                 │
│     /tmp/mcp-manager-update/                                  │
│                                                               │
│  5. Run update script                                         │
│     ./update.sh (Linux/macOS)                                 │
│     update.bat (Windows)                                      │
└──────────────────────────────────────────────────────────────┘
```

## Components

### 1. Version Management

**File: `include/Version.h`**
```cpp
#ifndef VERSION_H
#define VERSION_H

#define MCP_MANAGER_VERSION_MAJOR 1
#define MCP_MANAGER_VERSION_MINOR 0
#define MCP_MANAGER_VERSION_PATCH 0
#define MCP_MANAGER_VERSION_STRING "1.0.0"

#endif
```

### 2. Update Checker

**File: `include/UpdateChecker.h`**
```cpp
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    struct Version {
        int major;
        int minor;
        int patch;
        QString string;

        bool operator>(const Version& other) const;
    };

    struct ReleaseInfo {
        Version version;
        QString url;
        QString downloadUrl;
        QString releaseNotes;
        QDateTime publishedAt;
    };

    void checkForUpdates();
    void downloadUpdate(const QString& url);

signals:
    void updateAvailable(ReleaseInfo info);
    void noUpdateAvailable();
    void checkFailed(QString error);
    void downloadProgress(qint64 received, qint64 total);
    void downloadComplete(QString filePath);

private:
    QNetworkAccessManager* m_networkManager;
    Version parseVersion(const QString& versionString);
};
```

### 3. Update Dialog

**File: `include/UpdateDialog.h`**
```cpp
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    UpdateDialog(const UpdateChecker::ReleaseInfo& info, QWidget* parent);

private slots:
    void onDownloadClicked();
    void onCancelClicked();
    void updateProgress(qint64 received, qint64 total);

private:
    QLabel* m_currentVersionLabel;
    QLabel* m_latestVersionLabel;
    QTextEdit* m_releaseNotesText;
    QProgressBar* m_progressBar;
    QPushButton* m_downloadButton;
};
```

### 4. Update Scripts

**File: `update.sh` (Linux/macOS)**
```bash
#!/bin/bash
# Auto-update script for MCP Manager

OLD_DIR="$1"
NEW_DIR="$2"

# Stop running instance
killall mcp-manager

# Backup current version
cp -r "$OLD_DIR" "$OLD_DIR.backup"

# Copy new files
cp -r "$NEW_DIR"/* "$OLD_DIR"/

# Rebuild
cd "$OLD_DIR"
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Restart
"$OLD_DIR/build/mcp-manager" &
```

**File: `update.bat` (Windows)**
```batch
@echo off
REM Auto-update script for MCP Manager

set OLD_DIR=%1
set NEW_DIR=%2

REM Stop running instance
taskkill /IM mcp-manager.exe /F

REM Backup
xcopy /E /I "%OLD_DIR%" "%OLD_DIR%.backup"

REM Copy new files
xcopy /E /Y "%NEW_DIR%\*" "%OLD_DIR%\"

REM Rebuild
cd /d "%OLD_DIR%"
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release

REM Restart
start "" "%OLD_DIR%\build\Release\mcp-manager.exe"
```

## GitHub API Integration

### Check for Updates

**Endpoint:**
```
GET https://api.github.com/repos/goeiespullen/mcp-manager-standalone/releases/latest
```

**Response:**
```json
{
  "tag_name": "v1.1.0",
  "name": "Release 1.1.0",
  "body": "## Features\n- Auto-update support\n- Bug fixes",
  "published_at": "2025-10-26T10:00:00Z",
  "zipball_url": "https://api.github.com/repos/.../zipball/v1.1.0",
  "tarball_url": "https://api.github.com/repos/.../tarball/v1.1.0"
}
```

### Download Release

**Direct ZIP URL:**
```
https://github.com/goeiespullen/mcp-manager-standalone/archive/refs/tags/v1.1.0.zip
```

## User Flow

### Scenario 1: Update Available

1. User clicks **Help → Check for Updates**
2. MCP Manager queries GitHub API
3. Dialog shows:
   - Current version: 1.0.0
   - Latest version: 1.1.0
   - Release notes
   - **[Download Update]** button
4. User clicks Download
5. Progress bar shows download (with percentage)
6. ZIP extracted to temp directory
7. Update script runs:
   - Stops MCP Manager
   - Backs up current version
   - Copies new files
   - Rebuilds application
   - Restarts MCP Manager
8. Success notification

### Scenario 2: No Update Available

1. User clicks **Help → Check for Updates**
2. MCP Manager queries GitHub API
3. Dialog shows:
   - "You have the latest version (1.0.0)"
   - **[OK]** button

### Scenario 3: Check Failed

1. User clicks **Help → Check for Updates**
2. Network error or API rate limit
3. Error dialog shows:
   - "Failed to check for updates"
   - Error details
   - **[Retry]** **[Cancel]** buttons

## Implementation Phases

### Phase 1: Version Management ✅ Can Implement Now
- Add Version.h with current version
- Display version in About dialog
- Version comparison logic

### Phase 2: GitHub API Integration ✅ Can Implement Now
- UpdateChecker class
- HTTP requests to GitHub API
- JSON parsing for release info
- Version comparison

### Phase 3: Download & Extract ✅ Can Implement Now
- Download progress tracking
- ZIP file handling (QuaZip library)
- Temp directory management

### Phase 4: Update Installation ⚠️ Complex
- Update scripts (platform-specific)
- Self-restart mechanism
- Rollback on failure

### Phase 5: GUI Integration ✅ Can Implement Now
- Menu item: Help → Check for Updates
- Update dialog UI
- Progress indicators

## Security Considerations

### 1. HTTPS Only
- All GitHub API calls use HTTPS
- Verify SSL certificates

### 2. Signature Verification (Future)
- GPG-signed releases
- Verify signatures before installing

### 3. Backup Before Update
- Always backup current installation
- Provide rollback mechanism

### 4. User Confirmation
- Never auto-install without user approval
- Show release notes before downloading

## Platform Differences

### Linux
- Update via package manager preferred
- Manual update: extract + rebuild + restart
- Permissions: may need sudo for system install

### Windows
- No package manager (usually)
- Update: stop process, replace files, rebuild, restart
- May need admin rights

### macOS
- Could use .app bundle replacement
- May need to re-sign after update
- Gatekeeper considerations

## Dependencies

### Required Libraries

**Qt Modules:**
- QtNetwork (HTTP requests)
- QtCore (JSON parsing)

**Optional:**
- QuaZip (ZIP extraction) - https://github.com/stachenov/quazip
- Or use Qt's built-in compression (QZipReader - private API)

**Alternative:**
- System `unzip` command
- Platform-specific APIs

## CMakeLists.txt Changes

```cmake
# Add QtNetwork
find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)

# Add QuaZip (optional)
find_package(QuaZip-Qt6 QUIET)
if(QuaZip-Qt6_FOUND)
    add_definitions(-DHAS_QUAZIP)
    target_link_libraries(mcp-manager QuaZip::QuaZip)
endif()

# Link QtNetwork
target_link_libraries(mcp-manager
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network  # <-- Add this
)
```

## Configuration

### GitHub Repository Settings

**Required:**
- Public repository (for GitHub API access)
- Semantic versioning tags (v1.0.0, v1.1.0)
- GitHub Releases for each version

**Release Creation:**
```bash
# Tag the release
git tag -a v1.1.0 -m "Release 1.1.0"
git push origin v1.1.0

# Create GitHub release via CLI
gh release create v1.1.0 \
  --title "Release 1.1.0" \
  --notes "## Features
- Auto-update support
- Bug fixes"
```

## Testing Strategy

### Unit Tests
- Version comparison logic
- Version string parsing
- Error handling

### Integration Tests
- GitHub API mock responses
- Download simulation
- Update script dry-run

### Manual Tests
- Check for updates (with/without update available)
- Download and install update
- Rollback on failure
- Network error scenarios

## Future Enhancements

### Automatic Update Check
- Check on startup (optional setting)
- Check daily/weekly (background task)
- Notification badge when update available

### Update Channels
- Stable releases
- Beta releases
- Nightly builds

### Differential Updates
- Download only changed files
- Binary diff patches
- Reduce download size

## Minimal Viable Product (MVP)

For initial implementation:

1. ✅ **Version display** in About dialog
2. ✅ **Check for updates** menu item
3. ✅ **GitHub API query** for latest release
4. ✅ **Version comparison** and notification
5. ✅ **Download URL** provided to user
6. ⏳ **Manual update** instructions (Phase 1)
7. ⏳ **Automatic installation** (Phase 2 - future)

**Phase 1 (Simple):**
- Check for updates
- Show dialog with download link
- User manually downloads and installs

**Phase 2 (Advanced):**
- One-click download
- Automatic extraction
- Automatic installation
- Self-restart

## Recommendation

**Start with Phase 1:**
- Less complex
- No risk of breaking installations
- Users still benefit from update notifications
- Can iterate to Phase 2 later

Would you like me to:
1. Implement Phase 1 (update checker + notification)?
2. Go straight to Phase 2 (full auto-update)?
3. Create a prototype first?
