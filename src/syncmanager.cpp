#include "syncmanager.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <functional>

static const QString API_BASE = "https://www.googleapis.com/drive/v3";
static const QString UPLOAD_BASE = "https://www.googleapis.com/upload/drive/v3";
static const QString FILES_QUERY =
    "mimeType='application/json' and trashed=false";

SyncManager::SyncManager(AuthManager *auth, const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_auth(auth)
    , m_network(new QNetworkAccessManager(this))
    , m_settings("Allex", "AllexNotes")
    , m_dataDir(dataDir)
{
    m_lastSynced = m_settings.value("sync/lastSynced").toDateTime();

    m_syncTimer = new QTimer(this);
    connect(m_syncTimer, &QTimer::timeout, this, &SyncManager::syncNow);
    m_syncTimer->start(5 * 60 * 1000);

    connect(auth, &AuthManager::signedIn, this, &SyncManager::onSignedIn);
    connect(auth, &AuthManager::signedOut, this, &SyncManager::onSignedOut);
}

void SyncManager::onSignedIn() { syncNow(); }
void SyncManager::onSignedOut() { m_syncTimer->stop(); }

QList<Note> SyncManager::loadLocalNotes() {
    QList<Note> notes;
    QDir dir(m_dataDir);
    for (const QString &name : dir.entryList({"*.json"}, QDir::Files, QDir::Time)) {
        QFile file(dir.absoluteFilePath(name));
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            notes.append(Note::fromJson(doc.object()));
        }
    }
    return notes;
}

void SyncManager::saveLocalNote(const Note &note) {
    QFile file(m_dataDir + "/" + note.id().toString(QUuid::WithoutBraces) + ".json");
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(note.toJson()).toJson());
}

void SyncManager::syncNow() {
    if (m_syncing) return;
    if (!m_auth->isSignedIn()) return;
    m_syncing = true;
    emit syncProgress("Syncing...");
    pullRemoteNotes();
}

void SyncManager::pullRemoteNotes() {
    // Find app folder
    QString query = "name='allex-notes' and mimeType='application/vnd.google-apps.folder' and trashed=false";
    QUrl url(API_BASE + "/files?q=" + QUrl::toPercentEncoding(query) + "&fields=files(id)");
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_auth->accessToken()).toUtf8());

    QNetworkReply *reply = m_network->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_syncing = false;
            emit syncError("Folder query failed: " + reply->errorString());
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray files = doc["files"].toArray();
        if (files.isEmpty()) {
            createAppFolder();
        } else {
            QString folderId = files.at(0)["id"].toString();
            listRemoteNotes(folderId);
        }
    });
}

void SyncManager::createAppFolder() {
    QJsonObject body;
    body["name"] = "allex-notes";
    body["mimeType"] = "application/vnd.google-apps.folder";

    QNetworkRequest req(QUrl(API_BASE + "/files"));
    req.setRawHeader("Authorization", ("Bearer " + m_auth->accessToken()).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_network->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_syncing = false;
            emit syncError("Create folder failed: " + reply->errorString());
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        listRemoteNotes(doc["id"].toString());
    });
}

void SyncManager::listRemoteNotes(const QString &folderId) {
    QString query = FILES_QUERY + " and '" + folderId + "' in parents";
    QUrl url(API_BASE + "/files?q=" + QUrl::toPercentEncoding(query) +
             "&fields=files(id,name)&spaces=appDataFolder");
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_auth->accessToken()).toUtf8());

    QNetworkReply *reply = m_network->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, folderId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_syncing = false;
            emit syncError("List files failed: " + reply->errorString());
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray files = doc["files"].toArray();

        QMap<QString, QString> fileIds; // uuid -> fileId
        for (const QJsonValue &v : files) {
            QJsonObject file = v.toObject();
            QString name = file["name"].toString();
            QString uuid = name;
            if (uuid.endsWith(".json")) uuid.chop(5);
            fileIds[uuid] = file["id"].toString();
        }

        if (fileIds.isEmpty()) {
            QList<Note> local = loadLocalNotes();
            QMap<QString, Note> empty;
            pushLocalNotes(local, empty);
            return;
        }

        downloadAllRemote(fileIds, folderId);
    });
}

