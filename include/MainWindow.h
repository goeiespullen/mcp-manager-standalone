#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTableWidget>
#include <QComboBox>
#include <QListWidget>

class MCPServerManager;
class MCPGateway;
class TrafficMonitor;
class MCPServerInstance;

/**
 * @brief Main window for MCP Server Manager
 *
 * Provides GUI for managing multiple MCP servers:
 * - Start/stop individual servers
 * - Monitor traffic across all servers
 * - Configure server settings
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(MCPServerManager* manager, QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onStartAllClicked();
    void onStopAllClicked();
    void onServerSelected();
    void onStartServerClicked();
    void onStopServerClicked();
    void onRestartServerClicked();
    void onAddServerClicked();
    void onRemoveServerClicked();
    void onEditServerClicked();
    void onReloadConfigClicked();
    void onSaveConfigClicked();
    void updateStatusBar();
    void updateServerTable();
    void onServerStatusChanged(const QString& name, int oldStatus, int newStatus);
    void onServerOutput(const QString& name, const QString& line);
    void onServerError(const QString& name, const QString& error);
    void onShowGatewayHelp();
    void onShowTestingDocs();
    void onShowQuickReference();
    void onShowUserManual();
    void onOpenDocsFolder();
    void updateGatewayStatus();
    void onManageToolsClicked();
    void updateToolsServerList();

private:
    void setupUI();
    void createMenuBar();
    QWidget* createServersTab();
    QWidget* createGatewayTab();
    QWidget* createLogsTab();
    QWidget* createToolsBrowserTab();
    void loadSettings();
    void saveSettings();
    void populateServerTable();
    QString statusIcon(int status) const;
    QString statusColor(int status) const;
    QString getSelectedServerName() const;
    void showMarkdownDialog(const QString& title, const QString& filePath, const QString& description);

    // Helper methods for MCP server installation
    QString extractZipFile(const QString& zipPath, const QString& destDir, QTextEdit* log);
    QString detectServerType(const QString& dirPath, QTextEdit* log);
    QString installDependencies(const QString& dirPath, const QString& serverType, QTextEdit* log);
    QString findEntryPoint(const QString& dirPath, const QString& serverType);

    MCPServerManager* m_manager;
    MCPGateway* m_gateway;
    TrafficMonitor* m_trafficMonitor;

    QTabWidget* m_tabWidget;
    QLabel* m_statusLabel;

    // Server list
    QTableWidget* m_serverTable;
    QPushButton* m_startAllButton;
    QPushButton* m_stopAllButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_restartButton;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_editButton;
    QPushButton* m_toolsButton;

    // Log output
    QTextEdit* m_logDisplay;
    QComboBox* m_serverFilter;

    // Gateway status
    QLabel* m_gatewaySessionLabel;

    // Tools Browser
    QListWidget* m_toolsServerList;
    QTableWidget* m_toolsTable;
    QTextEdit* m_toolDetailsDisplay;
    QPushButton* m_refreshToolsButton;
};
