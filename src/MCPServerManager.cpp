#include "MCPServerManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

MCPServerManager::MCPServerManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "MCPServerManager initialized";
}

MCPServerManager::~MCPServerManager() {
    stopAll();
    // Qt will automatically delete child objects (MCPServerInstance has 'this' as parent)
    // No need for qDeleteAll - that would cause double-delete!
    m_servers.clear();
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

    // Load global permission defaults
    m_globalPermissions.clear();
    m_globalPermissions[MCPServerInstance::READ_REMOTE] = true;  // Default safe values
    m_globalPermissions[MCPServerInstance::WRITE_REMOTE] = false;
    m_globalPermissions[MCPServerInstance::WRITE_LOCAL] = false;
    m_globalPermissions[MCPServerInstance::EXECUTE_AI] = false;
    m_globalPermissions[MCPServerInstance::EXECUTE_CODE] = false;

    if (m_config.contains("permissions")) {
        QJsonObject perms = m_config["permissions"].toObject();
        if (perms.contains("global_defaults")) {
            QJsonObject globalDefaults = perms["global_defaults"].toObject();
            if (globalDefaults.contains("READ_REMOTE"))
                m_globalPermissions[MCPServerInstance::READ_REMOTE] = globalDefaults["READ_REMOTE"].toBool();
            if (globalDefaults.contains("WRITE_REMOTE"))
                m_globalPermissions[MCPServerInstance::WRITE_REMOTE] = globalDefaults["WRITE_REMOTE"].toBool();
            if (globalDefaults.contains("WRITE_LOCAL"))
                m_globalPermissions[MCPServerInstance::WRITE_LOCAL] = globalDefaults["WRITE_LOCAL"].toBool();
            if (globalDefaults.contains("EXECUTE_AI"))
                m_globalPermissions[MCPServerInstance::EXECUTE_AI] = globalDefaults["EXECUTE_AI"].toBool();
            if (globalDefaults.contains("EXECUTE_CODE"))
                m_globalPermissions[MCPServerInstance::EXECUTE_CODE] = globalDefaults["EXECUTE_CODE"].toBool();
        }
    }

    qDebug() << "Global permissions loaded:"
             << "READ_REMOTE=" << m_globalPermissions[MCPServerInstance::READ_REMOTE]
             << "WRITE_REMOTE=" << m_globalPermissions[MCPServerInstance::WRITE_REMOTE]
             << "WRITE_LOCAL=" << m_globalPermissions[MCPServerInstance::WRITE_LOCAL]
             << "EXECUTE_AI=" << m_globalPermissions[MCPServerInstance::EXECUTE_AI]
             << "EXECUTE_CODE=" << m_globalPermissions[MCPServerInstance::EXECUTE_CODE];

    // Clear existing servers
    stopAll();
    // Use deleteLater() to safely schedule deletion (avoids double-delete)
    for (MCPServerInstance* server : m_servers) {
        server->deleteLater();
    }
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
        QJsonObject serverConfig = server->config();

        // Update permissions in config - only save explicit overrides
        QMap<MCPServerInstance::PermissionCategory, bool> explicitPerms = server->explicitPermissions();
        if (!explicitPerms.isEmpty()) {
            QJsonObject permsObj;
            for (auto it = explicitPerms.begin(); it != explicitPerms.end(); ++it) {
                QString permName;
                switch (it.key()) {
                    case MCPServerInstance::READ_REMOTE: permName = "READ_REMOTE"; break;
                    case MCPServerInstance::WRITE_REMOTE: permName = "WRITE_REMOTE"; break;
                    case MCPServerInstance::WRITE_LOCAL: permName = "WRITE_LOCAL"; break;
                    case MCPServerInstance::EXECUTE_AI: permName = "EXECUTE_AI"; break;
                    case MCPServerInstance::EXECUTE_CODE: permName = "EXECUTE_CODE"; break;
                }
                permsObj[permName] = it.value();
            }
            serverConfig["permissions"] = permsObj;
        } else {
            // No explicit overrides, remove permissions key
            serverConfig.remove("permissions");
        }

        serversArray.append(serverConfig);
    }

    // Build root config with global permissions
    QJsonObject root = m_config;

    // Update global permissions
    QJsonObject globalDefaultsObj;
    globalDefaultsObj["READ_REMOTE"] = m_globalPermissions.value(MCPServerInstance::READ_REMOTE, true);
    globalDefaultsObj["WRITE_REMOTE"] = m_globalPermissions.value(MCPServerInstance::WRITE_REMOTE, false);
    globalDefaultsObj["WRITE_LOCAL"] = m_globalPermissions.value(MCPServerInstance::WRITE_LOCAL, false);
    globalDefaultsObj["EXECUTE_AI"] = m_globalPermissions.value(MCPServerInstance::EXECUTE_AI, false);
    globalDefaultsObj["EXECUTE_CODE"] = m_globalPermissions.value(MCPServerInstance::EXECUTE_CODE, false);

    QJsonObject permissionsObj;
    permissionsObj["global_defaults"] = globalDefaultsObj;
    root["permissions"] = permissionsObj;

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

