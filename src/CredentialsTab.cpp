#include "CredentialsTab.h"
#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QGroupBox>
#include <QPalette>

CredentialsTab::CredentialsTab(QWidget* parent)
    : QWidget(parent)
{
    // Initialize keystore
    m_keystore = new Keystore(this);

    // Initialize service name mappings
    m_serviceDisplayNames["azure"] = "🔷 Azure DevOps";
    m_serviceDisplayNames["teamcentraal"] = "👥 TeamCentraal";
    m_serviceDisplayNames["confluence"] = "📄 Confluence";
    m_serviceDisplayNames["chatns"] = "💬 ChatNS";

    m_serviceKeystoreNames["🔷 Azure DevOps"] = "azure";
    m_serviceKeystoreNames["👥 TeamCentraal"] = "teamcentraal";
    m_serviceKeystoreNames["📄 Confluence"] = "confluence";
    m_serviceKeystoreNames["💬 ChatNS"] = "chatns";

    setupUI();
}

void CredentialsTab::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Title and description
    QLabel* titleLabel = new QLabel("🔐 Credential Management");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    QLabel* descLabel = new QLabel("Manage encrypted credentials for MCP servers. Credentials are stored securely in the keystore.");
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #666; margin-bottom: 10px;");
    mainLayout->addWidget(descLabel);

    // Top section: User and Service selection
    QGroupBox* selectionGroup = new QGroupBox("User & Service Selection");
    QHBoxLayout* selectionLayout = new QHBoxLayout(selectionGroup);

    QLabel* userLabel = new QLabel("User Email:");
    m_userInput = new QLineEdit();
    m_userInput->setPlaceholderText("user@ns.nl");
    m_userInput->setMinimumWidth(200);
    connect(m_userInput, &QLineEdit::textChanged, this, &CredentialsTab::onUserChanged);

    QLabel* serviceLabel = new QLabel("Service:");
    m_serviceSelector = new QComboBox();
    m_serviceSelector->addItem("🔷 Azure DevOps");
    m_serviceSelector->addItem("👥 TeamCentraal");
    m_serviceSelector->addItem("📄 Confluence");
    m_serviceSelector->addItem("💬 ChatNS");
    m_serviceSelector->setMinimumWidth(200);
    connect(m_serviceSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CredentialsTab::onServiceChanged);

    selectionLayout->addWidget(userLabel);
    selectionLayout->addWidget(m_userInput, 1);
    selectionLayout->addSpacing(20);
    selectionLayout->addWidget(serviceLabel);
    selectionLayout->addWidget(m_serviceSelector, 1);
    selectionLayout->addStretch();

    mainLayout->addWidget(selectionGroup);

    // Middle section: Service-specific credential forms
    QGroupBox* formGroup = new QGroupBox("Credentials");
    QVBoxLayout* formGroupLayout = new QVBoxLayout(formGroup);

    m_formStack = new QStackedWidget();
    createServiceForms();
    formGroupLayout->addWidget(m_formStack);

    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_saveButton = new QPushButton("💾 Save Credentials");
    m_saveButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px 16px; font-weight: bold; } QPushButton:hover { background-color: #45a049; }");
    connect(m_saveButton, &QPushButton::clicked, this, &CredentialsTab::onSaveCredentials);

    m_deleteButton = new QPushButton("🗑️ Delete Credentials");
    m_deleteButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 8px 16px; } QPushButton:hover { background-color: #da190b; }");
    connect(m_deleteButton, &QPushButton::clicked, this, &CredentialsTab::onDeleteCredentials);

    m_refreshButton = new QPushButton("🔄 Refresh");
    connect(m_refreshButton, &QPushButton::clicked, this, &CredentialsTab::onRefreshTable);

    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addStretch();

    formGroupLayout->addLayout(buttonLayout);
    mainLayout->addWidget(formGroup);

    // Status label
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("QLabel { padding: 8px; border-radius: 4px; }");
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    // Bottom section: Credentials overview table
    QGroupBox* tableGroup = new QGroupBox("Stored Credentials Overview");
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);

    m_credentialsTable = new QTableWidget();
    m_credentialsTable->setColumnCount(3);
    m_credentialsTable->setHorizontalHeaderLabels({"Service", "User", "Last Modified"});
    m_credentialsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_credentialsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_credentialsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_credentialsTable->horizontalHeader()->setStretchLastSection(true);
    m_credentialsTable->setMinimumHeight(150);
    connect(m_credentialsTable, &QTableWidget::itemSelectionChanged,
            this, &CredentialsTab::onTableRowSelected);

    tableLayout->addWidget(m_credentialsTable);
    mainLayout->addWidget(tableGroup, 1);

    // Initial state
    onRefreshTable();
}

