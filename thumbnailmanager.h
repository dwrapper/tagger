#ifndef THUMBNAILMANAGER_H
#define THUMBNAILMANAGER_H

#pragma once
#include <QObject>
#include <QCache>
#include <QPixmap>
#include <QThreadPool>

class ThumbnailManager : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailManager(QObject* parent = nullptr);
    ~ThumbnailManager() override;

    void request(const QString& absPath, const QString& tsThumbPath, int token);
    void setCacheLimit(int costLimit) { m_cache.setMaxCost(costLimit); }

signals:
    void ready(const QString& absPath, const QPixmap& pix, int token);
    void unavailable(const QString& absPath, int token);

private:
    static bool isImageFile(const QString& absPath);
    static bool isVideoFileByExt(const QString& absPath);

    static QPixmap loadScaledMax400(const QString& path);
    static bool saveJpg(const QImage& img, const QString& outPath, int quality = 85);

    bool generateVideoThumbWithFfmpeg(const QString& videoPath, const QString& outJpg) const;

    QThreadPool m_pool;
    QCache<QString, QPixmap> m_cache;

    QString m_ffmpegPath;   // empty if not available
};


#endif // THUMBNAILMANAGER_H
