#include "MCPServerInstance.h"
#include "MCPServerManager.h"
#include <QDebug>
#include <QTcpSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QThread>
#include <QDir>

MCPServerInstance::MCPServerInstance(const QJsonObject& config, QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_healthTimer(new QTimer(this))
    , m_config(config)
    , m_status(Stopped)
    , m_maxOutputLines(500)
    , m_restartCount(0)
    , m_maxRestarts(3)
    , m_intentionalStop(false)
    , m_initialized(false)
    , m_pendingToolsRefresh(false)
    , m_manager(nullptr)
{
    // Parse configuration
    m_name = config["name"].toString("Unnamed Server");
    m_type = config["type"].toString("binary");
    m_command = config["command"].toString();
    m_port = static_cast<quint16>(config["port"].toInt(8765));
    m_workingDir = config["workingDir"].toString();
    m_githubRepo = config["githubRepo"].toString(""); // Optional GitHub repository URL
    m_autoStart = config["autostart"].toBool(false);
    m_healthCheckInterval = config["healthCheckInterval"].toInt(30000); // 30s default

    // Parse arguments
    QJsonArray argsArray = config["arguments"].toArray();
    for (const QJsonValue& val : argsArray) {
        m_arguments.append(val.toString());
    }

    // Parse environment
    m_environment = config["env"].toObject();

    // Parse server-specific permission overrides (explicit only, not defaults)
    // If a permission is not in the config, it will inherit from global defaults
    m_permissions.clear();
    if (config.contains("permissions")) {
        QJsonObject perms = config["permissions"].toObject();
        if (perms.contains("READ_REMOTE")) m_permissions[READ_REMOTE] = perms["READ_REMOTE"].toBool();
        if (perms.contains("WRITE_REMOTE")) m_permissions[WRITE_REMOTE] = perms["WRITE_REMOTE"].toBool();
        if (perms.contains("WRITE_LOCAL")) m_permissions[WRITE_LOCAL] = perms["WRITE_LOCAL"].toBool();
        if (perms.contains("EXECUTE_AI")) m_permissions[EXECUTE_AI] = perms["EXECUTE_AI"].toBool();
        if (perms.contains("EXECUTE_CODE")) m_permissions[EXECUTE_CODE] = perms["EXECUTE_CODE"].toBool();
    }

    qDebug() << "Explicit permission overrides for" << m_name << ":" << m_permissions.size() << "overrides";

    // Connect process signals
    connect(m_process, &QProcess::started, this, &MCPServerInstance::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MCPServerInstance::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &MCPServerInstance::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &MCPServerInstance::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &MCPServerInstance::onReadyReadStandardError);

    // Connect health timer
    connect(m_healthTimer, &QTimer::timeout, this, &MCPServerInstance::onHealthCheck);

    qDebug() << "MCPServerInstance created:" << m_name << "on port" << m_port;
}

MCPServerInstance::~MCPServerInstance() {
    if (isRunning()) {
        stop();
    }
}

bool MCPServerInstance::start() {
    if (m_status == Running || m_status == Starting) {
        qWarning() << "Server" << m_name << "already running or starting";
        return false;
    }

    if (m_command.isEmpty()) {
        m_lastError = "No command specified in configuration";
        emit errorOccurred(m_lastError);
        return false;
    }

    // Check if port is available
    if (!checkPortAvailable(m_port)) {
        m_lastError = QString("Port %1 is already in use").arg(m_port);
        emit errorOccurred(m_lastError);
        setStatus(Error);
        return false;
    }

    setStatus(Starting);

    // Setup process environment
    QProcessEnvironment env = buildEnvironment();
    m_process->setProcessEnvironment(env);

    // Set working directory if specified
    if (!m_workingDir.isEmpty()) {
        m_process->setWorkingDirectory(m_workingDir);
    }

    // Build command
    QStringList args = buildArguments();

    qDebug() << "Starting server:" << m_name;
    qDebug() << "Command:" << m_command;
    qDebug() << "Arguments:" << args;
    qDebug() << "Working dir:" << m_workingDir;

    // Start the process
    m_process->start(m_command, args);

    return m_process->waitForStarted(5000); // 5 second timeout
}

void MCPServerInstance::stop() {
    if (m_status == Stopped || m_status == Stopping) {
        return;
    }

    setStatus(Stopping);
    stopHealthMonitoring();

    // Reset initialization state
    m_initialized = false;
    m_pendingToolsRefresh = false;

    // Mark this as an intentional stop (not a crash)
    m_intentionalStop = true;

    qDebug() << "Stopping server:" << m_name;

    // Try graceful shutdown first
    m_process->terminate();

    // Wait up to 5 seconds for graceful shutdown
    if (!m_process->waitForFinished(5000)) {
        qWarning() << "Server" << m_name << "did not stop gracefully, killing...";
        m_process->kill();
        m_process->waitForFinished(2000);
    }

    setStatus(Stopped);
    m_intentionalStop = false;  // Reset flag
}

void MCPServerInstance::restart() {
    qDebug() << "Restarting server:" << m_name;
    m_restartCount++;

    if (m_restartCount > m_maxRestarts) {
        m_lastError = QString("Max restart attempts (%1) reached").arg(m_maxRestarts);
        emit errorOccurred(m_lastError);
        setStatus(Error);
        return;
    }

    stop();
    QThread::msleep(1000); // Wait 1 second before restart
    start();
}

void MCPServerInstance::kill() {
    qDebug() << "Killing server:" << m_name;
    stopHealthMonitoring();
    m_process->kill();
    m_process->waitForFinished(2000);
    setStatus(Stopped);
}

QString MCPServerInstance::statusString() const {
    switch (m_status) {
        case Stopped: return "Stopped";
        case Starting: return "Starting...";
        case Running: return "Running";
        case Stopping: return "Stopping...";
        case Crashed: return "Crashed";
        case Error: return "Error";
        default: return "Unknown";
    }
}

qint64 MCPServerInstance::pid() const {
    return m_process->processId();
}

QStringList MCPServerInstance::recentOutput(int lines) const {
    int start = qMax(0, m_outputBuffer.size() - lines);
    return m_outputBuffer.mid(start);
}

void MCPServerInstance::updateConfig(const QJsonObject& config) {
    // Only allow updates when stopped
    if (m_status != Stopped) {
        qWarning() << "Cannot update config while server is running";
        return;
    }
    m_config = config;
    // Re-parse configuration (similar to constructor)
    // ... (omitted for brevity)
}

void MCPServerInstance::onProcessStarted() {
    qDebug() << "Server" << m_name << "started with PID" << pid();
    setStatus(Running);
    m_restartCount = 0; // Reset restart counter on successful start
    startHealthMonitoring();
    emit started();
}

void MCPServerInstance::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    stopHealthMonitoring();

    // Reset initialization state
    m_initialized = false;
    m_pendingToolsRefresh = false;

    qDebug() << "Server" << m_name << "finished. Exit code:" << exitCode
             << "Status:" << (exitStatus == QProcess::NormalExit ? "Normal" : "Crash");

    // If we're intentionally stopping, treat as normal exit (not a crash)
    if (m_intentionalStop) {
        qDebug() << "Server" << m_name << "stopped intentionally (not a crash)";
        setStatus(Stopped);
        emit stopped();
        return;
    }

    if (exitStatus == QProcess::CrashExit) {
        m_lastError = QString("Process crashed with exit code %1").arg(exitCode);
        setStatus(Crashed);
        emit crashed();

        // Auto-restart if configured
        if (m_autoStart && m_restartCount < m_maxRestarts) {
            qDebug() << "Auto-restarting server" << m_name;
            QTimer::singleShot(2000, this, &MCPServerInstance::restart);
        }
    } else {
        setStatus(Stopped);
        emit stopped();
    }
}

