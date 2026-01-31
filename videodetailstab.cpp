#include "videodetailstab.h"

#include "mpvopenglwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QStackedWidget>
#include <QLabel>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QSignalBlocker>
#include <QTimer>
#include <QShortcut>
#include <cmath>

static QString fmtTime(double seconds) {
    if (seconds <= 0 || std::isnan(seconds) || std::isinf(seconds)) return "00:00";
    int s = int(seconds + 0.5);
    int h = s / 3600; s %= 3600;
    int m = s / 60;   s %= 60;
    if (h > 0) return QString("%1:%2:%3").arg(h).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
    return QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
}

static QString humanSize(qint64 bytes) {
    const double b = double(bytes);
    const char* units[] = {"B","KB","MB","GB","TB"};
    int i = 0;
    double v = b;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    return QString::number(v, 'f', (i==0?0:2)) + " " + units[i];
}

VideoDetailsTab::VideoDetailsTab(const FileItem& item, TaggerStore* store, FileHasher* hasher, QWidget* parent)
    : QWidget(parent), m_store(store), m_hasher(hasher), m_item(item) {
    setFocusPolicy(Qt::StrongFocus);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8,8,8,8);
    root->setSpacing(6);

    auto* topBar = new QHBoxLayout();

    auto* prevBtn = new QToolButton(this);
    prevBtn->setText("▲");
    prevBtn->setToolTip("Previous file (Up)");
    prevBtn->setAutoRaise(true);
    prevBtn->setFixedSize(24, 24);

    auto* nextBtn = new QToolButton(this);
    nextBtn->setText("▼");
    nextBtn->setToolTip("Next file (Down)");
    nextBtn->setAutoRaise(true);
    nextBtn->setFixedSize(24, 24);

    topBar->addWidget(prevBtn);
    topBar->addWidget(nextBtn);
    topBar->addStretch(1);

    m_infoBtn = new QToolButton(this);
    m_infoBtn->setText("i");
    m_infoBtn->setAutoRaise(true);
    m_infoBtn->setToolTip("Show info");
    m_infoBtn->setFixedSize(24, 24);
    topBar->addWidget(m_infoBtn, 0, Qt::AlignRight);
    root->addLayout(topBar);

    m_stack = new QStackedWidget(this);

    // Player page
    auto* playerPage = new QWidget(this);
    auto* playerLayout = new QVBoxLayout(playerPage);
    playerLayout->setContentsMargins(0,0,0,0);
    playerLayout->setSpacing(6);

    m_player = new MpvOpenGLWidget(playerPage);
    m_player->setFocusPolicy(Qt::StrongFocus); // key events
    playerLayout->addWidget(m_player, 1);

    auto* seekRow = new QHBoxLayout();
    seekRow->setContentsMargins(0,0,0,0);
    seekRow->setSpacing(8);

    auto* timeLabel = new QLabel("00:00 / 00:00", playerPage);
    timeLabel->setMinimumWidth(110);

    auto* seekSlider = new QSlider(Qt::Horizontal, playerPage);
    // Use a fixed resolution so you don’t fight floating point.
    seekSlider->setRange(0, 1000);
    seekSlider->setSingleStep(1);
    seekSlider->setPageStep(10);

    seekRow->addWidget(seekSlider, 1);
    seekRow->addWidget(timeLabel, 0);

    playerLayout->addLayout(seekRow);

    // State to avoid slider fighting the user
    auto dragging = std::make_shared<bool>(false);
    auto lastDur = std::make_shared<double>(0.0);
    auto lastPos = std::make_shared<double>(0.0);

    // Controls row
    auto* controls = new QHBoxLayout();
    controls->setContentsMargins(0,0,0,0);
    controls->setSpacing(8);

    auto* backBtn = new QPushButton("⟲ 5s", playerPage);
    auto* playPauseBtn = new QPushButton("Play", playerPage);
    auto* fwdBtn = new QPushButton("5s ⟳", playerPage);

    backBtn->setAutoDefault(false);
    playPauseBtn->setAutoDefault(false);
    fwdBtn->setAutoDefault(false);

    controls->addStretch(1);
    controls->addWidget(backBtn);
    controls->addWidget(playPauseBtn);
    controls->addWidget(fwdBtn);
    controls->addStretch(1);

    playerLayout->addLayout(controls);

    // Load video (will start paused due to MpvOpenGLWidget changes)
    if (!m_player->loadFile(item.absolutePath)) {
        auto* fail = new QLabel("Failed to load video in mpv.", this);
        fail->setAlignment(Qt::AlignCenter);
        m_stack->addWidget(fail);
    } else {
        m_stack->addWidget(playerPage);
    }

    // Wire buttons
    connect(backBtn, &QPushButton::clicked, this, [this] {
        if (m_player) m_player->seekSeconds(-5);
    });
    connect(fwdBtn, &QPushButton::clicked, this, [this] {
        if (m_player) m_player->seekSeconds(5);
    });
    connect(playPauseBtn, &QPushButton::clicked, this, [this, playPauseBtn] {
        if (!m_player) return;
        m_player->togglePause();
        playPauseBtn->setText(m_player->isPaused() ? "Play" : "Pause");
    });

    auto* spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    spaceShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(spaceShortcut, &QShortcut::activated, this, [this, playPauseBtn] {
        if (!m_player) return;
        m_player->togglePause();
        playPauseBtn->setText(m_player->isPaused() ? "Play" : "Pause");
    });

    auto* leftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    leftShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(leftShortcut, &QShortcut::activated, this, [this] {
        if (m_player) m_player->seekSeconds(-5);
    });

    auto* rightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    rightShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(rightShortcut, &QShortcut::activated, this, [this] {
        if (m_player) m_player->seekSeconds(5);
    });

    // Set initial label (paused)
    playPauseBtn->setText("Play");


    // Info page
    m_stack->addWidget(buildInfoPane(item));

    root->addWidget(m_stack, 1);

    connect(m_infoBtn, &QToolButton::clicked, this, [this] {
        setModePlayer(!m_playerMode);
    });

    connect(prevBtn, &QToolButton::clicked, this, [this] {
        emit navigateRequested(-1);
    });
    connect(nextBtn, &QToolButton::clicked, this, [this] {
        emit navigateRequested(1);
    });

    auto* prevShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    prevShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(prevShortcut, &QShortcut::activated, this, [this] {
        emit navigateRequested(-1);
    });

    auto* nextShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    nextShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(nextShortcut, &QShortcut::activated, this, [this] {
        emit navigateRequested(1);
    });

    connect(m_player, &MpvOpenGLWidget::pausedChanged, this, [playPauseBtn](bool paused){
        playPauseBtn->setText(paused ? "Play" : "Pause");
    });

    connect(m_player, &MpvOpenGLWidget::positionChanged, this,
            [seekSlider, timeLabel, dragging, lastDur, lastPos](double pos, double dur) {
                *lastPos = pos;
                if (dur > 0) *lastDur = dur;

                // Update time display always
                const double d = *lastDur;
                timeLabel->setText(QString("%1 / %2").arg(fmtTime(pos), fmtTime(d)));

                // If user is dragging, don't override slider position
                if (*dragging) return;

                if (d > 0.1) {
                    const int v = int((pos / d) * 1000.0);
                    QSignalBlocker b(seekSlider);
                    seekSlider->setValue(qBound(0, v, 1000));
                } else {
                    QSignalBlocker b(seekSlider);
                    seekSlider->setValue(0);
                }
            });

    connect(seekSlider, &QSlider::sliderPressed, this, [dragging] {
        *dragging = true;
    });

    connect(seekSlider, &QSlider::sliderReleased, this,
            [this, seekSlider, dragging, lastDur, lastPos, timeLabel] {
                *dragging = false;

                const double dur = *lastDur;
                if (!m_player || dur <= 0.1) return;

                const double target = (seekSlider->value() / 1000.0) * dur;
                m_player->seekTo(target);

                // Update label immediately to feel responsive
                *lastPos = target;
                timeLabel->setText(QString("%1 / %2").arg(fmtTime(target), fmtTime(dur)));
            });

    // Optional: live update label while dragging
    connect(seekSlider, &QSlider::valueChanged, this,
            [dragging, lastDur, timeLabel](int v) {
                if (!*dragging) return;
                const double dur = *lastDur;
                if (dur <= 0.1) return;
                const double target = (v / 1000.0) * dur;
                timeLabel->setText(QString("%1 / %2").arg(fmtTime(target), fmtTime(dur)));
            });


    setModePlayer(true);
}

