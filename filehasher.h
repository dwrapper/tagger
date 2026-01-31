#ifndef FILEHASHER_H
#define FILEHASHER_H

#pragma once
#include <QObject>
#include <QThreadPool>
#include <QPointer>

class TaggerStore;

class FileHasher : public QObject {
    Q_OBJECT
public:
    explicit FileHasher(TaggerStore* store, QObject* parent = nullptr);
    ~FileHasher() override;

    // Request hash; result comes via hashReady
    void request(const QString& path);

signals:
    void hashReady(const QString& path, const QString& hash);

private:
    TaggerStore* m_store = nullptr; // owned by MainWindow
    QThreadPool m_pool;
};



#endif // FILEHASHER_H
