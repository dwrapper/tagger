#include "workspacelistmodel.h"
#include <QDir>

WorkspaceListModel::WorkspaceListModel(QObject* parent) : QAbstractListModel(parent) {}

int WorkspaceListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_ws.size();
}

QVariant WorkspaceListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_ws.size()) return {};
    const auto& w = m_ws[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return w.name;
    case DirRole:  return w.dir;
    default: return {};
    }
}

QHash<int, QByteArray> WorkspaceListModel::roleNames() const {
    return {
        {NameRole, "name"},
        {DirRole, "dir"}
    };
}

void WorkspaceListModel::setWorkspaces(QVector<Workspace> w) {
    beginResetModel();
    m_ws = std::move(w);
    endResetModel();
}

Workspace WorkspaceListModel::workspaceAt(int row) const {
    if (row < 0 || row >= m_ws.size()) return {};
    return m_ws[row];
}

static QString normDir(QString p) {
    // Normalize to avoid duplicates like /a/b and /a/b/
    QDir d(p);
    return d.absolutePath();
}

int WorkspaceListModel::indexOfDir(const QString& dir) const {
    const QString nd = normDir(dir);
    for (int i = 0; i < m_ws.size(); ++i) {
        if (normDir(m_ws[i].dir) == nd) return i;
    }
    return -1;
}

bool WorkspaceListModel::containsDir(const QString& dir) const {
    return indexOfDir(dir) >= 0;
}

void WorkspaceListModel::addWorkspace(const Workspace& w) {
    if (containsDir(w.dir)) return;
    const int row = m_ws.size();
    beginInsertRows({}, row, row);
    m_ws.push_back(w);
    endInsertRows();
}

void WorkspaceListModel::removeAt(int row) {
    if (row < 0 || row >= m_ws.size()) return;
    beginRemoveRows({}, row, row);
    m_ws.removeAt(row);
    endRemoveRows();
}
