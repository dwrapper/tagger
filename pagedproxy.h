#ifndef PAGEDPROXY_H
#define PAGEDPROXY_H

// PagedProxy.h
#pragma once
#include <QAbstractProxyModel>

class PagedProxy : public QAbstractProxyModel {
    Q_OBJECT
public:
    explicit PagedProxy(QObject* parent = nullptr);

    void setSourceModel(QAbstractItemModel* sourceModel) override;

    QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;

    QModelIndex index(int row, int col, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex&) const override { return {}; }

    QVariant data(const QModelIndex& index, int role) const override;

    void setPageSize(int pageSize);
    int pageSize() const { return m_pageSize; }

    void setCurrentPage(int page); // 1-based
    int currentPage() const { return m_currentPage; }

    int totalItems() const;
    int totalPages() const;

signals:
    void pagingChanged();

private slots:
    void onSourceModelResetOrChanged();

private:
    int m_pageSize = 60;
    int m_currentPage = 1;
};


#endif // PAGEDPROXY_H
