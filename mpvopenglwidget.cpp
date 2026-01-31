#include "mpvopenglwidget.h"

#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QFile>
#include <QDebug>
#include <QKeyEvent>


static void* get_proc_address(void* ctx, const char* name) {
    QOpenGLContext* glctx = static_cast<QOpenGLContext*>(ctx);
    return reinterpret_cast<void*>(glctx->getProcAddress(QByteArray(name)));
}

MpvOpenGLWidget::MpvOpenGLWidget(QWidget* parent) : QOpenGLWidget(parent) {
    // Avoid Qt clearing the buffer behind mpv
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    initMpv();
}

MpvOpenGLWidget::~MpvOpenGLWidget() {
    makeCurrent();
    destroyMpv();
    doneCurrent();
}

void MpvOpenGLWidget::initMpv() {
    m_mpv = mpv_create();
    if (!m_mpv) {
        qWarning() << "mpv_create failed";
        return;
    }

    // You probably want these (tune as desired)
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "msg-level", "all=v"); // chatty; reduce later
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
    mpv_set_option_string(m_mpv, "vo", "libmpv");

    // Wakeup callback: mpv may call this from internal threads
    mpv_set_wakeup_callback(m_mpv, &MpvOpenGLWidget::onMpvWakeup, this);

    if (mpv_initialize(m_mpv) < 0) {
        qWarning() << "mpv_initialize failed";
        mpv_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    // Observe properties so UI can update without polling
    mpv_observe_property(m_mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 3, "pause", MPV_FORMAT_FLAG);

}

void MpvOpenGLWidget::initializeGL() {
    if (!m_mpv) return;

    initMpvGL();
    m_glReady = (m_mpvRender != nullptr);

    if (m_glReady && !m_pendingFile.isEmpty()) {
        QByteArray p = QFile::encodeName(m_pendingFile);
        const char* cmd[] = {"loadfile", p.constData(), nullptr};
        mpv_command(m_mpv, cmd);

        setPaused(true);
    }
}


void MpvOpenGLWidget::initMpvGL() {
    if (m_mpvRender) return;

    mpv_opengl_init_params gl_init_params;
    gl_init_params.get_proc_address = get_proc_address;
    gl_init_params.get_proc_address_ctx = QOpenGLContext::currentContext();

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    if (mpv_render_context_create(&m_mpvRender, m_mpv, params) < 0) {
        qWarning() << "mpv_render_context_create failed";
        m_mpvRender = nullptr;
        return;
    }

    // mpv tells us when a new frame is ready
    mpv_render_context_set_update_callback(m_mpvRender, &MpvOpenGLWidget::onMpvRenderUpdate, this);
}

void MpvOpenGLWidget::destroyMpv() {
    if (m_mpvRender) {
        mpv_render_context_set_update_callback(m_mpvRender, nullptr, nullptr);
        mpv_render_context_free(m_mpvRender);
        m_mpvRender = nullptr;
    }
    if (m_mpv) {
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void MpvOpenGLWidget::paintGL() {
    if (!m_mpvRender) {
        // Clear if mpv not ready
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    QMutexLocker locker(&m_renderMutex);
    m_needsRedraw = false;

    mpv_opengl_fbo fbo;
    fbo.fbo = defaultFramebufferObject();
    fbo.w = width() * devicePixelRatio();
    fbo.h = height() * devicePixelRatio();
    fbo.internal_format = 0;

    int flip = 1;
    mpv_render_param render_params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    mpv_render_context_render(m_mpvRender, render_params);
}

void MpvOpenGLWidget::resizeGL(int, int) {
    // mpv handles scaling itself via render()
}

bool MpvOpenGLWidget::loadFile(const QString& path) {
    if (!m_mpv) return false;

    m_pendingFile = path;

    if (!m_glReady) {
        // playback will start in initializeGL()
        return true;
    }

    QByteArray p = QFile::encodeName(path);
    const char* cmd[] = {"loadfile", p.constData(), nullptr};
    const bool ok = mpv_command(m_mpv, cmd) >= 0;

    setPaused(true);

    return ok;
}


void MpvOpenGLWidget::togglePause() {
    if (!m_mpv) return;
    int flag = 0;
    mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &flag);
    flag = !flag;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvOpenGLWidget::stop() {
    if (!m_mpv) return;
    const char* cmd[] = {"stop", nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvOpenGLWidget::onMpvWakeup(void* ctx) {
    auto* self = static_cast<MpvOpenGLWidget*>(ctx);
    // Schedule event handling on the GUI thread
    QMetaObject::invokeMethod(self, [self] { self->handleMpvEvents(); }, Qt::QueuedConnection);
}

void MpvOpenGLWidget::handleMpvEvents() {
    if (!m_mpv) return;

    static double s_pos = 0.0;
    static double s_dur = 0.0;

    while (true) {
        mpv_event* ev = mpv_wait_event(m_mpv, 0);
        if (!ev || ev->event_id == MPV_EVENT_NONE) break;

        if (ev->event_id == MPV_EVENT_LOG_MESSAGE) {
            auto* msg = static_cast<mpv_event_log_message*>(ev->data);
            if (msg && msg->text) emit mpvLogMessage(QString::fromUtf8(msg->text));
            continue;
        }

        if (ev->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            auto* prop = static_cast<mpv_event_property*>(ev->data);
            if (!prop || !prop->name) continue;

            const QString name = QString::fromUtf8(prop->name);

            if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE && prop->data) {
                s_pos = *static_cast<double*>(prop->data);
                emit positionChanged(s_pos, s_dur);
            } else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE && prop->data) {
                s_dur = *static_cast<double*>(prop->data);
                emit positionChanged(s_pos, s_dur);
            } else if (name == "pause" && prop->format == MPV_FORMAT_FLAG && prop->data) {
                int paused = *static_cast<int*>(prop->data);
                emit pausedChanged(paused != 0);
            }
        }
    }
}


void MpvOpenGLWidget::onMpvRenderUpdate(void* ctx) {
    auto* self = static_cast<MpvOpenGLWidget*>(ctx);
    {
        QMutexLocker locker(&self->m_renderMutex);
        self->m_needsRedraw = true;
    }
    // request an update on GUI thread
    QMetaObject::invokeMethod(self, [self] { self->update(); }, Qt::QueuedConnection);
}

void MpvOpenGLWidget::setPaused(bool paused) {
    if (!m_mpv) return;
    int flag = paused ? 1 : 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &flag);
    emit pausedChanged(paused);
}

bool MpvOpenGLWidget::isPaused() const {
    if (!m_mpv) return true;
    int flag = 1;
    mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &flag);
    return flag != 0;
}

void MpvOpenGLWidget::seekSeconds(int deltaSeconds) {
    if (!m_mpv) return;

    const QByteArray seconds = QByteArray::number(deltaSeconds);
    // "relative" seek in seconds
    const char* cmd[] = {"seek", seconds.constData(), "relative", nullptr};
    mpv_command(m_mpv, cmd);
}

void MpvOpenGLWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Space:
        togglePause();
        e->accept();
        return;
    case Qt::Key_Left:
        seekSeconds(-5);
        e->accept();
        return;
    case Qt::Key_Right:
        seekSeconds(5);
        e->accept();
        return;
    default:
        break;
    }
    QOpenGLWidget::keyPressEvent(e);
}

void MpvOpenGLWidget::seekTo(double seconds) {
    if (!m_mpv) return;

    QByteArray s = QByteArray::number(seconds, 'f', 3);
    const char* cmd[] = {"seek", s.constData(), "absolute", nullptr};
    mpv_command(m_mpv, cmd);
}
