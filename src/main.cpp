#include "MainWindow.h"
#include "MCPServerManager.h"
#include "Logger.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Initialize logging system FIRST
    qInstallMessageHandler(Logger::messageHandler);

    // Set application metadata
    app.setApplicationName("MCP Server Manager");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("MCPManager");
    app.setOrganizationDomain("mcp-manager.local");

    // Command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription("Multi-MCP Server Manager - Manage multiple Model Context Protocol servers");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOption(QStringList() << "c" << "config",
        "Path to configuration file (default: configs/servers.json)",
        "config",
        "configs/servers.json");
    parser.addOption(configOption);

    QCommandLineOption autoStartOption(QStringList() << "a" << "autostart",
        "Auto-start servers marked with autostart=true");
    parser.addOption(autoStartOption);

    QCommandLineOption headlessOption(QStringList() << "headless",
        "Run in headless mode (no GUI) - COMING SOON");
    parser.addOption(headlessOption);

    // Parse command line
    parser.process(app);

    // Get config path
    QString configPath = parser.value(configOption);
    bool autoStart = parser.isSet(autoStartOption);
    bool headless = parser.isSet(headlessOption);

    // Find config file (try relative paths)
    QFileInfo configInfo(configPath);
    if (!configInfo.exists()) {
        // Try relative to executable
        QString exeDir = QCoreApplication::applicationDirPath();
        QString altPath = exeDir + "/../" + configPath;
        QFileInfo altInfo(altPath);
        if (altInfo.exists()) {
            configPath = altInfo.absoluteFilePath();
        } else {
            qWarning() << "Config file not found:" << configPath;
            qWarning() << "Also tried:" << altPath;
            qWarning() << "Creating default config...";

            // Use the specified path anyway, will create default
            configPath = configInfo.absoluteFilePath();
        }
    } else {
        configPath = configInfo.absoluteFilePath();
    }

    qDebug() << "Using config file:" << configPath;

    // Headless mode not yet implemented
    if (headless) {
        qWarning() << "Headless mode is not yet implemented!";
        qWarning() << "Starting with GUI instead...";
    }

    // Create MCP Server Manager
    MCPServerManager* manager = new MCPServerManager(&app);

    // Load configuration
    if (!manager->loadConfig(configPath)) {
        qWarning() << "Failed to load config, starting with empty manager";
        qWarning() << "You can add servers manually or reload config from File menu";
    }

    // Auto-start servers if requested
    if (autoStart) {
        qDebug() << "Auto-starting configured servers...";
        manager->startAutoStartServers();
    }

    // Create and show main window
    MainWindow* window = new MainWindow(manager);
    window->show();

    qDebug() << "MCP Server Manager started successfully";
    qDebug() << "Loaded" << manager->serverCount() << "servers";
    qInfo() << "Logs directory:" << Logger::instance()->logDirectory();

    int result = app.exec();

    // Log shutdown
    qInfo() << "MCP Server Manager shutting down";
    return result;
}
