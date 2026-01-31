#include "taggerstore.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

static qint64 nowSecs() { return QDateTime::currentSecsSinceEpoch(); }

TaggerStore::TaggerStore(QObject* parent) : QObject(parent) {}

bool TaggerStore::openOrCreate(const QString& dbPath) {
    m_db = QSqlDatabase::addDatabase("QSQLITE", "tagger-conn");
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        qWarning() << "DB open failed:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    const char* stmts[] = {
        "PRAGMA journal_mode=WAL;",
        "PRAGMA synchronous=NORMAL;",
        "CREATE TABLE IF NOT EXISTS workspaces(dir TEXT PRIMARY KEY, name TEXT NOT NULL, added_at INTEGER NOT NULL);",
        "CREATE TABLE IF NOT EXISTS app_state(key TEXT PRIMARY KEY, value TEXT NOT NULL);",
        "CREATE TABLE IF NOT EXISTS open_tabs(path TEXT PRIMARY KEY, opened_at INTEGER NOT NULL);",
        "CREATE TABLE IF NOT EXISTS tags_by_path(path TEXT PRIMARY KEY, tags_json TEXT NOT NULL, updated_at INTEGER NOT NULL);",
        "CREATE TABLE IF NOT EXISTS tags_by_hash(hash TEXT PRIMARY KEY, tags_json TEXT NOT NULL, updated_at INTEGER NOT NULL);",
        "CREATE TABLE IF NOT EXISTS file_hash_cache(path TEXT PRIMARY KEY, size INTEGER NOT NULL, mtime INTEGER NOT NULL, hash TEXT NOT NULL, updated_at INTEGER NOT NULL);",
        "CREATE INDEX IF NOT EXISTS idx_tags_hash ON tags_by_hash(hash);"
    };
    for (auto s : stmts) {
        if (!q.exec(s)) {
            qWarning() << "DB init failed:" << q.lastError().text() << "SQL:" << s;
            return false;
        }
    }
    return true;
}

QString TaggerStore::tagsToJson(const QStringList& tags) {
    QJsonArray arr;
    for (const auto& t : tags) arr.append(t);
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QStringList TaggerStore::jsonToTags(const QString& json) {
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return {};
    QStringList out;
    for (const auto& v : doc.array()) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty()) out << s;
    }
    return out;
}

// Workspaces
QList<WorkspaceRec> TaggerStore::loadWorkspaces() {
    QList<WorkspaceRec> out;
    QSqlQuery q(m_db);
    q.exec("SELECT name, dir FROM workspaces ORDER BY added_at ASC;");
    while (q.next()) out.push_back({q.value(0).toString(), q.value(1).toString()});
    return out;
}

void TaggerStore::upsertWorkspace(const QString& dir, const QString& name) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO workspaces(dir,name,added_at) VALUES(?,?,?) "
              "ON CONFLICT(dir) DO UPDATE SET name=excluded.name;");
    q.addBindValue(dir);
    q.addBindValue(name);
    q.addBindValue(nowSecs());
    q.exec();
}

void TaggerStore::removeWorkspace(const QString& dir) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM workspaces WHERE dir=?;");
    q.addBindValue(dir);
    q.exec();
}

// State
void TaggerStore::setState(const QString& key, const QString& value) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO app_state(key,value) VALUES(?,?) "
              "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

std::optional<QString> TaggerStore::getState(const QString& key) {
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM app_state WHERE key=?;");
    q.addBindValue(key);
    if (!q.exec()) return std::nullopt;
    if (!q.next()) return std::nullopt;
    return q.value(0).toString();
}

// Tabs
QStringList TaggerStore::loadOpenTabs() {
    QStringList out;
    QSqlQuery q(m_db);
    q.exec("SELECT path FROM open_tabs ORDER BY opened_at ASC;");
    while (q.next()) out << q.value(0).toString();
    return out;
}
void TaggerStore::addOpenTab(const QString& path) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO open_tabs(path,opened_at) VALUES(?,?) "
              "ON CONFLICT(path) DO UPDATE SET opened_at=excluded.opened_at;");
    q.addBindValue(path);
    q.addBindValue(nowSecs());
    q.exec();
}
void TaggerStore::removeOpenTab(const QString& path) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM open_tabs WHERE path=?;");
    q.addBindValue(path);
    q.exec();
}
void TaggerStore::clearOpenTabs() {
    QSqlQuery q(m_db);
    q.exec("DELETE FROM open_tabs;");
}

// Tags
std::optional<QStringList> TaggerStore::getTagsByPath(const QString& path) {
    QSqlQuery q(m_db);
    q.prepare("SELECT tags_json FROM tags_by_path WHERE path=?;");
    q.addBindValue(path);
    if (!q.exec() || !q.next()) return std::nullopt;
    return jsonToTags(q.value(0).toString());
}

std::optional<QStringList> TaggerStore::getTagsByHash(const QString& hash) {
    QSqlQuery q(m_db);
    q.prepare("SELECT tags_json FROM tags_by_hash WHERE hash=?;");
    q.addBindValue(hash);
    if (!q.exec() || !q.next()) return std::nullopt;
    return jsonToTags(q.value(0).toString());
}

void TaggerStore::upsertTagsByPath(const QString& path, const QStringList& tags) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO tags_by_path(path,tags_json,updated_at) VALUES(?,?,?) "
              "ON CONFLICT(path) DO UPDATE SET tags_json=excluded.tags_json, updated_at=excluded.updated_at;");
    q.addBindValue(path);
    q.addBindValue(tagsToJson(tags));
    q.addBindValue(nowSecs());
    q.exec();
}

void TaggerStore::upsertTagsByHash(const QString& hash, const QStringList& tags) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO tags_by_hash(hash,tags_json,updated_at) VALUES(?,?,?) "
              "ON CONFLICT(hash) DO UPDATE SET tags_json=excluded.tags_json, updated_at=excluded.updated_at;");
    q.addBindValue(hash);
    q.addBindValue(tagsToJson(tags));
    q.addBindValue(nowSecs());
    q.exec();
}

// Hash cache
std::optional<QString> TaggerStore::getCachedHashIfValid(const QString& path, qint64 size, qint64 mtimeSecs) {
    QSqlQuery q(m_db);
    q.prepare("SELECT hash,size,mtime FROM file_hash_cache WHERE path=?;");
    q.addBindValue(path);
    if (!q.exec() || !q.next()) return std::nullopt;
    const qint64 sz = q.value(1).toLongLong();
    const qint64 mt = q.value(2).toLongLong();
    if (sz == size && mt == mtimeSecs) return q.value(0).toString();
    return std::nullopt;
}

void TaggerStore::upsertHashCache(const QString& path, qint64 size, qint64 mtimeSecs, const QString& hash) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO file_hash_cache(path,size,mtime,hash,updated_at) VALUES(?,?,?,?,?) "
              "ON CONFLICT(path) DO UPDATE SET size=excluded.size, mtime=excluded.mtime, hash=excluded.hash, updated_at=excluded.updated_at;");
    q.addBindValue(path);
    q.addBindValue(size);
    q.addBindValue(mtimeSecs);
    q.addBindValue(hash);
    q.addBindValue(nowSecs());
    q.exec();
}

