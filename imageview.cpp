#include "imageview.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QImageReader>

ImageView::ImageView(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_label->setScaledContents(false);

    layout->addWidget(m_label, 1);
}

bool ImageView::load(const QString& filePath) {
    QImageReader reader(filePath);
    reader.setAutoTransform(true);

    const QSize sz = reader.size();
    if (sz.isValid()) m_imagePixelSize = sz;

    const QImage img = reader.read();
    if (img.isNull()) return false;

    m_imagePixelSize = img.size();
    m_original = QPixmap::fromImage(img);
    updatePixmap();
    return true;
}

void ImageView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    updatePixmap();
}

void ImageView::updatePixmap() {
    if (m_original.isNull()) {
        m_label->clear();
        return;
    }

    // Available area inside this widget
    const QSize avail = size();
    if (avail.width() <= 1 || avail.height() <= 1) return;

    // Original size in device-independent pixels
    QSize orig = m_original.size() / m_original.devicePixelRatio();

    if (orig.width() <= avail.width() && orig.height() <= avail.height()) {
        // Fits: show at original size, centered
        m_label->setPixmap(m_original);
    } else {
        // Too big: scale down proportionally to fit
        const QSize target = orig.scaled(avail, Qt::KeepAspectRatio);
        QPixmap scaled = m_original.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_label->setPixmap(scaled);
    }
}

