#include "Keystore.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>
#include <QRandomGenerator>
#include <QDateTime>
#include <QtEndian>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>

Keystore::Keystore(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
{
    // Priority 1: Central shared keystore (preferred)
    QString centralPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        + "/MEGA/development/chatns";
    QString centralKeystore = centralPath + "/.keystore";
    QString centralKey = centralPath + "/.keystore.key";

    // Priority 2: Legacy dashboard keystore (fallback)
    QString dashboardPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        + "/MEGA/development/chatns/chatns_summerschool/dashapp";
    QString dashboardKeystore = dashboardPath + "/.keystore";
    QString dashboardKey = dashboardPath + "/.keystore.key";

    // Use central keystore if it exists, otherwise fall back to dashboard keystore
    if (QFile::exists(centralKeystore) && QFile::exists(centralKey)) {
        m_keystorePath = centralKeystore;
        m_masterKeyPath = centralKey;
        qDebug() << "Using central keystore:" << m_keystorePath;
    } else {
        m_keystorePath = dashboardKeystore;
        m_masterKeyPath = dashboardKey;
        qDebug() << "Using dashboard keystore:" << m_keystorePath;
    }

    initialize();
}

Keystore::Keystore(const QString& keystorePath, QObject* parent)
    : QObject(parent)
    , m_keystorePath(keystorePath)
    , m_initialized(false)
{
    QFileInfo fileInfo(keystorePath);
    m_masterKeyPath = fileInfo.absolutePath() + "/keystore.key";
    initialize();
}

Keystore::~Keystore() {
    // Clear sensitive data from memory
    m_aesKey.fill(0);
}

void Keystore::initialize() {
    // Ensure directory exists
    QFileInfo fileInfo(m_keystorePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Load or generate master key
    if (QFile::exists(m_masterKeyPath)) {
        m_initialized = loadMasterKey();
    } else {
        m_initialized = generateMasterKey();
    }

    if (!m_initialized) {
        qWarning() << "Failed to initialize keystore";
        emit error("Failed to initialize keystore encryption");
    }
}

bool Keystore::loadMasterKey() {
    QFile file(m_masterKeyPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open master key file:" << file.errorString();
        return false;
    }

    QByteArray encodedKey = file.readAll();
    file.close();

    // Python Fernet stores the key as base64url encoded (44 bytes)
    // Decode it to get the raw 32-byte key
    m_aesKey = QByteArray::fromBase64(encodedKey, QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    if (m_aesKey.size() != 32) {
        qWarning() << "Invalid master key size after decoding:" << m_aesKey.size() << "expected 32";
        return false;
    }

    qDebug() << "Loaded existing Fernet master key";
    return true;
}

bool Keystore::generateMasterKey() {
    // Generate random 256-bit key for Fernet
    // First 16 bytes for HMAC signing, last 16 bytes for AES-128-CBC encryption
    m_aesKey.resize(32);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(m_aesKey.data()), 32) != 1) {
        qWarning() << "Failed to generate random Fernet key";
        return false;
    }

    // Base64url encode the key for Python Fernet compatibility
    QByteArray encodedKey = m_aesKey.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    // Save to file with secure permissions
    QFile file(m_masterKeyPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to create master key file:" << file.errorString();
        return false;
    }

    file.write(encodedKey);
    file.close();

    // Set restrictive permissions (owner read/write only)
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

    qDebug() << "Generated new Fernet-compatible master key (base64url encoded)";
    return true;
}

QByteArray Keystore::encrypt(const QByteArray& plaintext) {
    // Fernet token format:
    // Version (1) | Timestamp (8) | IV (16) | Ciphertext (var) | HMAC (32)

    // Split the key: first 16 bytes for signing, last 16 bytes for encryption
    QByteArray signingKey = m_aesKey.left(16);
    QByteArray encryptionKey = m_aesKey.mid(16, 16);

    // Generate random IV
    QByteArray iv(16, 0);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), 16) != 1) {
        qWarning() << "Failed to generate IV";
        return QByteArray();
    }

    // Get current timestamp (seconds since epoch, big-endian 64-bit)
    qint64 timestamp = QDateTime::currentSecsSinceEpoch();
    QByteArray timestampBytes(8, 0);
    qToBigEndian(timestamp, timestampBytes.data());

    // Encrypt with AES-128-CBC
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qWarning() << "Failed to create cipher context";
        return QByteArray();
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(encryptionKey.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        qWarning() << "Failed to initialize encryption";
        return QByteArray();
    }

    QByteArray ciphertext;
    ciphertext.resize(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_128_cbc()));

    int len = 0;
    int ciphertext_len = 0;

    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.constData()),
                          plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        qWarning() << "Encryption update failed";
        return QByteArray();
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        qWarning() << "Encryption finalization failed";
        return QByteArray();
    }
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);

    EVP_CIPHER_CTX_free(ctx);

    // Build Fernet token: Version | Timestamp | IV | Ciphertext
    QByteArray token;
    token.append(static_cast<char>(0x80));  // Version
    token.append(timestampBytes);            // Timestamp
    token.append(iv);                        // IV
    token.append(ciphertext);                // Ciphertext

    // Compute HMAC-SHA256 over the token
    unsigned char hmac[32];
    unsigned int hmac_len = 32;

    if (!HMAC(EVP_sha256(),
              reinterpret_cast<const unsigned char*>(signingKey.constData()), 16,
              reinterpret_cast<const unsigned char*>(token.constData()), token.size(),
              hmac, &hmac_len)) {
        qWarning() << "HMAC computation failed";
        return QByteArray();
    }

    // Append HMAC to token
    token.append(reinterpret_cast<const char*>(hmac), 32);

    // Base64url encode
    QByteArray encoded = token.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    return encoded;
}

