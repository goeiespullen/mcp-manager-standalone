#include "MCPGateway.h"
#include "MCPServerInstance.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>

MCPGateway::MCPGateway(MCPServerManager* serverManager, QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_serverManager(serverManager)
    , m_port(0)
    , m_sessionCounter(0)
{
    connect(m_server, &QTcpServer::newConnection, this, &MCPGateway::onNewConnection);
    qDebug() << "MCPGateway initialized";
}

MCPGateway::~MCPGateway() {
    stop();
}

bool MCPGateway::start(quint16 port) {
    if (m_server->isListening()) {
        qWarning() << "Gateway already running on port" << m_port;
        return false;
    }

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        qWarning() << "Failed to start gateway on port" << port << ":" << m_server->errorString();
        return false;
    }

    m_port = m_server->serverPort();
    qDebug() << "MCPGateway listening on port" << m_port;

    return true;
}

void MCPGateway::stop() {
    if (!m_server->isListening()) {
        return;
    }

    qDebug() << "Stopping MCPGateway";

    // Clean up all sessions
    QStringList sessionIds = m_sessions.keys();
    for (const QString& sessionId : sessionIds) {
        cleanupSession(sessionId);
    }

    // Close all client connections
    for (QTcpSocket* client : m_clients.keys()) {
        client->disconnectFromHost();
    }

    m_server->close();
    qDebug() << "MCPGateway stopped";
}

bool MCPGateway::isRunning() const {
    return m_server->isListening();
}

void MCPGateway::onNewConnection() {
    QTcpSocket* client = m_server->nextPendingConnection();
    if (!client) {
        return;
    }

    QString clientId = getClientId(client);
    m_clients.insert(client, clientId);
    m_clientBuffers.insert(client, QString());

    qDebug() << "Client connected:" << clientId;

    connect(client, &QTcpSocket::readyRead, this, &MCPGateway::onClientReadyRead);
    connect(client, &QTcpSocket::disconnected, this, &MCPGateway::onClientDisconnected);

    emit clientConnected(clientId);
}

void MCPGateway::onClientReadyRead() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }

    // Read data into buffer
    QByteArray data = client->readAll();
    m_clientBuffers[client] += QString::fromUtf8(data);

    // Process complete messages (line-delimited JSON)
    QString& buffer = m_clientBuffers[client];
    while (buffer.contains('\n')) {
        int newlinePos = buffer.indexOf('\n');
        QString line = buffer.left(newlinePos).trimmed();
        buffer.remove(0, newlinePos + 1);

        if (line.isEmpty()) {
            continue;
        }

        qDebug() << "Received from" << getClientId(client) << ":" << line;
        emit messageTraffic("IN", getClientId(client), line);

        // Parse JSON message
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << parseError.errorString();
            sendError(client, QJsonValue(), -32700, "Parse error");
            continue;
        }

        if (!doc.isObject()) {
            sendError(client, QJsonValue(), -32600, "Invalid Request");
            continue;
        }

        handleMessage(client, doc.object());
    }
}

void MCPGateway::onClientDisconnected() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }

    QString clientId = getClientId(client);
    qDebug() << "Client disconnected:" << clientId;

    // Clean up any sessions for this client
    QStringList sessionsToRemove;
    for (auto it = m_sessionClients.begin(); it != m_sessionClients.end(); ++it) {
        if (it.value() == client) {
            sessionsToRemove.append(it.key());
        }
    }

    for (const QString& sessionId : sessionsToRemove) {
        qDebug() << "Auto-destroying session" << sessionId << "due to client disconnect";
        cleanupSession(sessionId);
    }

    m_clients.remove(client);
    m_clientBuffers.remove(client);
    client->deleteLater();

    emit clientDisconnected(clientId);
}

void MCPGateway::handleMessage(QTcpSocket* client, const QJsonObject& message) {
    QString method = message["method"].toString();
    QJsonValue id = message["id"];
    QJsonObject params = message["params"].toObject();

    qDebug() << "Handling method:" << method;

    if (method == "mcp-manager/create-session") {
        handleCreateSession(client, id, params);
    } else if (method == "mcp-manager/destroy-session") {
        handleDestroySession(client, id, params);
    } else if (method == "mcp-manager/list-sessions") {
        handleListSessions(client, id);
    } else if (method == "mcp-manager/list-servers") {
        handleListServers(client, id);
    } else if (method == "tools/call") {
        handleToolCall(client, id, params);
    } else {
        sendError(client, id, -32601, "Method not found: " + method);
    }
}

