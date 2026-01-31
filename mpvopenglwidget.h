#ifndef MPVOPENGLWIDGET_H
#define MPVOPENGLWIDGET_H

#pragma once
#include <QOpenGLWidget>
#include <QMutex>

extern "C" {
#include <mpv/client.h>
#include <mpv/render_gl.h>
}

class MpvOpenGLWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit MpvOpenGLWidget(QWidget* parent = nullptr);
    ~MpvOpenGLWidget() override;

    bool loadFile(const QString& path);

    // Optional controls
    void togglePause();
    void stop();

    void setPaused(bool paused);
    bool isPaused() const;

    void seekSeconds(int deltaSeconds); // negative = backward, positive = forward
    void seekTo(double seconds); // absolute seek

    mpv_handle* mpv() const { return m_mpv; }

signals:
    void mpvLogMessage(const QString& msg);
    void pausedChanged(bool paused);
    void positionChanged(double posSeconds, double durationSeconds);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void initMpv();
    void initMpvGL();
    void destroyMpv();

    static void onMpvWakeup(void* ctx);
    void handleMpvEvents();

    static void onMpvRenderUpdate(void* ctx);

    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_mpvRender = nullptr;

    // mpv wants you to schedule updates safely across threads
    QMutex m_renderMutex;
    bool m_needsRedraw = false;

    QString m_pendingFile;
    bool m_glReady = false;

};


#endif // MPVOPENGLWIDGET_H
