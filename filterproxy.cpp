#include "filterproxy.h"

#include "thumbnailmodel.h"

FilterProxy::FilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void FilterProxy::setNeedle(const QString& text) {
    m_needle = text.trimmed();
    invalidateFilter();
}

bool FilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (m_needle.isEmpty()) return true;

    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString name = sourceModel()->data(idx, ThumbnailModel::FileNameRole).toString();
    const QString path = sourceModel()->data(idx, ThumbnailModel::AbsolutePathRole).toString();

    return name.contains(m_needle, Qt::CaseInsensitive) ||
           path.contains(m_needle, Qt::CaseInsensitive);
}