void SyncManager::downloadAllRemote(const QMap<QString, QString> &fileIds,
                                     const QString &folderId) {
    QMap<QString, Note> remoteNotes;
    QList<QString> uuids = fileIds.keys();
    int idx = 0;

    std::function<void()> downloadNext;
    downloadNext = [this, &remoteNotes, &fileIds, &uuids, &idx, folderId, &downloadNext]() {
        if (idx >= uuids.size()) {
            QList<Note> local = loadLocalNotes();
            pushLocalNotes(local, remoteNotes);
            return;
        }

        QString uuid = uuids.at(idx);
        QString fileId = fileIds.value(uuid);
        idx++;

        QNetworkRequest req(QUrl(API_BASE + "/files/" + fileId + "?alt=media"));
        req.setRawHeader("Authorization", ("Bearer " + m_auth->accessToken()).toUtf8());

        QNetworkReply *reply = m_network->get(req);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, uuid, &remoteNotes, &downloadNext]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                remoteNotes[uuid] = Note::fromJson(doc.object());
            }
            downloadNext();
        });
    };

    downloadNext();
}

void SyncManager::pushLocalNotes(const QList<Note> &localNotes,
                                  const QMap<QString, Note> &remoteNotes) {
    QSet<QString> processed;

    for (const Note &local : localNotes) {
        QString uuid = local.id().toString(QUuid::WithoutBraces);
        processed.insert(uuid);

        if (remoteNotes.contains(uuid)) {
            const Note &remote = remoteNotes.value(uuid);
            if (local.isTrashed()) {
                deleteRemoteNote(uuid);
            } else if (!remote.isTrashed()) {
                if (local.modifiedAt() > remote.modifiedAt()) {
                    uploadNote(local, uuid);
                } else if (remote.modifiedAt() > local.modifiedAt()) {
                    saveLocalNote(remote);
                }
            }
        } else if (!local.isTrashed()) {
            uploadNote(local);
        }
    }

    for (auto it = remoteNotes.begin(); it != remoteNotes.end(); ++it) {
        if (!processed.contains(it.key()) && !it.value().isNull()) {
            saveLocalNote(it.value());
        }
    }

    m_syncing = false;
    m_lastSynced = QDateTime::currentDateTime();
    m_settings.setValue("sync/lastSynced", m_lastSynced);
    emit syncComplete();
}

QString SyncManager::uploadNote(const Note &note, const QString &existingFileId) {
    QString uuid = note.id().toString(QUuid::WithoutBraces);
    QByteArray fileData = QJsonDocument(note.toJson()).toJson();

    QUrl url;
    if (existingFileId.isEmpty())
        url = QUrl(UPLOAD_BASE + "/files?uploadType=multipart");
    else
        url = QUrl(UPLOAD_BASE + "/files/" + existingFileId + "?uploadType=multipart&addParents=appDataFolder");

    QString metadata = QString("{\"name\":\"%1.json\",\"parents\":[\"appDataFolder\"]}").arg(uuid);

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart metadataPart;
    metadataPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"metadata\""));
    metadataPart.setBody(metadata.toUtf8());

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"file\"; filename=\"" + uuid + ".json\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    filePart.setBody(fileData);

    multiPart->append(metadataPart);
    multiPart->append(filePart);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_auth->accessToken()).toUtf8());

    QNetworkReply *reply;
    if (existingFileId.isEmpty())
        reply = m_network->post(req, multiPart);
    else
        reply = m_network->put(req, multiPart);

    multiPart->setParent(reply);
    return uuid;
}

void SyncManager::deleteRemoteNote(const QString &uuid) {
    QUrl url(API_BASE + "/files?q=name='" + uuid + ".json' and 'appDataFolder' in parents&fields=files(id)");
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_auth->accessToken()).toUtf8());

    QNetworkReply *reply = m_network->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray files = doc["files"].toArray();
        if (files.isEmpty()) return;

        QString fileId = files.at(0)["id"].toString();
        QNetworkRequest delReq(QUrl(API_BASE + "/files/" + fileId));
        delReq.setRawHeader("Authorization", ("Bearer " + m_auth->accessToken()).toUtf8());
        m_network->deleteResource(delReq);
    });
}
