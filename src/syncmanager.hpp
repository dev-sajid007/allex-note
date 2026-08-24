#pragma once

#include "note.hpp"
#include "authmanager.hpp"

#include <QObject>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTimer>
#include <QMap>

class SyncManager : public QObject {
    Q_OBJECT
public:
    SyncManager(AuthManager *auth, const QString &dataDir, QObject *parent = nullptr);

    bool isSyncing() const { return m_syncing; }
    QDateTime lastSynced() const { return m_lastSynced; }
    void syncNow();

signals:
    void syncComplete();
    void syncError(const QString &error);
    void syncProgress(const QString &status);

private slots:
    void onSignedIn();
    void onSignedOut();

private:
    void pullRemoteNotes();
    void createAppFolder();
    void listRemoteNotes(const QString &folderId);
    void downloadAllRemote(const QMap<QString, QString> &fileIds, const QString &folderId);
    void pushLocalNotes(const QList<Note> &localNotes,
                        const QMap<QString, Note> &remoteNotes);
    void deleteRemoteNote(const QString &uuid);
    QString uploadNote(const Note &note, const QString &existingFileId = {});
    QList<Note> loadLocalNotes();
    void saveLocalNote(const Note &note);

    AuthManager *m_auth;
    QNetworkAccessManager *m_network;
    QSettings m_settings;
    QString m_dataDir;
    bool m_syncing = false;
    QDateTime m_lastSynced;
    QTimer *m_syncTimer;
};
