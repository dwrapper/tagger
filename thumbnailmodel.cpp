#include "thumbnailmodel.h"

// ThumbnailModel.cpp
#include <QDirIterator>
#include <QFileInfo>
#include <QFileIconProvider>
#include <algorithm>
#include <QImageReader>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static QStringList loadTagsFromSidecar(const QString& jsonPath) {
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return {};

    const QJsonObject obj = doc.object();
    const QJsonArray tagsArr = obj.value("tags").toArray();

    QStringList result;
    result.reserve(tagsArr.size());

    for (const QJsonValue& v : tagsArr) {
        const QJsonObject to = v.toObject();
        const QString title = to.value("title").toString().trimmed();
        if (!title.isEmpty())
            result << title;
    }

    return result;
}


static QDateTime bestEffortCreatedTime(const QFileInfo& fi) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6: QFileInfo::birthTime() exists on many platforms but can be invalid
    QDateTime bt = fi.birthTime();
    if (bt.isValid()) return bt;
#endif
    // Fallback: use metadata change time if birth time unavailable
    QDateTime ct = fi.metadataChangeTime();
    if (ct.isValid()) return ct;
    return fi.lastModified();
}

ThumbnailModel::ThumbnailModel(QObject* parent) : QAbstractListModel(parent) {
    m_thumbs = new ThumbnailManager(this);
    // optional: set cache limit
    // m_thumbs->setCacheLimit(512 * 1024);

    connect(m_thumbs, &ThumbnailManager::ready, this,
            [this](const QString& absPath, const QPixmap& pix, int token) {
                if (token != m_token) return; // old workspace result
                auto it = m_rowByPath.find(absPath);
                if (it == m_rowByPath.end()) return;
                const int row = it.value();
                if (row < 0 || row >= m_items.size()) return;

                m_items[row].thumbIcon = QIcon(pix);
                m_items[row].thumbStatus = ThumbStatus::Ready;

                const QModelIndex idx = index(row, 0);
                emit dataChanged(idx, idx, {Qt::DecorationRole, IconRole, ThumbStatusRole});
            });

    connect(m_thumbs, &ThumbnailManager::unavailable, this,
            [this](const QString& absPath, int token) {
                if (token != m_token) return;
                auto it = m_rowByPath.find(absPath);
                if (it == m_rowByPath.end()) return;
                const int row = it.value();
                if (row < 0 || row >= m_items.size()) return;

                m_items[row].thumbStatus = ThumbStatus::Unavailable;

                const QModelIndex idx = index(row, 0);
                emit dataChanged(idx, idx, {ThumbStatusRole});
            });
}

int ThumbnailModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_items.size();
}

QVariant ThumbnailModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) return {};
    const auto& it = m_items[index.row()];

    switch (role) {
    case Qt::DisplayRole:
    case FileNameRole: return it.fileName;

    case Qt::DecorationRole:
    case IconRole:
        return (it.thumbStatus == ThumbStatus::Ready && !it.thumbIcon.isNull())
                   ? QVariant(it.thumbIcon)
                   : QVariant(it.icon);

    case ThumbStatusRole:
        return int(it.thumbStatus);

    case AbsolutePathRole: return it.absolutePath;
    case ModifiedRole: return it.modified;
    case CreatedRole: return it.created;
    case SizeRole: return it.sizeBytes;
    case TagsRole: return it.tags;
    case FileKindRole: return static_cast<int>(it.kind);
    default: return {};
    }
}


QHash<int, QByteArray> ThumbnailModel::roleNames() const {
    return {
        {FileNameRole, "fileName"},
        {AbsolutePathRole, "absolutePath"},
        {ModifiedRole, "modified"},
        {CreatedRole, "created"},
        {SizeRole, "sizeBytes"},
        {TagsRole, "tags"},
        {IconRole, "icon"},
        {FileKindRole, "fileKind"},
    };
}

