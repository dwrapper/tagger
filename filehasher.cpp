#include "filehasher.h"

#include "taggerstore.h"

#include <QRunnable>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QDateTime>
#include <QMetaObject>

static QString md5File(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};

    QCryptographicHash h(QCryptographicHash::Md5);
    // 1 MiB chunks, decent tradeoff
    QByteArray buf;
    buf.resize(1 << 20);

    while (!f.atEnd()) {
        const qint64 n = f.read(buf.data(), buf.size());
        if (n > 0) h.addData(buf.constData(), n);
        else break;
    }
    return QString::fromLatin1(h.result().toHex());
}

FileHasher::FileHasher(TaggerStore* store, QObject* parent)
    : QObject(parent), m_store(store) {
    m_pool.setMaxThreadCount(qMax(2, QThread::idealThreadCount() - 1));
}

FileHasher::~FileHasher() {
    m_pool.clear();
    m_pool.waitForDone();
}

void FileHasher::request(const QString& path) {
    if (!m_store || path.isEmpty()) return;

    // Capture file info now (cheap) so worker can validate cache.
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) return;

    const qint64 size = fi.size();
    const qint64 mtime = fi.lastModified().toSecsSinceEpoch();

    // Fast path: cache hit
    if (auto cached = m_store->getCachedHashIfValid(path, size, mtime)) {
        emit hashReady(path, *cached);
        return;
    }

    // Async job
    struct Job : public QRunnable {
        QPointer<FileHasher> self;
        QString path;
        qint64 size;
        qint64 mtime;

        Job(QPointer<FileHasher> hasher, const QString &filePath, qint64 fileSize, qint64 fileMtime) {
            self = hasher;
            path = filePath;
            size = fileSize;
            mtime = fileMtime;
        }

        void run() override {
            if (!self) return;

            const QString hash = md5File(path);
            if (hash.isEmpty() || !self) return;

            // Store cache + emit on GUI thread
            QMetaObject::invokeMethod(self, [s=self, path=path, hash=hash, size=size, mtime=mtime]{
                if (!s || !s->m_store) return;
                s->m_store->upsertHashCache(path, size, mtime, hash);
                emit s->hashReady(path, hash);
            }, Qt::QueuedConnection);
        }
    };

    auto* job = new Job{QPointer<FileHasher>(this), path, size, mtime};
    job->setAutoDelete(true);
    m_pool.start(job);
}

