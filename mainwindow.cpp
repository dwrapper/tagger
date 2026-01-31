#include "mainwindow.h"

#include <QSplitter>
#include <QListView>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QItemSelectionModel>
#include <QToolButton>
#include <QStandardPaths>
#include <QDir>
#include <QFileIconProvider>
#include <QDateTime>
#include <optional>

#include "workspacelistmodel.h"
#include "thumbnailmodel.h"
#include "filterproxy.h"
#include "pagedproxy.h"
#include "paginationbar.h"
#include "thumbnaildelegate.h"
#include "filedetailstab.h"
#include "fileitem.h"
#include "picturedetailstab.h"
#include "videodetailstab.h"

static QDateTime bestEffortCreatedTime(const QFileInfo& fi) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QDateTime bt = fi.birthTime();
    if (bt.isValid()) return bt;
#endif
    QDateTime ct = fi.metadataChangeTime();
    if (ct.isValid()) return ct;
    return fi.lastModified();
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();

    // Create config dir + DB
    const QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(cfgDir);

    m_store = new TaggerStore(this);
    if (!m_store->openOrCreate(cfgDir + "/tagger.db")) {
        // If DB fails, app can still run, but persistence/tags won't work.
        // You can show a QMessageBox if you want.
    }

    m_hasher = new FileHasher(m_store, this);

    QVector<Workspace> workspaces;
    if (m_store) {
        const auto stored = m_store->loadWorkspaces();
        for (const auto& rec : stored) {
            workspaces.push_back({rec.name, rec.dir});
        }
    }
    if (workspaces.isEmpty()) {
        const QString homeDir = QDir::homePath();
        if (!homeDir.isEmpty()) {
            const QFileInfo fi(homeDir);
            const QString name = fi.fileName().isEmpty() ? fi.absoluteFilePath() : fi.fileName();
            workspaces.push_back({name, homeDir});
            if (m_store) m_store->upsertWorkspace(homeDir, name);
        }
    }
    m_workspaceModel->setWorkspaces(workspaces);

    // select first workspace
    if (m_workspaceModel->rowCount() > 0) {
        m_workspaceView->setCurrentIndex(m_workspaceModel->index(0,0));
        setWorkspaceDirectory(m_workspaceModel->data(m_workspaceModel->index(0,0), WorkspaceListModel::DirRole).toString());
    }

    restoreOpenTabs();
}

