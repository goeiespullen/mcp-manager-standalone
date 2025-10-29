#pragma once

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QString>
#include <QDateTime>

/**
 * @brief Centralized logging system for MCP Server Manager
 *
 * Logs all traffic, server behaviour, and system events to files.
 * Features:
 * - Multiple log files (general, traffic, errors)
 * - Thread-safe logging
 * - Automatic timestamping
 * - Log rotation
 */
class Logger : public QObject {
    Q_OBJECT

public:
    enum LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    enum LogCategory {
        General,      // General application logs
        Traffic,      // MCP protocol traffic
        Server,       // Server lifecycle events
        Gateway       // Gateway operations
    };

    static Logger* instance();

    void log(LogLevel level, LogCategory category, const QString& message);
    void logTraffic(const QString& direction, const QString& clientId, const QString& message);

    // Qt message handler integration
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    void setLogDirectory(const QString& dir);
    QString logDirectory() const { return m_logDir; }

    void enableCategory(LogCategory category, bool enabled);
    bool isCategoryEnabled(LogCategory category) const;

    void setMaxFileSize(qint64 bytes) { m_maxFileSize = bytes; }
    qint64 maxFileSize() const { return m_maxFileSize; }

signals:
    void logMessageReceived(const QString& timestamp, const QString& level, const QString& message);

private:
    Logger();
    ~Logger();

    void ensureLogDirectory();
    void rotateLogIfNeeded(QFile& file);
    void writeToFile(QFile& file, const QString& message);
    QString getLogFileName(LogCategory category) const;
    QString levelToString(LogLevel level) const;
    QString categoryToString(LogCategory category) const;

    static Logger* s_instance;

    QString m_logDir;
    QFile m_generalLog;
    QFile m_trafficLog;
    QFile m_serverLog;
    QFile m_gatewayLog;
    QFile m_errorLog;

    QTextStream m_generalStream;
    QTextStream m_trafficStream;
    QTextStream m_serverStream;
    QTextStream m_gatewayStream;
    QTextStream m_errorStream;

    QMutex m_mutex;

    qint64 m_maxFileSize;  // Max size before rotation (default 10MB)
    int m_maxBackups;      // Number of backup files to keep

    QMap<LogCategory, bool> m_enabledCategories;
};

// Helper macros for easier logging
#define LOG_DEBUG(category, msg) Logger::instance()->log(Logger::Debug, category, msg)
#define LOG_INFO(category, msg) Logger::instance()->log(Logger::Info, category, msg)
#define LOG_WARNING(category, msg) Logger::instance()->log(Logger::Warning, category, msg)
#define LOG_ERROR(category, msg) Logger::instance()->log(Logger::Error, category, msg)
#define LOG_CRITICAL(category, msg) Logger::instance()->log(Logger::Critical, category, msg)
#define LOG_TRAFFIC(direction, clientId, msg) Logger::instance()->logTraffic(direction, clientId, msg)