QByteArray Keystore::decrypt(const QByteArray& encoded) {
    // Fernet token format (after base64url decode):
    // Version (1) | Timestamp (8) | IV (16) | Ciphertext (var) | HMAC (32)

    // Base64url decode
    QByteArray token = QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    // Minimum token size: 1 (version) + 8 (timestamp) + 16 (IV) + 16 (min ciphertext block) + 32 (HMAC) = 73
    if (token.size() < 73) {
        qWarning() << "Invalid Fernet token size:" << token.size();
        return QByteArray();
    }

    // Split the key: first 16 bytes for signing, last 16 bytes for encryption
    QByteArray signingKey = m_aesKey.left(16);
    QByteArray encryptionKey = m_aesKey.mid(16, 16);

    // Extract components
    unsigned char version = static_cast<unsigned char>(token[0]);
    if (version != 0x80) {
        qWarning() << "Invalid Fernet version:" << version;
        return QByteArray();
    }

    // Extract HMAC (last 32 bytes)
    QByteArray receivedHmac = token.right(32);
    QByteArray tokenWithoutHmac = token.left(token.size() - 32);

    // Verify HMAC
    unsigned char computedHmac[32];
    unsigned int hmac_len = 32;

    if (!HMAC(EVP_sha256(),
              reinterpret_cast<const unsigned char*>(signingKey.constData()), 16,
              reinterpret_cast<const unsigned char*>(tokenWithoutHmac.constData()), tokenWithoutHmac.size(),
              computedHmac, &hmac_len)) {
        qWarning() << "HMAC computation failed";
        return QByteArray();
    }

    // Compare HMACs (constant time comparison)
    if (memcmp(receivedHmac.constData(), computedHmac, 32) != 0) {
        qWarning() << "HMAC verification failed - data may be corrupted or tampered";
        return QByteArray();
    }

    // Extract timestamp (bytes 1-8)
    QByteArray timestampBytes = token.mid(1, 8);
    qint64 timestamp = qFromBigEndian<qint64>(reinterpret_cast<const unsigned char*>(timestampBytes.constData()));

    // Optional: Check timestamp age (Fernet spec recommends TTL check)
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    if (currentTime < timestamp) {
        qWarning() << "Token timestamp is in the future";
        // Don't fail here, just warn
    }

    // Extract IV (bytes 9-24)
    QByteArray iv = token.mid(9, 16);

    // Extract ciphertext (bytes 25 to end-32)
    QByteArray ciphertext = token.mid(25, token.size() - 25 - 32);

    // Decrypt with AES-128-CBC
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qWarning() << "Failed to create cipher context";
        return QByteArray();
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(encryptionKey.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        qWarning() << "Failed to initialize decryption";
        return QByteArray();
    }

    QByteArray plaintext;
    plaintext.resize(ciphertext.size());

    int len = 0;
    int plaintext_len = 0;

    if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &len,
                          reinterpret_cast<const unsigned char*>(ciphertext.constData()),
                          ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        qWarning() << "Decryption update failed";
        return QByteArray();
    }
    plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        qWarning() << "Decryption finalization failed";
        return QByteArray();
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    plaintext.resize(plaintext_len);
    return plaintext;
}