void MainWindow::buildUi() {
    auto* splitter = new QSplitter(this);
    setCentralWidget(splitter);

    // Left workspace list
    auto* leftPanel = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(6,6,6,6);
    leftLayout->setSpacing(6);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0,0,0,0);

    auto* addBtn = new QToolButton(leftPanel);
    addBtn->setText("+");
    addBtn->setToolTip("Add workspace");

    auto* removeBtn = new QToolButton(leftPanel);
    removeBtn->setText("-");
    removeBtn->setToolTip("Remove workspace");

    btnRow->addWidget(addBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addStretch(1);

    leftLayout->addLayout(btnRow);

    m_workspaceView = new QListView(leftPanel);
    m_workspaceView->setSelectionMode(QAbstractItemView::SingleSelection);

    leftLayout->addWidget(m_workspaceView, 1);

    // Model
    m_workspaceModel = new WorkspaceListModel(this);
    m_workspaceView->setModel(m_workspaceModel);

    connect(addBtn, &QToolButton::clicked, this, [this]{
        const QString dir = QFileDialog::getExistingDirectory(this, "Choose workspace directory");
        if (dir.isEmpty()) return;

        addWorkspaceAndSelect(dir);
    });

    connect(removeBtn, &QToolButton::clicked, this, [this]{
        const QModelIndex cur = m_workspaceView->currentIndex();
        if (!cur.isValid()) return;

        const int row = cur.row();
        const QString dir = m_workspaceModel->data(m_workspaceModel->index(row, 0),
                                                   WorkspaceListModel::DirRole).toString();
        m_workspaceModel->removeAt(row);
        if (m_store && !dir.isEmpty()) m_store->removeWorkspace(dir);

        // pick next workspace to show
        const int count = m_workspaceModel->rowCount();
        if (count > 0) {
            const int newRow = qMin(row, count - 1);
            m_workspaceView->setCurrentIndex(m_workspaceModel->index(newRow, 0));
            const QString dir = m_workspaceModel->data(m_workspaceModel->index(newRow,0),
                                                       WorkspaceListModel::DirRole).toString();
            setWorkspaceDirectory(dir);
        } else {
            // no workspaces left: clear view
            m_tabs->setCurrentIndex(m_mainTabIndex);
            m_tabs->setTabText(m_mainTabIndex, "Main");
            m_search->clear();
            m_thumbModel->setDirectory(QString()); // implement as "clear" or make a clear() method
        }
    });

    connect(m_workspaceView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                const QString dir = m_workspaceModel->data(current, WorkspaceListModel::DirRole).toString();
                setWorkspaceDirectory(dir);
            });

    // Right tabs
    m_tabs = new QTabWidget(splitter);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int idx) {
        if (idx == m_mainTabIndex) return; // main tab can't die
        QWidget* w = m_tabs->widget(idx);
        if (m_store) {
            const QString path = w->property("filePath").toString();
            if (!path.isEmpty()) m_store->removeOpenTab(path);
        }
        m_tabs->removeTab(idx);
        w->deleteLater();
    });

    // Main tab
    QWidget* mainTab = buildMainTab();
    m_mainTabIndex = m_tabs->addTab(mainTab, "Main");
    m_tabs->tabBar()->setTabButton(m_mainTabIndex, QTabBar::RightSide, nullptr); // hide close button
    m_tabs->tabBar()->setTabButton(m_mainTabIndex, QTabBar::LeftSide, nullptr);

    // Split proportions: left half-ish, right bigger
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    // Toolbar: add workspace dir quickly
    auto* tb = addToolBar("Actions");
    auto* addWs = tb->addAction("Add Workspace…");
    connect(addWs, &QAction::triggered, this, [this]{
        const QString dir = QFileDialog::getExistingDirectory(this, "Choose workspace directory");
        if (dir.isEmpty()) return;
        addWorkspaceAndSelect(dir);
    });
}

QWidget* MainWindow::buildMainTab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);

    m_search = new QLineEdit(w);
    m_search->setPlaceholderText("Search files…");
    layout->addWidget(m_search);

    m_thumbView = new QListView(w);
    m_thumbView->setViewMode(QListView::IconMode);
    m_thumbView->setResizeMode(QListView::Adjust);
    m_thumbView->setMovement(QListView::Static);
    m_thumbView->setWrapping(true);
    m_thumbView->setWordWrap(false);
    m_thumbView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_thumbView->setSpacing(10);
    m_thumbView->setUniformItemSizes(true);

    auto* delegate = new ThumbnailDelegate(m_thumbView);
    delegate->setTileSize(QSize(150, 170));
    m_thumbView->setItemDelegate(delegate);

    layout->addWidget(m_thumbView, 1);

    m_pager = new PaginationBar(w);
    layout->addWidget(m_pager);

    // Models: base -> filter -> paged -> view
    m_thumbModel = new ThumbnailModel(this);
    m_thumbModel->setStore(m_store);

    m_filter = new FilterProxy(this);
    m_filter->setSourceModel(m_thumbModel);

    m_paged = new PagedProxy(this);
    m_paged->setSourceModel(m_filter);
    m_paged->setPageSize(60);

    m_thumbView->setModel(m_paged);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& t){
        m_filter->setNeedle(t);
        m_paged->setCurrentPage(1);
    });

    connect(m_paged, &PagedProxy::pagingChanged, this, [this]{
        m_pager->setPageInfo(m_paged->currentPage(), m_paged->totalPages());
    });

    connect(m_pager, &PaginationBar::pageRequested, this, [this](int page){
        m_paged->setCurrentPage(page);
    });

    connect(m_thumbView, &QListView::doubleClicked, this, [this](const QModelIndex& proxyIdx){
        if (!proxyIdx.isValid()) return;

        // map: paged proxy -> filter source -> thumbnail model source
        const QModelIndex filterIdx = m_paged->mapToSource(proxyIdx);
        const QModelIndex srcIdx = m_filter->mapToSource(filterIdx);

        const auto kindValue = m_thumbModel->data(srcIdx, ThumbnailModel::FileKindRole).toInt();
        const auto kind = static_cast<FileKind>(kindValue);
        const QString path = m_thumbModel->data(srcIdx, ThumbnailModel::AbsolutePathRole).toString();

        if (kind == FileKind::Directory) {
            // Add workspace if missing
            if (!m_workspaceModel->containsDir(path)) {
                addWorkspaceAndSelect(path);
            }

            // Select it (this triggers setWorkspaceDirectory via your selection handler)
            const int row = m_workspaceModel->indexOfDir(path);
            if (row >= 0) {
                m_workspaceView->setCurrentIndex(m_workspaceModel->index(row, 0));
            }
            return; // do not open details tab for folders
        }

        const FileItem& item = m_thumbModel->itemAt(srcIdx.row());

        openFileTab(item);

    });

    // init pager state
    m_pager->setPageInfo(1, 1);

    return w;
}

