#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

/**
 * @brief Client for Azure DevOps REST API
 *
 * Handles authentication and API calls to Azure DevOps services.
 */
class AzureDevOpsClient : public QObject {
    Q_OBJECT

public:
    explicit AzureDevOpsClient(QObject* parent = nullptr);
    ~AzureDevOpsClient();

    void setOrganization(const QString& org);
    void setPAT(const QString& pat);

    QString organization() const { return m_organization; }
    bool isConfigured() const;

    // Tool implementations
    void listProjects(std::function<void(bool, const QJsonObject&)> callback);
    void listTeams(const QString& project, std::function<void(bool, const QJsonObject&)> callback);
    void getTeamIterations(const QString& project, const QString& team,
                          std::function<void(bool, const QJsonObject&)> callback);
    void getWorkItems(const QString& project, const QString& wiqlQuery, int limit,
                     std::function<void(bool, const QJsonObject&)> callback);
    void listRepositories(const QString& project, std::function<void(bool, const QJsonObject&)> callback);

signals:
    void requestStarted(const QString& method, const QString& url);
    void requestFinished(const QString& method, int statusCode, const QString& response);
    void errorOccurred(const QString& error);

private:
    void makeRequest(const QString& url, std::function<void(bool, const QJsonObject&)> callback);
    QNetworkRequest createRequest(const QString& url) const;

    QNetworkAccessManager* m_manager;
    QString m_organization;
    QString m_pat;
};
