#include "notemanager.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

NoteManager::NoteManager(const QString &dataDir)
    : m_dataDir(dataDir)
{
    ensureDataDir();
}

QString NoteManager::notePath(const QUuid &id) const {
    return m_dataDir + "/" + id.toString(QUuid::WithoutBraces) + ".json";
}

QString NoteManager::trashPath(const QUuid &id) const {
    return m_dataDir + "/trash/" + id.toString(QUuid::WithoutBraces) + ".json";
}

void NoteManager::ensureDataDir() {
    QDir dir(m_dataDir);
    if (!dir.exists()) dir.mkpath(".");
    QDir trashDir(m_dataDir + "/trash");
    if (!trashDir.exists()) trashDir.mkpath(".");
}

Note NoteManager::createNote(const QString &title, const QString &content,
                              const QString &folder) {
    Note note(title, content);
    note.setFolder(folder);
    saveNote(note);
    return note;
}

bool NoteManager::saveNote(const Note &note) {
    QFile file(notePath(note.id()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(note.toJson()).toJson());
    return true;
}

bool NoteManager::deleteNote(const QUuid &id) {
    return trashNote(id);
}

bool NoteManager::trashNote(const QUuid &id) {
    QString src = notePath(id);
    QString dst = trashPath(id);
    if (!QFile::exists(src)) return false;
    return QFile::rename(src, dst);
}

bool NoteManager::restoreNote(const QUuid &id) {
    QString src = trashPath(id);
    QString dst = notePath(id);
    if (!QFile::exists(src)) return false;
    return QFile::rename(src, dst);
}

bool NoteManager::permanentlyDeleteNote(const QUuid &id) {
    return QFile::remove(trashPath(id));
}

void NoteManager::emptyTrash() {
    QDir trashDir(m_dataDir + "/trash");
    for (const QString &name : trashDir.entryList({"*.json"}, QDir::Files)) {
        QFile::remove(trashDir.absoluteFilePath(name));
    }
}

Note NoteManager::loadNote(const QUuid &id) {
    QFile file(notePath(id));
    if (!file.open(QIODevice::ReadOnly)) {
        QFile fileTrash(trashPath(id));
        if (!fileTrash.open(QIODevice::ReadOnly))
            return Note();
        QJsonDocument doc = QJsonDocument::fromJson(fileTrash.readAll());
        return Note::fromJson(doc.object());
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return Note::fromJson(doc.object());
}

QList<Note> NoteManager::allNotes(const QString &folder) const {
    QList<Note> notes;
    QDir dir(m_dataDir);
    for (const QString &name : dir.entryList({"*.json"}, QDir::Files, QDir::Time)) {
        QFile file(dir.absoluteFilePath(name));
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            Note n = Note::fromJson(doc.object());
            if (n.isTrashed()) continue;
            if (!folder.isEmpty() && n.folder() != folder) continue;
            notes.append(n);
        }
    }
    return notes;
}

QList<Note> NoteManager::search(const QString &query, const QString &folder) const {
    if (query.isEmpty())
        return allNotes(folder);
    QList<Note> results;
    for (const Note &note : allNotes(folder)) {
        if (note.title().contains(query, Qt::CaseInsensitive) ||
            note.content().contains(query, Qt::CaseInsensitive)) {
            results.append(note);
        }
    }
    return results;
}

QList<Note> NoteManager::trashedNotes() const {
    QList<Note> notes;
    QDir dir(m_dataDir + "/trash");
    for (const QString &name : dir.entryList({"*.json"}, QDir::Files, QDir::Time)) {
        QFile file(dir.absoluteFilePath(name));
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            notes.append(Note::fromJson(doc.object()));
        }
    }
    return notes;
}

QStringList NoteManager::folders() const {
    QSet<QString> folderSet;
    QDir dir(m_dataDir);
    for (const QString &name : dir.entryList({"*.json"}, QDir::Files)) {
        QFile file(dir.absoluteFilePath(name));
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            Note n = Note::fromJson(doc.object());
            if (!n.isTrashed() && !n.folder().isEmpty())
                folderSet.insert(n.folder());
        }
    }
    QStringList list = folderSet.values();
    list.sort();
    return list;
}