bool MainWindow::openFileTab(const FileItem& item, bool setCurrent, bool persist) {
    if (item.absolutePath.isEmpty() || item.kind == FileKind::Directory) return false;

    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget* existing = m_tabs->widget(i);
        if (existing->property("filePath").toString() == item.absolutePath) {
            if (setCurrent) m_tabs->setCurrentIndex(i);
            if (persist && m_store) m_store->addOpenTab(item.absolutePath);
            return true;
        }
    }

    QWidget* tab = nullptr;
    switch (item.kind) {
    case FileKind::Picture:
        tab = new PictureDetailsTab(item, m_store, m_hasher, m_tabs);
        break;
    case FileKind::Video:
        tab = new VideoDetailsTab(item, m_store, m_hasher, m_tabs);
        break;
    case FileKind::Directory:
    case FileKind::GenericFile:
    default:
        tab = new FileDetailsTab(item, m_store, m_hasher, m_tabs);
        break;
    }

    tab->setProperty("filePath", item.absolutePath);
    const int idx = m_tabs->addTab(tab, item.fileName);
    if (setCurrent) m_tabs->setCurrentIndex(idx);
    if (persist && m_store) m_store->addOpenTab(item.absolutePath);
    return true;
}

bool MainWindow::openFileTab(const QString& path, bool setCurrent, bool persist) {
    const QFileInfo fi(path);
    if (!fi.exists()) return false;

    FileItem item;
    item.absolutePath = fi.absoluteFilePath();
    item.fileName = fi.fileName();
    item.kind = classifyFileKind(fi);
    if (item.kind == FileKind::Directory) return false;
    item.modified = fi.lastModified();
    item.created = bestEffortCreatedTime(fi);
    item.sizeBytes = fi.size();

    QFileIconProvider iconProvider;
    item.icon = iconProvider.icon(fi);

    if (m_store) {
        if (auto tags = m_store->getTagsByPath(item.absolutePath)) {
            item.tags = *tags;
        }
    }

    return openFileTab(item, setCurrent, persist);
}

void MainWindow::restoreOpenTabs() {
    if (!m_store) return;
    const QStringList tabs = m_store->loadOpenTabs();
    for (const auto& path : tabs) {
        if (!openFileTab(path, false, false)) {
            m_store->removeOpenTab(path);
        }
    }
}

void MainWindow::addWorkspaceAndSelect(const QString& dir) {
    if (dir.isEmpty()) return;
    const QFileInfo fi(dir);
    const QString name = fi.fileName().isEmpty() ? fi.absoluteFilePath() : fi.fileName();
    m_workspaceModel->addWorkspace({name, dir});
    if (m_store) m_store->upsertWorkspace(dir, name);
    const int row = m_workspaceModel->indexOfDir(dir);
    if (row >= 0) {
        m_workspaceView->setCurrentIndex(m_workspaceModel->index(row, 0));
    }
}

void MainWindow::setWorkspaceDirectory(const QString& dir) {
    if (dir.isEmpty()) return;
    m_tabs->setTabText(m_mainTabIndex, QString("Main (%1)").arg(QFileInfo(dir).fileName()));
    m_search->clear();
    m_thumbModel->setDirectory(dir);
    m_paged->setCurrentPage(1);
}
