#include "thumbnaildelegate.h"

#include "thumbnailmodel.h"
#include <QPainter>
#include <QAbstractItemView>

ThumbnailDelegate::ThumbnailDelegate(QAbstractItemView* view)
    : QStyledItemDelegate(view), m_view(view) {

    m_timer = new QTimer(this);
    m_timer->setInterval(80);
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_frame = (m_frame + 1) % 12;
        if (m_view && m_view->viewport())
            m_view->viewport()->update();
    });
    m_timer->start();
}


QSize ThumbnailDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return m_tile;
}

void ThumbnailDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt,
                              const QModelIndex& idx) const {
    p->save();

    const QRect r = opt.rect.adjusted(6, 6, -6, -6);
    const QIcon icon = qvariant_cast<QIcon>(idx.data(Qt::DecorationRole));
    const QString name = idx.data(Qt::DisplayRole).toString();

    const bool selected = opt.state & QStyle::State_Selected;
    if (selected) {
        p->setBrush(opt.palette.highlight());
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(opt.rect.adjusted(2,2,-2,-2), 8, 8);
    }

    QRect iconRect = r;
    iconRect.setHeight(int(r.height() * 0.70));
    const int iconSize = qMin(iconRect.width(), iconRect.height()) - 12;
    const QRect centeredIcon(
        iconRect.center().x() - iconSize/2,
        iconRect.center().y() - iconSize/2,
        iconSize, iconSize
        );

    icon.paint(p, centeredIcon);

    QRect textRect = r;
    textRect.setTop(iconRect.bottom() + 6);

    p->setPen(selected ? opt.palette.highlightedText().color()
                       : opt.palette.text().color());

    QFont f = opt.font;
    f.setPointSizeF(f.pointSizeF() - 0.5);
    p->setFont(f);

    const QString elided = QFontMetrics(f).elidedText(name, Qt::ElideRight, textRect.width());
    p->drawText(textRect, Qt::AlignTop | Qt::AlignHCenter, elided);

    const int st = idx.data(ThumbnailModel::ThumbStatusRole).toInt();
    if (st == int(ThumbStatus::Loading)) {
        paintBusy(p, opt.rect.adjusted(0,0,0,0));
    }


    p->restore();
}

void ThumbnailDelegate::paintBusy(QPainter* p, const QRect& rect) const {
    // draw in the top-right corner of the tile
    QRect r = rect.adjusted(rect.width() - 34, 10, -10, -rect.height() + 34);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // subtle dark background bubble
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 90));
    p->drawEllipse(r);

    const QPoint c = r.center();
    p->translate(c);

    // 12 spokes
    for (int i = 0; i < 12; ++i) {
        const int idx = (i + m_frame) % 12;
        const int alpha = 40 + (idx * 18); // fades
        QPen pen(QColor(255, 255, 255, qMin(alpha, 255)));
        pen.setWidth(2);
        p->setPen(pen);

        p->drawLine(0, -10, 0, -5);
        p->rotate(30);
    }

    p->restore();
}


