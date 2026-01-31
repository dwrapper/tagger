#ifndef PICTUREDETAILSTAB_H
#define PICTUREDETAILSTAB_H

#pragma once
#include <QWidget>
#include "fileitem.h"
#include "taggerstore.h"
#include "filehasher.h"

class QStackedWidget;
class QToolButton;
class ImageView;

class PictureDetailsTab : public QWidget {
    Q_OBJECT
public:
    PictureDetailsTab(const FileItem& item, TaggerStore* store, FileHasher* hasher, QWidget* parent=nullptr);

signals:
    void navigateRequested(int direction);

private:
    QWidget* buildInfoPane(const FileItem& item, const QSize& imgSizePx);
    void setModeImage(bool imageMode);

    QToolButton* m_infoBtn = nullptr;
    QStackedWidget* m_stack = nullptr;
    ImageView* m_imageView = nullptr;

    bool m_imageMode = true;

    TaggerStore* m_store = nullptr;
    FileHasher* m_hasher = nullptr;
    FileItem m_item;
    QStringList m_currentTags;
};


#endif // PICTUREDETAILSTAB_H