void CredentialsTab::createServiceForms() {
    // Azure DevOps form
    m_formStack->addWidget(createAzureForm());

    // TeamCentraal form
    m_formStack->addWidget(createTeamCentraalForm());

    // Confluence form
    m_formStack->addWidget(createConfluenceForm());

    // ChatNS form
    m_formStack->addWidget(createChatNSForm());
}

QWidget* CredentialsTab::createAzureForm() {
    QWidget* form = new QWidget();
    QFormLayout* layout = new QFormLayout(form);
    layout->setLabelAlignment(Qt::AlignRight);

    m_azureTokenInput = new QLineEdit();
    m_azureTokenInput->setEchoMode(QLineEdit::Password);
    m_azureTokenInput->setPlaceholderText("Enter your Azure DevOps PAT");

    QLabel* helpLabel = new QLabel("<a href='https://dev.azure.com'>Generate at: dev.azure.com → User Settings → Personal Access Tokens</a>");
    helpLabel->setOpenExternalLinks(true);
    helpLabel->setStyleSheet("color: #0066cc;");

    layout->addRow("Personal Access Token:", m_azureTokenInput);
    layout->addRow("", helpLabel);

    return form;
}

QWidget* CredentialsTab::createTeamCentraalForm() {
    QWidget* form = new QWidget();
    QFormLayout* layout = new QFormLayout(form);
    layout->setLabelAlignment(Qt::AlignRight);

    m_teamcUsername = new QLineEdit();
    m_teamcUsername->setPlaceholderText("Enter username");

    m_teamcPassword = new QLineEdit();
    m_teamcPassword->setEchoMode(QLineEdit::Password);
    m_teamcPassword->setPlaceholderText("Enter password");

    m_teamcUrl = new QLineEdit();
    m_teamcUrl->setPlaceholderText("https://teamcentraal.ns.nl/odata/POS_Odata_v4");
    m_teamcUrl->setText("https://teamcentraal.ns.nl/odata/POS_Odata_v4");

    layout->addRow("Username:", m_teamcUsername);
    layout->addRow("Password:", m_teamcPassword);
    layout->addRow("API URL:", m_teamcUrl);

    return form;
}

QWidget* CredentialsTab::createConfluenceForm() {
    QWidget* form = new QWidget();
    QFormLayout* layout = new QFormLayout(form);
    layout->setLabelAlignment(Qt::AlignRight);

    m_confEmail = new QLineEdit();
    m_confEmail->setPlaceholderText("your.email@ns.nl");

    m_confToken = new QLineEdit();
    m_confToken->setEchoMode(QLineEdit::Password);
    m_confToken->setPlaceholderText("Enter Atlassian API token");

    m_confUrl = new QLineEdit();
    m_confUrl->setPlaceholderText("https://your-domain.atlassian.net/wiki");

    QLabel* helpLabel = new QLabel("<a href='https://id.atlassian.com/manage-profile/security/api-tokens'>Generate at: id.atlassian.com</a>");
    helpLabel->setOpenExternalLinks(true);
    helpLabel->setStyleSheet("color: #0066cc;");

    layout->addRow("Email Address:", m_confEmail);
    layout->addRow("API Token:", m_confToken);
    layout->addRow("Confluence URL:", m_confUrl);
    layout->addRow("", helpLabel);

    return form;
}

QWidget* CredentialsTab::createChatNSForm() {
    QWidget* form = new QWidget();
    QFormLayout* layout = new QFormLayout(form);
    layout->setLabelAlignment(Qt::AlignRight);

    m_chatnsApiKey = new QLineEdit();
    m_chatnsApiKey->setEchoMode(QLineEdit::Password);
    m_chatnsApiKey->setPlaceholderText("Enter ChatNS APIM subscription key");

    layout->addRow("API Key:", m_chatnsApiKey);

    return form;
}