void VideoDetailsTab::setModePlayer(bool playerMode) {
    m_playerMode = playerMode;
    m_stack->setCurrentIndex(playerMode ? 0 : 1);
    m_infoBtn->setToolTip(playerMode ? "Show info" : "Show player");

    if (playerMode) {
        if (m_player) m_player->setFocus();
    }
}

QWidget* VideoDetailsTab::buildInfoPane(const FileItem& item) {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0,0,0,0);

    auto* head = new QHBoxLayout();
    auto* icon = new QLabel(w);
    icon->setPixmap(item.icon.pixmap(48,48));
    icon->setFixedSize(56,56);

    auto* title = new QLabel(item.fileName, w);
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 3);
    tf.setBold(true);
    title->setFont(tf);

    head->addWidget(icon);
    head->addWidget(title, 1);
    layout->addLayout(head);

    auto* form = new QFormLayout();
    auto addRO = [&](const QString& label, const QString& value) {
        auto* le = new QLineEdit(value, w);
        le->setReadOnly(true);
        form->addRow(label, le);
    };

    addRO("Modified:", item.modified.toString(Qt::ISODate));
    addRO("Created:", item.created.toString(Qt::ISODate));
    addRO("Size:", humanSize(item.sizeBytes));
    addRO("Path:", item.absolutePath);

    layout->addLayout(form);

    layout->addWidget(new QLabel("Tags:", w));
    auto* tags = new QTextEdit(w);
    tags->setText(item.tags.join(", "));
    layout->addWidget(tags, 1);

    auto* saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(300);

    auto normalize = [](const QString& s) {
        QStringList out;
        for (auto part : s.split(',', Qt::SkipEmptyParts)) {
            const QString t = part.trimmed();
            if (!t.isEmpty()) out << t;
        }
        out.removeDuplicates();
        return out;
    };

    connect(tags, &QTextEdit::textChanged, this, [saveTimer]{
        saveTimer->start();
    });

    connect(saveTimer, &QTimer::timeout, this, [this, tags, normalize]{
        if (!m_store) return;
        m_currentTags = normalize(tags->toPlainText());

        // 1) by path immediately
        m_store->upsertTagsByPath(m_item.absolutePath, m_currentTags);

        // 2) by hash when available
        if (m_hasher) m_hasher->request(m_item.absolutePath);
    });

    if (m_hasher) {
        connect(m_hasher, &FileHasher::hashReady, this,
                [this](const QString& path, const QString& hash){
                    if (!m_store) return;
                    if (path != m_item.absolutePath) return;
                    if (m_currentTags.isEmpty()) return;
                    m_store->upsertTagsByHash(hash, m_currentTags);
                });
    }

    return w;
}