QJsonObject Keystore::loadEncryptedData() {
    if (!QFile::exists(m_keystorePath)) {
        return QJsonObject();
    }

    QFile file(m_keystorePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open keystore file:" << file.errorString();
        emit error("Failed to open keystore file");
        return QJsonObject();
    }

    QByteArray encryptedData = file.readAll();
    file.close();

    if (encryptedData.isEmpty()) {
        return QJsonObject();
    }

    QByteArray decryptedData = decrypt(encryptedData);
    if (decryptedData.isEmpty()) {
        qWarning() << "Failed to decrypt keystore data";
        emit error("Failed to decrypt keystore data");
        return QJsonObject();
    }

    QJsonDocument doc = QJsonDocument::fromJson(decryptedData);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON in keystore";
        emit error("Invalid keystore format");
        return QJsonObject();
    }

    return doc.object();
}

bool Keystore::saveEncryptedData(const QJsonObject& data) {
    QJsonDocument doc(data);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    QByteArray encryptedData = encrypt(jsonData);
    if (encryptedData.isEmpty()) {
        qWarning() << "Failed to encrypt keystore data";
        emit error("Failed to encrypt keystore data");
        return false;
    }

    QFile file(m_keystorePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to write keystore file:" << file.errorString();
        emit error("Failed to write keystore file");
        return false;
    }

    file.write(encryptedData);
    file.close();

    // Set restrictive permissions
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);

    return true;
}

bool Keystore::setCredential(const QString& service, const QString& key, const QString& value) {
    if (!m_initialized) {
        emit error("Keystore not initialized");
        return false;
    }

    QJsonObject data = loadEncryptedData();

    // Create service object if it doesn't exist
    QJsonObject serviceObj;
    if (data.contains(service)) {
        serviceObj = data[service].toObject();
    }

    serviceObj[key] = value;
    data[service] = serviceObj;

    if (saveEncryptedData(data)) {
        qDebug() << "Stored credential:" << service << "." << key;
        emit credentialChanged(service, key);
        return true;
    }

    return false;
}

QString Keystore::getCredential(const QString& service, const QString& key, const QString& defaultValue) {
    if (!m_initialized) {
        return defaultValue;
    }

    QJsonObject data = loadEncryptedData();

    if (data.contains(service)) {
        QJsonObject serviceObj = data[service].toObject();
        if (serviceObj.contains(key)) {
            return serviceObj[key].toString();
        }
    }

    return defaultValue;
}

bool Keystore::deleteCredential(const QString& service, const QString& key) {
    if (!m_initialized) {
        emit error("Keystore not initialized");
        return false;
    }

    QJsonObject data = loadEncryptedData();

    if (data.contains(service)) {
        QJsonObject serviceObj = data[service].toObject();
        if (serviceObj.contains(key)) {
            serviceObj.remove(key);

            // Remove service if empty
            if (serviceObj.isEmpty()) {
                data.remove(service);
            } else {
                data[service] = serviceObj;
            }

            if (saveEncryptedData(data)) {
                qDebug() << "Deleted credential:" << service << "." << key;
                emit credentialChanged(service, key);
                return true;
            }
        }
    }

    return false;
}

QMap<QString, QString> Keystore::getServiceCredentials(const QString& service) {
    QMap<QString, QString> result;

    if (!m_initialized) {
        return result;
    }

    QJsonObject data = loadEncryptedData();

    if (data.contains(service)) {
        QJsonObject serviceObj = data[service].toObject();
        for (auto it = serviceObj.constBegin(); it != serviceObj.constEnd(); ++it) {
            result[it.key()] = it.value().toString();
        }
    }

    return result;
}

