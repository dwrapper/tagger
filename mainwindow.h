#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "taggerstore.h"
#include "filehasher.h"

class QListView;
class QTabWidget;
class QLineEdit;
class QListView;
class ThumbnailModel;
class FilterProxy;
class PagedProxy;
class PaginationBar;
class WorkspaceListModel;
struct FileItem;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void buildUi();
    void setWorkspaceDirectory(const QString& dir);
    void addWorkspaceAndSelect(const QString& dir);
    void restoreOpenTabs();
    QWidget* createDetailsTab(const FileItem& item);
    bool navigateDetailsTab(QWidget* tab, int direction);
    bool openFileTab(const FileItem& item, bool setCurrent = true, bool persist = true);
    bool openFileTab(const QString& path, bool setCurrent = true, bool persist = true);

    QWidget* buildMainTab();

    QListView* m_workspaceView = nullptr;
    WorkspaceListModel* m_workspaceModel = nullptr;

    QTabWidget* m_tabs = nullptr;

    // main tab widgets
    QLineEdit* m_search = nullptr;
    QListView* m_thumbView = nullptr;
    PaginationBar* m_pager = nullptr;

    // data
    ThumbnailModel* m_thumbModel = nullptr;
    FilterProxy* m_filter = nullptr;
    PagedProxy* m_paged = nullptr;

    int m_mainTabIndex = 0;

    TaggerStore* m_store = nullptr;
    FileHasher* m_hasher = nullptr;
};

#endif // MAINWINDOW_H
