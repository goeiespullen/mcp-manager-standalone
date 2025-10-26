#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QProcess>
#include <QTcpSocket>
#include <QJsonObject>
#include <QMap>

/**
 * @brief Represents a client session with an MCP server
 *
 * Each session represents one client connection with its own:
 * - Dedicated MCP server subprocess
 * - Client-specific credentials
 * - Request/response routing
 */
class MCPSession : public QObject {
    Q_OBJECT

public:
    explicit MCPSession(
        const QString& sessionId,
        const QString& serverType,
        const QJsonObject& serverConfig,
        const QJsonObject& credentials,
        QTcpSocket* clientSocket,
        QObject* parent = nullptr
    );
    ~MCPSession();

    // Session info
    QString sessionId() const { return m_sessionId; }
    QString serverType() const { return m_serverType; }
    QDateTime created() const { return m_created; }
    QDateTime lastActivity() const { return m_lastActivity; }
    bool isActive() const;

    // Client connection
    QTcpSocket* clientSocket() const { return m_clientSocket; }
    bool isClientConnected() const;

    // Lifecycle
    bool startServer();
    void stopServer();
    bool isServerRunning() const;

    // Request routing
    void sendRequest(const QJsonObject& request);

    // Stats
    int requestCount() const { return m_requestCount; }
    QString lastError() const { return m_lastError; }

signals:
    void serverStarted();
    void serverStopped();
    void serverError(const QString& error);
    void responseReceived(const QJsonObject& response);
    void clientDisconnected();

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessReadyRead();
    void onClientDisconnected();

private:
    void updateActivity();
    QProcessEnvironment buildEnvironment() const;
    void initializeServerProtocol();  // NEW: MCP protocol initialization

    QString m_sessionId;
    QString m_serverType;
    QJsonObject m_serverConfig;
    QJsonObject m_credentials;
    QDateTime m_created;
    QDateTime m_lastActivity;

    QProcess* m_process;
    QTcpSocket* m_clientSocket;

    int m_requestCount;
    QString m_lastError;

    // Process communication buffer
    QString m_outputBuffer;

    // MCP protocol state
    bool m_initialized;  // NEW: Track if MCP initialize handshake is complete
    int m_initRequestId;  // NEW: Track initialize request ID
    QList<QJsonObject> m_pendingRequests;  // NEW: Queue requests until initialized
};
