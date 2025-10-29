#include "UpdateChecker.h"
#include "Version.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentVersion(MCP_MANAGER_VERSION_MAJOR, MCP_MANAGER_VERSION_MINOR, MCP_MANAGER_VERSION_PATCH)
{
}

UpdateChecker::~UpdateChecker()
{
}

void UpdateChecker::checkForUpdates()
{
    // GitHub API endpoint for latest release
    QString apiUrl = QString("https://api.github.com/repos/%1/%2/releases/latest")
                        .arg(MCP_MANAGER_REPO_OWNER)
                        .arg(MCP_MANAGER_REPO_NAME);

    QNetworkRequest request{QUrl(apiUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "MCP-Manager-UpdateChecker");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &UpdateChecker::onNetworkReplyFinished);
}

UpdateChecker::Version UpdateChecker::getCurrentVersion() const
{
    return m_currentVersion;
}

void UpdateChecker::onNetworkReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("Network error: %1").arg(reply->errorString());
        emit checkFailed(errorMsg);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    try {
        ReleaseInfo releaseInfo = parseReleaseInfo(data);

        // Compare versions
        if (releaseInfo.version > m_currentVersion) {
            emit updateAvailable(releaseInfo);
        } else {
            emit noUpdateAvailable();
        }
    } catch (const QString& error) {
        emit checkFailed(error);
    }
}

UpdateChecker::Version UpdateChecker::parseVersion(const QString& versionString)
{
    // Remove leading 'v' if present (e.g., "v1.2.3" -> "1.2.3")
    QString cleanVersion = versionString;
    if (cleanVersion.startsWith('v', Qt::CaseInsensitive)) {
        cleanVersion = cleanVersion.mid(1);
    }

    // Parse semantic version (major.minor.patch)
    QRegularExpression re(R"(^(\d+)\.(\d+)\.(\d+))");
    QRegularExpressionMatch match = re.match(cleanVersion);

    if (!match.hasMatch()) {
        throw QString("Invalid version format: %1").arg(versionString);
    }

    int major = match.captured(1).toInt();
    int minor = match.captured(2).toInt();
    int patch = match.captured(3).toInt();

    return Version(major, minor, patch);
}

UpdateChecker::ReleaseInfo UpdateChecker::parseReleaseInfo(const QByteArray& jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        throw QString("Invalid JSON response from GitHub API");
    }

    QJsonObject obj = doc.object();

    ReleaseInfo info;

    // Parse tag name (version)
    info.tagName = obj["tag_name"].toString();
    if (info.tagName.isEmpty()) {
        throw QString("Missing 'tag_name' in release info");
    }

    info.version = parseVersion(info.tagName);

    // Parse release name
    info.name = obj["name"].toString();
    if (info.name.isEmpty()) {
        info.name = info.tagName;  // Fallback to tag name
    }

    // Parse release URL
    info.url = obj["html_url"].toString();

    // Parse download URL (use GitHub's zipball_url from API)
    info.downloadUrl = obj["zipball_url"].toString();
    if (info.downloadUrl.isEmpty()) {
        // Fallback to archive URL if zipball_url not available
        info.downloadUrl = QString("https://github.com/%1/%2/archive/refs/tags/%3.zip")
                              .arg(MCP_MANAGER_REPO_OWNER)
                              .arg(MCP_MANAGER_REPO_NAME)
                              .arg(info.tagName);
    }

    // Parse release notes (body)
    info.releaseNotes = obj["body"].toString();
    if (info.releaseNotes.isEmpty()) {
        info.releaseNotes = "No release notes available.";
    }

    // Parse published date
    QString publishedStr = obj["published_at"].toString();
    if (!publishedStr.isEmpty()) {
        info.publishedAt = QDateTime::fromString(publishedStr, Qt::ISODate);
    }

    return info;
}
