#include "MCPGateway.h"
#include "MCPServerInstance.h"
#include "Logger.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
#include <QProcess>
#include <QDir>
#include <QFile>

MCPGateway::MCPGateway(MCPServerManager* serverManager, QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_serverManager(serverManager)
    , m_keystore(new Keystore(this))
    , m_port(0)
    , m_sessionCounter(0)
{
    connect(m_server, &QTcpServer::newConnection, this, &MCPGateway::onNewConnection);

    // Connect permission change signals to destroy sessions when permissions change
    connect(m_serverManager, &MCPServerManager::serverPermissionsChanged,
            this, &MCPGateway::onServerPermissionsChanged);
    connect(m_serverManager, &MCPServerManager::globalPermissionsChanged,
            this, &MCPGateway::onGlobalPermissionsChanged);

    qDebug() << "MCPGateway initialized with C++ Keystore";
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

    // Check if client still exists (could have disconnected during signal processing)
    if (!m_clientBuffers.contains(client)) {
        return;
    }

    // Read data into buffer
    QByteArray data = client->readAll();
    m_clientBuffers[client] += QString::fromUtf8(data);

    // Check again - client could have disconnected after readAll()
    if (!m_clientBuffers.contains(client)) {
        return;
    }

    // Process complete messages (line-delimited JSON)
    QString& buffer = m_clientBuffers[client];
    while (buffer.contains('\n')) {
        // Check if still connected (signals can be processed during event loop)
        if (!m_clientBuffers.contains(client)) {
            return;
        }

        int newlinePos = buffer.indexOf('\n');
        QString line = buffer.left(newlinePos).trimmed();
        buffer.remove(0, newlinePos + 1);

        if (line.isEmpty()) {
            continue;
        }

        qDebug() << "Received from" << getClientId(client) << ":" << line;
        emit messageTraffic("IN", getClientId(client), line);
        LOG_TRAFFIC("IN", getClientId(client), line);

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
    } else if (method == "tools/list") {
        handleToolsList(client, id, params);
    } else if (method == "tools/call") {
        handleToolCall(client, id, params);
    } else {
        sendError(client, id, -32601, "Method not found: " + method);
    }
}

void MCPGateway::handleCreateSession(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params) {
    QString serverType = params["serverType"].toString();
    QString userId = params["userId"].toString();
    QString clientApp = params["clientApp"].toString();
    QJsonObject credentials = params["credentials"].toObject();

    // Default clientApp if not provided
    if (clientApp.isEmpty()) {
        clientApp = "Unknown";
    }

    if (serverType.isEmpty()) {
        sendError(client, id, -32602, "Missing serverType parameter");
        return;
    }

    // USER-BASED AUTHENTICATION: If userId is provided, lookup token from keystore
    if (!userId.isEmpty()) {
        qDebug() << "User-based authentication: userId =" << userId;
        LOG_INFO(Logger::Gateway,
                QString("Creating session for user %1, server %2")
                .arg(userId).arg(serverType));

        // Get token from encrypted keystore via Python helper
        QString token = getTokenForUser(userId, serverType);

        if (token.isEmpty()) {
            sendError(client, id, -32001,
                     QString("No credentials found for user %1, system %2. "
                            "Please register credentials using register_token.py")
                     .arg(userId).arg(serverType));
            return;
        }

        // Build credentials object based on server type
        credentials = QJsonObject();
        if (serverType == "azure" || serverType == "Azure DevOps") {
            credentials["pat"] = token;
        } else if (serverType == "confluence" || serverType == "Atlassian") {
            credentials["token"] = token;
        } else {
            // Generic fallback
            credentials["token"] = token;
        }

        qDebug() << "User credentials loaded for" << userId << "- server type:" << serverType;
    }
    // LEGACY: credentials object provided directly (backward compatible)
    else if (!credentials.isEmpty()) {
        qDebug() << "Legacy authentication: using provided credentials";
        LOG_WARNING(Logger::Gateway,
                   QString("Session created using legacy credentials (no userId) for server %1")
                   .arg(serverType));
    }
    // ERROR: neither userId nor credentials provided
    else {
        sendError(client, id, -32602,
                 "Missing authentication: provide either 'userId' or 'credentials' parameter");
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
        userId,
        clientApp,
        client,
        this
    );

    // AUTO-REGISTER CLIENT (for dynamic client permissions tracking)
    if (!userId.isEmpty() && !clientApp.isEmpty()) {
        m_serverManager->registerClient(userId, clientApp);
        qDebug() << "Auto-registered client:" << userId << "|" << clientApp;
        LOG_DEBUG(Logger::Gateway,
                 QString("Registered client: %1 | %2").arg(userId, clientApp));
    }

    // Connect session signals
    connect(session, &MCPSession::responseReceived, this, &MCPGateway::onSessionResponse);
    connect(session, &MCPSession::serverError, this, &MCPGateway::onSessionServerError);
    connect(session, &MCPSession::clientDisconnected, this, &MCPGateway::onSessionClientDisconnected);

    // Load and apply user permissions from C++ Keystore
    if (!userId.isEmpty()) {
        QStringList permissionsList = m_keystore->getUserPermissions(userId, serverType.toLower());
        QSet<QString> permissions = QSet<QString>(permissionsList.begin(), permissionsList.end());
        session->setPermissions(permissions);

        if (permissions.isEmpty()) {
            qDebug() << "No user-specific permissions for user:" << userId << "- will use global permissions";
            LOG_INFO(Logger::Gateway,
                    QString("User %1 has no user-specific permissions for %2 - will inherit global permissions")
                    .arg(userId).arg(serverType));
        } else {
            qDebug() << "Loaded" << permissions.size() << "user-specific permissions for user:" << userId << "server:" << serverType;
            LOG_INFO(Logger::Gateway,
                    QString("User %1 has %2 user-specific allowed tools for %3 (overrides global): %4")
                    .arg(userId).arg(permissions.size()).arg(serverType)
                    .arg(permissionsList.join(", ")));
        }
    }

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
    QString toolName = params["name"].toString();

    // Log tool call start with timing
    LOG_INFO(Logger::Gateway, QString("Tool call started: session=%1, tool=%2, id=%3")
        .arg(sessionId).arg(toolName).arg(id.toString()));

    if (sessionId.isEmpty()) {
        sendError(client, id, -32602, "Missing sessionId parameter");
        return;
    }

    MCPSession* session = m_sessions.value(sessionId);
    if (!session) {
        sendError(client, id, -32602, "Session not found: " + sessionId);
        LOG_ERROR(Logger::Gateway, QString("Tool call failed: session not found: %1").arg(sessionId));
        return;
    }

    // Verify client owns this session
    if (m_sessionClients[sessionId] != client) {
        sendError(client, id, -32603, "Session owned by different client");
        LOG_ERROR(Logger::Gateway, QString("Tool call failed: session owned by different client"));
        return;
    }

    // Check if tool is enabled for this server
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
        LOG_WARNING(Logger::Gateway, QString("Tool call blocked: tool %1 disabled for server %2").arg(toolName, serverType));
        return;
    }

    // Permission hierarchy: user-specific permissions override global permissions
    if (session->hasUserSpecificPermissions()) {
        // User has explicit permissions set - check for block-all marker first
        if (session->hasPermission("__BLOCK_ALL__")) {
            sendError(client, id, -32005, QString("Tool '%1' blocked: user '%2' has all permissions blocked").arg(toolName, session->userId()));
            LOG_WARNING(Logger::Gateway, QString("Tool call blocked: user %1 has block-all restriction (all tools denied)").arg(session->userId()));
            return;
        }

        // Check against user allowlist
        if (!session->hasPermission(toolName)) {
            sendError(client, id, -32004, QString("Tool '%1' blocked: user '%2' does not have permission").arg(toolName, session->userId()));
            LOG_WARNING(Logger::Gateway, QString("Tool call blocked: user %1 lacks permission for tool %2 (user-specific restrictions)").arg(session->userId(), toolName));
            return;
        }
        LOG_DEBUG(Logger::Gateway, QString("Tool %1 allowed for user %2 via user-specific permissions").arg(toolName, session->userId()));
    } else {
        // No user-specific permissions - check global tool permissions
        if (server && !server->checkToolPermissions(toolName)) {
            sendError(client, id, -32003, QString("Tool '%1' blocked: insufficient permissions for server '%2'").arg(toolName, serverType));
            LOG_WARNING(Logger::Gateway, QString("Tool call blocked: insufficient permissions for tool %1 on server %2 (global restrictions)").arg(toolName, serverType));
            return;
        }
        LOG_DEBUG(Logger::Gateway, QString("Tool %1 allowed for user %2 via global permissions").arg(toolName, session->userId()));
    }

    // Log tool arguments for debugging
    QJsonDocument argsDoc(params["arguments"].toObject());
    LOG_DEBUG(Logger::Gateway, QString("Tool call arguments: %1").arg(QString(argsDoc.toJson(QJsonDocument::Compact))));

    // Forward request to MCP server subprocess
    QJsonObject mcpRequest;
    mcpRequest["jsonrpc"] = "2.0";
    mcpRequest["id"] = id;
    mcpRequest["method"] = "tools/call";

    QJsonObject mcpParams;
    mcpParams["name"] = params["name"];
    mcpParams["arguments"] = params["arguments"];
    mcpRequest["params"] = mcpParams;

    LOG_INFO(Logger::Gateway, QString("Forwarding tool call to MCP server: session=%1, tool=%2").arg(sessionId, toolName));
    session->sendRequest(mcpRequest);
}

