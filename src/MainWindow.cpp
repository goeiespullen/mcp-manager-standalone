#include "MainWindow.h"
#include "MCPServerManager.h"
#include "MCPServerInstance.h"
#include "MCPGateway.h"
#include "TrafficMonitor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QSettings>
#include <QMessageBox>
#include <QTimer>
#include <QHeaderView>
#include <QFileDialog>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QScrollArea>
#include <QCheckBox>
#include <QProcess>
#include <QProgressDialog>
#include <QFileInfo>
#include <QDir>
#include <QTabWidget>
#include <QRegularExpression>
#include <QListWidget>
#include <QApplication>

MainWindow::MainWindow(MCPServerManager* manager, QWidget* parent)
    : QMainWindow(parent)
    , m_manager(manager)
    , m_gateway(nullptr)
{
    setWindowTitle("MCP Server Manager");
    resize(1200, 800);

    // Create Traffic monitor
    m_trafficMonitor = new TrafficMonitor(this);

    // Create and start Gateway
    m_gateway = new MCPGateway(manager, this);
    if (m_gateway->start(8700)) {
        qDebug() << "MCP Gateway started on port 8700";
    } else {
        qWarning() << "Failed to start MCP Gateway";
    }

    setupUI();
    createMenuBar();
    loadSettings();

    // Connect manager signals
    connect(m_manager, &MCPServerManager::serverAdded,
            this, &MainWindow::updateServerTable);
    connect(m_manager, &MCPServerManager::serverRemoved,
            this, &MainWindow::updateServerTable);
    connect(m_manager, &MCPServerManager::serverStatusChanged,
            this, &MainWindow::onServerStatusChanged);
    connect(m_manager, &MCPServerManager::serverOutputReceived,
            this, &MainWindow::onServerOutput);
    connect(m_manager, &MCPServerManager::serverErrorOccurred,
            this, &MainWindow::onServerError);

    // Connect gateway signals
    connect(m_gateway, &MCPGateway::sessionCreated,
            this, &MainWindow::updateGatewayStatus);
    connect(m_gateway, &MCPGateway::sessionDestroyed,
            this, &MainWindow::updateGatewayStatus);
    connect(m_gateway, &MCPGateway::messageTraffic,
            m_trafficMonitor, &TrafficMonitor::logMessage);

    // Status update timer
    QTimer* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    statusTimer->start(1000);

    // Initial population
    populateServerTable();
    updateStatusBar();
}

MainWindow::~MainWindow() {
    saveSettings();
}

void MainWindow::setupUI() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Tab widget
    m_tabWidget = new QTabWidget();
    m_tabWidget->addTab(createServersTab(), "Servers");
    m_tabWidget->addTab(createToolsBrowserTab(), "Tools Browser");
    m_tabWidget->addTab(createGatewayTab(), "Gateway (Port 8700)");
    m_tabWidget->addTab(createLogsTab(), "Logs");
    m_tabWidget->addTab(m_trafficMonitor, "Traffic Monitor");

    mainLayout->addWidget(m_tabWidget);

    // Status bar
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel);
}

QWidget* MainWindow::createGatewayTab() {
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    // Info label
    QLabel* infoLabel = new QLabel(
        "<b>MCP Gateway - Session-based Multi-Client Access</b><br><br>"
        "The gateway allows multiple clients (dashboards, CLIs) to connect and create isolated sessions.<br>"
        "Each session runs its own MCP server with client-specific credentials.<br><br>"
        "<b>Gateway Port:</b> 8700<br>"
        "<b>Protocol:</b> JSON-RPC over TCP"
    );
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { padding: 15px; background-color: #f0f0f0; border-radius: 5px; }");
    layout->addWidget(infoLabel);

    // Gateway status
    QGroupBox* statusGroup = new QGroupBox("Gateway Status");
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);

    QLabel* portLabel = new QLabel(QString("Listening on: <b>localhost:8700</b>"));
    m_gatewaySessionLabel = new QLabel("Active sessions: <b>0</b>");

    statusLayout->addWidget(portLabel);
    statusLayout->addWidget(m_gatewaySessionLabel);

    layout->addWidget(statusGroup);

    // Help button for Python usage
    QPushButton* helpButton = new QPushButton("📖 View Python Client Example...");
    helpButton->setMaximumWidth(300);
    connect(helpButton, &QPushButton::clicked, this, &MainWindow::onShowGatewayHelp);
    layout->addWidget(helpButton);

    layout->addStretch();

    return widget;
}

QWidget* MainWindow::createServersTab() {
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    // Info label
    QLabel* infoLabel = new QLabel(
        "Manage multiple MCP servers from a single interface. "
        "Start, stop, and monitor all your MCP integrations here."
    );
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { color: #666; padding: 10px; }");
    layout->addWidget(infoLabel);

    // Global controls
    QHBoxLayout* globalLayout = new QHBoxLayout();
    m_startAllButton = new QPushButton("Start All");
    m_stopAllButton = new QPushButton("Stop All");

    m_startAllButton->setIcon(QIcon::fromTheme("media-playback-start"));
    m_stopAllButton->setIcon(QIcon::fromTheme("media-playback-stop"));

    globalLayout->addWidget(m_startAllButton);
    globalLayout->addWidget(m_stopAllButton);
    globalLayout->addStretch();

    layout->addLayout(globalLayout);

    // Server table
    m_serverTable = new QTableWidget();
    m_serverTable->setColumnCount(5);
    m_serverTable->setHorizontalHeaderLabels({"Status", "Name", "Type", "Port", "PID"});
    m_serverTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_serverTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_serverTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_serverTable->horizontalHeader()->setStretchLastSection(true);
    m_serverTable->setAlternatingRowColors(true);

    // Set column widths
    m_serverTable->setColumnWidth(0, 100);
    m_serverTable->setColumnWidth(1, 200);
    m_serverTable->setColumnWidth(2, 100);
    m_serverTable->setColumnWidth(3, 80);
    m_serverTable->setColumnWidth(4, 100);

    layout->addWidget(m_serverTable);

    // Server control buttons
    QHBoxLayout* controlLayout = new QHBoxLayout();

    m_startButton = new QPushButton("Start");
    m_stopButton = new QPushButton("Stop");
    m_restartButton = new QPushButton("Restart");
    m_addButton = new QPushButton("Add Server...");
    m_editButton = new QPushButton("Edit...");
    m_toolsButton = new QPushButton("Tools...");
    m_removeButton = new QPushButton("Remove");

    m_startButton->setIcon(QIcon::fromTheme("media-playback-start"));
    m_stopButton->setIcon(QIcon::fromTheme("media-playback-stop"));
    m_restartButton->setIcon(QIcon::fromTheme("view-refresh"));
    m_addButton->setIcon(QIcon::fromTheme("list-add"));
    m_editButton->setIcon(QIcon::fromTheme("document-edit"));
    m_toolsButton->setIcon(QIcon::fromTheme("configure"));
    m_removeButton->setIcon(QIcon::fromTheme("list-remove"));

    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
    m_editButton->setEnabled(false);
    m_toolsButton->setEnabled(false);
    m_removeButton->setEnabled(false);

    controlLayout->addWidget(m_startButton);
    controlLayout->addWidget(m_stopButton);
    controlLayout->addWidget(m_restartButton);
    controlLayout->addStretch();
    controlLayout->addWidget(m_addButton);
    controlLayout->addWidget(m_editButton);
    controlLayout->addWidget(m_toolsButton);
    controlLayout->addWidget(m_removeButton);

    layout->addLayout(controlLayout);

    // Connect signals
    connect(m_startAllButton, &QPushButton::clicked, this, &MainWindow::onStartAllClicked);
    connect(m_stopAllButton, &QPushButton::clicked, this, &MainWindow::onStopAllClicked);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartServerClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopServerClicked);
    connect(m_restartButton, &QPushButton::clicked, this, &MainWindow::onRestartServerClicked);
    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::onAddServerClicked);
    connect(m_editButton, &QPushButton::clicked, this, &MainWindow::onEditServerClicked);
    connect(m_toolsButton, &QPushButton::clicked, this, &MainWindow::onManageToolsClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveServerClicked);
    connect(m_serverTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onServerSelected);

    return widget;
}

