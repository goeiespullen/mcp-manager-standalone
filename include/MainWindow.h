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
#include "UpdateChecker.h"

class MCPServerManager;
class MCPGateway;
class TrafficMonitor;
class MCPServerInstance;
class QNetworkAccessManager;

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
    void onShowAzureDevOpsApiGuide();
    void onShowTeamCentraalApiGuide();
    void onOpenDocsFolder();
    void onOpenLogsFolder();
    void updateGatewayStatus();
    void onManageToolsClicked();
    void updateToolsServerList();
    void refreshToolsBrowserTable();
    void onCheckForUpdates();
    void onUpdateAvailable(const UpdateChecker::ReleaseInfo& info);
    void onNoUpdateAvailable();
    void onUpdateCheckFailed(const QString& error);
    void onPermissionsChanged();
    void onGlobalPermissionChanged(int category, bool enabled);
    void onServerPermissionChanged(const QString& serverName, int category, bool enabled);
    void onResetServerPermissions(const QString& serverName, int row);
    void onDiscardAllChanges();

private:
    void setupUI();
    void createMenuBar();
    QWidget* createServersTab();
    QWidget* createGatewayTab();
    QWidget* createLogsTab();
    QWidget* createToolsBrowserTab();
    QWidget* createApiTesterTab();
    QWidget* createPermissionsTab();
    QWidget* createToolsAndPermissionsTab();
    void loadSettings();
    void saveSettings();
    void populateServerTable();
    QString statusIcon(int status) const;
    QString statusColor(int status) const;
    void updateChangeLog();
    void clearChangeLog();
    void addChangeLogEntry(const QString& entry);
    QString getSelectedServerName() const;
    void showMarkdownDialog(const QString& title, const QString& filePath, const QString& description);
    void executeApiCall();

    // Helper methods for MCP server installation
    QString extractZipFile(const QString& zipPath, const QString& destDir, QTextEdit* log);
    QString detectServerType(const QString& dirPath, QTextEdit* log);
    QString installDependencies(const QString& dirPath, const QString& serverType, QTextEdit* log);
    QString findEntryPoint(const QString& dirPath, const QString& serverType);
    QString downloadFile(const QString& url, const QString& destPath);

    MCPServerManager* m_manager;
    MCPGateway* m_gateway;
    TrafficMonitor* m_trafficMonitor;
    UpdateChecker* m_updateChecker;

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
    QTableWidget* m_gatewaySessionsTable;

    // Tools Browser
    QListWidget* m_toolsServerList;
    QTableWidget* m_toolsTable;
    QTextEdit* m_toolDetailsDisplay;
    QPushButton* m_refreshToolsButton;

    // API Tester
    QComboBox* m_apiTypeCombo;
    QLineEdit* m_apiOrgInput;
    QLineEdit* m_apiProjectInput;
    QLineEdit* m_apiUsernameInput;
    QLineEdit* m_apiPasswordInput;
    QComboBox* m_apiMethodCombo;
    QComboBox* m_apiTemplateCombo;
    QLineEdit* m_apiEndpointInput;
    QLineEdit* m_apiPatInput;
    QTextEdit* m_apiRequestBody;
    QTextEdit* m_apiResponseDisplay;
    QPushButton* m_apiExecuteButton;
    QNetworkAccessManager* m_apiNetworkManager;
    QLabel* m_apiInfoLabel;

    // Permissions
    QTableWidget* m_permissionsTable;
    QMap<int, class QCheckBox*> m_globalPermissionCheckboxes;  // category -> checkbox
    QPushButton* m_discardButton;
    QStringList m_changeLogEntries;  // Unsaved changes (cleared on save)
    QStringList m_changeHistoryEntries;  // Permanent history with timestamps
};
