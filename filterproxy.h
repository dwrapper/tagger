#ifndef FILTERPROXY_H
#define FILTERPROXY_H

// FilterProxy.h
#pragma once
#include <QSortFilterProxyModel>

class FilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FilterProxy(QObject* parent = nullptr);
    void setNeedle(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_needle;
};


#endif // FILTERPROXY_H
