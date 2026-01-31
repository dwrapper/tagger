#include "filedetailstab.h"

// FileDetailsTab.cpp
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QShortcut>

static QString humanSize(qint64 bytes) {
    const double b = double(bytes);
    const char* units[] = {"B","KB","MB","GB","TB"};
    int i = 0;
    double v = b;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    return QString::number(v, 'f', (i==0?0:2)) + " " + units[i];
}

FileDetailsTab::FileDetailsTab(const FileItem& item, TaggerStore* store, FileHasher* hasher, QWidget* parent)
    : QWidget(parent), m_store(store), m_hasher(hasher), m_item(item) {

    auto* root = new QVBoxLayout(this);

    auto* top = new QHBoxLayout();
    auto* icon = new QLabel(this);
    icon->setPixmap(item.icon.pixmap(64,64));
    icon->setFixedSize(72,72);

    auto* title = new QLabel(item.fileName, this);
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 4);
    tf.setBold(true);
    title->setFont(tf);

    top->addWidget(icon);
    top->addWidget(title, 1);

    auto* navLayout = new QVBoxLayout();
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

    navLayout->addWidget(prevBtn);
    navLayout->addWidget(nextBtn);
    top->addLayout(navLayout);
    root->addLayout(top);

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

    auto* form = new QFormLayout();
    auto addRO = [&](const QString& label, const QString& value) {
        auto* le = new QLineEdit(value, this);
        le->setReadOnly(true);
        form->addRow(label, le);
    };

    addRO("Modified:", item.modified.toString(Qt::ISODate));
    addRO("Created:", item.created.toString(Qt::ISODate));
    addRO("Size:", humanSize(item.sizeBytes));
    addRO("Path:", item.absolutePath);

    root->addLayout(form);

    auto* tags = new QTextEdit(this);
    tags->setPlaceholderText("Tags (stub). Wire to your tagging system.");
    tags->setText(item.tags.join(", "));
    root->addWidget(new QLabel("Tags:", this));
    root->addWidget(tags, 1);

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

}