QWidget* MainWindow::createLogsTab() {
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    // Filter
    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("Filter by server:"));
    m_serverFilter = new QComboBox();
    m_serverFilter->addItem("All Servers", "");
    filterLayout->addWidget(m_serverFilter);
    filterLayout->addStretch();

    QPushButton* clearButton = new QPushButton("Clear Logs");
    clearButton->setIcon(QIcon::fromTheme("edit-clear"));
    connect(clearButton, &QPushButton::clicked, [this]() {
        m_logDisplay->clear();
    });
    filterLayout->addWidget(clearButton);

    layout->addLayout(filterLayout);

    // Log display
    m_logDisplay = new QTextEdit();
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setFontFamily("Monospace");
    layout->addWidget(m_logDisplay);

    return widget;
}

QWidget* MainWindow::createToolsBrowserTab() {
    QWidget* widget = new QWidget();
    QHBoxLayout* mainLayout = new QHBoxLayout(widget);

    // Left panel: Server list
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* serverLabel = new QLabel("<b>Select Server:</b>");
    leftLayout->addWidget(serverLabel);

    m_toolsServerList = new QListWidget();
    m_toolsServerList->setMaximumWidth(250);
    leftLayout->addWidget(m_toolsServerList);

    // Populate server list
    QList<MCPServerInstance*> servers = m_manager->allServers();
    for (MCPServerInstance* server : servers) {
        QListWidgetItem* item = new QListWidgetItem(server->name());
        if (server->isRunning()) {
            item->setIcon(QIcon::fromTheme("dialog-ok"));
            item->setForeground(QColor(0, 128, 0));
        } else {
            item->setIcon(QIcon::fromTheme("dialog-cancel"));
            item->setForeground(QColor(128, 128, 128));
        }
        m_toolsServerList->addItem(item);
    }

    mainLayout->addWidget(leftPanel);

    // Right panel: Tools table and details
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    QLabel* toolsLabel = new QLabel("<b>Available Tools:</b>");
    toolbarLayout->addWidget(toolsLabel);
    toolbarLayout->addStretch();

    m_refreshToolsButton = new QPushButton("🔄 Refresh Tools");
    m_refreshToolsButton->setEnabled(false);
    connect(m_refreshToolsButton, &QPushButton::clicked, [this]() {
        QListWidgetItem* item = m_toolsServerList->currentItem();
        if (!item) return;

        QString serverName = item->text();
        MCPServerInstance* server = m_manager->getServer(serverName);
        if (!server || !server->isRunning()) {
            QMessageBox::warning(this, "Error", "Server must be running to refresh tools");
            return;
        }

        // Show loading message
        m_toolDetailsDisplay->setPlainText(QString("Querying tools from %1...\nPlease wait...").arg(serverName));
        m_toolsTable->setRowCount(0);

        // Create timeout timer
        QTimer* timeoutTimer = new QTimer(this);
        timeoutTimer->setSingleShot(true);

        // Connect to toolsChanged signal with single-shot connection
        // Use a shared pointer to the connection to allow disconnection from within the lambda
        QMetaObject::Connection* conn = new QMetaObject::Connection();
        *conn = connect(server, &MCPServerInstance::toolsChanged, [this, server, conn, timeoutTimer]() {
            // Stop timeout timer
            timeoutTimer->stop();
            timeoutTimer->deleteLater();

            // Disconnect this connection so it only fires once
            QObject::disconnect(*conn);
            delete conn;

            // Populate tools table
            m_toolsTable->setRowCount(0);
            QList<MCPServerInstance::ToolInfo> tools = server->availableTools();

            for (const MCPServerInstance::ToolInfo& tool : tools) {
                int row = m_toolsTable->rowCount();
                m_toolsTable->insertRow(row);

                QTableWidgetItem* nameItem = new QTableWidgetItem(tool.name);
                nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
                m_toolsTable->setItem(row, 0, nameItem);

                QTableWidgetItem* descItem = new QTableWidgetItem(tool.description);
                descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
                m_toolsTable->setItem(row, 1, descItem);

                QTableWidgetItem* statusItem = new QTableWidgetItem(tool.enabled ? "✅ Enabled" : "❌ Disabled");
                statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
                if (tool.enabled) {
                    statusItem->setForeground(QColor(0, 128, 0));
                } else {
                    statusItem->setForeground(QColor(180, 0, 0));
                }
                m_toolsTable->setItem(row, 2, statusItem);
            }

            if (tools.isEmpty()) {
                m_toolDetailsDisplay->setPlainText("No tools found. Make sure the server is running and supports the MCP tools/list method.");
            } else {
                m_toolDetailsDisplay->setPlainText(QString("Successfully loaded %1 tool(s) from %2.\n\nSelect a tool to view details.")
                    .arg(tools.size()).arg(server->name()));
            }
        });

        // Setup timeout handler (5 seconds)
        connect(timeoutTimer, &QTimer::timeout, [this, conn, serverName]() {
            // Disconnect the signal connection
            QObject::disconnect(*conn);
            delete conn;

            // Show timeout error
            m_toolDetailsDisplay->setPlainText(QString(
                "⚠️ Timeout querying tools from %1\n\n"
                "The server did not respond within 5 seconds.\n\n"
                "Possible causes:\n"
                "• Server is still initializing\n"
                "• Server doesn't support tools/list method\n"
                "• Server is not responding to stdin\n\n"
                "Try again in a few seconds, or check the Logs tab for errors."
            ).arg(serverName));
        });

        // Start timeout timer
        timeoutTimer->start(5000);

        // Request tools refresh from server
        server->refreshTools();
    });
    toolbarLayout->addWidget(m_refreshToolsButton);

    rightLayout->addLayout(toolbarLayout);

    // Tools table
    m_toolsTable = new QTableWidget();
    m_toolsTable->setColumnCount(3);
    m_toolsTable->setHorizontalHeaderLabels({"Tool Name", "Description", "Status"});
    m_toolsTable->horizontalHeader()->setStretchLastSection(false);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_toolsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_toolsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rightLayout->addWidget(m_toolsTable, 2);

    // Tool details
    QLabel* detailsLabel = new QLabel("<b>Tool Details:</b>");
    rightLayout->addWidget(detailsLabel);

    m_toolDetailsDisplay = new QTextEdit();
    m_toolDetailsDisplay->setReadOnly(true);
    m_toolDetailsDisplay->setMaximumHeight(200);
    m_toolDetailsDisplay->setPlainText("Select a server and click 'Refresh Tools' to see available tools.");
    rightLayout->addWidget(m_toolDetailsDisplay, 1);

    mainLayout->addWidget(rightPanel, 1);

    // Connect server selection changed
    connect(m_toolsServerList, &QListWidget::currentItemChanged, [this](QListWidgetItem* current, QListWidgetItem*) {
        if (!current) {
            m_refreshToolsButton->setEnabled(false);
            return;
        }

        QString serverName = current->text();
        MCPServerInstance* server = m_manager->getServer(serverName);

        if (server && server->isRunning()) {
            m_refreshToolsButton->setEnabled(true);
            m_toolDetailsDisplay->setPlainText(
                QString("Server: %1\nStatus: Running\nPort: %2\n\n"
                        "Click 'Refresh Tools' to query available tools from this server.")
                .arg(serverName).arg(server->port())
            );
        } else {
            m_refreshToolsButton->setEnabled(false);
            m_toolDetailsDisplay->setPlainText(
                QString("Server: %1\nStatus: Stopped\n\n"
                        "Start the server to query its tools.")
                .arg(serverName)
            );
        }

        // Clear tools table
        m_toolsTable->setRowCount(0);
    });

    // Connect tool selection to show details
    connect(m_toolsTable, &QTableWidget::currentItemChanged, [this](QTableWidgetItem* current, QTableWidgetItem*) {
        if (!current) return;

        int row = current->row();
        QString toolName = m_toolsTable->item(row, 0)->text();
        QString toolDesc = m_toolsTable->item(row, 1)->text();
        QString toolStatus = m_toolsTable->item(row, 2)->text();

        // Get server
        QListWidgetItem* serverItem = m_toolsServerList->currentItem();
        if (!serverItem) return;

        QString serverName = serverItem->text();
        MCPServerInstance* server = m_manager->getServer(serverName);
        if (!server) return;

        // Find tool details
        QList<MCPServerInstance::ToolInfo> tools = server->availableTools();
        for (const MCPServerInstance::ToolInfo& tool : tools) {
            if (tool.name == toolName) {
                // Format tool details
                QString details = QString("<b>Tool:</b> %1<br><br>").arg(tool.name);
                details += QString("<b>Description:</b><br>%1<br><br>").arg(tool.description);
                details += QString("<b>Status:</b> %1<br><br>").arg(tool.enabled ? "✅ Enabled" : "❌ Disabled");

                // Show parameter schema
                if (!tool.schema.isEmpty()) {
                    details += "<b>Parameters:</b><br>";
                    QJsonDocument doc(tool.schema);
                    details += QString("<pre>%1</pre>").arg(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
                }

                m_toolDetailsDisplay->setHtml(details);
                break;
            }
        }
    });

    return widget;
}

void MainWindow::createMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");

    QAction* reloadAction = fileMenu->addAction("&Reload Config");
    reloadAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(reloadAction, &QAction::triggered, this, &MainWindow::onReloadConfigClicked);

    QAction* saveAction = fileMenu->addAction("&Save Config");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveConfigClicked);

    fileMenu->addSeparator();

    QAction* quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // Servers menu
    QMenu* serversMenu = menuBar->addMenu("&Servers");

    QAction* startAllAction = serversMenu->addAction("Start &All");
    connect(startAllAction, &QAction::triggered, this, &MainWindow::onStartAllClicked);

    QAction* stopAllAction = serversMenu->addAction("Stop A&ll");
    connect(stopAllAction, &QAction::triggered, this, &MainWindow::onStopAllClicked);

    // Help menu
    QMenu* helpMenu = menuBar->addMenu("&Help");

    QAction* gatewayHelpAction = helpMenu->addAction("&Gateway Usage...");
    connect(gatewayHelpAction, &QAction::triggered, this, &MainWindow::onShowGatewayHelp);

    helpMenu->addSeparator();

    // Testing Documentation submenu
    QMenu* docsMenu = helpMenu->addMenu("📚 &Testing Documentation");

    QAction* testingDocsAction = docsMenu->addAction("📖 Documentation Index");
    connect(testingDocsAction, &QAction::triggered, this, &MainWindow::onShowTestingDocs);

    QAction* quickRefAction = docsMenu->addAction("⚡ Quick Reference");
    connect(quickRefAction, &QAction::triggered, this, &MainWindow::onShowQuickReference);

    QAction* userManualAction = docsMenu->addAction("📘 User Manual");
    connect(userManualAction, &QAction::triggered, this, &MainWindow::onShowUserManual);

    docsMenu->addSeparator();

    QAction* openDocsAction = docsMenu->addAction("📂 Open Documentation Folder...");
    connect(openDocsAction, &QAction::triggered, this, &MainWindow::onOpenDocsFolder);

    helpMenu->addSeparator();

    QAction* aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About MCP Server Manager",
            "<h3>MCP Server Manager v2.0</h3>"
            "<p>Manage multiple Model Context Protocol servers from a single interface.</p>"
            "<p>Features:</p>"
            "<ul>"
            "<li>Start/stop multiple MCP servers</li>"
            "<li>Monitor server health and output</li>"
            "<li>Traffic monitoring</li>"
            "<li>Centralized configuration</li>"
            "</ul>"
            "<p>Built with Qt6 and C++</p>");
    });
}