void MCPGateway::handleCreateSession(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params) {
    QString serverType = params["serverType"].toString();
    QJsonObject credentials = params["credentials"].toObject();

    if (serverType.isEmpty()) {
        sendError(client, id, -32602, "Missing serverType parameter");
        return;
    }

    // Get server configuration from manager
    QJsonObject serverConfig;
    QList<MCPServerInstance*> servers = m_serverManager->allServers();
    for (MCPServerInstance* server : servers) {
        if (server->name() == serverType) {
            serverConfig = server->config();
            break;
        }
    }

    if (serverConfig.isEmpty()) {
        sendError(client, id, -32602, "Unknown server type: " + serverType);
        return;
    }

    // Generate session ID
    QString sessionId = generateSessionId();

    // Create session
    MCPSession* session = new MCPSession(
        sessionId,
        serverType,
        serverConfig,
        credentials,
        client,
        this
    );

    // Connect session signals
    connect(session, &MCPSession::responseReceived, this, &MCPGateway::onSessionResponse);
    connect(session, &MCPSession::serverError, this, &MCPGateway::onSessionServerError);
    connect(session, &MCPSession::clientDisconnected, this, &MCPGateway::onSessionClientDisconnected);

    // Start server
    if (!session->startServer()) {
        sendError(client, id, -32603, "Failed to start MCP server: " + session->lastError());
        delete session;
        return;
    }

    // Store session
    m_sessions.insert(sessionId, session);
    m_sessionClients.insert(sessionId, client);

    qDebug() << "Session created:" << sessionId << "for server type:" << serverType;
    emit sessionCreated(sessionId);

    // Send success response
    QJsonObject result;
    result["sessionId"] = sessionId;
    result["serverType"] = serverType;
    result["created"] = session->created().toString(Qt::ISODate);

    sendSuccess(client, id, result);
}

void MCPGateway::handleDestroySession(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params) {
    QString sessionId = params["sessionId"].toString();

    if (sessionId.isEmpty()) {
        sendError(client, id, -32602, "Missing sessionId parameter");
        return;
    }

    if (!m_sessions.contains(sessionId)) {
        sendError(client, id, -32602, "Session not found: " + sessionId);
        return;
    }

    // Verify client owns this session
    if (m_sessionClients[sessionId] != client) {
        sendError(client, id, -32603, "Session owned by different client");
        return;
    }

    cleanupSession(sessionId);

    QJsonObject result;
    result["sessionId"] = sessionId;
    result["destroyed"] = true;

    sendSuccess(client, id, result);
}

void MCPGateway::handleToolCall(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params) {
    QString sessionId = params["sessionId"].toString();

    if (sessionId.isEmpty()) {
        sendError(client, id, -32602, "Missing sessionId parameter");
        return;
    }

    MCPSession* session = m_sessions.value(sessionId);
    if (!session) {
        sendError(client, id, -32602, "Session not found: " + sessionId);
        return;
    }

    // Verify client owns this session
    if (m_sessionClients[sessionId] != client) {
        sendError(client, id, -32603, "Session owned by different client");
        return;
    }

    // Check if tool is enabled for this server
    QString toolName = params["name"].toString();
    QString serverType = session->serverType();

    // Get server instance to check if tool is enabled
    MCPServerInstance* server = nullptr;
    QList<MCPServerInstance*> servers = m_serverManager->allServers();
    for (MCPServerInstance* s : servers) {
        if (s->name() == serverType) {
            server = s;
            break;
        }
    }

    if (server && !server->isToolEnabled(toolName)) {
        sendError(client, id, -32001, QString("Tool '%1' is disabled for server '%2'").arg(toolName, serverType));
        qWarning() << "Blocked disabled tool:" << toolName << "for server:" << serverType;
        return;
    }

    // Forward request to MCP server subprocess
    QJsonObject mcpRequest;
    mcpRequest["jsonrpc"] = "2.0";
    mcpRequest["id"] = id;
    mcpRequest["method"] = "tools/call";

    QJsonObject mcpParams;
    mcpParams["name"] = params["name"];
    mcpParams["arguments"] = params["arguments"];
    mcpRequest["params"] = mcpParams;

    session->sendRequest(mcpRequest);
}

