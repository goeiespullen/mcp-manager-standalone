#include "TrafficMonitor.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

TrafficMonitor::TrafficMonitor(QWidget* parent)
    : QWidget(parent)
    , m_messageCount(0)
{
    setupUI();
}

void TrafficMonitor::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Stats label
    m_statsLabel = new QLabel("Messages: 0");
    m_statsLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    mainLayout->addWidget(m_statsLabel);

    // Log display
    m_logDisplay = new QTextEdit();
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setFont(QFont("Courier", 9));
    m_logDisplay->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "}"
    );
    mainLayout->addWidget(m_logDisplay);

    // Clear button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_clearButton = new QPushButton("Clear Log");
    m_clearButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #d13438;"
        "  color: white;"
        "  padding: 5px 15px;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #a02a2d;"
        "}"
    );
    connect(m_clearButton, &QPushButton::clicked, this, &TrafficMonitor::clear);
    buttonLayout->addWidget(m_clearButton);

    mainLayout->addLayout(buttonLayout);
}

void TrafficMonitor::logMessage(const QString& direction, const QString& clientId,
                                 const QString& message) {
    m_messageCount++;
    m_statsLabel->setText(QString("Messages: %1").arg(m_messageCount));

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    QString color;
    QString arrow;
    if (direction == "IN" || direction == "⬇") {
        color = "#4ec9b0";  // Cyan
        arrow = "⬇ IN";
    } else {
        color = "#ce9178";  // Orange
        arrow = "⬆ OUT";
    }

    QString formatted = formatJson(message);

    QString html = QString(
        "<div style='margin: 10px 0; padding: 10px; background-color: #252526; border-left: 3px solid %1;'>"
        "  <span style='color: %1; font-weight: bold;'>%2</span>"
        "  <span style='color: #858585; margin-left: 10px;'>%3</span>"
        "  <span style='color: #858585; margin-left: 10px;'>%4</span>"
        "  <pre style='margin-top: 5px; color: #d4d4d4; font-size: 8pt;'>%5</pre>"
        "</div>"
    ).arg(color, arrow, timestamp, clientId, formatted.toHtmlEscaped());

    m_logDisplay->append(html);

    // Auto-scroll to bottom
    QTextCursor cursor = m_logDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logDisplay->setTextCursor(cursor);
}

void TrafficMonitor::clear() {
    m_logDisplay->clear();
    m_messageCount = 0;
    m_statsLabel->setText("Messages: 0");
}

QString TrafficMonitor::formatJson(const QString& json) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        return json;
    }

    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}
