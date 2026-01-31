#ifndef FILEITEM_H
#define FILEITEM_H

#pragma once
#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QIcon>

enum class ThumbStatus : quint8 {
    NotRequested = 0,
    Loading,
    Ready,
    Unavailable
};

struct FileItem {
    QString absolutePath;
    QString fileName;
    QIcon icon;       // always: real file icon (details tab)
    QIcon thumbIcon;  // only: used in main grid when ready
    ThumbStatus thumbStatus = ThumbStatus::NotRequested;
    bool isDir = false;
    QDateTime modified;
    QDateTime created;
    qint64 sizeBytes = 0;
    QStringList tags;
};


#endif // FILEITEM_H
