#include "pagedproxy.h"

// PagedProxy.cpp
#include "pagedproxy.h"
#include <QtMath>

PagedProxy::PagedProxy(QObject* parent) : QAbstractProxyModel(parent) {}

void PagedProxy::setSourceModel(QAbstractItemModel* sm) {
    if (sourceModel()) {
        disconnect(sourceModel(), nullptr, this, nullptr);
    }
    QAbstractProxyModel::setSourceModel(sm);
    if (sm) {
        connect(sm, &QAbstractItemModel::modelReset, this, &PagedProxy::onSourceModelResetOrChanged);
        connect(sm, &QAbstractItemModel::layoutChanged, this, &PagedProxy::onSourceModelResetOrChanged);
        connect(sm, &QAbstractItemModel::rowsInserted, this, &PagedProxy::onSourceModelResetOrChanged);
        connect(sm, &QAbstractItemModel::rowsRemoved, this, &PagedProxy::onSourceModelResetOrChanged);
    }
    onSourceModelResetOrChanged();
}

int PagedProxy::columnCount(const QModelIndex& parent) const {
    if (parent.isValid() || !sourceModel()) return 0;
    return sourceModel()->columnCount();
}

int PagedProxy::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !sourceModel()) return 0;
    const int start = (m_currentPage - 1) * m_pageSize;
    const int end = qMin(start + m_pageSize, sourceModel()->rowCount());
    return qMax(0, end - start);
}

QModelIndex PagedProxy::index(int row, int col, const QModelIndex& parent) const {
    if (parent.isValid()) return {};
    if (row < 0 || col < 0) return {};
    if (row >= rowCount() || col >= columnCount()) return {};
    return createIndex(row, col);
}

QModelIndex PagedProxy::mapToSource(const QModelIndex& proxyIndex) const {
    if (!sourceModel() || !proxyIndex.isValid()) return {};
    const int sourceRow = (m_currentPage - 1) * m_pageSize + proxyIndex.row();
    return sourceModel()->index(sourceRow, proxyIndex.column());
}

QModelIndex PagedProxy::mapFromSource(const QModelIndex& sourceIndex) const {
    if (!sourceModel() || !sourceIndex.isValid()) return {};
    const int start = (m_currentPage - 1) * m_pageSize;
    const int r = sourceIndex.row() - start;
    if (r < 0 || r >= m_pageSize) return {};
    return index(r, sourceIndex.column());
}

QVariant PagedProxy::data(const QModelIndex& idx, int role) const {
    return sourceModel() ? sourceModel()->data(mapToSource(idx), role) : QVariant{};
}

void PagedProxy::setPageSize(int ps) {
    ps = qMax(1, ps);
    if (m_pageSize == ps) return;
    beginResetModel();
    m_pageSize = ps;
    m_currentPage = 1;
    endResetModel();
    emit pagingChanged();
}

int PagedProxy::totalItems() const {
    return sourceModel() ? sourceModel()->rowCount() : 0;
}

int PagedProxy::totalPages() const {
    const int items = totalItems();
    if (items <= 0) return 1;
    return qMax(1, (items + m_pageSize - 1) / m_pageSize);
}

void PagedProxy::setCurrentPage(int page) {
    const int tp = totalPages();
    page = qBound(1, page, tp);
    if (m_currentPage == page) return;
    beginResetModel();
    m_currentPage = page;
    endResetModel();
    emit pagingChanged();
}

void PagedProxy::onSourceModelResetOrChanged() {
    const int tp = totalPages();
    if (m_currentPage > tp) m_currentPage = tp;
    beginResetModel();
    endResetModel();
    emit pagingChanged();
}

