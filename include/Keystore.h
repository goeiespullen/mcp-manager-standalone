#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QFile>

/**
 * @brief Encrypted credential storage for MCP Manager
 *
 * Provides secure, encrypted storage for sensitive credentials using Fernet encryption.
 *
 * Features:
 * - Fernet encryption (AES-128-CBC + HMAC-SHA256)
 * - JSON-based storage format
 * - Service-based credential organization
 * - Compatible with Python dashboard keystore
 *
 * Storage format:
 * - Master key: .keystore.key (32 bytes: 16 for HMAC signing + 16 for AES encryption)
 * - Data file: .keystore (Fernet-encrypted JSON)
 */
class Keystore : public QObject {
    Q_OBJECT

public:
    explicit Keystore(QObject* parent = nullptr);
    explicit Keystore(const QString& keystorePath, QObject* parent = nullptr);
    ~Keystore();

    /**
     * @brief Store a credential securely
     * @param service Service name (e.g., "azure", "teamcentraal")
     * @param key Credential key (e.g., "pat", "username", "password")
     * @param value Credential value
     * @return true if successful
     */
    bool setCredential(const QString& service, const QString& key, const QString& value);

    /**
     * @brief Retrieve a credential
     * @param service Service name
     * @param key Credential key
     * @param defaultValue Default value if not found
     * @return Credential value or default
     */
    QString getCredential(const QString& service, const QString& key, const QString& defaultValue = QString());

    /**
     * @brief Delete a specific credential
     * @param service Service name
     * @param key Credential key
     * @return true if deleted, false if not found
     */
    bool deleteCredential(const QString& service, const QString& key);

    /**
     * @brief Get all credentials for a service
     * @param service Service name
     * @return Map of key-value pairs
     */
    QMap<QString, QString> getServiceCredentials(const QString& service);

    /**
     * @brief Delete all credentials for a service
     * @param service Service name
     * @return true if service existed and was cleared
     */
    bool clearService(const QString& service);

    /**
     * @brief List all services with stored credentials
     * @return List of service names
     */
    QStringList listServices();

    /**
     * @brief List all credential keys for a service
     * @param service Service name
     * @return List of credential keys
     */
    QStringList listCredentials(const QString& service);

    // ========== Per-User Credential Methods ==========

    /**
     * @brief Store a user-specific credential securely
     * @param userId User identifier (e.g., email address)
     * @param service Service name (e.g., "azure", "teamcentraal")
     * @param key Credential key (e.g., "pat", "username")
     * @param value Credential value
     * @return true if successful
     */
    bool setUserCredential(const QString& userId, const QString& service, const QString& key, const QString& value);

    /**
     * @brief Retrieve a user-specific credential with fallback to shared
     *
     * Lookup priority:
     * 1. User-specific credential (users[userId][service][key])
     * 2. Shared credential (shared[service][key])
     * 3. Legacy flat credential (service[key]) - backward compatibility
     *
     * @param userId User identifier
     * @param service Service name
     * @param key Credential key
     * @param defaultValue Default value if not found
     * @return Credential value or default
     */
    QString getUserCredential(const QString& userId, const QString& service, const QString& key, const QString& defaultValue = QString());

    /**
     * @brief Delete a user-specific credential
     * @param userId User identifier
     * @param service Service name
     * @param key Credential key
     * @return true if deleted, false if not found
     */
    bool deleteUserCredential(const QString& userId, const QString& service, const QString& key);

    /**
     * @brief Get all credentials for a user's service
     * @param userId User identifier
     * @param service Service name
     * @return Map of key-value pairs
     */
    QMap<QString, QString> getUserServiceCredentials(const QString& userId, const QString& service);

    /**
     * @brief Delete all credentials for a user's service
     * @param userId User identifier
     * @param service Service name
     * @return true if service existed and was cleared
     */
    bool clearUserService(const QString& userId, const QString& service);

    /**
     * @brief List all users with stored credentials
     * @return List of user IDs
     */
    QStringList listUsers();

    /**
     * @brief List all services with credentials for a specific user
     * @param userId User identifier
     * @return List of service names
     */
    QStringList listUserServices(const QString& userId);

    /**
     * @brief Migrate existing flat/shared credentials to user-specific
     * @param userId User to assign existing credentials to
     * @return Number of credentials migrated
     */
    int migrateToUser(const QString& userId);

    // ========== Permission Management Methods ==========

    /**
     * @brief Set allowed tools (permissions) for a user's service
     * @param userId User identifier
     * @param service Service name (e.g., "azure", "confluence")
     * @param permissions List of allowed tool names
     * @return true if successful
     */
    bool setUserPermissions(const QString& userId, const QString& service, const QStringList& permissions);

    /**
     * @brief Get allowed tools (permissions) for a user's service
     * @param userId User identifier
     * @param service Service name
     * @return List of allowed tool names (empty list = all tools allowed)
     */
    QStringList getUserPermissions(const QString& userId, const QString& service);

    /**
     * @brief Check if user has permission for a specific tool
     * @param userId User identifier
     * @param service Service name
     * @param toolName Tool name to check
     * @return true if user has permission (or if no restrictions set)
     */
    bool hasUserPermission(const QString& userId, const QString& service, const QString& toolName);

signals:
    void credentialChanged(const QString& service, const QString& key);
    void userCredentialChanged(const QString& userId, const QString& service, const QString& key);
    void serviceCleared(const QString& service);
    void error(const QString& message);

private:
    void initialize();
    bool loadMasterKey();
    bool generateMasterKey();
    QJsonObject loadEncryptedData();
    bool saveEncryptedData(const QJsonObject& data);

    QByteArray encrypt(const QByteArray& plaintext);
    QByteArray decrypt(const QByteArray& ciphertext);

    QString m_keystorePath;
    QString m_masterKeyPath;
    QByteArray m_aesKey;  // 32 bytes: 16 for HMAC signing, 16 for AES encryption
    bool m_initialized;
};
