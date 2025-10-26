#include "MCPServerManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

MCPServerManager::MCPServerManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "MCPServerManager initialized";
}

MCPServerManager::~MCPServerManager() {
    stopAll();
    qDeleteAll(m_servers);
}

bool MCPServerManager::loadConfig(const QString& configPath) {
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << configPath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "Config root must be an object";
        return false;
    }

    m_config = doc.object();
    m_configPath = configPath;

    // Clear existing servers
    stopAll();
    qDeleteAll(m_servers);
    m_servers.clear();

    // Load servers from config
    QJsonArray servers = m_config["servers"].toArray();
    for (const QJsonValue& val : servers) {
        if (val.isObject()) {
            QJsonObject serverConfig = val.toObject();
            if (!addServer(serverConfig)) {
                qWarning() << "Failed to add server:" << serverConfig["name"].toString();
            }
        }
    }

    qDebug() << "Loaded" << m_servers.size() << "servers from config";
    emit configChanged();

    return true;
}

bool MCPServerManager::saveConfig(const QString& configPath) {
    // Build servers array from current instances
    QJsonArray serversArray;
    for (MCPServerInstance* server : m_servers) {
        serversArray.append(server->config());
    }

    QJsonObject root = m_config;
    root["servers"] = serversArray;

    QJsonDocument doc(root);
    QFile file(configPath.isEmpty() ? m_configPath : configPath);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << file.fileName();
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Config saved to" << file.fileName();
    return true;
}

QJsonObject MCPServerManager::currentConfig() const {
    return m_config;
}

bool MCPServerManager::addServer(const QJsonObject& serverConfig) {
    QString errorMsg;
    if (!validateServerConfig(serverConfig, errorMsg)) {
        qWarning() << "Invalid server config:" << errorMsg;
        return false;
    }

    QString name = serverConfig["name"].toString();

    // Check for duplicate name
    if (m_servers.contains(name)) {
        qWarning() << "Server with name" << name << "already exists";
        return false;
    }

    // Create new instance
    MCPServerInstance* server = new MCPServerInstance(serverConfig, this);

    // Connect signals
    connect(server, &MCPServerInstance::statusChanged,
            this, &MCPServerManager::onServerStatusChanged);
    connect(server, &MCPServerInstance::outputReceived,
            this, &MCPServerManager::onServerOutput);
    connect(server, &MCPServerInstance::errorOccurred,
            this, &MCPServerManager::onServerError);

    m_servers.insert(name, server);

    qDebug() << "Added server:" << name;
    emit serverAdded(name);

    return true;
}

bool MCPServerManager::removeServer(const QString& name) {
    if (!m_servers.contains(name)) {
        qWarning() << "Server not found:" << name;
        return false;
    }

    MCPServerInstance* server = m_servers.value(name);

    // Stop if running
    if (server->isRunning()) {
        server->stop();
    }

    m_servers.remove(name);
    server->deleteLater();

    qDebug() << "Removed server:" << name;
    emit serverRemoved(name);

    return true;
}

MCPServerInstance* MCPServerManager::getServer(const QString& name) {
    return m_servers.value(name, nullptr);
}

QList<MCPServerInstance*> MCPServerManager::allServers() const {
    return m_servers.values();
}

QStringList MCPServerManager::serverNames() const {
    return m_servers.keys();
}

bool MCPServerManager::startServer(const QString& name) {
    MCPServerInstance* server = getServer(name);
    if (!server) {
        qWarning() << "Server not found:" << name;
        return false;
    }

    return server->start();
}

bool MCPServerManager::stopServer(const QString& name) {
    MCPServerInstance* server = getServer(name);
    if (!server) {
        qWarning() << "Server not found:" << name;
        return false;
    }

    server->stop();
    return true;
}

bool MCPServerManager::restartServer(const QString& name) {
    MCPServerInstance* server = getServer(name);
    if (!server) {
        qWarning() << "Server not found:" << name;
        return false;
    }

    server->restart();
    return true;
}

void MCPServerManager::startAll() {
    qDebug() << "Starting all servers...";
    for (MCPServerInstance* server : m_servers) {
        if (!server->isRunning()) {
            server->start();
        }
    }
}

void MCPServerManager::stopAll() {
    qDebug() << "Stopping all servers...";
    for (MCPServerInstance* server : m_servers) {
        if (server->isRunning()) {
            server->stop();
        }
    }
}

void MCPServerManager::startAutoStartServers() {
    qDebug() << "Starting auto-start servers...";
    for (MCPServerInstance* server : m_servers) {
        if (server->autoStart() && !server->isRunning()) {
            qDebug() << "Auto-starting:" << server->name();
            server->start();
        }
    }
}

int MCPServerManager::runningCount() const {
    int count = 0;
    for (MCPServerInstance* server : m_servers) {
        if (server->isRunning()) {
            count++;
        }
    }
    return count;
}

int MCPServerManager::stoppedCount() const {
    int count = 0;
    for (MCPServerInstance* server : m_servers) {
        if (server->status() == MCPServerInstance::Stopped) {
            count++;
        }
    }
    return count;
}

QMap<QString, MCPServerInstance::ServerStatus> MCPServerManager::allStatuses() const {
    QMap<QString, MCPServerInstance::ServerStatus> statuses;
    for (auto it = m_servers.begin(); it != m_servers.end(); ++it) {
        statuses.insert(it.key(), it.value()->status());
    }
    return statuses;
}

void MCPServerManager::onServerStatusChanged(MCPServerInstance::ServerStatus oldStatus,
                                             MCPServerInstance::ServerStatus newStatus) {
    MCPServerInstance* server = qobject_cast<MCPServerInstance*>(sender());
    if (!server) return;

    QString name = findServerNameByInstance(server);
    if (!name.isEmpty()) {
        qDebug() << "Server" << name << "status changed:"
                 << server->statusString();
        emit serverStatusChanged(name, oldStatus, newStatus);
    }
}

void MCPServerManager::onServerOutput(const QString& line) {
    MCPServerInstance* server = qobject_cast<MCPServerInstance*>(sender());
    if (!server) return;

    QString name = findServerNameByInstance(server);
    if (!name.isEmpty()) {
        emit serverOutputReceived(name, line);
    }
}

void MCPServerManager::onServerError(const QString& error) {
    MCPServerInstance* server = qobject_cast<MCPServerInstance*>(sender());
    if (!server) return;

    QString name = findServerNameByInstance(server);
    if (!name.isEmpty()) {
        qWarning() << "Server" << name << "error:" << error;
        emit serverErrorOccurred(name, error);
    }
}

QString MCPServerManager::findServerNameByInstance(MCPServerInstance* instance) const {
    for (auto it = m_servers.begin(); it != m_servers.end(); ++it) {
        if (it.value() == instance) {
            return it.key();
        }
    }
    return QString();
}

bool MCPServerManager::validateServerConfig(const QJsonObject& config, QString& errorMsg) {
    // Check required fields
    if (!config.contains("name") || config["name"].toString().isEmpty()) {
        errorMsg = "Server name is required";
        return false;
    }

    if (!config.contains("command") || config["command"].toString().isEmpty()) {
        errorMsg = "Server command is required";
        return false;
    }

    if (!config.contains("port")) {
        errorMsg = "Server port is required";
        return false;
    }

    int port = config["port"].toInt();
    if (port < 1024 || port > 65535) {
        errorMsg = "Port must be between 1024 and 65535";
        return false;
    }

    return true;
}
