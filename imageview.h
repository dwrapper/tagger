#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#pragma once
#include <QWidget>
#include <QPixmap>

class QLabel;

class ImageView : public QWidget {
    Q_OBJECT
public:
    explicit ImageView(QWidget* parent = nullptr);

    bool load(const QString& filePath);
    QSize imagePixelSize() const { return m_imagePixelSize; }

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void updatePixmap();

    QLabel* m_label = nullptr;
    QPixmap m_original;
    QSize m_imagePixelSize; // actual image pixel dimensions (not scaled)
};


#endif // IMAGEVIEW_H
