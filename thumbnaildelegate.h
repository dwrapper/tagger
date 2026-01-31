#ifndef THUMBNAILDELEGATE_H
#define THUMBNAILDELEGATE_H

// ThumbnailDelegate.h
#pragma once
#include <QStyledItemDelegate>
#include <QTimer>

class ThumbnailDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ThumbnailDelegate(QAbstractItemView* view);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    void setTileSize(const QSize& s) { m_tile = s; }
private:
    QSize m_tile = QSize(140, 160);

    void paintBusy(QPainter* p, const QRect& r) const;

    QAbstractItemView* m_view = nullptr;
    mutable int m_frame = 0;
    QTimer* m_timer = nullptr;
};


#endif // THUMBNAILDELEGATE_H
