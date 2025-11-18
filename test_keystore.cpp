#include "include/Keystore.h"
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    qDebug() << "=== Fernet Keystore Compatibility Test ===";

    // Use the dashboard keystore location
    QString keystorePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        + "/MEGA/development/chatns/chatns_summerschool/dashapp/.keystore";

    qDebug() << "Testing keystore at:" << keystorePath;

    // Create keystore instance
    Keystore keystore(keystorePath);

    // List all services
    QStringList services = keystore.listServices();
    qDebug() << "\n📋 Services in keystore:" << services;

    // For each service, list credentials
    for (const QString& service : services) {
        qDebug() << "\n🔑 Service:" << service;
        QStringList keys = keystore.listCredentials(service);
        qDebug() << "  Keys:" << keys;

        // Get all credentials
        QMap<QString, QString> creds = keystore.getServiceCredentials(service);
        for (auto it = creds.constBegin(); it != creds.constEnd(); ++it) {
            QString value = it.value();
            QString masked = value.left(8) + "..." + value.right(4);
            qDebug() << "  " << it.key() << ":" << masked;
        }
    }

    qDebug() << "\n✅ Test completed successfully!";

    return 0;
}
