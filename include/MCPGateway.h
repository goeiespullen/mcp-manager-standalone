#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QJsonObject>
#include "MCPSession.h"
#include "MCPServerManager.h"

/**
 * @brief MCP Gateway - Master TCP server for session-based MCP access
 *
 * The gateway provides:
 * - Session management (create, destroy)
 * - Credential injection per session
 * - Request routing to appropriate MCP server subprocess
 * - Multi-client support with isolation
 *
 * Listens on port 8700 (master port) for client connections.
 */
class MCPGateway : public QObject {
    Q_OBJECT

public:
    explicit MCPGateway(MCPServerManager* serverManager, QObject* parent = nullptr);
    ~MCPGateway();

    bool start(quint16 port = 8700);
    void stop();
    bool isRunning() const;
    quint16 port() const { return m_port; }

    // Session management
    int activeSessionCount() const { return m_sessions.size(); }
    QStringList activeSessions() const { return m_sessions.keys(); }

signals:
    void clientConnected(const QString& clientId);
    void clientDisconnected(const QString& clientId);
    void sessionCreated(const QString& sessionId);
    void sessionDestroyed(const QString& sessionId);
    void errorOccurred(const QString& error);
    void messageTraffic(const QString& direction, const QString& clientId, const QString& message);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onSessionResponse(const QJsonObject& response);
    void onSessionServerError(const QString& error);
    void onSessionClientDisconnected();
    void onServerPermissionsChanged(const QString& serverName);
    void onGlobalPermissionsChanged();

private:
    // Protocol handlers
    void handleMessage(QTcpSocket* client, const QJsonObject& message);
    void handleCreateSession(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params);
    void handleDestroySession(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params);
    void handleToolCall(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params);
    void handleToolsList(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params);
    void handleListSessions(QTcpSocket* client, const QJsonValue& id);
    void handleListServers(QTcpSocket* client, const QJsonValue& id);

    // Response helpers
    void sendResponse(QTcpSocket* client, const QJsonObject& response);
    void sendError(QTcpSocket* client, const QJsonValue& id, int code, const QString& message);
    void sendSuccess(QTcpSocket* client, const QJsonValue& id, const QJsonObject& result);

    // Session helpers
    QString generateSessionId();
    QString getClientId(QTcpSocket* socket) const;
    MCPSession* findSessionByClient(QTcpSocket* client) const;
    void cleanupSession(const QString& sessionId);

    QTcpServer* m_server;
    MCPServerManager* m_serverManager;
    quint16 m_port;

    // Client connections
    QMap<QTcpSocket*, QString> m_clients;  // socket -> client ID
    QMap<QTcpSocket*, QString> m_clientBuffers;  // socket -> receive buffer

    // Active sessions
    QMap<QString, MCPSession*> m_sessions;  // session ID -> session
    QMap<QString, QTcpSocket*> m_sessionClients;  // session ID -> client socket

    int m_sessionCounter;
};
