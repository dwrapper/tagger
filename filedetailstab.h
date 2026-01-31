#ifndef FILEDETAILSTAB_H
#define FILEDETAILSTAB_H

// FileDetailsTab.h
#pragma once
#include <QWidget>
#include "fileitem.h"
#include "taggerstore.h"
#include "filehasher.h"

class FileDetailsTab : public QWidget {
    Q_OBJECT
public:    
    FileDetailsTab(const FileItem& item, TaggerStore* store, FileHasher* hasher, QWidget* parent=nullptr);

    TaggerStore* m_store = nullptr;
    FileHasher* m_hasher = nullptr;
    FileItem m_item;
    QStringList m_currentTags;
};


#endif // FILEDETAILSTAB_H