void MCPGateway::handleToolsList(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params) {
    QString sessionId = params["sessionId"].toString();

    LOG_INFO(Logger::Gateway, QString("Tools list requested: session=%1").arg(sessionId));

    if (sessionId.isEmpty()) {
        sendError(client, id, -32602, "Missing sessionId parameter");
        return;
    }

    MCPSession* session = m_sessions.value(sessionId);
    if (!session) {
        sendError(client, id, -32602, "Session not found: " + sessionId);
        LOG_ERROR(Logger::Gateway, QString("Tools list failed: session not found: %1").arg(sessionId));
        return;
    }

    // Verify client owns this session
    if (m_sessionClients[sessionId] != client) {
        sendError(client, id, -32603, "Session owned by different client");
        LOG_ERROR(Logger::Gateway, QString("Tools list failed: session owned by different client"));
        return;
    }

    // Forward tools/list request to MCP server subprocess
    QJsonObject mcpRequest;
    mcpRequest["jsonrpc"] = "2.0";
    mcpRequest["id"] = id;
    mcpRequest["method"] = "tools/list";
    mcpRequest["params"] = QJsonObject();  // tools/list has no params

    LOG_INFO(Logger::Gateway, QString("Forwarding tools/list to MCP server: session=%1").arg(sessionId));
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
    LOG_TRAFFIC("OUT", getClientId(client), QString::fromUtf8(data.trimmed()));

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

void MCPGateway::onServerPermissionsChanged(const QString& serverName) {
    qDebug() << "Server permissions changed for:" << serverName << "- destroying all sessions for this server";
    LOG_WARNING(Logger::Gateway, QString("Permissions changed for server %1, destroying all related sessions").arg(serverName));

    // Find and destroy all sessions for this server type
    QStringList sessionsToDestroy;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        MCPSession* session = it.value();
        if (session->serverType() == serverName) {
            sessionsToDestroy.append(it.key());
        }
    }

    // Destroy the sessions
    for (const QString& sessionId : sessionsToDestroy) {
        qDebug() << "Destroying session" << sessionId << "due to permission change";
        cleanupSession(sessionId);
    }

    if (!sessionsToDestroy.isEmpty()) {
        qDebug() << "Destroyed" << sessionsToDestroy.size() << "session(s) for server" << serverName;
    }
}

