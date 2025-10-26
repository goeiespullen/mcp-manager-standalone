#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

class AzureDevOpsClient;

/**
 * @brief MCP (Model Context Protocol) Server using TCP sockets
 *
 * Implements JSON-RPC 2.0 protocol for communication with MCP clients.
 * Supports multiple concurrent client connections.
 */
class MCPServer : public QObject {
    Q_OBJECT

public:
    explicit MCPServer(AzureDevOpsClient* devopsClient, QObject* parent = nullptr);
    ~MCPServer();

    bool start(quint16 port = 8765);
    void stop();
    bool isRunning() const;
    quint16 port() const { return m_port; }
    int clientCount() const { return m_clients.size(); }

signals:
    void clientConnected(const QString& clientId);
    void clientDisconnected(const QString& clientId);
    void messageReceived(const QString& clientId, const QString& method, const QJsonObject& message);
    void messageSent(const QString& clientId, const QJsonObject& message);
    void errorOccurred(const QString& error);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientReadyRead();

private:
    void handleMessage(QTcpSocket* client, const QJsonObject& message);
    void sendResponse(QTcpSocket* client, const QJsonObject& response);
    void sendError(QTcpSocket* client, const QJsonValue& id, int code, const QString& message);

    // JSON-RPC handlers
    void handleInitialize(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params);
    void handleToolCall(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params);
    void handleListTools(QTcpSocket* client, const QJsonValue& id);

    QString getClientId(QTcpSocket* client) const;

    QTcpServer* m_server;
    AzureDevOpsClient* m_devopsClient;
    QMap<QTcpSocket*, QString> m_clients; // socket -> client ID
    quint16 m_port;
    bool m_initialized;
};
