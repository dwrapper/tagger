#include "paginationbar.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>

static QToolButton* makeBtn(const QString& text, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setAutoRaise(true);
    return b;
}

PaginationBar::PaginationBar(QWidget* parent) : QWidget(parent) {
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0,0,0,0);
    m_layout->setSpacing(4);

    m_first = makeBtn("First", this);
    m_prev  = makeBtn("Prev", this);
    m_next  = makeBtn("Next", this);
    m_last  = makeBtn("Last", this);

    connect(m_first, &QToolButton::clicked, this, [this]{ emit pageRequested(1); });
    connect(m_prev,  &QToolButton::clicked, this, [this]{ emit pageRequested(m_current - 1); });
    connect(m_next,  &QToolButton::clicked, this, [this]{ emit pageRequested(m_current + 1); });
    connect(m_last,  &QToolButton::clicked, this, [this]{ emit pageRequested(m_total); });

    rebuild();
}

void PaginationBar::setPageInfo(int currentPage, int totalPages) {
    m_current = qBound(1, currentPage, qMax(1,totalPages));
    m_total = qMax(1, totalPages);
    rebuild();
}

void PaginationBar::rebuild() {
    // Remove everything from layout, but only delete dynamic widgets.
    while (auto* item = m_layout->takeAt(0)) {
        if (auto* w = item->widget()) {
            const bool isPersistent =
                (w == m_first || w == m_prev || w == m_next || w == m_last);

            if (!isPersistent) {
                w->deleteLater(); // delete only page buttons / dots / stretch-spacers-as-widgets
            }
            // persistent ones: just removed from layout, kept alive
        }
        delete item; // delete the layout item wrapper
    }

    // Now safely re-add the persistent widgets
    m_layout->addWidget(m_first);
    m_layout->addWidget(m_prev);

    m_first->setEnabled(m_current > 1);
    m_prev->setEnabled(m_current > 1);
    m_next->setEnabled(m_current < m_total);
    m_last->setEnabled(m_current < m_total);

    const int window = 7;
    int start = qMax(1, m_current - window / 2);
    int end   = qMin(m_total, start + window - 1);
    start     = qMax(1, end - window + 1);

    if (start > 1) {
        m_layout->addWidget(new QLabel("...", this));
    }

    for (int p = start; p <= end; ++p) {
        auto* b = makeBtn(QString::number(p), this);
        b->setCheckable(true);
        b->setChecked(p == m_current);
        connect(b, &QToolButton::clicked, this, [this, p] { emit pageRequested(p); });
        m_layout->addWidget(b);
    }

    if (end < m_total) {
        m_layout->addWidget(new QLabel("...", this));
    }

    m_layout->addStretch(1);

    m_layout->addWidget(m_next);
    m_layout->addWidget(m_last);
}


