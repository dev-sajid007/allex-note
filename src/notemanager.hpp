#pragma once

#include "note.hpp"
#include <QList>
#include <QString>

enum class SortMode { Modified, Created, Title };

class NoteManager {
public:
    NoteManager(const QString &dataDir);

    Note createNote(const QString &title, const QString &content = {},
                    const QString &folder = {});
    bool saveNote(const Note &note);
    bool deleteNote(const QUuid &id);
    bool trashNote(const QUuid &id);
    bool restoreNote(const QUuid &id);
    bool permanentlyDeleteNote(const QUuid &id);
    void emptyTrash();
    Note loadNote(const QUuid &id);
    QList<Note> allNotes(const QString &folder = {}) const;
    QList<Note> search(const QString &query, const QString &folder = {}) const;
    QList<Note> trashedNotes() const;
    QStringList folders() const;
    QString dataDir() const { return m_dataDir; }

private:
    QString notePath(const QUuid &id) const;
    QString trashPath(const QUuid &id) const;
    void ensureDataDir();
    QString m_dataDir;
};