void MainWindow::populateServerTable() {
    m_serverTable->setRowCount(0);
    m_serverFilter->clear();
    m_serverFilter->addItem("All Servers", "");

    QList<MCPServerInstance*> servers = m_manager->allServers();

    for (MCPServerInstance* server : servers) {
        int row = m_serverTable->rowCount();
        m_serverTable->insertRow(row);

        // Status
        QString status = server->statusString();
        QTableWidgetItem* statusItem = new QTableWidgetItem(statusIcon(server->status()) + " " + status);
        statusItem->setForeground(QBrush(QColor(statusColor(server->status()))));
        m_serverTable->setItem(row, 0, statusItem);

        // Name
        m_serverTable->setItem(row, 1, new QTableWidgetItem(server->name()));

        // Type
        m_serverTable->setItem(row, 2, new QTableWidgetItem(server->type()));

        // Port
        m_serverTable->setItem(row, 3, new QTableWidgetItem(QString::number(server->port())));

        // PID
        QString pidStr = server->isRunning() ? QString::number(server->pid()) : "-";
        m_serverTable->setItem(row, 4, new QTableWidgetItem(pidStr));

        // Add to filter
        m_serverFilter->addItem(server->name(), server->name());
    }
}

void MainWindow::updateServerTable() {
    populateServerTable();
}

void MainWindow::onServerStatusChanged(const QString& name, int oldStatus, int newStatus) {
    Q_UNUSED(oldStatus);
    Q_UNUSED(newStatus);

    updateServerTable();
    updateStatusBar();
    updateToolsServerList();  // Update Tools Browser server list

    // Log the change
    MCPServerInstance* server = m_manager->getServer(name);
    if (server) {
        QString msg = QString("[%1] Status: %2")
            .arg(name)
            .arg(server->statusString());
        m_logDisplay->append(msg);
    }
}

void MainWindow::onServerOutput(const QString& name, const QString& line) {
    // Filter logs
    QString filter = m_serverFilter->currentData().toString();
    if (!filter.isEmpty() && filter != name) {
        return;
    }

    QString msg = QString("[%1] %2").arg(name).arg(line);
    m_logDisplay->append(msg);

    // Auto-scroll
    m_logDisplay->verticalScrollBar()->setValue(
        m_logDisplay->verticalScrollBar()->maximum()
    );
}

void MainWindow::onServerError(const QString& name, const QString& error) {
    QString msg = QString("<font color='red'>[%1] ERROR: %2</font>")
        .arg(name).arg(error);
    m_logDisplay->append(msg);

    QMessageBox::warning(this, "Server Error",
        QString("Server '%1' encountered an error:\n%2").arg(name).arg(error));
}

void MainWindow::onStartAllClicked() {
    m_manager->startAll();
    m_logDisplay->append("<b>Starting all servers...</b>");
}

void MainWindow::onStopAllClicked() {
    m_manager->stopAll();
    m_logDisplay->append("<b>Stopping all servers...</b>");
}

void MainWindow::onServerSelected() {
    bool hasSelection = m_serverTable->selectionModel()->hasSelection();

    if (hasSelection) {
        QString serverName = getSelectedServerName();
        MCPServerInstance* server = m_manager->getServer(serverName);

        if (server) {
            bool isRunning = server->isRunning();
            m_startButton->setEnabled(!isRunning);
            m_stopButton->setEnabled(isRunning);
            m_restartButton->setEnabled(isRunning);
            m_editButton->setEnabled(!isRunning);
            m_toolsButton->setEnabled(true);  // Tools can be managed anytime
            m_removeButton->setEnabled(!isRunning);
        }
    } else {
        m_startButton->setEnabled(false);
        m_stopButton->setEnabled(false);
        m_restartButton->setEnabled(false);
        m_editButton->setEnabled(false);
        m_toolsButton->setEnabled(false);
        m_removeButton->setEnabled(false);
    }
}

void MainWindow::onStartServerClicked() {
    QString serverName = getSelectedServerName();
    if (serverName.isEmpty()) return;

    m_manager->startServer(serverName);
    m_logDisplay->append(QString("<b>Starting server: %1</b>").arg(serverName));
}

