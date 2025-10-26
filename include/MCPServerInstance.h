#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QString>
#include <QTimer>

/**
 * @brief Manages a single external MCP server process
 *
 * Each instance represents one MCP server (e.g., Azure DevOps, Confluence)
 * running as a separate process. Handles process lifecycle, health monitoring,
 * and communication.
 */
class MCPServerInstance : public QObject {
    Q_OBJECT

public:
    enum ServerStatus {
        Stopped,
        Starting,
        Running,
        Stopping,
        Crashed,
        Error
    };
    Q_ENUM(ServerStatus)

    explicit MCPServerInstance(const QJsonObject& config, QObject* parent = nullptr);
    ~MCPServerInstance();

    // Lifecycle
    bool start();
    void stop();
    void restart();
    void kill();

    // Status
    QString name() const { return m_name; }
    QString type() const { return m_type; }
    quint16 port() const { return m_port; }
    ServerStatus status() const { return m_status; }
    QString statusString() const;
    bool isRunning() const { return m_status == Running; }
    bool autoStart() const { return m_autoStart; }

    // Tool management
    struct ToolInfo {
        QString name;
        QString description;
        bool enabled;
        QJsonObject schema;
    };
    QList<ToolInfo> availableTools() const { return m_tools; }
    bool isToolEnabled(const QString& toolName) const;
    void setToolEnabled(const QString& toolName, bool enabled);
    void refreshTools(); // Query tools from running server

    // Process info
    qint64 pid() const;
    QString lastError() const { return m_lastError; }
    QStringList recentOutput(int lines = 50) const;

    // Configuration
    QJsonObject config() const { return m_config; }
    void updateConfig(const QJsonObject& config);

signals:
    void statusChanged(ServerStatus oldStatus, ServerStatus newStatus);
    void started();
    void stopped();
    void crashed();
    void errorOccurred(const QString& error);
    void outputReceived(const QString& line);
    void errorOutputReceived(const QString& line);
    void toolsChanged(); // Emitted when tools list is updated

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onHealthCheck();

private:
    void setStatus(ServerStatus status);
    void startHealthMonitoring();
    void stopHealthMonitoring();
    bool checkPortAvailable(quint16 port);
    QString buildCommand() const;
    QStringList buildArguments() const;
    QProcessEnvironment buildEnvironment() const;
    void parseToolsListResponse(const QJsonObject& response);

    QProcess* m_process;
    QTimer* m_healthTimer;

    // Configuration
    QJsonObject m_config;
    QString m_name;
    QString m_type;          // "python", "node", "binary"
    QString m_command;
    QStringList m_arguments;
    quint16 m_port;
    QString m_workingDir;
    QJsonObject m_environment;
    bool m_autoStart;
    int m_healthCheckInterval;  // milliseconds

    // State
    ServerStatus m_status;
    QString m_lastError;
    QStringList m_outputBuffer;  // Circular buffer for recent output
    int m_maxOutputLines;
    int m_restartCount;
    int m_maxRestarts;
    bool m_intentionalStop;  // Track if we're stopping intentionally (not a crash)

    // Tools
    QList<ToolInfo> m_tools;  // Available tools from this server
    bool m_initialized;  // Track if MCP initialization handshake is complete
    bool m_pendingToolsRefresh;  // Track if we need to refresh tools after initialization
};
