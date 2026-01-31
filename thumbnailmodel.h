#ifndef THUMBNAILMODEL_H
#define THUMBNAILMODEL_H

// ThumbnailModel.h
#pragma once
#include <QAbstractListModel>
#include <QVector>
#include "fileitem.h"
#include "thumbnailmanager.h"
#include "taggerstore.h"

class ThumbnailModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        FileNameRole = Qt::UserRole + 1,
        AbsolutePathRole,
        ModifiedRole,
        CreatedRole,
        SizeRole,
        TagsRole,
        IconRole,
        ThumbStatusRole,
        FileKindRole
    };

    explicit ThumbnailModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDirectory(const QString& dirPath);
    const FileItem& itemAt(int row) const;
    const FileItem* neighborFile(const QString& currentPath, int direction) const;

    void setStore(TaggerStore* store) { m_store = store; }

private:
    void loadDirectory(const QString& dirPath);

    void startThumbRequests();

    ThumbnailManager* m_thumbs = nullptr;
    QHash<QString, int> m_rowByPath;
    int m_token = 0; // increments each loadDirectory

    QVector<FileItem> m_items;
    QString m_dir;
    TaggerStore* m_store = nullptr;
};


#endif // THUMBNAILMODEL_H
