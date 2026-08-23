#include "note.hpp"

Note::Note()
    : m_id(QUuid::createUuid())
    , m_created(QDateTime::currentDateTime())
    , m_modified(m_created)
{
}

Note::Note(const QString &title, const QString &content)
    : m_id(QUuid::createUuid())
    , m_title(title)
    , m_content(content)
    , m_created(QDateTime::currentDateTime())
    , m_modified(m_created)
{
}

QJsonObject Note::toJson() const {
    QJsonObject obj;
    obj["id"] = m_id.toString(QUuid::WithoutBraces);
    obj["title"] = m_title;
    obj["content"] = m_content;
    obj["created"] = m_created.toString(Qt::ISODate);
    obj["modified"] = m_modified.toString(Qt::ISODate);
    obj["pinned"] = m_pinned;
    obj["color"] = m_color;
    obj["folder"] = m_folder;
    obj["trashed"] = m_trashed;
    if (m_reminder.isValid())
        obj["reminder"] = m_reminder.toString(Qt::ISODate);
    else
        obj["reminder"] = QString();
    return obj;
}

Note Note::fromJson(const QJsonObject &obj) {
    Note note;
    note.m_id = QUuid(obj["id"].toString());
    note.m_title = obj["title"].toString();
    note.m_content = obj["content"].toString();
    note.m_created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
    note.m_modified = QDateTime::fromString(obj["modified"].toString(), Qt::ISODate);
    note.m_pinned = obj["pinned"].toBool();
    note.m_color = obj["color"].toString();
    note.m_folder = obj["folder"].toString();
    note.m_trashed = obj["trashed"].toBool();
    QString rem = obj["reminder"].toString();
    if (!rem.isEmpty())
        note.m_reminder = QDateTime::fromString(rem, Qt::ISODate);
    return note;
}
