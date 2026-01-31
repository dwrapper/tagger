#include "thumbnailmanager.h"
#include <QRunnable>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QImageWriter>
#include <QPointer>
#include <QStandardPaths>
#include <QProcess>


ThumbnailManager::ThumbnailManager(QObject* parent) : QObject(parent) {
    m_pool.setMaxThreadCount(qMax(2, QThread::idealThreadCount() - 1));
    m_cache.setMaxCost(256 * 1024); // arbitrary default “cost”; tune later

    m_ffmpegPath = QStandardPaths::findExecutable("ffmpeg");
}

static int pixCost(const QPixmap& p) {
    // Rough cost in bytes. QCache wants int, so clamp.
    const qint64 bytes = qint64(p.width()) * p.height() * 4;
    return int(qMin<qint64>(bytes, INT_MAX));
}

bool ThumbnailManager::isImageFile(const QString& absPath) {
    // Light heuristic: can also use QImageReader::imageFormat(absPath).isEmpty()
    QImageReader r(absPath);
    return r.canRead();
}

bool ThumbnailManager::isVideoFileByExt(const QString& absPath) {
    const QString ext = QFileInfo(absPath).suffix().toLower();
    static const QSet<QString> vids = {
        "mp4","webm","mov","m4v","mkv","avi","flv","wmv","mpg","mpeg","3gp"
    };
    return vids.contains(ext);
}

bool ThumbnailManager::generateVideoThumbWithFfmpeg(const QString& videoPath, const QString& outJpg) const {
    if (m_ffmpegPath.isEmpty()) return false;

    QDir().mkpath(QFileInfo(outJpg).absolutePath());

    // scale: fit inside 400x400, keep aspect ratio, do not upscale
    // force_original_aspect_ratio=decrease prevents stretching
    // -frames:v 1 grabs one frame
    const QString vf = "scale=400:400:force_original_aspect_ratio=decrease";

    auto tryAt = [&](const QString& ss) -> bool {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);

        QStringList args = {
            "-y",
            "-hide_banner",
            "-loglevel", "error",
            "-ss", ss,
            "-i", videoPath,
            "-frames:v", "1",
            "-vf", vf,
            outJpg
        };

        p.start(m_ffmpegPath, args);
        if (!p.waitForFinished(8000)) { // avoid hanging forever
            p.kill();
            p.waitForFinished(1000);
            return false;
        }
        return (p.exitStatus() == QProcess::NormalExit &&
                p.exitCode() == 0 &&
                QFileInfo::exists(outJpg) &&
                QFileInfo(outJpg).size() > 0);
    };

    // First try 1 second (often less black than 0), fallback to 0
    if (tryAt("1")) return true;
    return tryAt("0");
}


QPixmap ThumbnailManager::loadScaledMax400(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);

    const QSize original = reader.size();
    if (!original.isValid()) return {};

    const int maxW = 400, maxH = 400;

    QSize target = original;
    if (original.width() > maxW || original.height() > maxH) {
        target.scale(QSize(maxW, maxH), Qt::KeepAspectRatio);
        reader.setScaledSize(target);
    }
    // If smaller than 400x400, do not upscale: keep original
    QImage img = reader.read();
    if (img.isNull()) return {};

    return QPixmap::fromImage(img);
}

bool ThumbnailManager::saveJpg(const QImage& img, const QString& outPath, int quality) {
    QDir().mkpath(QFileInfo(outPath).absolutePath());
    QImageWriter writer(outPath, "jpg");
    writer.setQuality(quality);
    return writer.write(img);
}

