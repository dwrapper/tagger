#include "picturedetailstab.h"

#include "imageview.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QStackedWidget>
#include <QLabel>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QFileInfo>
#include <QTimer>
#include <QShortcut>

static QString humanSize(qint64 bytes) {
    const double b = double(bytes);
    const char* units[] = {"B","KB","MB","GB","TB"};
    int i = 0;
    double v = b;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    return QString::number(v, 'f', (i==0?0:2)) + " " + units[i];
}

PictureDetailsTab::PictureDetailsTab(const FileItem& item, TaggerStore* store, FileHasher* hasher, QWidget* parent)
    : QWidget(parent), m_store(store), m_hasher(hasher), m_item(item) {
    setFocusPolicy(Qt::StrongFocus);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8,8,8,8);
    root->setSpacing(6);

    // Top bar: navigation + right-aligned "i" button
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

    // Stack: [0] image view, [1] info view
    m_stack = new QStackedWidget(this);

    m_imageView = new ImageView(this);
    const bool ok = m_imageView->load(item.absolutePath);
    if (!ok) {
        // If loading fails, still give them an info pane so the tab isn't useless.
        auto* fail = new QLabel("Failed to load image.", this);
        fail->setAlignment(Qt::AlignCenter);
        m_stack->addWidget(fail);
    } else {
        m_stack->addWidget(m_imageView);
    }

    const QSize imgPx = ok ? m_imageView->imagePixelSize() : QSize();
    m_stack->addWidget(buildInfoPane(item, imgPx));

    root->addWidget(m_stack, 1);

    connect(m_infoBtn, &QToolButton::clicked, this, [this] {
        setModeImage(!m_imageMode);
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

    setModeImage(true);
}

void PictureDetailsTab::setModeImage(bool imageMode) {
    m_imageMode = imageMode;
    m_stack->setCurrentIndex(imageMode ? 0 : 1);
    m_infoBtn->setToolTip(imageMode ? "Show info" : "Show image");
    // Keep the same "i" text if you want; or flip to something else:
    // m_infoBtn->setText(imageMode ? "i" : "⟲");
}

QWidget* PictureDetailsTab::buildInfoPane(const FileItem& item, const QSize& imgSizePx) {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0,0,0,0);

    // Header row (icon + filename)
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

    if (imgSizePx.isValid()) {
        addRO("Dimensions:", QString("%1 × %2 px").arg(imgSizePx.width()).arg(imgSizePx.height()));
    } else {
        addRO("Dimensions:", "(unknown)");
    }

    layout->addLayout(form);

    layout->addWidget(new QLabel("Tags:", w));

    auto* tags = new QTextEdit(w);
    tags->setPlaceholderText("Tags");
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
