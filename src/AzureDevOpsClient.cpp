#include "AzureDevOpsClient.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>

AzureDevOpsClient::AzureDevOpsClient(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_organization("nsdevelopment")  // Default
{
}

AzureDevOpsClient::~AzureDevOpsClient() {
}

void AzureDevOpsClient::setOrganization(const QString& org) {
    m_organization = org;
}

void AzureDevOpsClient::setPAT(const QString& pat) {
    m_pat = pat;
}

bool AzureDevOpsClient::isConfigured() const {
    return !m_organization.isEmpty() && !m_pat.isEmpty();
}

QNetworkRequest AzureDevOpsClient::createRequest(const QString& url) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AzureDevOps-MCP-Server/1.0");

    if (!m_pat.isEmpty()) {
        // PAT authentication: use empty username with PAT as password
        QString auth = ":" + m_pat;
        QByteArray authData = auth.toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + authData);
    }

    return request;
}

void AzureDevOpsClient::makeRequest(const QString& url,
                                    std::function<void(bool, const QJsonObject&)> callback) {
    if (!isConfigured()) {
        QJsonObject error;
        error["error"] = "Azure DevOps not configured (missing PAT or organization)";
        callback(false, error);
        return;
    }

    QNetworkRequest request = createRequest(url);
    emit requestStarted("GET", url);

    QNetworkReply* reply = m_manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, url]() {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray data = reply->readAll();

        emit requestFinished("GET", statusCode, QString::fromUtf8(data));

        if (reply->error() != QNetworkReply::NoError) {
            QJsonObject error;
            error["error"] = reply->errorString();
            error["statusCode"] = statusCode;
            emit errorOccurred(reply->errorString());
            callback(false, error);
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            QJsonObject error;
            error["error"] = "JSON parse error: " + parseError.errorString();
            callback(false, error);
            reply->deleteLater();
            return;
        }

        callback(true, doc.object());
        reply->deleteLater();
    });
}

void AzureDevOpsClient::listProjects(std::function<void(bool, const QJsonObject&)> callback) {
    QString url = QString("https://dev.azure.com/%1/_apis/projects?$top=1000&api-version=7.0")
                    .arg(m_organization);

    makeRequest(url, [callback](bool success, const QJsonObject& data) {
        if (!success) {
            callback(false, data);
            return;
        }

        QJsonArray projects = data["value"].toArray();
        QStringList projectNames;
        for (const QJsonValue& proj : projects) {
            projectNames.append(proj.toObject()["name"].toString());
        }

        QJsonObject result;
        result["status"] = "success";
        result["count"] = projects.size();
        result["projects"] = projectNames.join(", ");
        callback(true, result);
    });
}

void AzureDevOpsClient::listTeams(const QString& project,
                                  std::function<void(bool, const QJsonObject&)> callback) {
    QString url = QString("https://dev.azure.com/%1/_apis/projects/%2/teams?$top=1000&api-version=7.0")
                    .arg(m_organization)
                    .arg(QString(QUrl::toPercentEncoding(project)));

    makeRequest(url, [callback](bool success, const QJsonObject& data) {
        if (!success) {
            callback(false, data);
            return;
        }

        QJsonArray teams = data["value"].toArray();
        QStringList teamNames;
        for (const QJsonValue& team : teams) {
            teamNames.append(team.toObject()["name"].toString());
        }

        QJsonObject result;
        result["status"] = "success";
        result["count"] = teams.size();
        result["teams"] = teamNames.join(", ");
        callback(true, result);
    });
}

void AzureDevOpsClient::getTeamIterations(const QString& project, const QString& team,
                                         std::function<void(bool, const QJsonObject&)> callback) {
    QString url = QString("https://dev.azure.com/%1/%2/%3/_apis/work/teamsettings/iterations?api-version=7.1-preview.1")
                    .arg(m_organization)
                    .arg(QString(QUrl::toPercentEncoding(project)))
                    .arg(QString(QUrl::toPercentEncoding(team)));

    makeRequest(url, [callback](bool success, const QJsonObject& data) {
        if (!success) {
            callback(false, data);
            return;
        }

        QJsonArray iterations = data["value"].toArray();
        QStringList iterationInfo;
        for (const QJsonValue& iter : iterations) {
            QJsonObject iteration = iter.toObject();
            QString name = iteration["name"].toString();
            QString path = iteration["path"].toString();
            QJsonObject attrs = iteration["attributes"].toObject();
            QString start = attrs["startDate"].toString();
            QString finish = attrs["finishDate"].toString();

            iterationInfo.append(QString("%1 (%2 to %3)").arg(name).arg(start).arg(finish));
        }

        QJsonObject result;
        result["status"] = "success";
        result["count"] = iterations.size();
        result["iterations"] = iterationInfo.join(", ");
        callback(true, result);
    });
}

void AzureDevOpsClient::getWorkItems(const QString& project, const QString& wiqlQuery, int limit,
                                    std::function<void(bool, const QJsonObject&)> callback) {
    // Implement WIQL query for work items
    QJsonObject result;
    result["status"] = "not_implemented";
    result["message"] = "getWorkItems not yet implemented";
    callback(true, result);
}

void AzureDevOpsClient::listRepositories(const QString& project,
                                        std::function<void(bool, const QJsonObject&)> callback) {
    QString url = QString("https://dev.azure.com/%1/_apis/git/repositories?project=%2&api-version=7.0")
                    .arg(m_organization)
                    .arg(QString(QUrl::toPercentEncoding(project)));

    makeRequest(url, [callback](bool success, const QJsonObject& data) {
        if (!success) {
            callback(false, data);
            return;
        }

        QJsonArray repos = data["value"].toArray();
        QStringList repoNames;
        for (const QJsonValue& repo : repos) {
            repoNames.append(repo.toObject()["name"].toString());
        }

        QJsonObject result;
        result["status"] = "success";
        result["count"] = repos.size();
        result["repositories"] = repoNames.join(", ");
        callback(true, result);
    });
}
