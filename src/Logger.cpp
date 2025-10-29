#include "Logger.h"
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>

Logger* Logger::s_instance = nullptr;

Logger::Logger()
    : m_maxFileSize(10 * 1024 * 1024)  // 10 MB default
    , m_maxBackups(5)
{
    // Default log directory: ~/.local/share/mcp-manager/logs
    QString defaultLogDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    setLogDirectory(defaultLogDir);

    // Enable all categories by default
    m_enabledCategories[General] = true;
    m_enabledCategories[Traffic] = true;
    m_enabledCategories[Server] = true;
    m_enabledCategories[Gateway] = true;
}

Logger::~Logger() {
    QMutexLocker locker(&m_mutex);

    if (m_generalLog.isOpen()) {
        m_generalStream.flush();
        m_generalLog.close();
    }
    if (m_trafficLog.isOpen()) {
        m_trafficStream.flush();
        m_trafficLog.close();
    }
    if (m_serverLog.isOpen()) {
        m_serverStream.flush();
        m_serverLog.close();
    }
    if (m_gatewayLog.isOpen()) {
        m_gatewayStream.flush();
        m_gatewayLog.close();
    }
    if (m_errorLog.isOpen()) {
        m_errorStream.flush();
        m_errorLog.close();
    }
}

Logger* Logger::instance() {
    if (!s_instance) {
        s_instance = new Logger();
    }
    return s_instance;
}

void Logger::setLogDirectory(const QString& dir) {
    QMutexLocker locker(&m_mutex);

    m_logDir = dir;
    ensureLogDirectory();

    // Close existing files
    if (m_generalLog.isOpen()) m_generalLog.close();
    if (m_trafficLog.isOpen()) m_trafficLog.close();
    if (m_serverLog.isOpen()) m_serverLog.close();
    if (m_gatewayLog.isOpen()) m_gatewayLog.close();
    if (m_errorLog.isOpen()) m_errorLog.close();

    // Open new files
    m_generalLog.setFileName(m_logDir + "/general.log");
    m_trafficLog.setFileName(m_logDir + "/traffic.log");
    m_serverLog.setFileName(m_logDir + "/server.log");
    m_gatewayLog.setFileName(m_logDir + "/gateway.log");
    m_errorLog.setFileName(m_logDir + "/errors.log");

    m_generalLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_trafficLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_serverLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_gatewayLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_errorLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    m_generalStream.setDevice(&m_generalLog);
    m_trafficStream.setDevice(&m_trafficLog);
    m_serverStream.setDevice(&m_serverLog);
    m_gatewayStream.setDevice(&m_gatewayLog);
    m_errorStream.setDevice(&m_errorLog);

    // Log initialization
    QString initMsg = QString("\n========== MCP Manager Started - %1 ==========\n")
                        .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    m_generalStream << initMsg;
    m_generalStream.flush();
}

void Logger::ensureLogDirectory() {
    QDir dir;
    if (!dir.exists(m_logDir)) {
        dir.mkpath(m_logDir);
    }
}

void Logger::log(LogLevel level, LogCategory category, const QString& message) {
    if (!isCategoryEnabled(category)) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = levelToString(level);
    QString categoryStr = categoryToString(category);
    QString fullMessage = QString("[%1] [%2] [%3] %4\n")
                            .arg(timestamp)
                            .arg(levelStr)
                            .arg(categoryStr)
                            .arg(message);

    // Write to category-specific log
    switch (category) {
    case General:
        rotateLogIfNeeded(m_generalLog);
        m_generalStream << fullMessage;
        m_generalStream.flush();
        break;
    case Traffic:
        rotateLogIfNeeded(m_trafficLog);
        m_trafficStream << fullMessage;
        m_trafficStream.flush();
        break;
    case Server:
        rotateLogIfNeeded(m_serverLog);
        m_serverStream << fullMessage;
        m_serverStream.flush();
        break;
    case Gateway:
        rotateLogIfNeeded(m_gatewayLog);
        m_gatewayStream << fullMessage;
        m_gatewayStream.flush();
        break;
    }

    // Also write errors/warnings to error log
    if (level >= Warning) {
        rotateLogIfNeeded(m_errorLog);
        m_errorStream << fullMessage;
        m_errorStream.flush();
    }

    // Emit signal for UI updates
    emit logMessageReceived(timestamp, levelStr, message);
}