void MCPServerInstance::onProcessError(QProcess::ProcessError error) {
    // If we're intentionally stopping and get a "Crashed" error, ignore it
    if (m_intentionalStop && error == QProcess::Crashed) {
        qDebug() << "Server" << m_name << "got crash signal during intentional stop (ignoring)";
        return;
    }

    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start. Check if command exists and is executable.";
            break;
        case QProcess::Crashed:
            errorMsg = "Process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "Process timed out";
            break;
        case QProcess::WriteError:
            errorMsg = "Write error";
            break;
        case QProcess::ReadError:
            errorMsg = "Read error";
            break;
        case QProcess::UnknownError:
        default:
            errorMsg = "Unknown error";
            break;
    }

    m_lastError = errorMsg;
    qWarning() << "Server" << m_name << "error:" << errorMsg;

    setStatus(Error);
    emit errorOccurred(errorMsg);
}

void MCPServerInstance::onReadyReadStandardOutput() {
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data).trimmed();

    if (output.isEmpty()) return;

    // Split into lines and add to buffer
    QStringList lines = output.split('\n');
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (!trimmedLine.isEmpty()) {
            m_outputBuffer.append(trimmedLine);

            // Keep buffer size limited
            if (m_outputBuffer.size() > m_maxOutputLines) {
                m_outputBuffer.removeFirst();
            }

            // Try to parse as JSON-RPC response
            if (trimmedLine.startsWith('{')) {
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(trimmedLine.toUtf8(), &parseError);

                if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                    QJsonObject obj = doc.object();

                    // Check if this is an initialize response (id = 1)
                    if (obj.contains("id") && obj["id"].toInt() == 1 && obj.contains("result")) {
                        qDebug() << "Received initialize response for" << m_name;
                        m_initialized = true;

                        // Send initialized notification (no id, it's a notification)
                        QJsonObject notification;
                        notification["jsonrpc"] = "2.0";
                        notification["method"] = "notifications/initialized";

                        QJsonDocument notifDoc(notification);
                        QByteArray notifData = notifDoc.toJson(QJsonDocument::Compact) + "\n";
                        m_process->write(notifData);
                        m_process->waitForBytesWritten(1000);
                        qDebug() << "Sent initialized notification to" << m_name;

                        // If we have a pending tools refresh, do it now
                        if (m_pendingToolsRefresh) {
                            m_pendingToolsRefresh = false;
                            qDebug() << "Proceeding with pending tools refresh";
                            refreshTools();  // This will now send tools/list since m_initialized = true
                        }

                        continue;  // Don't emit as regular output
                    }

                    // Check if this is a tools/list response (id = 999)
                    if (obj.contains("id") && obj["id"].toInt() == 999 && obj.contains("result")) {
                        parseToolsListResponse(obj);
                        continue;  // Don't emit as regular output
                    }
                }
            }

            emit outputReceived(trimmedLine);
        }
    }
}

