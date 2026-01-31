#ifndef VIDEODETAILSTAB_H
#define VIDEODETAILSTAB_H

#pragma once
#include <QWidget>
#include "fileitem.h"
#include "taggerstore.h"
#include "filehasher.h"


class QStackedWidget;
class QToolButton;
class MpvOpenGLWidget;

class VideoDetailsTab : public QWidget {
    Q_OBJECT
public:
    VideoDetailsTab(const FileItem& item, TaggerStore* store, FileHasher* hasher, QWidget* parent=nullptr);

signals:
    void navigateRequested(int direction);

private:
    QWidget* buildInfoPane(const FileItem& item);
    void setModePlayer(bool playerMode);
    void updatePlayPauseText();

    QToolButton* m_infoBtn = nullptr;
    QStackedWidget* m_stack = nullptr;
    MpvOpenGLWidget* m_player = nullptr;
    bool m_playerMode = true;

    TaggerStore* m_store = nullptr;
    FileHasher* m_hasher = nullptr;
    FileItem m_item;
    QStringList m_currentTags;

};


#endif // VIDEODETAILSTAB_H
