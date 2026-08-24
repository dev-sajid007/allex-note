#include "authmanager.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QTimer>
#include <QNetworkReply>
#include <QSet>

static const QString AUTH_URL = "https://accounts.google.com/o/oauth2/v2/auth";
static const QString TOKEN_URL = "https://oauth2.googleapis.com/token";
static const QByteArray SCOPE = "https://www.googleapis.com/auth/drive.appdata";

AuthManager::AuthManager(QObject *parent)
    : QObject(parent)
    , m_oauth(new QOAuth2AuthorizationCodeFlow(this))
    , m_replyHandler(new QOAuthHttpServerReplyHandler(0, this))
    , m_network(new QNetworkAccessManager(this))
    , m_settings("Allex", "AllexNotes")
{
    m_oauth->setReplyHandler(m_replyHandler);
    connect(m_oauth, &QOAuth2AuthorizationCodeFlow::granted,
            this, &AuthManager::onAuthGranted);

    loadCredentials();
    startRefreshTimer();
}

void AuthManager::loadCredentials() {
    m_clientId = m_settings.value("google/clientId").toString();
    m_clientSecret = m_settings.value("google/clientSecret").toString();
    m_token = m_settings.value("google/accessToken").toString();
    m_refreshToken = m_settings.value("google/refreshToken").toString();
}

void AuthManager::saveCredentials(const QString &clientId, const QString &clientSecret) {
    m_clientId = clientId;
    m_clientSecret = clientSecret;
    m_settings.setValue("google/clientId", clientId);
    m_settings.setValue("google/clientSecret", clientSecret);
}

void AuthManager::signIn(const QString &clientId, const QString &clientSecret) {
    saveCredentials(clientId, clientSecret);

    m_oauth->setAuthorizationUrl(QUrl(AUTH_URL));
    m_oauth->setTokenUrl(QUrl(TOKEN_URL));
    m_oauth->setClientIdentifier(clientId);
    m_oauth->setClientIdentifierSharedKey(clientSecret);
    QSet<QByteArray> scopes;
    scopes.insert(SCOPE);
    m_oauth->setRequestedScopeTokens(scopes);

    m_oauth->grant();
}

void AuthManager::onAuthGranted() {
    m_token = m_oauth->token();
    m_refreshToken = m_oauth->refreshToken();
    m_settings.setValue("google/accessToken", m_token);
    m_settings.setValue("google/refreshToken", m_refreshToken);
    emit signedIn();
}

void AuthManager::signOut() {
    m_token.clear();
    m_refreshToken.clear();
    m_settings.remove("google/accessToken");
    m_settings.remove("google/refreshToken");
    emit signedOut();
}

void AuthManager::refreshToken() {
    if (m_refreshToken.isEmpty() || m_clientId.isEmpty()) return;

    QUrlQuery params;
    params.addQueryItem("grant_type", "refresh_token");
    params.addQueryItem("client_id", m_clientId);
    params.addQueryItem("client_secret", m_clientSecret);
    params.addQueryItem("refresh_token", m_refreshToken);

    QNetworkRequest request{QUrl(TOKEN_URL)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    QNetworkReply *reply = m_network->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onTokenResponse(reply);
        reply->deleteLater();
    });
}

void AuthManager::onTokenResponse(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        emit authError(reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();

    if (obj.contains("access_token")) {
        m_token = obj["access_token"].toString();
        m_settings.setValue("google/accessToken", m_token);
        if (obj.contains("refresh_token")) {
            m_refreshToken = obj["refresh_token"].toString();
            m_settings.setValue("google/refreshToken", m_refreshToken);
        }
        emit signedIn();
    } else if (obj.contains("error")) {
        emit authError(obj["error_description"].toString());
    }
}

void AuthManager::startRefreshTimer() {
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        if (!m_refreshToken.isEmpty() && m_clientId.isEmpty()) {
            m_clientId = m_settings.value("google/clientId").toString();
            m_clientSecret = m_settings.value("google/clientSecret").toString();
        }
        if (!m_refreshToken.isEmpty() && !m_clientId.isEmpty()) {
            refreshToken();
        }
    });
    timer->start(50 * 60 * 1000);
}