void MainWindow::onStopServerClicked() {
    QString serverName = getSelectedServerName();
    if (serverName.isEmpty()) return;

    m_manager->stopServer(serverName);
    m_logDisplay->append(QString("<b>Stopping server: %1</b>").arg(serverName));
}

void MainWindow::onRestartServerClicked() {
    QString serverName = getSelectedServerName();
    if (serverName.isEmpty()) return;

    m_manager->restartServer(serverName);
    m_logDisplay->append(QString("<b>Restarting server: %1</b>").arg(serverName));
}

void MainWindow::onAddServerClicked() {
    // Create dialog with tabs
    QDialog dialog(this);
    dialog.setWindowTitle("Add MCP Server");
    dialog.setMinimumWidth(700);
    dialog.setMinimumHeight(500);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // Create tab widget
    QTabWidget* tabs = new QTabWidget(&dialog);

    // === TAB 1: Install from Zip ===
    QWidget* zipTab = new QWidget();
    QVBoxLayout* zipLayout = new QVBoxLayout(zipTab);

    // Info
    QLabel* zipInfo = new QLabel(
        "<b>Install MCP Server from Zip File</b><br>"
        "Select a downloaded MCP server zip file (from GitHub, etc.) and "
        "it will be automatically extracted, installed, and configured."
    );
    zipInfo->setWordWrap(true);
    zipInfo->setStyleSheet("QLabel { background-color: #e3f2fd; padding: 10px; border-radius: 4px; }");
    zipLayout->addWidget(zipInfo);

    // File picker
    QHBoxLayout* fileLayout = new QHBoxLayout();
    QLineEdit* zipPathEdit = new QLineEdit();
    zipPathEdit->setPlaceholderText("Select a .zip file...");
    zipPathEdit->setReadOnly(true);
    QPushButton* browseBtn = new QPushButton("Browse...");
    fileLayout->addWidget(new QLabel("Zip File:"));
    fileLayout->addWidget(zipPathEdit);
    fileLayout->addWidget(browseBtn);
    zipLayout->addLayout(fileLayout);

    // Installation name
    QHBoxLayout* nameLayout = new QHBoxLayout();
    QLineEdit* installNameEdit = new QLineEdit();
    installNameEdit->setPlaceholderText("e.g., email-server, github-server");
    nameLayout->addWidget(new QLabel("Install Name:"));
    nameLayout->addWidget(installNameEdit);
    zipLayout->addLayout(nameLayout);

    // Install button
    QPushButton* installBtn = new QPushButton("Extract and Install");
    installBtn->setEnabled(false);
    zipLayout->addWidget(installBtn);

    // Progress display
    QTextEdit* progressDisplay = new QTextEdit();
    progressDisplay->setReadOnly(true);
    progressDisplay->setMaximumHeight(250);
    progressDisplay->setPlaceholderText("Installation progress will appear here...");
    zipLayout->addWidget(progressDisplay);

    // Auto-detected config (hidden initially)
    QGroupBox* configGroup = new QGroupBox("Auto-Detected Configuration");
    configGroup->setVisible(false);
    QFormLayout* configForm = new QFormLayout(configGroup);
    QLineEdit* detectedTypeEdit = new QLineEdit();
    detectedTypeEdit->setReadOnly(true);
    QLineEdit* detectedCommandEdit = new QLineEdit();
    detectedCommandEdit->setReadOnly(true);
    QLineEdit* detectedArgsEdit = new QLineEdit();
    detectedArgsEdit->setReadOnly(true);
    QLineEdit* detectedWorkDirEdit = new QLineEdit();
    detectedWorkDirEdit->setReadOnly(true);
    QSpinBox* detectedPortSpin = new QSpinBox();
    detectedPortSpin->setRange(8000, 9999);
    detectedPortSpin->setValue(8768);

    configForm->addRow("Type:", detectedTypeEdit);
    configForm->addRow("Command:", detectedCommandEdit);
    configForm->addRow("Arguments:", detectedArgsEdit);
    configForm->addRow("Working Dir:", detectedWorkDirEdit);
    configForm->addRow("Port:", detectedPortSpin);
    zipLayout->addWidget(configGroup);

    tabs->addTab(zipTab, "Install from Zip");

    // === TAB 2: Manual Configuration ===
    QWidget* manualTab = new QWidget();
    QVBoxLayout* manualLayout = new QVBoxLayout(manualTab);

    QLabel* manualInfo = new QLabel(
        "<b>Manual Configuration</b><br>"
        "Add an already-installed MCP server by providing its configuration details."
    );
    manualInfo->setWordWrap(true);
    manualInfo->setStyleSheet("QLabel { background-color: #fff3e0; padding: 10px; border-radius: 4px; }");
    manualLayout->addWidget(manualInfo);

    QFormLayout* form = new QFormLayout();

    QLineEdit* nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("e.g., GitHub, Postgres, Demo");
    form->addRow("Server Name:", nameEdit);

    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItem("Python", "python");
    typeCombo->addItem("Node.js", "node");
    typeCombo->addItem("Binary", "binary");
    form->addRow("Type:", typeCombo);

    QLineEdit* commandEdit = new QLineEdit();
    commandEdit->setPlaceholderText("/full/path/to/executable");
    form->addRow("Command:", commandEdit);

    QLineEdit* argsEdit = new QLineEdit();
    argsEdit->setPlaceholderText("-m package_name (or leave empty)");
    form->addRow("Arguments:", argsEdit);

    QLineEdit* workDirEdit = new QLineEdit();
    workDirEdit->setPlaceholderText("/full/path/to/working/directory");
    form->addRow("Working Dir:", workDirEdit);

    QSpinBox* portSpin = new QSpinBox();
    portSpin->setRange(8000, 9999);
    portSpin->setValue(8768);
    form->addRow("Port:", portSpin);

    QLineEdit* descEdit = new QLineEdit();
    descEdit->setPlaceholderText("Brief description of what this server does");
    form->addRow("Description:", descEdit);

    manualLayout->addLayout(form);

    QLabel* hint = new QLabel(
        "<b>Examples:</b><br>"
        "<i>Python MCP:</i> command=/path/to/venv/bin/python, args=-m mcp_package<br>"
        "<i>Node MCP:</i> command=/path/to/node, args=dist/index.js<br>"
        "<i>Script:</i> command=/path/to/script.py, args=(empty)"
    );
    hint->setWordWrap(true);
    hint->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border-radius: 4px; }");
    manualLayout->addWidget(hint);

    manualLayout->addStretch();

    tabs->addTab(manualTab, "Manual Configuration");

    mainLayout->addWidget(tabs);

    // Dialog buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
    );
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false); // Disable OK until installation completes
    mainLayout->addWidget(buttons);

    // Connect browse button
    connect(browseBtn, &QPushButton::clicked, [&]() {
        QString zipFile = QFileDialog::getOpenFileName(&dialog,
            "Select MCP Server Zip File",
            QDir::homePath() + "/Downloads",
            "Zip Files (*.zip)");

        if (!zipFile.isEmpty()) {
            zipPathEdit->setText(zipFile);
            installBtn->setEnabled(true);

            // Suggest install name from filename
            QFileInfo info(zipFile);
            QString baseName = info.baseName();
            baseName = baseName.replace("_", "-").replace(" ", "-");
            if (installNameEdit->text().isEmpty()) {
                installNameEdit->setText(baseName);
            }
        }
    });

    // Connect install button
    connect(installBtn, &QPushButton::clicked, [&]() {
        QString zipPath = zipPathEdit->text();
        QString installName = installNameEdit->text().trimmed();

        if (zipPath.isEmpty() || installName.isEmpty()) {
            progressDisplay->append("<font color='red'>❌ Please select a zip file and provide an install name</font>");
            return;
        }

        installBtn->setEnabled(false);
        browseBtn->setEnabled(false);
        progressDisplay->clear();

        // Extract and install
        progressDisplay->append("<b>Starting installation...</b>");
        progressDisplay->append(QString("Zip file: %1").arg(zipPath));
        progressDisplay->append(QString("Install name: %1").arg(installName));
        progressDisplay->append("");

        // Determine destination directory
        // Go up from build directory to project root: build/ -> mcp-manager/ -> project-root/
        QDir appDir(QCoreApplication::applicationDirPath());
        appDir.cdUp();  // Go up from build/
        appDir.cdUp();  // Go up from mcp-manager/
        QString mcpServersDir = appDir.absolutePath() + "/mcp-servers";
        QDir().mkpath(mcpServersDir);
        QString destDir = mcpServersDir + "/" + installName;

        progressDisplay->append(QString("📁 Destination: %1").arg(destDir));
        progressDisplay->append("");

        // Check if directory exists
        if (QDir(destDir).exists()) {
            progressDisplay->append("<font color='orange'>⚠️  Directory already exists!</font>");
            progressDisplay->append(QString("   %1").arg(destDir));

            QMessageBox::StandardButton reply = QMessageBox::question(&dialog,
                "Directory Exists",
                QString("The directory already exists:\n\n%1\n\n"
                        "Do you want to remove it and continue with the installation?").arg(destDir),
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::No) {
                progressDisplay->append("<font color='red'>❌ Installation cancelled by user</font>");
                installBtn->setEnabled(true);
                browseBtn->setEnabled(true);
                return;
            }

            // User confirmed - remove existing directory
            progressDisplay->append("<font color='orange'>🗑️  Removing existing directory...</font>");
            QApplication::processEvents();

            QDir existingDir(destDir);
            if (!existingDir.removeRecursively()) {
                progressDisplay->append("<font color='red'>❌ Failed to remove existing directory!</font>");
                installBtn->setEnabled(true);
                browseBtn->setEnabled(true);
                return;
            }

            progressDisplay->append("<font color='green'>✅ Existing directory removed</font>");
            progressDisplay->append("");
        }

        // Extract zip
        progressDisplay->append("📦 Extracting zip file...");
        QApplication::processEvents();

        QString extractError = extractZipFile(zipPath, destDir, progressDisplay);
        if (!extractError.isEmpty()) {
            progressDisplay->append(QString("<font color='red'>❌ Extraction failed: %1</font>").arg(extractError));
            installBtn->setEnabled(true);
            browseBtn->setEnabled(true);
            return;
        }

        progressDisplay->append("<font color='green'>✅ Extraction complete</font>");
        progressDisplay->append("");

        // Detect server type
        progressDisplay->append("🔍 Detecting server type...");
        QApplication::processEvents();

        QString serverType = detectServerType(destDir, progressDisplay);
        if (serverType.isEmpty()) {
            progressDisplay->append("<font color='red'>❌ Could not detect server type (no package.json or pyproject.toml found)</font>");
            installBtn->setEnabled(true);
            browseBtn->setEnabled(true);
            return;
        }

        progressDisplay->append(QString("<font color='green'>✅ Detected type: %1</font>").arg(serverType));
        progressDisplay->append("");

        // Install dependencies
        progressDisplay->append("📥 Installing dependencies...");
        QApplication::processEvents();

        QString installError = installDependencies(destDir, serverType, progressDisplay);
        if (!installError.isEmpty()) {
            progressDisplay->append(QString("<font color='orange'>⚠️  Installation warning: %1</font>").arg(installError));
        } else {
            progressDisplay->append("<font color='green'>✅ Dependencies installed</font>");
        }
        progressDisplay->append("");

        // Find entry point
        QString entryPoint = findEntryPoint(destDir, serverType);
        QString command;
        QString args;

        if (serverType == "python") {
            // Check for venv
            QString venvPython = destDir + "/.venv/bin/python";
            if (QFile::exists(venvPython)) {
                command = venvPython;
            } else {
                command = "python3";
            }

            if (!entryPoint.isEmpty()) {
                args = entryPoint;
            }
        } else if (serverType == "node") {
            command = "node";
            if (!entryPoint.isEmpty()) {
                args = entryPoint;
            }
        }

        // Populate detected config
        detectedTypeEdit->setText(serverType);
        detectedCommandEdit->setText(command);
        detectedArgsEdit->setText(args);
        detectedWorkDirEdit->setText(destDir);
        configGroup->setVisible(true);

        progressDisplay->append("🎉 <b>Installation complete!</b>");
        progressDisplay->append("");
        progressDisplay->append("<b>Detected Configuration:</b>");
        progressDisplay->append(QString("  Type: %1").arg(serverType));
        progressDisplay->append(QString("  Command: %1").arg(command));
        progressDisplay->append(QString("  Arguments: %1").arg(args));
        progressDisplay->append(QString("  Working Dir: %1").arg(destDir));
        progressDisplay->append("");
        progressDisplay->append("Click OK to add this server to the gateway.");

        buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
    });

    // Connect tab changes to enable/disable OK button
    connect(tabs, &QTabWidget::currentChanged, [&, buttons, detectedWorkDirEdit, detectedCommandEdit, detectedTypeEdit](int index) {
        if (index == 0) {
            // Install from Zip tab - OK only enabled after successful installation
            bool installComplete = !detectedWorkDirEdit->text().trimmed().isEmpty() &&
                                   !detectedCommandEdit->text().trimmed().isEmpty() &&
                                   !detectedTypeEdit->text().trimmed().isEmpty();
            buttons->button(QDialogButtonBox::Ok)->setEnabled(installComplete);
        } else {
            // Manual Configuration tab - OK always enabled
            buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
        }
    });

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Show dialog
    if (dialog.exec() == QDialog::Accepted) {
        if (tabs->currentIndex() == 0) {
            // Install from Zip tab
            // Check if installation is complete by verifying required fields are filled
            QString workingDir = detectedWorkDirEdit->text().trimmed();
            QString command = detectedCommandEdit->text().trimmed();
            QString type = detectedTypeEdit->text().trimmed();

            if (workingDir.isEmpty() || command.isEmpty() || type.isEmpty()) {
                QMessageBox::warning(this, "Not Ready",
                    "Please complete the installation first by clicking 'Extract and Install'.\n\n"
                    "Make sure you see '🎉 Installation complete!' before clicking OK.");
                return;
            }

            QString name = installNameEdit->text().trimmed();
            if (name.isEmpty()) {
                QMessageBox::warning(this, "Invalid Input", "Install name is required!");
                return;
            }

            // Build config from detected values
            QJsonObject config;
            config["name"] = name;
            config["type"] = detectedTypeEdit->text();
            config["command"] = detectedCommandEdit->text();

            QJsonArray argsArray;
            QString argsStr = detectedArgsEdit->text().trimmed();
            if (!argsStr.isEmpty()) {
                for (const QString& arg : argsStr.split(' ', Qt::SkipEmptyParts)) {
                    argsArray.append(arg);
                }
            }
            config["arguments"] = argsArray;

            config["port"] = detectedPortSpin->value();
            config["workingDir"] = detectedWorkDirEdit->text();
            config["env"] = QJsonObject();
            config["autostart"] = false;
            config["healthCheckInterval"] = 30000;
            config["description"] = QString("Auto-installed MCP server: %1").arg(name);

            // Add to manager
            if (m_manager->addServer(config)) {
                m_logDisplay->append(QString("<b style='color:green'>✅ Installed and added server: %1</b>").arg(name));
                QMessageBox::information(this, "Success",
                    QString("Server '%1' installed and added successfully!\n\n"
                            "Don't forget to save the configuration (File → Save Config)").arg(name));
            } else {
                QMessageBox::critical(this, "Error",
                    QString("Failed to add server '%1'.\n"
                            "A server with this name might already exist.").arg(name));
            }
        } else {
            // Manual Configuration tab
            QString name = nameEdit->text().trimmed();
            QString command = commandEdit->text().trimmed();

            if (name.isEmpty() || command.isEmpty()) {
                QMessageBox::warning(this, "Invalid Input",
                    "Server name and command are required!");
                return;
            }

            // Build server config
            QJsonObject config;
            config["name"] = name;
            config["type"] = typeCombo->currentData().toString();
            config["command"] = command;

            // Arguments
            QString argsStr = argsEdit->text().trimmed();
            if (!argsStr.isEmpty()) {
                QJsonArray argsArray;
                for (const QString& arg : argsStr.split(' ', Qt::SkipEmptyParts)) {
                    argsArray.append(arg);
                }
                config["arguments"] = argsArray;
            } else {
                config["arguments"] = QJsonArray();
            }

            config["port"] = portSpin->value();
            config["workingDir"] = workDirEdit->text().trimmed();
            config["env"] = QJsonObject();
            config["autostart"] = false;
            config["healthCheckInterval"] = 30000;
            config["description"] = descEdit->text().trimmed();

            // Add to manager
            if (m_manager->addServer(config)) {
                m_logDisplay->append(QString("<b style='color:green'>✅ Added server: %1</b>").arg(name));
                QMessageBox::information(this, "Success",
                    QString("Server '%1' added successfully!\n\n"
                            "Don't forget to save the configuration (File → Save Config)").arg(name));
            } else {
                QMessageBox::critical(this, "Error",
                    QString("Failed to add server '%1'.\n"
                            "A server with this name might already exist.").arg(name));
            }
        }
    }
}