void MCPGateway::handleListSessions(QTcpSocket* client, const QJsonValue& id) {
    QJsonArray sessions;

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        MCPSession* session = it.value();

        // Only show sessions owned by this client
        if (m_sessionClients[it.key()] != client) {
            continue;
        }

        QJsonObject sessionInfo;
        sessionInfo["sessionId"] = session->sessionId();
        sessionInfo["serverType"] = session->serverType();
        sessionInfo["created"] = session->created().toString(Qt::ISODate);
        sessionInfo["lastActivity"] = session->lastActivity().toString(Qt::ISODate);
        sessionInfo["requestCount"] = session->requestCount();
        sessionInfo["active"] = session->isActive();

        sessions.append(sessionInfo);
    }

    QJsonObject result;
    result["sessions"] = sessions;
    result["count"] = sessions.size();

    sendSuccess(client, id, result);
}

void MCPGateway::handleListServers(QTcpSocket* client, const QJsonValue& id) {
    QJsonArray serversArray;

    QList<MCPServerInstance*> servers = m_serverManager->allServers();
    for (MCPServerInstance* server : servers) {
        QJsonObject serverObj;
        serverObj["name"] = server->name();
        serverObj["type"] = server->type();
        serverObj["port"] = static_cast<int>(server->port());
        serverObj["status"] = server->statusString();
        serverObj["isRunning"] = server->isRunning();
        serverObj["autoStart"] = server->autoStart();

        serversArray.append(serverObj);
    }

    QJsonObject result;
    result["servers"] = serversArray;
    result["count"] = serversArray.size();

    sendSuccess(client, id, result);
}

void MCPGateway::onSessionResponse(const QJsonObject& response) {
    MCPSession* session = qobject_cast<MCPSession*>(sender());
    if (!session) {
        return;
    }

    // Find client for this session
    QTcpSocket* client = m_sessionClients.value(session->sessionId());
    if (!client || !client->isOpen()) {
        qWarning() << "Session" << session->sessionId() << "has no connected client";
        return;
    }

    // Forward response to client
    sendResponse(client, response);
}

void MCPGateway::onSessionServerError(const QString& error) {
    MCPSession* session = qobject_cast<MCPSession*>(sender());
    if (!session) {
        return;
    }

    qWarning() << "Session" << session->sessionId() << "server error:" << error;

    // Optionally notify client
    QTcpSocket* client = m_sessionClients.value(session->sessionId());
    if (client && client->isOpen()) {
        QJsonObject notification;
        notification["jsonrpc"] = "2.0";
        notification["method"] = "mcp-manager/session-error";

        QJsonObject params;
        params["sessionId"] = session->sessionId();
        params["error"] = error;
        notification["params"] = params;

        sendResponse(client, notification);
    }
}

void MCPGateway::onSessionClientDisconnected() {
    MCPSession* session = qobject_cast<MCPSession*>(sender());
    if (!session) {
        return;
    }

    qDebug() << "Session" << session->sessionId() << "client disconnected via session signal";
    cleanupSession(session->sessionId());
}

void MCPGateway::sendResponse(QTcpSocket* client, const QJsonObject& response) {
    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";

    qDebug() << "Sending to" << getClientId(client) << ":" << data.trimmed();
    emit messageTraffic("OUT", getClientId(client), QString::fromUtf8(data.trimmed()));

    client->write(data);
    client->flush();
}

void MCPGateway::sendError(QTcpSocket* client, const QJsonValue& id, int code, const QString& message) {
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;

    QJsonObject error;
    error["code"] = code;
    error["message"] = message;
    response["error"] = error;

    sendResponse(client, response);
}

void MCPGateway::sendSuccess(QTcpSocket* client, const QJsonValue& id, const QJsonObject& result) {
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;

    sendResponse(client, response);
}

QString MCPGateway::generateSessionId() {
    m_sessionCounter++;
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(16);
}

QString MCPGateway::getClientId(QTcpSocket* socket) const {
    return m_clients.value(socket, QString("unknown-%1").arg(socket->socketDescriptor()));
}

MCPSession* MCPGateway::findSessionByClient(QTcpSocket* client) const {
    for (auto it = m_sessionClients.begin(); it != m_sessionClients.end(); ++it) {
        if (it.value() == client) {
            return m_sessions.value(it.key());
        }
    }
    return nullptr;
}

void MCPGateway::cleanupSession(const QString& sessionId) {
    MCPSession* session = m_sessions.value(sessionId);
    if (!session) {
        return;
    }

    qDebug() << "Cleaning up session:" << sessionId;

    session->stopServer();
    m_sessions.remove(sessionId);
    m_sessionClients.remove(sessionId);

    session->deleteLater();

    emit sessionDestroyed(sessionId);
}
