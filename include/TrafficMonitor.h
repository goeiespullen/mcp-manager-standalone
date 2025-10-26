#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

/**
 * @brief Widget for monitoring MCP traffic
 *
 * Displays real-time JSON-RPC messages sent and received.
 */
class TrafficMonitor : public QWidget {
    Q_OBJECT

public:
    explicit TrafficMonitor(QWidget* parent = nullptr);

public slots:
    void logMessage(const QString& direction, const QString& clientId, const QString& message);
    void clear();

private:
    void setupUI();
    QString formatJson(const QString& json);

    QTextEdit* m_logDisplay;
    QLabel* m_statsLabel;
    QPushButton* m_clearButton;
    int m_messageCount;
};
