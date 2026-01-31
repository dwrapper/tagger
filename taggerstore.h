#ifndef TAGGERSTORE_H
#define TAGGERSTORE_H

#pragma once
#include <QObject>
#include <QStringList>
#include <QSqlDatabase>
#include <optional>

struct WorkspaceRec { QString name; QString dir; };

class TaggerStore : public QObject {
    Q_OBJECT
public:
    explicit TaggerStore(QObject* parent = nullptr);

    bool openOrCreate(const QString& dbPath);

    // Workspaces
    QList<WorkspaceRec> loadWorkspaces();
    void upsertWorkspace(const QString& dir, const QString& name);
    void removeWorkspace(const QString& dir);

    // App state
    void setState(const QString& key, const QString& value);
    std::optional<QString> getState(const QString& key);

    // Tabs
    QStringList loadOpenTabs();
    void addOpenTab(const QString& path);
    void removeOpenTab(const QString& path);
    void clearOpenTabs();

    // Tags
    std::optional<QStringList> getTagsByPath(const QString& path);
    std::optional<QStringList> getTagsByHash(const QString& hash);
    void upsertTagsByPath(const QString& path, const QStringList& tags);
    void upsertTagsByHash(const QString& hash, const QStringList& tags);

    // Hash cache
    std::optional<QString> getCachedHashIfValid(const QString& path, qint64 size, qint64 mtimeSecs);
    void upsertHashCache(const QString& path, qint64 size, qint64 mtimeSecs, const QString& hash);

private:
    static QString tagsToJson(const QStringList& tags);
    static QStringList jsonToTags(const QString& json);

    QSqlDatabase m_db;
};

#endif // TAGGERSTORE_H
