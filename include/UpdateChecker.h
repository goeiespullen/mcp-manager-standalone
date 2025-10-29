#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    struct Version {
        int major;
        int minor;
        int patch;
        QString string;

        Version() : major(0), minor(0), patch(0) {}
        Version(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {
            string = QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
        }

        bool operator>(const Version& other) const {
            if (major != other.major) return major > other.major;
            if (minor != other.minor) return minor > other.minor;
            return patch > other.patch;
        }

        bool operator==(const Version& other) const {
            return major == other.major && minor == other.minor && patch == other.patch;
        }
    };

    struct ReleaseInfo {
        Version version;
        QString tagName;
        QString name;
        QString url;
        QString downloadUrl;
        QString releaseNotes;
        QDateTime publishedAt;
    };

    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker();

    void checkForUpdates();
    Version getCurrentVersion() const;

signals:
    void updateAvailable(const ReleaseInfo& info);
    void noUpdateAvailable();
    void checkFailed(const QString& error);

private slots:
    void onNetworkReplyFinished();

private:
    QNetworkAccessManager* m_networkManager;
    Version m_currentVersion;

    Version parseVersion(const QString& versionString);
    ReleaseInfo parseReleaseInfo(const QByteArray& jsonData);
};

#endif // UPDATECHECKER_H
