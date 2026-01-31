#ifndef FILEITEM_H
#define FILEITEM_H

#pragma once
#include <QDateTime>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QSet>
#include <QString>
#include <QStringList>

enum class ThumbStatus : quint8 {
    NotRequested = 0,
    Loading,
    Ready,
    Unavailable
};

enum class FileKind : quint8 {
    Directory = 0,
    Picture,
    Video,
    GenericFile
};

inline bool isVideoExt(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    static const QSet<QString> vids = {"mp4","webm","mov","m4v","mkv","avi","flv","wmv","mpg","mpeg","3gp"};
    return vids.contains(ext);
}

inline FileKind classifyFileKind(const QFileInfo& fi) {
    if (fi.isDir()) return FileKind::Directory;
    QImageReader reader(fi.absoluteFilePath());
    if (reader.canRead()) return FileKind::Picture;
    if (isVideoExt(fi.absoluteFilePath())) return FileKind::Video;
    return FileKind::GenericFile;
}

struct FileItem {
    QString absolutePath;
    QString fileName;
    QIcon icon;       // always: real file icon (details tab)
    QIcon thumbIcon;  // only: used in main grid when ready
    ThumbStatus thumbStatus = ThumbStatus::NotRequested;
    FileKind kind = FileKind::GenericFile;
    QDateTime modified;
    QDateTime created;
    qint64 sizeBytes = 0;
    QStringList tags;
};


#endif // FILEITEM_H
