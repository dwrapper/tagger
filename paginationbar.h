#ifndef PAGINATIONBAR_H
#define PAGINATIONBAR_H

// PaginationBar.h
#pragma once
#include <QWidget>

class QToolButton;
class QHBoxLayout;

class PaginationBar : public QWidget {
    Q_OBJECT
public:
    explicit PaginationBar(QWidget* parent = nullptr);

    void setPageInfo(int currentPage, int totalPages);

signals:
    void pageRequested(int page);

private:
    void rebuild();

    int m_current = 1;
    int m_total = 1;

    QHBoxLayout* m_layout = nullptr;
    QToolButton *m_first = nullptr, *m_prev = nullptr, *m_next = nullptr, *m_last = nullptr;
};


#endif // PAGINATIONBAR_H
