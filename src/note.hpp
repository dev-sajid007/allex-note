#pragma once

#include <QString>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>

class Note {
public:
    Note();
    explicit Note(const QString &title, const QString &content = {});

    QUuid id() const { return m_id; }
    QString title() const { return m_title; }
    void setTitle(const QString &title) { m_title = title; m_modified = QDateTime::currentDateTime(); }

    QString content() const { return m_content; }
    void setContent(const QString &content) { m_content = content; m_modified = QDateTime::currentDateTime(); }

    QDateTime createdAt() const { return m_created; }
    QDateTime modifiedAt() const { return m_modified; }

    bool isPinned() const { return m_pinned; }
    void setPinned(bool pinned) { m_pinned = pinned; }

    QString color() const { return m_color; }
    void setColor(const QString &color) { m_color = color; }

    QString folder() const { return m_folder; }
    void setFolder(const QString &folder) { m_folder = folder; }

    bool isTrashed() const { return m_trashed; }
    void setTrashed(bool trashed) { m_trashed = trashed; }

    QDateTime reminder() const { return m_reminder; }
    void setReminder(const QDateTime &dt) { m_reminder = dt; }
    void clearReminder() { m_reminder = QDateTime(); }
    bool hasReminder() const { return m_reminder.isValid(); }

    bool isLocked() const { return m_locked; }
    void setLocked(bool locked) { m_locked = locked; }
    QByteArray lockedSalt() const { return m_lockedSalt; }
    void setLockedSalt(const QByteArray &salt) { m_lockedSalt = salt; }

    bool isNull() const { return m_id.isNull(); }

    QJsonObject toJson() const;
    static Note fromJson(const QJsonObject &obj);

private:
    QUuid m_id;
    QString m_title;
    QString m_content;
    QDateTime m_created;
    QDateTime m_modified;
    bool m_pinned = false;
    QString m_color;
    QString m_folder;
    bool m_trashed = false;
    QDateTime m_reminder;
    bool m_locked = false;
    QByteArray m_lockedSalt;
};