void ThumbnailManager::request(const QString& absPath, const QString& tsThumbPath, int token) {
    if (absPath.isEmpty()) return;

    // Cache hit: deliver immediately
    if (auto* cached = m_cache.object(absPath)) {
        emit ready(absPath, *cached, token);
        return;
    }

    // Run async job
    struct Job : public QRunnable {
        QPointer<ThumbnailManager> mgr;
        QString absPath;
        QString tsThumbPath;
        int token;

        Job(QPointer<ThumbnailManager> manager, const QString &absolutePath, const QString &tsPath, int tok) {
            mgr = manager;
            absPath = absolutePath;
            tsThumbPath = tsPath;
            token = tok;
        }

        void run() override {
            if (!mgr) return;
            // 1) If .ts thumb exists, load it
            if (QFileInfo::exists(tsThumbPath)) {
                const QPixmap pm = ThumbnailManager::loadScaledMax400(tsThumbPath);
                if (!pm.isNull()) {
                    // store in cache on GUI thread via queued invoke
                    QPointer<ThumbnailManager> m = mgr;
                    QMetaObject::invokeMethod(m, [m, absPath = absPath, token = token, pm]() {
                        m->m_cache.insert(absPath, new QPixmap(pm), pixCost(pm));
                        emit m->ready(absPath, pm, token);
                    }, Qt::QueuedConnection);
                    return;
                }
            }

            // Video path (only if ffmpeg exists)
            if (ThumbnailManager::isVideoFileByExt(absPath) && !mgr->m_ffmpegPath.isEmpty()) {
                const bool ok = mgr->generateVideoThumbWithFfmpeg(absPath, tsThumbPath);
                if (ok) {
                    const QPixmap pm = ThumbnailManager::loadScaledMax400(tsThumbPath);
                    if (!pm.isNull()) {
                        QPointer<ThumbnailManager> m = mgr;
                        QMetaObject::invokeMethod(m, [m, absPath = absPath, pm, token = token]() {
                            if (!m) return;
                            m->m_cache.insert(absPath, new QPixmap(pm), pixCost(pm));
                            emit m->ready(absPath, pm, token);
                        }, Qt::QueuedConnection);
                        return;
                    }
                }

                // Failed to generate/read
                QPointer<ThumbnailManager> m = mgr;
                QMetaObject::invokeMethod(m, [m, absPath = absPath, token = token]() {
                    if (!m) return;
                    emit m->unavailable(absPath, token);
                }, Qt::QueuedConnection);
                return;
            }

            // 2) If not exists: generate only for images
            if (!ThumbnailManager::isImageFile(absPath)) {
                QPointer<ThumbnailManager> m = mgr;
                QMetaObject::invokeMethod(m, [m, absPath = absPath, token = token]() {
                    if (!m) return;
                    emit m->unavailable(absPath, token);
                }, Qt::QueuedConnection);
                return;
            }

            // Load original (scaled <= 400, no upscale)
            QImageReader reader(absPath);
            reader.setAutoTransform(true);

            const QSize original = reader.size();
            if (!original.isValid()) {
                QPointer<ThumbnailManager> m = mgr;
                QMetaObject::invokeMethod(m, [m, absPath = absPath, token = token]() {
                    if (!m) return;
                    emit m->unavailable(absPath, token);
                }, Qt::QueuedConnection);
                return;
            }

            QSize target = original;
            const QSize maxSz(400, 400);
            if (original.width() > 400 || original.height() > 400) {
                target.scale(maxSz, Qt::KeepAspectRatio);
                reader.setScaledSize(target);
            }

            QImage img = reader.read();
            if (img.isNull()) {
                QPointer<ThumbnailManager> m = mgr;
                QMetaObject::invokeMethod(m, [m, absPath = absPath, token = token]() {
                    if (!m) return;
                    emit m->unavailable(absPath, token);
                }, Qt::QueuedConnection);
                return;
            }

            // Save into .ts as jpg
            ThumbnailManager::saveJpg(img, tsThumbPath, 85);

            QPixmap pm = QPixmap::fromImage(img);
            if (pm.isNull()) {
                QPointer<ThumbnailManager> m = mgr;
                QMetaObject::invokeMethod(m, [m, absPath = absPath, token = token]() {
                    if (!m) return;
                    emit m->unavailable(absPath, token);
                }, Qt::QueuedConnection);
                return;
            }


            QPointer<ThumbnailManager> m = mgr;
            QMetaObject::invokeMethod(m, [m, absPath = absPath, token = token, pm]() {
                if (!m) return;
                m->m_cache.insert(absPath, new QPixmap(pm), pixCost(pm));
                emit m->ready(absPath, pm, token);
            }, Qt::QueuedConnection);
        }
    };

    auto* job = new Job{QPointer<ThumbnailManager>(this), absPath, tsThumbPath, token};
    job->setAutoDelete(true);
    m_pool.start(job);
}

ThumbnailManager::~ThumbnailManager() {
    m_pool.clear();       // stops queued-but-not-started
    m_pool.waitForDone(); // waits for running ones
}