QString MCPServerManager::configPath() const {
    return m_configPath;
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

    // Set manager pointer for global permission fallback
    server->setManager(this);

    // Connect signals
    connect(server, &MCPServerInstance::statusChanged,
            this, &MCPServerManager::onServerStatusChanged);
    connect(server, &MCPServerInstance::outputReceived,
            this, &MCPServerManager::onServerOutput);
    connect(server, &MCPServerInstance::errorOccurred,
            this, &MCPServerManager::onServerError);
    connect(server, &MCPServerInstance::permissionsChanged,
            this, &MCPServerManager::onServerPermissionsChanged);

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

bool MCPServerManager::getGlobalPermission(MCPServerInstance::PermissionCategory category) const {
    return m_globalPermissions.value(category, false);
}

void MCPServerManager::setGlobalPermission(MCPServerInstance::PermissionCategory category, bool enabled) {
    m_globalPermissions[category] = enabled;
    qDebug() << "Global permission" << category << "set to" << enabled;
    emit globalPermissionsChanged();
}

void MCPServerManager::onServerPermissionsChanged() {
    MCPServerInstance* server = qobject_cast<MCPServerInstance*>(sender());
    if (!server) return;

    QString name = findServerNameByInstance(server);
    if (!name.isEmpty()) {
        qDebug() << "Server" << name << "permissions changed, emitting signal";
        emit serverPermissionsChanged(name);
    }
}

// ========== CLIENT REGISTRATION METHODS ==========

void MCPServerManager::registerClient(const QString& userId, const QString& clientApp) {
    if (userId.isEmpty() || clientApp.isEmpty()) {
        qWarning() << "Cannot register client: empty userId or clientApp";
        return;
    }

    // Check if already registered
    QJsonArray clients = m_config["registered_clients"].toArray();
    QString currentTime = QDateTime::currentDateTime().toString(Qt::ISODate);

    bool found = false;
    for (int i = 0; i < clients.size(); i++) {
        QJsonObject client = clients[i].toObject();
        if (client["userId"].toString() == userId &&
            client["clientApp"].toString() == clientApp) {
            // Update lastSeen
            client["lastSeen"] = currentTime;
            clients[i] = client;
            found = true;
            break;
        }
    }

    if (!found) {
        // Add new registration
        QJsonObject newClient;
        newClient["userId"] = userId;
        newClient["clientApp"] = clientApp;
        newClient["firstSeen"] = currentTime;
        newClient["lastSeen"] = currentTime;
        clients.append(newClient);

        qDebug() << "Registered new client:" << userId << clientApp;
    }

    m_config["registered_clients"] = clients;
    saveConfig(m_configPath);
}

QList<QPair<QString, QString>> MCPServerManager::getRegisteredClients() const {
    QList<QPair<QString, QString>> result;
    QJsonArray clients = m_config["registered_clients"].toArray();

    for (const QJsonValue& val : clients) {
        QJsonObject client = val.toObject();
        QString userId = client["userId"].toString();
        QString clientApp = client["clientApp"].toString();
        result.append(qMakePair(userId, clientApp));
    }

    return result;
}

void MCPServerManager::setClientPermission(const QString& userId, const QString& clientApp,
                                          MCPServerInstance::PermissionCategory category, bool allowed) {
    if (userId.isEmpty() || clientApp.isEmpty()) {
        qWarning() << "Cannot set client permission: empty userId or clientApp";
        return;
    }

    qDebug() << "setClientPermission: userId=" << userId << "clientApp=" << clientApp
             << "category=" << category << "allowed=" << allowed;

    QString key = makeClientKey(userId, clientApp);
    QJsonObject clientPerms = m_config["client_permissions"].toObject();
    QJsonObject perms = clientPerms[key].toObject();

    QString permName;
    switch (category) {
        case MCPServerInstance::READ_REMOTE: permName = "READ_REMOTE"; break;
        case MCPServerInstance::WRITE_REMOTE: permName = "WRITE_REMOTE"; break;
        case MCPServerInstance::WRITE_LOCAL: permName = "WRITE_LOCAL"; break;
        case MCPServerInstance::EXECUTE_AI: permName = "EXECUTE_AI"; break;
        case MCPServerInstance::EXECUTE_CODE: permName = "EXECUTE_CODE"; break;
        default:
            qWarning() << "Invalid permission category:" << category;
            return;
    }

    qDebug() << "Setting permission" << permName << "to" << allowed << "for key" << key;

    perms[permName] = allowed;
    clientPerms[key] = perms;
    m_config["client_permissions"] = clientPerms;

    qDebug() << "About to save config...";
    bool saved = saveConfig(m_configPath);
    qDebug() << "Config save" << (saved ? "succeeded" : "FAILED");

    qDebug() << "Set client permission:" << key << permName << "=" << allowed;
}

bool MCPServerManager::getClientPermission(const QString& userId, const QString& clientApp,
                                          MCPServerInstance::PermissionCategory category, bool* hasExplicit) const {
    QString key = makeClientKey(userId, clientApp);
    QJsonObject clientPerms = m_config["client_permissions"].toObject();

    if (!clientPerms.contains(key)) {
        // No explicit permission set
        if (hasExplicit) *hasExplicit = false;
        return getGlobalPermission(category);  // Return global default
    }

    QJsonObject perms = clientPerms[key].toObject();
    QString permName;
    switch (category) {
        case MCPServerInstance::READ_REMOTE: permName = "READ_REMOTE"; break;
        case MCPServerInstance::WRITE_REMOTE: permName = "WRITE_REMOTE"; break;
        case MCPServerInstance::WRITE_LOCAL: permName = "WRITE_LOCAL"; break;
        case MCPServerInstance::EXECUTE_AI: permName = "EXECUTE_AI"; break;
        case MCPServerInstance::EXECUTE_CODE: permName = "EXECUTE_CODE"; break;
    }

    if (!perms.contains(permName)) {
        // Client exists but this specific permission not set
        if (hasExplicit) *hasExplicit = false;
        return getGlobalPermission(category);  // Return global default
    }

    // Explicit permission found
    if (hasExplicit) *hasExplicit = true;
    return perms[permName].toBool();
}

QString MCPServerManager::makeClientKey(const QString& userId, const QString& clientApp) const {
    return QString("%1|%2").arg(userId, clientApp);
}