bool Keystore::clearService(const QString& service) {
    if (!m_initialized) {
        emit error("Keystore not initialized");
        return false;
    }

    QJsonObject data = loadEncryptedData();

    if (data.contains(service)) {
        data.remove(service);

        if (saveEncryptedData(data)) {
            qDebug() << "Cleared service:" << service;
            emit serviceCleared(service);
            return true;
        }
    }

    return false;
}

QStringList Keystore::listServices() {
    QStringList services;

    if (!m_initialized) {
        return services;
    }

    QJsonObject data = loadEncryptedData();
    return data.keys();
}

QStringList Keystore::listCredentials(const QString& service) {
    QStringList credentials;

    if (!m_initialized) {
        return credentials;
    }

    QJsonObject data = loadEncryptedData();

    if (data.contains(service)) {
        QJsonObject serviceObj = data[service].toObject();
        return serviceObj.keys();
    }

    return credentials;
}

// ========== Per-User Credential Methods Implementation ==========

bool Keystore::setUserCredential(const QString& userId, const QString& service, const QString& key, const QString& value) {
    if (!m_initialized) {
        qWarning() << "Keystore not initialized";
        return false;
    }

    QJsonObject data = loadEncryptedData();

    // Ensure "users" object exists
    if (!data.contains("users")) {
        data["users"] = QJsonObject();
    }

    QJsonObject users = data["users"].toObject();

    // Ensure user object exists
    if (!users.contains(userId)) {
        users[userId] = QJsonObject();
    }

    QJsonObject user = users[userId].toObject();

    // Ensure service object exists for user
    if (!user.contains(service)) {
        user[service] = QJsonObject();
    }

    QJsonObject serviceObj = user[service].toObject();
    serviceObj[key] = value;
    user[service] = serviceObj;
    users[userId] = user;
    data["users"] = users;

    if (!saveEncryptedData(data)) {
        return false;
    }

    emit userCredentialChanged(userId, service, key);
    qDebug() << "Set user credential:" << userId << service << key;
    return true;
}

QString Keystore::getUserCredential(const QString& userId, const QString& service, const QString& key, const QString& defaultValue) {
    if (!m_initialized) {
        qWarning() << "Keystore not initialized";
        return defaultValue;
    }

    QJsonObject data = loadEncryptedData();

    // Priority 1: User-specific credential (users[userId][service][key])
    if (data.contains("users")) {
        QJsonObject users = data["users"].toObject();
        if (users.contains(userId)) {
            QJsonObject user = users[userId].toObject();
            if (user.contains(service)) {
                QJsonObject serviceObj = user[service].toObject();
                if (serviceObj.contains(key)) {
                    return serviceObj[key].toString();
                }
            }
        }
    }

    // Priority 2: Shared credential (shared[service][key])
    if (data.contains("shared")) {
        QJsonObject shared = data["shared"].toObject();
        if (shared.contains(service)) {
            QJsonObject serviceObj = shared[service].toObject();
            if (serviceObj.contains(key)) {
                qDebug() << "Using shared credential for" << userId << service << key;
                return serviceObj[key].toString();
            }
        }
    }

    // Priority 3: Legacy flat credential (service[key]) - backward compatibility
    if (data.contains(service)) {
        QJsonObject serviceObj = data[service].toObject();
        if (serviceObj.contains(key)) {
            qDebug() << "Using legacy credential for" << userId << service << key;
            return serviceObj[key].toString();
        }
    }

    return defaultValue;
}

bool Keystore::deleteUserCredential(const QString& userId, const QString& service, const QString& key) {
    if (!m_initialized) {
        qWarning() << "Keystore not initialized";
        return false;
    }

    QJsonObject data = loadEncryptedData();

    if (!data.contains("users")) {
        return false;
    }

    QJsonObject users = data["users"].toObject();
    if (!users.contains(userId)) {
        return false;
    }

    QJsonObject user = users[userId].toObject();
    if (!user.contains(service)) {
        return false;
    }

    QJsonObject serviceObj = user[service].toObject();
    if (!serviceObj.contains(key)) {
        return false;
    }

    serviceObj.remove(key);

    // Clean up empty objects
    if (serviceObj.isEmpty()) {
        user.remove(service);
    } else {
        user[service] = serviceObj;
    }

    if (user.isEmpty()) {
        users.remove(userId);
    } else {
        users[userId] = user;
    }

    if (users.isEmpty()) {
        data.remove("users");
    } else {
        data["users"] = users;
    }

    if (!saveEncryptedData(data)) {
        return false;
    }

    emit userCredentialChanged(userId, service, key);
    qDebug() << "Deleted user credential:" << userId << service << key;
    return true;
}