void Logger::logTraffic(const QString& direction, const QString& clientId, const QString& message) {
    if (!isCategoryEnabled(Traffic)) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString fullMessage = QString("[%1] [%2] [%3] %4\n")
                            .arg(timestamp)
                            .arg(direction)
                            .arg(clientId)
                            .arg(message);

    rotateLogIfNeeded(m_trafficLog);
    m_trafficStream << fullMessage;
    m_trafficStream.flush();
}

void Logger::rotateLogIfNeeded(QFile& file) {
    if (!file.isOpen()) {
        return;
    }

    if (file.size() >= m_maxFileSize) {
        QString basePath = file.fileName();

        // Close current file
        file.close();

        // Rotate existing backups
        for (int i = m_maxBackups - 1; i >= 1; --i) {
            QString oldBackup = QString("%1.%2").arg(basePath).arg(i);
            QString newBackup = QString("%1.%2").arg(basePath).arg(i + 1);

            if (QFile::exists(oldBackup)) {
                QFile::remove(newBackup);
                QFile::rename(oldBackup, newBackup);
            }
        }

        // Rename current to .1
        QString firstBackup = QString("%1.1").arg(basePath);
        QFile::remove(firstBackup);
        QFile::rename(basePath, firstBackup);

        // Reopen file
        file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    LogLevel level;
    LogCategory category = General;

    switch (type) {
    case QtDebugMsg:
        level = Debug;
        break;
    case QtInfoMsg:
        level = Info;
        break;
    case QtWarningMsg:
        level = Warning;
        break;
    case QtCriticalMsg:
        level = Critical;
        break;
    case QtFatalMsg:
        level = Critical;
        break;
    default:
        level = Info;
    }

    // Categorize based on message content
    if (msg.contains("Gateway") || msg.contains("gateway")) {
        category = Gateway;
    } else if (msg.contains("Server") || msg.contains("server") || msg.contains("MCP")) {
        category = Server;
    } else if (msg.contains("Traffic") || msg.contains("traffic") || msg.contains("JSON")) {
        category = Traffic;
    }

    // Log with context info
    QString contextInfo;
    if (context.file) {
        contextInfo = QString(" (%1:%2)").arg(context.file).arg(context.line);
    }

    Logger::instance()->log(level, category, msg + contextInfo);

    // Also output to console for debugging (optional)
    if (type == QtFatalMsg) {
        abort();
    }
}

void Logger::enableCategory(LogCategory category, bool enabled) {
    QMutexLocker locker(&m_mutex);
    m_enabledCategories[category] = enabled;
}

bool Logger::isCategoryEnabled(LogCategory category) const {
    return m_enabledCategories.value(category, true);
}

QString Logger::levelToString(LogLevel level) const {
    switch (level) {
    case Debug:    return "DEBUG";
    case Info:     return "INFO ";
    case Warning:  return "WARN ";
    case Error:    return "ERROR";
    case Critical: return "CRIT ";
    default:       return "UNKN ";
    }
}

QString Logger::categoryToString(LogCategory category) const {
    switch (category) {
    case General: return "GENERAL";
    case Traffic: return "TRAFFIC";
    case Server:  return "SERVER ";
    case Gateway: return "GATEWAY";
    default:      return "UNKNOWN";
    }
}

QString Logger::getLogFileName(LogCategory category) const {
    switch (category) {
    case General: return "general.log";
    case Traffic: return "traffic.log";
    case Server:  return "server.log";
    case Gateway: return "gateway.log";
    default:      return "unknown.log";
    }
}
