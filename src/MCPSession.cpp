#include "MCPSession.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>

MCPSession::MCPSession(
    const QString& sessionId,
    const QString& serverType,
    const QJsonObject& serverConfig,
    const QJsonObject& credentials,
    QTcpSocket* clientSocket,
    QObject* parent
)
    : QObject(parent)
    , m_sessionId(sessionId)
    , m_serverType(serverType)
    , m_serverConfig(serverConfig)
    , m_credentials(credentials)
    , m_created(QDateTime::currentDateTime())
    , m_lastActivity(QDateTime::currentDateTime())
    , m_process(new QProcess(this))
    , m_clientSocket(clientSocket)
    , m_requestCount(0)
    , m_initialized(false)
    , m_initRequestId(999)
{
    qDebug() << "MCPSession created:" << m_sessionId << "for" << m_serverType;

    // Connect process signals
    connect(m_process, &QProcess::started, this, &MCPSession::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MCPSession::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &MCPSession::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &MCPSession::onProcessReadyRead);

    // Connect client socket signals
    if (m_clientSocket) {
        connect(m_clientSocket, &QTcpSocket::disconnected, this, &MCPSession::onClientDisconnected);
    }
}

MCPSession::~MCPSession() {
    qDebug() << "MCPSession destroyed:" << m_sessionId;
    stopServer();
}

bool MCPSession::isActive() const {
    // Session is active if client is connected and server is running
    return isClientConnected() && isServerRunning();
}

bool MCPSession::isClientConnected() const {
    return m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState;
}

bool MCPSession::startServer() {
    if (m_process->state() != QProcess::NotRunning) {
        qWarning() << "Session" << m_sessionId << "server already running";
        return false;
    }

    // Get command from config
    QString command = m_serverConfig["command"].toString();
    if (command.isEmpty()) {
        m_lastError = "No command specified in server config";
        qWarning() << "Session" << m_sessionId << m_lastError;
        return false;
    }

    // Get arguments
    QStringList args;
    QJsonArray argsArray = m_serverConfig["arguments"].toArray();
    for (const QJsonValue& val : argsArray) {
        args.append(val.toString());
    }

    // Build environment with credentials
    QProcessEnvironment env = buildEnvironment();

    // Set working directory if specified
    QString workingDir = m_serverConfig["workingDir"].toString();
    if (!workingDir.isEmpty()) {
        m_process->setWorkingDirectory(workingDir);
    }

    m_process->setProcessEnvironment(env);

    qDebug() << "Session" << m_sessionId << "starting server:" << command << args;

    // Start process
    m_process->start(command, args);

    return m_process->waitForStarted(5000);
}

void MCPSession::stopServer() {
    if (m_process->state() == QProcess::NotRunning) {
        return;
    }

    qDebug() << "Session" << m_sessionId << "stopping server";

    m_process->terminate();
    if (!m_process->waitForFinished(5000)) {
        qWarning() << "Session" << m_sessionId << "server did not terminate, killing";
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

bool MCPSession::isServerRunning() const {
    return m_process->state() == QProcess::Running;
}

void MCPSession::sendRequest(const QJsonObject& request) {
    if (!isServerRunning()) {
        qWarning() << "Session" << m_sessionId << "cannot send request - server not running";
        return;
    }

    // Queue requests if not yet initialized (except initialize request itself)
    if (!m_initialized && request["id"].toInt() != m_initRequestId) {
        qDebug() << "Session" << m_sessionId << "queuing request until initialized:" << request["method"].toString();
        m_pendingRequests.append(request);
        return;
    }

    updateActivity();
    m_requestCount++;

    // Convert to JSON and send via stdin
    QJsonDocument doc(request);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";

    qDebug() << "Session" << m_sessionId << "sending request:" << data.trimmed();

    m_process->write(data);
    m_process->waitForBytesWritten();
}

void MCPSession::onProcessStarted() {
    qDebug() << "Session" << m_sessionId << "server started with PID" << m_process->processId();

    // Start MCP protocol initialization
    initializeServerProtocol();

    emit serverStarted();
}

void MCPSession::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qDebug() << "Session" << m_sessionId << "server finished. Exit code:" << exitCode;

    if (exitStatus == QProcess::CrashExit) {
        m_lastError = QString("Server crashed with exit code %1").arg(exitCode);
        emit serverError(m_lastError);
    }

    emit serverStopped();
}

void MCPSession::onProcessError(QProcess::ProcessError error) {
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start server process";
            break;
        case QProcess::Crashed:
            errorMsg = "Server process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "Server process timed out";
            break;
        default:
            errorMsg = "Unknown server error";
            break;
    }

    m_lastError = errorMsg;
    qWarning() << "Session" << m_sessionId << "error:" << errorMsg;
    emit serverError(errorMsg);
}

