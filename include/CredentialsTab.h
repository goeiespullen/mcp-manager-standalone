#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMap>
#include <QStackedWidget>
#include "Keystore.h"

/**
 * @brief Credentials Management Tab
 *
 * Provides a GUI for managing encrypted user credentials per service.
 * Supports Azure DevOps, TeamCentraal, Confluence, and ChatNS.
 *
 * Features:
 * - User selection (email-based)
 * - Service-specific credential forms
 * - Encrypted storage using C++ AES-256 keystore
 * - View/Edit/Delete credentials
 * - Real-time credential overview table
 */
class CredentialsTab : public QWidget {
    Q_OBJECT

public:
    explicit CredentialsTab(QWidget* parent = nullptr);

private slots:
    void onServiceChanged(int index);
    void onUserChanged();
    void onSaveCredentials();
    void onDeleteCredentials();
    void onRefreshTable();
    void onTableRowSelected();

private:
    // UI setup
    void setupUI();
    void createServiceForms();
    void updateFormVisibility();
    void loadUserCredentials(const QString& userId);

    // Keystore operations (C++ native)
    bool saveToKeystore(const QString& userId, const QString& service, const QMap<QString, QString>& credentials);
    bool deleteFromKeystore(const QString& userId, const QString& service);
    QMap<QString, QString> loadFromKeystore(const QString& userId, const QString& service);
    QStringList listKeystoreUsers();
    QStringList listUserServices(const QString& userId);

    // Service form creation
    QWidget* createAzureForm();
    QWidget* createTeamCentraalForm();
    QWidget* createConfluenceForm();
    QWidget* createChatNSForm();

    // UI Components
    QLineEdit* m_userInput;
    QComboBox* m_serviceSelector;
    QStackedWidget* m_formStack;
    QPushButton* m_saveButton;
    QPushButton* m_deleteButton;
    QPushButton* m_refreshButton;
    QTableWidget* m_credentialsTable;
    QLabel* m_statusLabel;

    // Azure DevOps form fields
    QLineEdit* m_azureTokenInput;

    // TeamCentraal form fields
    QLineEdit* m_teamcUsername;
    QLineEdit* m_teamcPassword;
    QLineEdit* m_teamcUrl;

    // Confluence form fields
    QLineEdit* m_confEmail;
    QLineEdit* m_confToken;
    QLineEdit* m_confUrl;

    // ChatNS form fields
    QLineEdit* m_chatnsApiKey;

    // Service names mapping
    QMap<QString, QString> m_serviceDisplayNames;
    QMap<QString, QString> m_serviceKeystoreNames;

    // Keystore instance
    Keystore* m_keystore;
};