QMap<QString, QString> Keystore::getUserServiceCredentials(const QString& userId, const QString& service) {
    QMap<QString, QString> credentials;

    if (!m_initialized) {
        return credentials;
    }

    QJsonObject data = loadEncryptedData();

    // Get user-specific credentials
    if (data.contains("users")) {
        QJsonObject users = data["users"].toObject();
        if (users.contains(userId)) {
            QJsonObject user = users[userId].toObject();
            if (user.contains(service)) {
                QJsonObject serviceObj = user[service].toObject();
                for (const QString& key : serviceObj.keys()) {
                    credentials[key] = serviceObj[key].toString();
                }
            }
        }
    }

    return credentials;
}

bool Keystore::clearUserService(const QString& userId, const QString& service) {
    if (!m_initialized) {
        qWarning() << "Keystore not initialized";
        return false;
    }

    QJsonObject data = loadEncryptedData();

    if (!data.contains("users")) {
        return false;
    }

    QJsonObject users = data["users"].toObject();
    if (!users.contains(userId)) {
        return false;
    }

    QJsonObject user = users[userId].toObject();
    if (!user.contains(service)) {
        return false;
    }

    user.remove(service);

    if (user.isEmpty()) {
        users.remove(userId);
    } else {
        users[userId] = user;
    }

    if (users.isEmpty()) {
        data.remove("users");
    } else {
        data["users"] = users;
    }

    if (!saveEncryptedData(data)) {
        return false;
    }

    qDebug() << "Cleared user service:" << userId << service;
    return true;
}

QStringList Keystore::listUsers() {
    QStringList users;

    if (!m_initialized) {
        return users;
    }

    QJsonObject data = loadEncryptedData();

    if (data.contains("users")) {
        QJsonObject usersObj = data["users"].toObject();
        users = usersObj.keys();
    }

    return users;
}

QStringList Keystore::listUserServices(const QString& userId) {
    QStringList services;

    if (!m_initialized) {
        return services;
    }

    QJsonObject data = loadEncryptedData();

    if (data.contains("users")) {
        QJsonObject users = data["users"].toObject();
        if (users.contains(userId)) {
            QJsonObject user = users[userId].toObject();
            services = user.keys();
        }
    }

    return services;
}

int Keystore::migrateToUser(const QString& userId) {
    if (!m_initialized) {
        qWarning() << "Keystore not initialized";
        return 0;
    }

    QJsonObject data = loadEncryptedData();
    int migratedCount = 0;

    // Migrate flat/legacy credentials to user-specific
    QJsonObject shared;
    QStringList keysToRemove;

    for (const QString& key : data.keys()) {
        // Skip special keys
        if (key == "users" || key == "shared" || key == "version") {
            continue;
        }

        // This is a service at root level - legacy format
        QJsonObject serviceObj = data[key].toObject();
        if (!serviceObj.isEmpty()) {
            shared[key] = serviceObj;
            keysToRemove.append(key);
            migratedCount++;
        }
    }

    if (migratedCount > 0) {
        // Remove old flat keys
        for (const QString& key : keysToRemove) {
            data.remove(key);
        }

        // Move to shared or user-specific based on userId
        if (!userId.isEmpty()) {
            // Migrate to specific user
            QJsonObject users = data.value("users").toObject();
            QJsonObject user = users.value(userId).toObject();

            // Merge shared into user (user-specific takes precedence)
            for (const QString& service : shared.keys()) {
                if (!user.contains(service)) {
                    user[service] = shared[service];
                }
            }

            users[userId] = user;
            data["users"] = users;
            qDebug() << "Migrated" << migratedCount << "services to user" << userId;
        } else {
            // Move to shared section
            data["shared"] = shared;
            qDebug() << "Migrated" << migratedCount << "services to shared";
        }

        data["version"] = "2.0";
        saveEncryptedData(data);
    }

    return migratedCount;
}
