#pragma once

#include <QObject>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QNetworkAccessManager>
#include <QSettings>

class AuthManager : public QObject {
    Q_OBJECT
public:
    explicit AuthManager(QObject *parent = nullptr);

    bool isSignedIn() const { return !m_token.isEmpty(); }
    QString accessToken() const { return m_token; }

    void signIn(const QString &clientId, const QString &clientSecret);
    void signOut();
    void refreshToken();

signals:
    void signedIn();
    void signedOut();
    void authError(const QString &error);

private slots:
    void onAuthGranted();
    void onTokenResponse(QNetworkReply *reply);

private:
    void saveCredentials(const QString &clientId, const QString &clientSecret);
    void loadCredentials();
    void startRefreshTimer();

    QOAuth2AuthorizationCodeFlow *m_oauth;
    QOAuthHttpServerReplyHandler *m_replyHandler;
    QNetworkAccessManager *m_network;
    QSettings m_settings;
    QString m_token;
    QString m_refreshToken;
    QString m_clientId;
    QString m_clientSecret;
};