void ThumbnailModel::setDirectory(const QString& dirPath) {
    if (dirPath.isEmpty()) {
        beginResetModel();
        m_items.clear();
        m_rowByPath.clear();
        m_dir.clear();
        ++m_token;
        endResetModel();
        return;
    }
    if (m_dir == dirPath) return;
    loadDirectory(dirPath);
}


void ThumbnailModel::loadDirectory(const QString& dirPath) {
    beginResetModel();
    m_items.clear();
    m_rowByPath.clear();
    m_dir = dirPath;
    ++m_token;

    QFileIconProvider iconProvider;
    const QDir baseDir(dirPath);
    const QString tsDirPath = baseDir.absoluteFilePath(".ts");
    QDir tsDir(tsDirPath);

    QDirIterator it(dirPath,
                    QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::NoIteratorFlags);

    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();

        FileItem item;
        item.absolutePath = fi.absoluteFilePath();
        item.fileName = fi.fileName();
        item.kind = classifyFileKind(fi);

        item.modified = fi.lastModified();
        item.created = bestEffortCreatedTime(fi);
        item.sizeBytes = item.kind == FileKind::Directory ? 0 : fi.size();

        // Icon always
        item.icon = iconProvider.icon(fi);

        // Thumbnails only for files (folders shouldn’t have “.ts thumbnails”)
        if (item.kind == FileKind::Directory) {
            item.thumbStatus = ThumbStatus::Unavailable; // no spinner
        } else {
            item.thumbStatus = ThumbStatus::Loading;     // your async pipeline will resolve it

            if (m_store) {
                if (auto t = m_store->getTagsByPath(item.absolutePath)) {
                    item.tags = *t;
                } else {
                    // sidecar tags and thumb requests (your existing logic) still apply for files only
                    // Sidecar tags: .ts/<originalFileName>.json
                    const QString sidecarPath = tsDir.absoluteFilePath(item.fileName + ".json");
                    if (QFileInfo::exists(sidecarPath)) {
                        item.tags = loadTagsFromSidecar(sidecarPath);
                    }
                }
            }
        }




        m_items.push_back(std::move(item));
    }

    std::sort(m_items.begin(), m_items.end(), [](const FileItem& a, const FileItem& b) {
        const bool aDir = a.kind == FileKind::Directory;
        const bool bDir = b.kind == FileKind::Directory;
        if (aDir != bDir) return aDir > bDir; // dirs first
        if (a.modified != b.modified) return a.modified > b.modified;
        if (a.created  != b.created)  return a.created  > b.created;
        if (a.sizeBytes != b.sizeBytes) return a.sizeBytes > b.sizeBytes;
        return a.fileName.localeAwareCompare(b.fileName) < 0;
    });


    for (int i = 0; i < m_items.size(); ++i)
        m_rowByPath.insert(m_items[i].absolutePath, i);

    endResetModel();

    // Start async requests AFTER reset to avoid chaos
    for (const auto& item : m_items) {
        if (item.kind == FileKind::Directory) continue;
        const QString tsThumbPath = tsDir.absoluteFilePath(item.fileName + ".jpg");
        m_thumbs->request(item.absolutePath, tsThumbPath, m_token);
    }

}



const FileItem& ThumbnailModel::itemAt(int row) const {
    static FileItem dummy;
    if (row < 0 || row >= m_items.size()) return dummy;
    return m_items[row];
}

const FileItem* ThumbnailModel::neighborFile(const QString& currentPath, int direction) const {
    if (currentPath.isEmpty() || m_items.isEmpty()) return nullptr;
    const int row = m_rowByPath.value(currentPath, -1);
    if (row < 0) return nullptr;

    const int step = direction >= 0 ? 1 : -1;
    for (int i = row + step; i >= 0 && i < m_items.size(); i += step) {
        if (m_items[i].kind == FileKind::Directory) continue;
        return &m_items[i];
    }
    return nullptr;
}