void MCPServerInstance::onReadyReadStandardError() {
    QByteArray data = m_process->readAllStandardError();
    QString output = QString::fromUtf8(data).trimmed();

    if (!output.isEmpty()) {
        qWarning() << "Server" << m_name << "stderr:" << output;
        emit errorOutputReceived(output);
    }
}

void MCPServerInstance::onHealthCheck() {
    if (!isRunning()) {
        return;
    }

    // Simple health check: verify process is still alive and port is listening
    if (m_process->state() != QProcess::Running) {
        qWarning() << "Health check failed for" << m_name << "- process not running";
        setStatus(Crashed);
        emit crashed();
        return;
    }

    // TODO: Could add TCP port check here
    // For now, just verify process state
}

void MCPServerInstance::setStatus(ServerStatus status) {
    if (m_status == status) return;

    ServerStatus oldStatus = m_status;
    m_status = status;

    qDebug() << "Server" << m_name << "status changed:"
             << statusString() << "(was:" << statusString() << ")";

    emit statusChanged(oldStatus, status);
}

void MCPServerInstance::startHealthMonitoring() {
    if (m_healthCheckInterval > 0) {
        m_healthTimer->start(m_healthCheckInterval);
    }
}

void MCPServerInstance::stopHealthMonitoring() {
    m_healthTimer->stop();
}

bool MCPServerInstance::isToolEnabled(const QString& toolName) const {
    for (const ToolInfo& tool : m_tools) {
        if (tool.name == toolName) {
            return tool.enabled;
        }
    }
    // Tool not found - allow by default (for backwards compatibility)
    return true;
}

void MCPServerInstance::setToolEnabled(const QString& toolName, bool enabled) {
    for (ToolInfo& tool : m_tools) {
        if (tool.name == toolName) {
            tool.enabled = enabled;
            qDebug() << "Tool" << toolName << "in server" << m_name
                     << (enabled ? "enabled" : "disabled");
            return;
        }
    }
    qWarning() << "Tool" << toolName << "not found in server" << m_name;
}

void MCPServerInstance::refreshTools() {
    if (!isRunning()) {
        qWarning() << "Cannot refresh tools - server" << m_name << "is not running";
        return;
    }

    qDebug() << "Refreshing tools for" << m_name;

    // Check if we need to initialize first
    if (!m_initialized) {
        qDebug() << "Server not initialized, sending initialize handshake first";
        m_pendingToolsRefresh = true;

        // Send MCP initialize request
        QJsonObject params;
        params["protocolVersion"] = "2024-11-05";
        params["capabilities"] = QJsonObject();  // Empty capabilities

        QJsonObject clientInfo;
        clientInfo["name"] = "MCP Manager";
        clientInfo["version"] = "1.0.0";
        params["clientInfo"] = clientInfo;

        QJsonObject request;
        request["jsonrpc"] = "2.0";
        request["id"] = 1;  // Use ID 1 for initialize
        request["method"] = "initialize";
        request["params"] = params;

        QJsonDocument doc(request);
        QByteArray requestData = doc.toJson(QJsonDocument::Compact) + "\n";

        qint64 written = m_process->write(requestData);
        if (written == -1) {
            qWarning() << "Failed to write initialize request to" << m_name;
            m_pendingToolsRefresh = false;
            return;
        }

        m_process->waitForBytesWritten(1000);
        qDebug() << "Initialize request sent to" << m_name;
        return;  // Tools will be refreshed after initialization completes
    }

    // Server is initialized, send tools/list directly
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = 999;  // Special ID for tools query
    request["method"] = "tools/list";

    QJsonDocument doc(request);
    QByteArray requestData = doc.toJson(QJsonDocument::Compact) + "\n";

    qint64 written = m_process->write(requestData);
    if (written == -1) {
        qWarning() << "Failed to write tools/list request to" << m_name;
        return;
    }

    m_process->waitForBytesWritten(1000);
    qDebug() << "Tools/list request sent to" << m_name;

    // Note: Response will be received asynchronously via onReadyReadStandardOutput()
}

