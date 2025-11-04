#pragma once

#include <QObject>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include "MCPServerInstance.h"

/**
 * @brief Manages multiple MCP server instances
 *
 * Central manager for all MCP servers. Handles:
 * - Loading/saving configuration
 * - Starting/stopping servers
 * - Server lifecycle management
 * - Global operations (start all, stop all, etc.)
 */
class MCPServerManager : public QObject {
    Q_OBJECT

public:
    explicit MCPServerManager(QObject* parent = nullptr);
    ~MCPServerManager();

    // Configuration
    bool loadConfig(const QString& configPath);
    bool saveConfig(const QString& configPath);
    QJsonObject currentConfig() const;
    QString configPath() const;

    // Server management
    bool addServer(const QJsonObject& serverConfig);
    bool removeServer(const QString& name);
    MCPServerInstance* getServer(const QString& name);
    QList<MCPServerInstance*> allServers() const;
    QStringList serverNames() const;
    int serverCount() const { return m_servers.size(); }

    // Lifecycle operations
    bool startServer(const QString& name);
    bool stopServer(const QString& name);
    bool restartServer(const QString& name);
    void startAll();
    void stopAll();
    void startAutoStartServers();

    // Status queries
    int runningCount() const;
    int stoppedCount() const;
    QMap<QString, MCPServerInstance::ServerStatus> allStatuses() const;

signals:
    void serverAdded(const QString& name);
    void serverRemoved(const QString& name);
    void serverStatusChanged(const QString& name,
                            MCPServerInstance::ServerStatus oldStatus,
                            MCPServerInstance::ServerStatus newStatus);
    void serverOutputReceived(const QString& name, const QString& line);
    void serverErrorOccurred(const QString& name, const QString& error);
    void configChanged();

private slots:
    void onServerStatusChanged(MCPServerInstance::ServerStatus oldStatus,
                              MCPServerInstance::ServerStatus newStatus);
    void onServerOutput(const QString& line);
    void onServerError(const QString& error);

private:
    QString findServerNameByInstance(MCPServerInstance* instance) const;
    bool validateServerConfig(const QJsonObject& config, QString& errorMsg);

    QMap<QString, MCPServerInstance*> m_servers;
    QString m_configPath;
    QJsonObject m_config;
};
