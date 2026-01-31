#ifndef WORKSPACELISTMODEL_H
#define WORKSPACELISTMODEL_H

// WorkspaceListModel.h
#pragma once
#include <QAbstractListModel>

struct Workspace {
    QString name;
    QString dir;
};

class WorkspaceListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { NameRole = Qt::UserRole + 1, DirRole };

    explicit WorkspaceListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setWorkspaces(QVector<Workspace> w);
    Workspace workspaceAt(int row) const;

    bool containsDir(const QString& dir) const;
    int indexOfDir(const QString& dir) const;

    void addWorkspace(const Workspace& w);
    void removeAt(int row);

private:
    QVector<Workspace> m_ws;
};


#endif // WORKSPACELISTMODEL_H