void MCPServerInstance::parseToolsListResponse(const QJsonObject& response) {
    qDebug() << "Parsing tools/list response for" << m_name;

    if (!response.contains("result")) {
        qWarning() << "No result in tools/list response";
        return;
    }

    QJsonObject result = response["result"].toObject();
    if (!result.contains("tools")) {
        qWarning() << "No tools array in result";
        return;
    }

    QJsonArray toolsArray = result["tools"].toArray();
    m_tools.clear();

    for (const QJsonValue& toolValue : toolsArray) {
        QJsonObject toolObj = toolValue.toObject();

        ToolInfo tool;
        tool.name = toolObj["name"].toString();
        tool.description = toolObj["description"].toString();
        tool.enabled = true;  // Enable by default
        tool.schema = toolObj["inputSchema"].toObject();

        // Parse permission metadata if available
        if (toolObj.contains("permissions")) {
            QJsonObject perms = toolObj["permissions"].toObject();
            if (perms.contains("categories")) {
                QJsonArray cats = perms["categories"].toArray();
                for (const QJsonValue& cat : cats) {
                    tool.permissions.append(cat.toString());
                }
            }
        }

        m_tools.append(tool);
        qDebug() << "  - Tool:" << tool.name << "-" << tool.description;
    }

    qDebug() << "Loaded" << m_tools.size() << "tools for" << m_name;

    // Emit signal to notify UI that tools have been updated
    emit toolsChanged();
}

bool MCPServerInstance::checkPortAvailable(quint16 port) {
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", port);

    // If connection succeeds, port is in use
    bool inUse = socket.waitForConnected(100);
    socket.disconnectFromHost();

    return !inUse;
}

QString MCPServerInstance::buildCommand() const {
    return m_command;
}

QStringList MCPServerInstance::buildArguments() const {
    return m_arguments;
}

QProcessEnvironment MCPServerInstance::buildEnvironment() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Add custom environment variables from config
    QJsonObject envObj = m_environment;
    for (auto it = envObj.begin(); it != envObj.end(); ++it) {
        env.insert(it.key(), it.value().toString());
    }

    return env;
}

// Permission management methods
bool MCPServerInstance::hasPermission(PermissionCategory category) const {
    // Check if there's an explicit override for this server
    if (m_permissions.contains(category)) {
        return m_permissions[category];
    }

    // Otherwise, fall back to global default from manager
    if (m_manager) {
        return m_manager->getGlobalPermission(category);
    }

    // Ultimate fallback: safe default (only READ_REMOTE allowed)
    return category == READ_REMOTE;
}

void MCPServerInstance::setPermission(PermissionCategory category, bool enabled) {
    m_permissions[category] = enabled;
    qDebug() << "Permission" << category << "set to" << enabled << "for" << m_name << "(explicit override)";
}

void MCPServerInstance::clearPermission(PermissionCategory category) {
    m_permissions.remove(category);
    qDebug() << "Permission" << category << "cleared for" << m_name << "(will use global default)";
}

bool MCPServerInstance::hasExplicitPermission(PermissionCategory category) const {
    return m_permissions.contains(category);
}

bool MCPServerInstance::checkToolPermissions(const QString& toolName) const {
    // Find the tool
    for (const ToolInfo& tool : m_tools) {
        if (tool.name == toolName) {
            // Check if all required permissions are granted
            for (const QString& permStr : tool.permissions) {
                PermissionCategory perm = READ_REMOTE;  // default

                if (permStr == "READ_REMOTE") perm = READ_REMOTE;
                else if (permStr == "WRITE_REMOTE") perm = WRITE_REMOTE;
                else if (permStr == "WRITE_LOCAL") perm = WRITE_LOCAL;
                else if (permStr == "EXECUTE_AI") perm = EXECUTE_AI;
                else if (permStr == "EXECUTE_CODE") perm = EXECUTE_CODE;

                if (!hasPermission(perm)) {
                    qWarning() << "Tool" << toolName << "blocked: missing permission" << permStr;
                    return false;
                }
            }
            return true;
        }
    }

    // Tool not found, allow by default (will be caught by tool existence check elsewhere)
    return true;
}