void CredentialsTab::onServiceChanged(int index) {
    m_formStack->setCurrentIndex(index);

    // Load credentials for current user/service if user is set
    QString userId = m_userInput->text().trimmed();
    if (!userId.isEmpty()) {
        QString service = m_serviceKeystoreNames[m_serviceSelector->currentText()];
        QMap<QString, QString> credentials = loadFromKeystore(userId, service);

        // Populate form fields based on service
        if (index == 0) {  // Azure DevOps
            m_azureTokenInput->setText(credentials.value("token", credentials.value("pat", "")));
        } else if (index == 1) {  // TeamCentraal
            m_teamcUsername->setText(credentials.value("username", ""));
            m_teamcPassword->setText(credentials.value("password", ""));
            m_teamcUrl->setText(credentials.value("url", "https://teamcentraal.ns.nl/odata/POS_Odata_v4"));
        } else if (index == 2) {  // Confluence
            m_confEmail->setText(credentials.value("email", ""));
            m_confToken->setText(credentials.value("token", ""));
            m_confUrl->setText(credentials.value("url", ""));
        } else if (index == 3) {  // ChatNS
            m_chatnsApiKey->setText(credentials.value("api_key", ""));
        }
    }
}

void CredentialsTab::onUserChanged() {
    // Reload credentials when user changes
    onServiceChanged(m_serviceSelector->currentIndex());
    onRefreshTable();
}

void CredentialsTab::onSaveCredentials() {
    QString userId = m_userInput->text().trimmed();

    if (userId.isEmpty()) {
        QMessageBox::warning(this, "Missing User", "Please enter a user email address.");
        return;
    }

    if (!userId.contains("@")) {
        QMessageBox::warning(this, "Invalid Email", "Please enter a valid email address.");
        return;
    }

    QString serviceDisplay = m_serviceSelector->currentText();
    QString service = m_serviceKeystoreNames[serviceDisplay];
    QMap<QString, QString> credentials;

    int currentIndex = m_serviceSelector->currentIndex();

    // Collect credentials based on current service
    if (currentIndex == 0) {  // Azure DevOps
        QString token = m_azureTokenInput->text().trimmed();
        if (token.isEmpty()) {
            QMessageBox::warning(this, "Missing Token", "Please enter a Personal Access Token.");
            return;
        }
        credentials["pat"] = token;
    } else if (currentIndex == 1) {  // TeamCentraal
        QString username = m_teamcUsername->text().trimmed();
        QString password = m_teamcPassword->text().trimmed();
        QString url = m_teamcUrl->text().trimmed();

        if (username.isEmpty() || password.isEmpty()) {
            QMessageBox::warning(this, "Missing Fields", "Please enter both username and password.");
            return;
        }

        credentials["username"] = username;
        credentials["password"] = password;
        credentials["url"] = url.isEmpty() ? "https://teamcentraal.ns.nl/odata/POS_Odata_v4" : url;
    } else if (currentIndex == 2) {  // Confluence
        QString email = m_confEmail->text().trimmed();
        QString token = m_confToken->text().trimmed();
        QString url = m_confUrl->text().trimmed();

        if (email.isEmpty() || token.isEmpty()) {
            QMessageBox::warning(this, "Missing Fields", "Please enter both email and API token.");
            return;
        }

        credentials["email"] = email;
        credentials["token"] = token;
        if (!url.isEmpty()) {
            credentials["url"] = url;
        }
    } else if (currentIndex == 3) {  // ChatNS
        QString apiKey = m_chatnsApiKey->text().trimmed();
        if (apiKey.isEmpty()) {
            QMessageBox::warning(this, "Missing API Key", "Please enter the ChatNS API key.");
            return;
        }
        credentials["api_key"] = apiKey;
    }

    // Save to keystore
    if (saveToKeystore(userId, service, credentials)) {
        m_statusLabel->setText("✅ Credentials saved successfully!");
        m_statusLabel->setStyleSheet("QLabel { background-color: #d4edda; color: #155724; padding: 8px; border-radius: 4px; }");
        onRefreshTable();
    } else {
        m_statusLabel->setText("❌ Failed to save credentials. Check logs for details.");
        m_statusLabel->setStyleSheet("QLabel { background-color: #f8d7da; color: #721c24; padding: 8px; border-radius: 4px; }");
    }
}