void MainWindow::onRemoveServerClicked() {
    QString serverName = getSelectedServerName();
    if (serverName.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Remove Server",
        QString("Are you sure you want to remove server '%1'?").arg(serverName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_manager->removeServer(serverName);
        m_logDisplay->append(QString("<b>Removed server: %1</b>").arg(serverName));
    }
}

void MainWindow::onEditServerClicked() {
    QMessageBox::information(this, "Edit Server",
        "Edit server functionality coming soon!\n\n"
        "For now, edit the configs/servers.json file and reload.");
}

void MainWindow::onReloadConfigClicked() {
    QSettings settings;
    QString configPath = settings.value("configPath",
        "../configs/servers.json").toString();

    if (m_manager->loadConfig(configPath)) {
        m_logDisplay->append("<b>Configuration reloaded successfully</b>");
        QMessageBox::information(this, "Success", "Configuration reloaded successfully!");
    } else {
        m_logDisplay->append("<font color='red'><b>Failed to reload configuration</b></font>");
        QMessageBox::warning(this, "Error", "Failed to reload configuration file!");
    }
}

void MainWindow::onSaveConfigClicked() {
    QSettings settings;
    QString configPath = settings.value("configPath",
        "../configs/servers.json").toString();

    if (m_manager->saveConfig(configPath)) {
        m_logDisplay->append("<b>Configuration saved successfully</b>");
        QMessageBox::information(this, "Success", "Configuration saved successfully!");
    } else {
        m_logDisplay->append("<font color='red'><b>Failed to save configuration</b></font>");
        QMessageBox::warning(this, "Error", "Failed to save configuration file!");
    }
}

void MainWindow::updateStatusBar() {
    int total = m_manager->serverCount();
    int running = m_manager->runningCount();
    int stopped = m_manager->stoppedCount();

    QString status = QString("Servers: %1 total | %2 running | %3 stopped")
        .arg(total)
        .arg(running)
        .arg(stopped);

    m_statusLabel->setText(status);
}

QString MainWindow::getSelectedServerName() const {
    QList<QTableWidgetItem*> selected = m_serverTable->selectedItems();
    if (selected.isEmpty()) return QString();

    int row = selected.first()->row();
    return m_serverTable->item(row, 1)->text();
}

QString MainWindow::statusIcon(int status) const {
    switch (static_cast<MCPServerInstance::ServerStatus>(status)) {
        case MCPServerInstance::Running:
            return "●";
        case MCPServerInstance::Stopped:
            return "○";
        case MCPServerInstance::Starting:
            return "◐";
        case MCPServerInstance::Stopping:
            return "◑";
        case MCPServerInstance::Crashed:
            return "✗";
        case MCPServerInstance::Error:
            return "⚠";
        default:
            return "?";
    }
}

QString MainWindow::statusColor(int status) const {
    switch (static_cast<MCPServerInstance::ServerStatus>(status)) {
        case MCPServerInstance::Running:
            return "#00AA00";
        case MCPServerInstance::Stopped:
            return "#888888";
        case MCPServerInstance::Starting:
        case MCPServerInstance::Stopping:
            return "#FF8800";
        case MCPServerInstance::Crashed:
        case MCPServerInstance::Error:
            return "#CC0000";
        default:
            return "#000000";
    }
}

void MainWindow::loadSettings() {
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveSettings() {
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

// ===== Helper Methods for MCP Server Installation =====

QString MainWindow::extractZipFile(const QString& zipPath, const QString& destDir, QTextEdit* log) {
    // Create parent directory if needed (but NOT the destDir itself!)
    QFileInfo destInfo(destDir);
    QDir().mkpath(destInfo.absolutePath());

    // Use unzip command to extract
    QProcess process;
    process.setWorkingDirectory(QDir::currentPath());

    // Check if unzip is available
    QProcess checkUnzip;
    checkUnzip.start("which", QStringList() << "unzip");
    checkUnzip.waitForFinished();
    if (checkUnzip.exitCode() != 0) {
        return "unzip command not found. Please install unzip: sudo apt install unzip";
    }

    // Extract to a temporary directory first
    QString tempDir = destDir + "_temp";
    process.start("unzip", QStringList() << "-q" << zipPath << "-d" << tempDir);

    if (!process.waitForFinished(30000)) { // 30 second timeout
        QDir(tempDir).removeRecursively();
        return "Extraction timed out";
    }

    if (process.exitCode() != 0) {
        QString error = process.readAllStandardError();
        QDir(tempDir).removeRecursively();
        return QString("Unzip failed: %1").arg(error);
    }

    // Check if extraction created a single subdirectory (common with GitHub zips)
    QDir tempDirObj(tempDir);
    QStringList entries = tempDirObj.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    if (entries.size() == 1) {
        // Move contents of subdirectory to destDir
        QString subDir = tempDir + "/" + entries.first();
        if (log) {
            log->append(QString("   Found subdirectory: %1").arg(entries.first()));
        }

        // Rename the subdirectory to final destination
        if (!QDir().rename(subDir, destDir)) {
            QDir(tempDir).removeRecursively();
            return QString("Failed to move extracted directory");
        }

        // Remove temp directory
        QDir(tempDir).removeRecursively();
    } else {
        // Move entire temp directory to destination
        if (!QDir().rename(tempDir, destDir)) {
            QDir(tempDir).removeRecursively();
            return QString("Failed to move extracted directory");
        }
    }

    return QString(); // Success
}

QString MainWindow::detectServerType(const QString& dirPath, QTextEdit* log) {
    QDir dir(dirPath);

    // Check for Node.js indicators
    if (dir.exists("package.json")) {
        if (log) {
            log->append("   Found package.json → Node.js project");
        }
        return "node";
    }

    // Check for Python indicators
    if (dir.exists("pyproject.toml")) {
        if (log) {
            log->append("   Found pyproject.toml → Python project");
        }
        return "python";
    }

    if (dir.exists("requirements.txt")) {
        if (log) {
            log->append("   Found requirements.txt → Python project");
        }
        return "python";
    }

    if (dir.exists("setup.py")) {
        if (log) {
            log->append("   Found setup.py → Python project");
        }
        return "python";
    }

    return QString(); // Unknown type
}

QString MainWindow::installDependencies(const QString& dirPath, const QString& serverType, QTextEdit* log) {
    QProcess process;
    process.setWorkingDirectory(dirPath);

    if (serverType == "python") {
        // Check if we should use uv or pip
        QFile pyprojectFile(dirPath + "/pyproject.toml");
        bool useUv = false;

        if (pyprojectFile.exists() && pyprojectFile.open(QIODevice::ReadOnly)) {
            QString content = pyprojectFile.readAll();
            pyprojectFile.close();

            // Check if pyproject.toml mentions uv or has [tool.uv] section
            if (content.contains("uv") || content.contains("[tool.uv]")) {
                useUv = true;
            }
        }

        // Also check for uv.lock file
        if (QFile::exists(dirPath + "/uv.lock")) {
            useUv = true;
        }

        if (useUv) {
            // Use uv
            if (log) {
                log->append("   Installing with uv...");
            }

            // Check if uv is installed
            QProcess checkUv;
            checkUv.start("which", QStringList() << "uv");
            checkUv.waitForFinished();

            if (checkUv.exitCode() != 0) {
                return "uv not found. Install it with: pip install uv";
            }

            process.start("uv", QStringList() << "sync");
            if (!process.waitForFinished(120000)) { // 2 minute timeout
                return "uv sync timed out";
            }

            if (process.exitCode() != 0) {
                QString error = process.readAllStandardError();
                return QString("uv sync failed: %1").arg(error);
            }

            if (log) {
                QString output = process.readAllStandardOutput();
                if (!output.isEmpty()) {
                    log->append("   " + output.replace("\n", "\n   "));
                }
            }
        } else {
            // Use pip with venv
            if (log) {
                log->append("   Creating virtual environment...");
            }

            // Create venv
            process.start("python3", QStringList() << "-m" << "venv" << ".venv");
            if (!process.waitForFinished(60000)) {
                return "venv creation timed out";
            }

            if (process.exitCode() != 0) {
                QString error = process.readAllStandardError();
                return QString("venv creation failed: %1").arg(error);
            }

            // Install dependencies
            QString pipPath = dirPath + "/.venv/bin/pip";

            if (QFile::exists(dirPath + "/requirements.txt")) {
                if (log) {
                    log->append("   Installing from requirements.txt...");
                }

                process.start(pipPath, QStringList() << "install" << "-r" << "requirements.txt");
                if (!process.waitForFinished(180000)) { // 3 minute timeout
                    return "pip install timed out";
                }

                if (process.exitCode() != 0) {
                    QString error = process.readAllStandardError();
                    return QString("pip install failed: %1").arg(error);
                }
            } else if (QFile::exists(dirPath + "/setup.py")) {
                if (log) {
                    log->append("   Installing with pip install -e .");
                }

                process.start(pipPath, QStringList() << "install" << "-e" << ".");
                if (!process.waitForFinished(180000)) {
                    return "pip install timed out";
                }

                if (process.exitCode() != 0) {
                    QString error = process.readAllStandardError();
                    return QString("pip install failed: %1").arg(error);
                }
            }
        }
    } else if (serverType == "node") {
        if (log) {
            log->append("   Running npm install...");
        }

        process.start("npm", QStringList() << "install");
        if (!process.waitForFinished(180000)) { // 3 minute timeout
            return "npm install timed out";
        }

        if (process.exitCode() != 0) {
            QString error = process.readAllStandardError();
            return QString("npm install failed: %1").arg(error);
        }

        if (log) {
            QString output = process.readAllStandardOutput();
            if (!output.isEmpty()) {
                log->append("   " + output.split("\n").last());
            }
        }
    }

    return QString(); // Success
}

QString MainWindow::findEntryPoint(const QString& dirPath, const QString& serverType) {
    QDir dir(dirPath);

    if (serverType == "python") {
        // Common Python entry points
        QStringList candidates = {
            "main.py",
            "src/main.py",
            "__main__.py",
            "server.py",
            "src/server.py"
        };

        for (const QString& candidate : candidates) {
            if (dir.exists(candidate)) {
                return candidate;
            }
        }

        // Check pyproject.toml for entry point
        QFile pyproject(dirPath + "/pyproject.toml");
        if (pyproject.open(QIODevice::ReadOnly)) {
            QString content = pyproject.readAll();
            pyproject.close();

            // Look for [project.scripts] or [tool.poetry.scripts]
            QRegularExpression re("\\[.*scripts.*\\]([^\\[]+)");
            QRegularExpressionMatch match = re.match(content);
            if (match.hasMatch()) {
                QString scripts = match.captured(1);
                // Parse the first script entry
                QRegularExpression scriptRe("(\\w+)\\s*=\\s*['\"](.+?)['\"]");
                QRegularExpressionMatch scriptMatch = scriptRe.match(scripts);
                if (scriptMatch.hasMatch()) {
                    QString modulePath = scriptMatch.captured(2);
                    // Convert "package.module:function" to "-m package.module"
                    if (modulePath.contains(":")) {
                        modulePath = modulePath.split(":").first();
                    }
                    if (modulePath.contains(".")) {
                        return "-m " + modulePath;
                    }
                }
            }
        }

        return "main.py"; // Default fallback
    } else if (serverType == "node") {
        // Check package.json for main entry point
        QFile packageJson(dirPath + "/package.json");
        if (packageJson.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(packageJson.readAll());
            packageJson.close();

            if (doc.isObject()) {
                QJsonObject obj = doc.object();

                // Check "main" field
                if (obj.contains("main")) {
                    return obj["main"].toString();
                }

                // Check "bin" field
                if (obj.contains("bin")) {
                    QJsonValue bin = obj["bin"];
                    if (bin.isString()) {
                        return bin.toString();
                    } else if (bin.isObject()) {
                        // Get first bin entry
                        QJsonObject binObj = bin.toObject();
                        if (!binObj.isEmpty()) {
                            return binObj.begin().value().toString();
                        }
                    }
                }
            }
        }

        // Fallback to common entry points
        QStringList candidates = {
            "dist/index.js",
            "build/index.js",
            "index.js",
            "src/index.js"
        };

        for (const QString& candidate : candidates) {
            if (dir.exists(candidate)) {
                return candidate;
            }
        }

        return "index.js"; // Default fallback
    }

    return QString();
}

void MainWindow::onShowGatewayHelp() {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Gateway Python Client Example");
    dialog->resize(700, 550);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* infoLabel = new QLabel(
        "<b>How to Connect to MCP Gateway from Python</b><br><br>"
        "Use the following code to connect your application to the MCP Gateway "
        "and create sessions with credentials:"
    );
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    QTextEdit* exampleCode = new QTextEdit();
    exampleCode->setReadOnly(true);
    exampleCode->setFontFamily("Monospace");
    exampleCode->setPlainText(
        "import socket\n"
        "import json\n\n"
        "# Connect to gateway\n"
        "sock = socket.socket()\n"
        "sock.connect(('localhost', 8700))\n\n"
        "# Create session with credentials\n"
        "request = {\n"
        "    'jsonrpc': '2.0',\n"
        "    'id': '1',\n"
        "    'method': 'mcp-manager/create-session',\n"
        "    'params': {\n"
        "        'serverType': 'Confluence',\n"
        "        'credentials': {\n"
        "            'CONFLUENCE_API_TOKEN': 'your-token',\n"
        "            'CONFLUENCE_USERNAME': 'user@ns.nl'\n"
        "        }\n"
        "    }\n"
        "}\n"
        "sock.send(json.dumps(request).encode() + b'\\n')\n\n"
        "# Get session ID\n"
        "response = json.loads(sock.recv(4096).decode())\n"
        "session_id = response['result']['sessionId']\n\n"
        "# Call tools using session\n"
        "tool_request = {\n"
        "    'jsonrpc': '2.0',\n"
        "    'id': '2',\n"
        "    'method': 'tools/call',\n"
        "    'params': {\n"
        "        'sessionId': session_id,\n"
        "        'name': 'search_confluence',\n"
        "        'arguments': {'query': 'scrum'}\n"
        "    }\n"
        "}\n"
        "sock.send(json.dumps(tool_request).encode() + b'\\n')\n"
        "result = json.loads(sock.recv(4096).decode())\n\n"
        "# Cleanup\n"
        "destroy_request = {\n"
        "    'jsonrpc': '2.0',\n"
        "    'id': '3',\n"
        "    'method': 'mcp-manager/destroy-session',\n"
        "    'params': {'sessionId': session_id}\n"
        "}\n"
        "sock.send(json.dumps(destroy_request).encode() + b'\\n')\n"
        "sock.close()\n"
    );

    layout->addWidget(exampleCode);

    QLabel* noteLabel = new QLabel(
        "<i>💡 Tip: See <b>mcp_client/gateway_dashboard_client.py</b> for a complete client implementation.</i>"
    );
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog->exec();
}

void MainWindow::onShowTestingDocs() {
    // Show the testing documentation index
    // Path: mcp-manager/build -> mcp-manager -> chatns_summerschool (project root)
    QString docPath = QApplication::applicationDirPath() + "/../../DOCUMENTATION_INDEX.md";
    showMarkdownDialog("Testing Documentation Index", docPath,
        "<b>MCP Gateway Testing Framework - Documentation Index</b><br><br>"
        "This index helps you navigate through all available testing documentation.");
}

void MainWindow::onShowQuickReference() {
    // Show the quick reference cheat sheet
    QString docPath = QApplication::applicationDirPath() + "/../../QUICK_REFERENCE_TESTING.md";
    showMarkdownDialog("Quick Reference - Cheat Sheet", docPath,
        "<b>Quick Reference for Daily Use</b><br><br>"
        "A cheat sheet with the most common commands and troubleshooting tips.");
}

void MainWindow::onShowUserManual() {
    // Show the complete user manual
    QString docPath = QApplication::applicationDirPath() + "/../../USER_MANUAL_TESTING.md";
    showMarkdownDialog("User Manual - Complete Guide", docPath,
        "<b>Complete User Manual (60+ pages)</b><br><br>"
        "Comprehensive guide covering installation, testing, troubleshooting, and advanced usage.");
}

void MainWindow::onOpenDocsFolder() {
    // Open the documentation folder in system file browser
    // Path: mcp-manager/build -> mcp-manager -> chatns_summerschool (project root)
    QString docsPath = QApplication::applicationDirPath() + "/../..";
    QDir docsDir(docsPath);

    if (docsDir.exists()) {
        // Try to open in system file manager
        #ifdef Q_OS_LINUX
            QProcess::startDetached("xdg-open", QStringList() << docsDir.absolutePath());
        #elif defined(Q_OS_MAC)
            QProcess::startDetached("open", QStringList() << docsDir.absolutePath());
        #elif defined(Q_OS_WIN)
            QProcess::startDetached("explorer", QStringList() << docsDir.absolutePath());
        #endif

        statusBar()->showMessage("Opened documentation folder", 3000);
    } else {
        QMessageBox::warning(this, "Folder Not Found",
            "Documentation folder not found at: " + docsDir.absolutePath());
    }
}

void MainWindow::showMarkdownDialog(const QString& title, const QString& filePath, const QString& description) {
    QFile file(filePath);

    if (!file.exists()) {
        QMessageBox::warning(this, "File Not Found",
            QString("Documentation file not found:\n%1\n\n"
                    "Make sure the documentation files are in the project root directory.").arg(filePath));
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            QString("Could not open file:\n%1").arg(filePath));
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Create dialog
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(title);
    dialog->resize(900, 700);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    // Info label
    QLabel* infoLabel = new QLabel(description);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // File info
    QLabel* fileLabel = new QLabel(QString("<i>File: %1 | Size: %2 characters</i>")
        .arg(QFileInfo(filePath).fileName())
        .arg(content.size()));
    layout->addWidget(fileLabel);

    layout->addSpacing(10);

    // Text display with markdown-like formatting
    QTextEdit* textDisplay = new QTextEdit();
    textDisplay->setReadOnly(true);
    textDisplay->setFontFamily("Monospace");
    textDisplay->setPlainText(content);
    layout->addWidget(textDisplay);

    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    QPushButton* openFileButton = new QPushButton("📂 Open in External Editor");
    connect(openFileButton, &QPushButton::clicked, [filePath]() {
        #ifdef Q_OS_LINUX
            QProcess::startDetached("xdg-open", QStringList() << filePath);
        #elif defined(Q_OS_MAC)
            QProcess::startDetached("open", QStringList() << filePath);
        #elif defined(Q_OS_WIN)
            QProcess::startDetached("notepad", QStringList() << filePath);
        #endif
    });
    buttonLayout->addWidget(openFileButton);

    buttonLayout->addStretch();

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    buttonLayout->addWidget(buttons);

    layout->addLayout(buttonLayout);

    dialog->exec();
}

void MainWindow::updateGatewayStatus() {
    if (m_gateway && m_gatewaySessionLabel) {
        int sessionCount = m_gateway->activeSessionCount();
        m_gatewaySessionLabel->setText(QString("Active sessions: <b>%1</b>").arg(sessionCount));
    }
}

void MainWindow::onManageToolsClicked() {
    QString serverName = getSelectedServerName();
    if (serverName.isEmpty()) return;

    MCPServerInstance* server = m_manager->getServer(serverName);
    if (!server) return;

    // Create dialog
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(QString("Manage Tools - %1").arg(serverName));
    dialog->resize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    // Info label
    QLabel* infoLabel = new QLabel(QString(
        "<b>Tools for %1</b><br>"
        "Enable/disable individual MCP tools. Disabled tools will be blocked by the gateway."
    ).arg(serverName));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { padding: 10px; background-color: #f0f0f0; border-radius: 5px; }");
    layout->addWidget(infoLabel);

    // Refresh button
    QPushButton* refreshButton = new QPushButton("🔄 Refresh Tools from Server");
    refreshButton->setEnabled(server->isRunning());
    connect(refreshButton, &QPushButton::clicked, [server]() {
        server->refreshTools();
    });
    layout->addWidget(refreshButton);

    // Tools list with checkboxes
    QScrollArea* scrollArea = new QScrollArea();
    QWidget* scrollWidget = new QWidget();
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollWidget);

    QList<MCPServerInstance::ToolInfo> tools = server->availableTools();

    if (tools.isEmpty()) {
        QLabel* emptyLabel = new QLabel(
            "⚠️ No tools available yet.\n\n"
            "If the server is running, click 'Refresh Tools' to query available tools.\n"
            "Otherwise, start the server first."
        );
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("QLabel { color: #666; padding: 20px; }");
        scrollLayout->addWidget(emptyLabel);
    } else {
        for (const MCPServerInstance::ToolInfo& tool : tools) {
            QCheckBox* checkbox = new QCheckBox(tool.name);
            checkbox->setChecked(tool.enabled);
            checkbox->setToolTip(tool.description);

            // Connect checkbox to toggle tool
            connect(checkbox, &QCheckBox::toggled, [server, toolName = tool.name](bool checked) {
                server->setToolEnabled(toolName, checked);
            });

            QLabel* descLabel = new QLabel(tool.description);
            descLabel->setWordWrap(true);
            descLabel->setStyleSheet("QLabel { color: #666; margin-left: 25px; }");

            scrollLayout->addWidget(checkbox);
            scrollLayout->addWidget(descLabel);
            scrollLayout->addSpacing(10);
        }
    }

    scrollLayout->addStretch();
    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    layout->addWidget(scrollArea);

    // Close button
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog->exec();
}

void MainWindow::updateToolsServerList() {
    if (!m_toolsServerList) return;

    // Update each item in the tools server list
    for (int i = 0; i < m_toolsServerList->count(); ++i) {
        QListWidgetItem* item = m_toolsServerList->item(i);
        QString serverName = item->text();

        MCPServerInstance* server = m_manager->getServer(serverName);
        if (server) {
            if (server->isRunning()) {
                item->setIcon(QIcon::fromTheme("dialog-ok"));
                item->setForeground(QColor(0, 128, 0));
            } else {
                item->setIcon(QIcon::fromTheme("dialog-cancel"));
                item->setForeground(QColor(128, 128, 128));
            }
        }
    }
}