void MCPGateway::onGlobalPermissionsChanged() {
    qDebug() << "Global permissions changed - destroying ALL sessions";
    LOG_WARNING(Logger::Gateway, "Global permissions changed, destroying all active sessions");

    // Destroy all active sessions when global permissions change
    QStringList sessionIds = m_sessions.keys();
    for (const QString& sessionId : sessionIds) {
        qDebug() << "Destroying session" << sessionId << "due to global permission change";
        cleanupSession(sessionId);
    }

    if (!sessionIds.isEmpty()) {
        qDebug() << "Destroyed" << sessionIds.size() << "total session(s) due to global permission change";
    }
}

QString MCPGateway::getTokenForUser(const QString& userId, const QString& system) {
    /**
     * Get credential token for user from C++ Keystore.
     *
     * This method uses the C++ Keystore to retrieve user-specific credentials
     * with fallback to shared credentials.
     *
     * @param userId User email address (e.g., "user@ns.nl")
     * @param system System name (e.g., "azure", "confluence", "teamcentraal")
     * @return Token string, or empty string if not found
     */

    qDebug() << "Looking up token for user:" << userId << "system:" << system;

    // Map system names to credential keys
    QString credentialKey;
    if (system.toLower() == "azure" || system == "Azure DevOps") {
        credentialKey = "pat";  // Personal Access Token
    } else if (system.toLower() == "confluence" || system == "Atlassian") {
        credentialKey = "token";  // API Token
    } else if (system.toLower() == "teamcentraal") {
        credentialKey = "password";  // Password
    } else if (system.toLower() == "chatns") {
        credentialKey = "api_key";  // API Key
    } else {
        // Generic fallback - try "token" first, then "password"
        credentialKey = "token";
    }

    // Retrieve credential from C++ Keystore with user-specific lookup
    QString token = m_keystore->getUserCredential(userId, system.toLower(), credentialKey);

    // If generic fallback didn't work with "token", try "password"
    if (token.isEmpty() && credentialKey == "token") {
        token = m_keystore->getUserCredential(userId, system.toLower(), "password");
    }

    if (token.isEmpty()) {
        qWarning() << "No credential found for user:" << userId << "system:" << system << "key:" << credentialKey;
        LOG_WARNING(Logger::Gateway,
                   QString("No credential for user %1, system %2, key %3")
                   .arg(userId).arg(system).arg(credentialKey));
        return QString();
    }

    qDebug() << "Token retrieved successfully from C++ Keystore for user:" << userId << "system:" << system;
    LOG_INFO(Logger::Gateway,
            QString("Token retrieved for user %1, system %2")
            .arg(userId).arg(system));

    return token;
}