void CredentialsTab::onDeleteCredentials() {
    QString userId = m_userInput->text().trimmed();

    if (userId.isEmpty()) {
        QMessageBox::warning(this, "Missing User", "Please enter a user email address.");
        return;
    }

    QString serviceDisplay = m_serviceSelector->currentText();
    QString service = m_serviceKeystoreNames[serviceDisplay];

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Deletion",
        QString("Are you sure you want to delete %1 credentials for %2?").arg(serviceDisplay, userId),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (deleteFromKeystore(userId, service)) {
            m_statusLabel->setText("✅ Credentials deleted successfully!");
            m_statusLabel->setStyleSheet("QLabel { background-color: #d4edda; color: #155724; padding: 8px; border-radius: 4px; }");

            // Clear form fields
            int currentIndex = m_serviceSelector->currentIndex();
            if (currentIndex == 0) {
                m_azureTokenInput->clear();
            } else if (currentIndex == 1) {
                m_teamcUsername->clear();
                m_teamcPassword->clear();
            } else if (currentIndex == 2) {
                m_confEmail->clear();
                m_confToken->clear();
                m_confUrl->clear();
            } else if (currentIndex == 3) {
                m_chatnsApiKey->clear();
            }

            onRefreshTable();
        } else {
            m_statusLabel->setText("❌ Failed to delete credentials. They may not exist.");
            m_statusLabel->setStyleSheet("QLabel { background-color: #f8d7da; color: #721c24; padding: 8px; border-radius: 4px; }");
        }
    }
}

void CredentialsTab::onRefreshTable() {
    m_credentialsTable->setRowCount(0);

    // Use Python script to list all credentials
    // For now, we'll just show a placeholder - we need a list_all_credentials script
    QStringList users = listKeystoreUsers();

    int row = 0;
    for (const QString& userId : users) {
        QStringList services = listUserServices(userId);

        for (const QString& service : services) {
            m_credentialsTable->insertRow(row);

            QString displayName = m_serviceDisplayNames.value(service, service);
            m_credentialsTable->setItem(row, 0, new QTableWidgetItem(displayName));
            m_credentialsTable->setItem(row, 1, new QTableWidgetItem(userId));
            m_credentialsTable->setItem(row, 2, new QTableWidgetItem("-"));  // TODO: Add timestamp

            row++;
        }
    }

    m_credentialsTable->resizeColumnsToContents();
}

void CredentialsTab::onTableRowSelected() {
    QList<QTableWidgetItem*> selected = m_credentialsTable->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    int row = selected.first()->row();
    QString serviceDisplay = m_credentialsTable->item(row, 0)->text();
    QString userId = m_credentialsTable->item(row, 1)->text();

    // Set user and service in selectors
    m_userInput->setText(userId);

    int serviceIndex = m_serviceSelector->findText(serviceDisplay);
    if (serviceIndex >= 0) {
        m_serviceSelector->setCurrentIndex(serviceIndex);
    }
}

bool CredentialsTab::saveToKeystore(const QString& userId, const QString& service, const QMap<QString, QString>& credentials) {
    qDebug() << "Saving credentials: user=" << userId << "service=" << service;

    // Save all credentials for this service
    for (auto it = credentials.constBegin(); it != credentials.constEnd(); ++it) {
        if (!m_keystore->setCredential(service, it.key(), it.value())) {
            qWarning() << "Failed to save credential:" << service << it.key();
            return false;
        }
    }

    qDebug() << "Credentials saved successfully";
    return true;
}

bool CredentialsTab::deleteFromKeystore(const QString& userId, const QString& service) {
    return m_keystore->clearService(service);
}

QMap<QString, QString> CredentialsTab::loadFromKeystore(const QString& userId, const QString& service) {
    return m_keystore->getServiceCredentials(service);
}

QStringList CredentialsTab::listKeystoreUsers() {
    // In C++ keystore, we don't have per-user separation (yet)
    // Return a placeholder for compatibility
    QStringList users;
    if (!m_keystore->listServices().isEmpty()) {
        users << "default";
    }
    return users;
}

QStringList CredentialsTab::listUserServices(const QString& userId) {
    return m_keystore->listServices();
}