void MCPSession::onProcessReadyRead() {
    // Read all available output
    QByteArray data = m_process->readAllStandardOutput();
    m_outputBuffer += QString::fromUtf8(data);

    // Process complete JSON objects (line-delimited)
    while (m_outputBuffer.contains('\n')) {
        int newlinePos = m_outputBuffer.indexOf('\n');
        QString line = m_outputBuffer.left(newlinePos).trimmed();
        m_outputBuffer.remove(0, newlinePos + 1);

        if (line.isEmpty()) {
            continue;
        }

        qDebug() << "Session" << m_sessionId << "received:" << line;

        // Parse JSON response
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "Session" << m_sessionId << "JSON parse error:" << parseError.errorString();
            continue;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();

            // Check if this is the initialize response
            if (!m_initialized && obj["id"].toInt() == m_initRequestId) {
                qDebug() << "Session" << m_sessionId << "received initialize response";

                // Send 'initialized' notification (REQUIRED by MCP SDK!)
                QJsonObject notif;
                notif["jsonrpc"] = "2.0";
                notif["method"] = "notifications/initialized";
                notif["params"] = QJsonObject();

                QJsonDocument notifDoc(notif);
                QByteArray notifData = notifDoc.toJson(QJsonDocument::Compact) + "\n";

                qDebug() << "Session" << m_sessionId << "sending 'initialized' notification";
                m_process->write(notifData);
                m_process->waitForBytesWritten();

                m_initialized = true;
                qDebug() << "Session" << m_sessionId << "MCP protocol initialized ✅";

                // Send any queued requests now that we're initialized
                if (!m_pendingRequests.isEmpty()) {
                    qDebug() << "Session" << m_sessionId << "sending" << m_pendingRequests.size() << "queued requests";
                    for (const QJsonObject& queuedRequest : m_pendingRequests) {
                        sendRequest(queuedRequest);
                    }
                    m_pendingRequests.clear();
                }

                // Don't forward initialize response to client - they didn't request it
                continue;
            }

            // Log response with timing info
            QString method = obj["method"].toString();
            QJsonValue id = obj["id"];
            bool isError = obj.contains("error");

            if (!method.isEmpty()) {
                // This is a notification
                qDebug() << "Session" << m_sessionId << "received notification:" << method;
            } else if (id.isDouble() || id.isString()) {
                // This is a response to a request
                qDebug() << "Session" << m_sessionId << "received response for id:" << id.toString()
                         << (isError ? "[ERROR]" : "[SUCCESS]");
            }

            emit responseReceived(obj);
        }
    }
}

void MCPSession::onClientDisconnected() {
    qDebug() << "Session" << m_sessionId << "client disconnected";
    emit clientDisconnected();
}

void MCPSession::updateActivity() {
    m_lastActivity = QDateTime::currentDateTime();
}

QProcessEnvironment MCPSession::buildEnvironment() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Add base environment from server config
    QJsonObject baseEnv = m_serverConfig["env"].toObject();
    for (auto it = baseEnv.begin(); it != baseEnv.end(); ++it) {
        env.insert(it.key(), it.value().toString());
    }

    // Override with session-specific credentials
    for (auto it = m_credentials.begin(); it != m_credentials.end(); ++it) {
        QString value = it.value().toString();
        if (!value.isEmpty()) {
            env.insert(it.key(), value);
            qDebug() << "Session" << m_sessionId << "setting credential:" << it.key();
        }
    }

    return env;
}

void MCPSession::initializeServerProtocol() {
    // Send MCP initialize request
    QJsonObject initRequest;
    initRequest["jsonrpc"] = "2.0";
    initRequest["id"] = m_initRequestId;
    initRequest["method"] = "initialize";

    QJsonObject params;
    params["protocolVersion"] = "2024-11-05";
    params["capabilities"] = QJsonObject();

    QJsonObject clientInfo;
    clientInfo["name"] = "mcp-gateway";
    clientInfo["version"] = "1.0.0";
    params["clientInfo"] = clientInfo;

    initRequest["params"] = params;

    qDebug() << "Session" << m_sessionId << "sending MCP initialize request";
    sendRequest(initRequest);
}
