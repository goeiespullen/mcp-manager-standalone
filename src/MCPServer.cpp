#include "MCPServer.h"
#include "AzureDevOpsClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

MCPServer::MCPServer(AzureDevOpsClient* devopsClient, QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_devopsClient(devopsClient)
    , m_port(0)
    , m_initialized(false)
{
    connect(m_server, &QTcpServer::newConnection, this, &MCPServer::onNewConnection);
}

MCPServer::~MCPServer() {
    stop();
}

bool MCPServer::start(quint16 port) {
    if (m_server->isListening()) {
        qWarning() << "Server already running";
        return false;
    }

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        emit errorOccurred(QString("Failed to start server: %1").arg(m_server->errorString()));
        return false;
    }

    m_port = m_server->serverPort();
    qInfo() << "MCP Server started on port" << m_port;
    return true;
}

void MCPServer::stop() {
    if (!m_server->isListening()) {
        return;
    }

    // Disconnect all clients
    for (auto client : m_clients.keys()) {
        client->disconnectFromHost();
    }
    m_clients.clear();

    m_server->close();
    m_port = 0;
    qInfo() << "MCP Server stopped";
}

bool MCPServer::isRunning() const {
    return m_server->isListening();
}

void MCPServer::onNewConnection() {
    QTcpSocket* client = m_server->nextPendingConnection();
    if (!client) return;

    QString clientId = QString("client_%1").arg(reinterpret_cast<quintptr>(client));
    m_clients[client] = clientId;

    connect(client, &QTcpSocket::disconnected, this, &MCPServer::onClientDisconnected);
    connect(client, &QTcpSocket::readyRead, this, &MCPServer::onClientReadyRead);

    qInfo() << "Client connected:" << clientId;
    emit clientConnected(clientId);
}

void MCPServer::onClientDisconnected() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    QString clientId = m_clients.value(client, "unknown");
    m_clients.remove(client);

    qInfo() << "Client disconnected:" << clientId;
    emit clientDisconnected(clientId);

    client->deleteLater();
}

void MCPServer::onClientReadyRead() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    while (client->canReadLine()) {
        QByteArray data = client->readLine().trimmed();
        if (data.isEmpty()) continue;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << parseError.errorString();
            sendError(client, QJsonValue(), -32700, "Parse error");
            continue;
        }

        QJsonObject message = doc.object();
        QString method = message["method"].toString();
        emit messageReceived(getClientId(client), method, message);

        handleMessage(client, message);
    }
}

void MCPServer::handleMessage(QTcpSocket* client, const QJsonObject& message) {
    QString method = message["method"].toString();
    QJsonValue id = message["id"];
    QJsonObject params = message["params"].toObject();

    if (method == "initialize") {
        handleInitialize(client, id, params);
    } else if (method == "tools/list") {
        handleListTools(client, id);
    } else if (method == "tools/call") {
        handleToolCall(client, id, params);
    } else if (method == "notifications/initialized") {
        // Just acknowledge, no response needed
        m_initialized = true;
    } else {
        sendError(client, id, -32601, "Method not found");
    }
}

void MCPServer::handleInitialize(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params) {
    QJsonObject result;
    result["protocolVersion"] = "2024-11-05";

    QJsonObject capabilities;
    capabilities["tools"] = QJsonObject{{"listChanged", false}};
    result["capabilities"] = capabilities;

    QJsonObject serverInfo;
    serverInfo["name"] = "azuredevops-mcp-server";
    serverInfo["version"] = "1.0.0";
    result["serverInfo"] = serverInfo;

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;

    sendResponse(client, response);
}

void MCPServer::handleListTools(QTcpSocket* client, const QJsonValue& id) {
    QJsonArray tools;

    // list_projects tool
    QJsonObject listProjects;
    listProjects["name"] = "list_projects";
    listProjects["description"] = "Get list of all Azure DevOps projects";
    listProjects["inputSchema"] = QJsonObject{
        {"type", "object"},
        {"properties", QJsonObject{}},
        {"required", QJsonArray()}
    };
    tools.append(listProjects);

    // list_teams tool
    QJsonObject listTeams;
    listTeams["name"] = "list_teams";
    listTeams["description"] = "Get list of teams for a specific project";
    listTeams["inputSchema"] = QJsonObject{
        {"type", "object"},
        {"properties", QJsonObject{
            {"project", QJsonObject{{"type", "string"}, {"description", "Project name"}}}
        }},
        {"required", QJsonArray{"project"}}
    };
    tools.append(listTeams);

    // get_team_iterations tool
    QJsonObject getIterations;
    getIterations["name"] = "get_team_iterations";
    getIterations["description"] = "Get list of sprints/iterations for a team";
    getIterations["inputSchema"] = QJsonObject{
        {"type", "object"},
        {"properties", QJsonObject{
            {"project", QJsonObject{{"type", "string"}}},
            {"team", QJsonObject{{"type", "string"}}}
        }},
        {"required", QJsonArray{"project", "team"}}
    };
    tools.append(getIterations);

    QJsonObject result;
    result["tools"] = tools;

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;

    sendResponse(client, response);
}

void MCPServer::handleToolCall(QTcpSocket* client, const QJsonValue& id, const QJsonObject& params) {
    QString toolName = params["name"].toString();
    QJsonObject arguments = params["arguments"].toObject();

    qDebug() << "Tool call:" << toolName << "with args:" << arguments;

    // Extract client-provided credentials from arguments
    QString clientPAT = arguments["pat"].toString();
    QString clientOrg = arguments["organization"].toString();

    // Temporarily set client credentials in DevOps client
    if (!clientPAT.isEmpty()) {
        qDebug() << "📍 Using client-provided PAT token";
        m_devopsClient->setPAT(clientPAT);
    }
    if (!clientOrg.isEmpty()) {
        qDebug() << "📍 Using client-provided organization:" << clientOrg;
        m_devopsClient->setOrganization(clientOrg);
    }

    auto sendToolResponse = [this, client, id](bool success, const QJsonObject& data) {
        QJsonArray content;
        QJsonObject textContent;
        textContent["type"] = "text";
        textContent["text"] = QString::fromUtf8(QJsonDocument(data).toJson());
        content.append(textContent);

        QJsonObject result;
        result["content"] = content;
        result["isError"] = !success;

        QJsonObject response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = result;

        sendResponse(client, response);
    };

    if (toolName == "list_projects") {
        m_devopsClient->listProjects(sendToolResponse);
    } else if (toolName == "list_teams") {
        QString project = arguments["project"].toString();
        m_devopsClient->listTeams(project, sendToolResponse);
    } else if (toolName == "get_team_iterations") {
        QString project = arguments["project"].toString();
        QString team = arguments["team"].toString();
        m_devopsClient->getTeamIterations(project, team, sendToolResponse);
    } else if (toolName == "list_repositories") {
        QString project = arguments["project"].toString();
        m_devopsClient->listRepositories(project, sendToolResponse);
    } else {
        sendError(client, id, -32602, "Unknown tool: " + toolName);
    }
}

void MCPServer::sendResponse(QTcpSocket* client, const QJsonObject& response) {
    QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact) + "\n";
    client->write(data);
    client->flush();

    emit messageSent(getClientId(client), response);
}

void MCPServer::sendError(QTcpSocket* client, const QJsonValue& id, int code, const QString& message) {
    QJsonObject error;
    error["code"] = code;
    error["message"] = message;

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = error;

    sendResponse(client, response);
}

QString MCPServer::getClientId(QTcpSocket* client) const {
    return m_clients.value(client, "unknown");
}
