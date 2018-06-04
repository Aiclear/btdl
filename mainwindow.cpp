#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "addtorrentdialog.h"
#include "fileutils.h"

#include <QItemDelegate>
#include <QFileDialog>
#include <QDesktopWidget>
#include <QLabel>
#include <QDesktopServices>
#include <QPushButton>

class TorrentViewDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    inline TorrentViewDelegate(MainWindow *mainWindow) : QItemDelegate(mainWindow) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        if (index.column() != 2) {
            QItemDelegate::paint(painter, option, index);
            return;
        }

        // Set up a QStyleOptionProgressBar to precisely mimic the
        // environment of a progress bar.
        QStyleOptionProgressBar progressBarOption;
        progressBarOption.state = QStyle::State_Enabled;
        progressBarOption.direction = QApplication::layoutDirection();
        progressBarOption.rect = option.rect;
        progressBarOption.fontMetrics = QApplication::fontMetrics();
        progressBarOption.minimum = 0;
        progressBarOption.maximum = 100;
        progressBarOption.textAlignment = Qt::AlignCenter;
        progressBarOption.textVisible = true;

        // Set the progress and text values of the style option.
        int progress = qobject_cast<MainWindow *>(parent())->clientForRow(index.row())->progress();
        progressBarOption.progress = progress < 0 ? 0 : progress;
        progressBarOption.text = QString().sprintf("%d%%", progressBarOption.progress);

        // Draw the progress bar onto the view.
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
    }
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 设置正在下载widget中的UI界面
    QStringList headers;
    headers << tr("种子文件") << tr("连接数/作种者") << tr("进度")
            << tr("下载速度") << tr("上传速率") << tr("状态");

    ui->treeOfDownloading->setItemDelegate(new TorrentViewDelegate(this));
    ui->treeOfDownloading->setHeaderLabels(headers);
    ui->treeOfDownloading->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->treeOfDownloading->setAlternatingRowColors(true);
    ui->treeOfDownloading->setRootIsDecorated(false);

    // 设置treewidget中每一列的宽度，可容纳下头item的标题为基本的宽度
    QFontMetrics fm = fontMetrics();
    QHeaderView *header = ui->treeOfDownloading->header();
    header->resizeSection(0, fm.width("typical-name-for-a-torrent.torrent"));
    header->resizeSection(1, fm.width(headers.at(1) + "  "));
    header->resizeSection(2, fm.width(headers.at(2) + "  "));
    header->resizeSection(3, qMax(fm.width(headers.at(3) + "  "), fm.width(" 1234.0 KB/s ")));
    header->resizeSection(4, qMax(fm.width(headers.at(4) + "  "), fm.width(" 1234.0 KB/s ")));
    header->resizeSection(5, qMax(fm.width(headers.at(5) + "  "), fm.width(tr("Downloading") + "  ")));

    // 设置action的图标
    QAction *newTorrentAction = new QAction(QIcon(":/icons/download1.png"), tr("添加种子(&A)"), this);
    pauseTorrentAction = new QAction(QIcon(":/icons/pause.png"), tr("暂停(&P)"), this);
    removeTorrentAction = new QAction(QIcon(":/icons/delete.png"), tr("删除种子(&D)"), this);

    // 文件菜单的功能
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(newTorrentAction);
    fileMenu->addAction(pauseTorrentAction);
    fileMenu->addAction(removeTorrentAction);
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon(":/icons/exit.png"), tr("退出(&E)"), this, SLOT(close()));

    // 帮助菜单栏的内容：主要为一些自己的信息，软件信息等的内容
    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(QIcon(":/icons/about.png"), tr("关于(&A)"), this, &MainWindow::about);
    helpMenu->addAction(tr("关于 &Qt"), qApp, SLOT(aboutQt()));

    // 工具栏的动作绑定
    ui->mainToolBar->setMovable(false);
    ui->mainToolBar->addAction(newTorrentAction);
    ui->mainToolBar->addAction(removeTorrentAction);
    ui->mainToolBar->addAction(pauseTorrentAction);
    ui->mainToolBar->addSeparator();
    upActionTool = ui->mainToolBar->addAction(QIcon(":/icons/uparrow.png"), tr("向上移动"));
    downActionTool = ui->mainToolBar->addAction(QIcon(":/icons/downarrow.png"), tr("向下移动"));

    // 连接信号的操作 file menu
    connect(newTorrentAction, &QAction::triggered,
            this, static_cast<bool (MainWindow::*)()>(&MainWindow::addTorrent));
    connect(removeTorrentAction, &QAction::triggered,
            this, &MainWindow::removeTorrent);
    connect(pauseTorrentAction, &QAction::triggered,
            this, &MainWindow::pauseTorrent);
    connect(ui->treeOfDownloading, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::setActionsEnabled);

    // 设置第tablewidget中第二个table中的treewidget的标题
    QStringList header2;
    header2 << tr("种子文件") /*<< tr("查看文件") << tr("删除任务")*/;
    ui->treeOfDownloaded->setHeaderLabels(header2);
    ui->treeOfDownloaded->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->treeOfDownloaded->setAlternatingRowColors(false);
    ui->treeOfDownloaded->setRootIsDecorated(false);
    QHeaderView *headerView2 = ui->treeOfDownloaded->header();
    headerView2->resizeSection(0, fm.width("typical-name-for-a-torrent.torrent"));
//    headerView2->resizeSection(1, fm.width(header2.at(1) + "             "));
//    headerView2->resizeSection(2, fm.width(header2.at(2)));

    // 动作和属性
    mouseRightMenu = new QMenu();
    openDownloadFileDir = new QAction(QIcon(":/icons/open_file.png"), tr("打开下载文件目录"));
    removeTorrentItem = new QAction(QIcon(":/icons/cdelete.png"), tr("删除当前任务"));
    removeTorrentItemAndFiles = new QAction(QIcon(":/icons/delete-file.png"), tr("删除任务以及下载文件"));
    mouseRightMenu->addAction(openDownloadFileDir);
    mouseRightMenu->addSeparator();
    mouseRightMenu->addAction(removeTorrentItem);
    mouseRightMenu->addAction(removeTorrentItemAndFiles);

    // 第三个tab
    QStringList header3;
    header3 << tr("种子文件");
    ui->treeOfTrash->setHeaderLabels(header3);
    ui->treeOfTrash->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->treeOfTrash->setAlternatingRowColors(false);
    ui->treeOfTrash->setRootIsDecorated(false);
    QHeaderView *headerView3 = ui->treeOfTrash->header();
    headerView3->resizeSection(0, fm.width("typical-name-for-a-torrent.torrent"));

    trashMenu = new QMenu();
    trashDelete = new QAction(QIcon(":/icons/delete-file.png"), tr("删除任务及文件"));
    trashMenu->addAction(trashDelete);

    // 鼠标右键菜单信号
    connect(ui->treeOfDownloaded, &QTreeWidget::itemPressed,
            this, &MainWindow::treeWidgetItemPressed);
    connect(openDownloadFileDir, &QAction::triggered,
            this, &MainWindow::openDownloadFileDirHandler);
    connect(removeTorrentItem, &QAction::triggered,
            this, &MainWindow::removeTorrentItemHandler);
    connect(removeTorrentItemAndFiles, &QAction::triggered,
            this, &MainWindow::removeTorrentItemAndFilesHandler);

    // trash tab 中的item点击鼠标右键时的动作
    connect(ui->treeOfTrash, &QTreeWidget::itemPressed,
            this, &MainWindow::trashTreeWidgetItemPressed);
    connect(trashDelete, &QAction::triggered,
            this, &MainWindow::removeTorrentItemAndFilesHandler);

    // tabWidget
    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::tabChangeHandler);
    setWindowTitle(tr("Torrent Client"));
    setActionsEnabled();
    QMetaObject::invokeMethod(this, "loadSettings", Qt::QueuedConnection);
}

MainWindow::~MainWindow()
{
    delete ui;
}

const Torrent *MainWindow::clientForRow(int row) const
{
    // 计算某个下载条目在当前客户端的位置，就是第几行
    return jobs.at(row).torrent;
}

// 当用户关闭窗口的时候保存客户端的那个前正在执行的所有任务
// 以及用户相关的使用习惯的一些默认数据
void MainWindow::closeEvent(QCloseEvent */*event*/)
{
    saveChanges = true;
    saveSettings();
    saveChanges = false;
}

void MainWindow::treeWidgetItemPressed(QTreeWidgetItem */*pressedItem*/, int /*colum*/)
{
    if (qApp->mouseButtons() == Qt::RightButton) {
        mouseRightMenu->exec(QCursor::pos());
    }
}

void MainWindow::trashTreeWidgetItemPressed(QTreeWidgetItem */*pressedItem*/, int /*colum*/)
{
    if (qApp->mouseButtons() == Qt::RightButton) {
        trashMenu->exec(QCursor::pos());
    }
}

bool MainWindow::addTorrent()
{
    // 种子文件选择窗口
    QString fileName = QFileDialog::getOpenFileName(this, tr("选者种子文件"),
                                                    lastDirectory,
                                                    tr("Torrents (*.torrent);;"
                                                       " All files (*.*)"));

    if (fileName.isEmpty())
        return false;
    lastDirectory = QFileInfo(fileName).absolutePath();

    // 弹出下载种子文件的一些具体的信息
    AddTorrentDialog *addTorrentDialog = new AddTorrentDialog(this);
    addTorrentDialog->setTorrent(fileName);
    addTorrentDialog->deleteLater();
    if (!addTorrentDialog->exec())
        return false;

    // 添加一个种子到下载列表
    addTorrent(fileName, addTorrentDialog->destinationFolder());
    if (!saveChanges) {
        saveChanges = true;
        QTimer::singleShot(1000, this, SLOT(saveSettings()));
    }

    ui->tabWidget->setCurrentIndex(0);
    return true;
}

void MainWindow::pauseTorrent()
{
    int row = ui->treeOfDownloading->indexOfTopLevelItem(ui->treeOfDownloading->currentItem());
    Torrent *client = jobs.at(row).torrent;
    client->setPaused(client->state() != Torrent::Paused);
    setActionsEnabled();
}

void MainWindow::removeTorrent()
{
    int row = ui->treeOfDownloading->indexOfTopLevelItem(ui->treeOfDownloading->currentItem());
    Torrent *client = jobs.at(row).torrent;

    client->removeHandler();
    client->disconnect();
    client->deleteLater();

    delete ui->treeOfDownloading->takeTopLevelItem(row);
    jobs.removeAt(row);

    saveChanges = true;
    saveSettings();
}

void MainWindow::about()
{
    QLabel *icon = new QLabel;
    icon->setPixmap(QPixmap(":/icons/p2p.png"));

    QLabel *text = new QLabel;
    text->setWordWrap(true);
    text->setText("<p>应用: <strong>BTX</strong><br/>"
                  "版本: <label style=\"color: blue;\">v1.0.0</label><br/>"
                  "作者: Aicler<br/>"
                  "描述: 一个简单的BT下载工具</p>");

    QPushButton *quitButton = new QPushButton("确定");

    QVBoxLayout *topLayout = new QVBoxLayout;
    topLayout->setMargin(10);
    topLayout->setSpacing(10);
    topLayout->addWidget(icon);
    topLayout->addWidget(text);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch();
    bottomLayout->addWidget(quitButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(bottomLayout);

    QDialog about(this);
    about.setModal(true);
    about.setWindowTitle(tr("关于客户端"));
    about.setLayout(mainLayout);
    about.setFixedSize(250, 250);

    connect(quitButton, &QPushButton::clicked, &about, &QDialog::close);

    about.exec();
}

void MainWindow::loadSettings()
{
    QSettings settings("QtBtProject", "Btx");

    // 加载用户的默认设置
    setGeometry(settings.value("geometry", QRect(400, 200, 600, 300)).toRect());

    appInstallPath = settings.value("appInstallPath").toString();

    lastDirectory = settings.value("LastDirectory").toString();
    if (lastDirectory.isEmpty())
        lastDirectory = QDir::currentPath();

    // 恢复之前的下载
    int size = settings.beginReadArray("Torrents");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString fileName = settings.value("sourceFileName").toString();
        QString dest = settings.value("destinationFolder").toString();

        addTorrent(fileName, dest);
    }
    settings.endArray();

    size = settings.beginReadArray("CTorrents");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString fileName = settings.value("sourceFileName").toString();
        QString dest = settings.value("destinationFolder").toString();
        bool statu = settings.value("statu").toBool();

        if (statu)
            addCompleteTorrent(fileName, dest, statu);
        else
            addTrashTask(fileName, dest, statu);
    }
    settings.endArray();

//    size = settings.beginReadArray("TrashTask");
//    for (int i = 0; i < size; ++i) {
//        settings.setArrayIndex(i);
//        QString fileName = settings.value("sourceFileName").toString();
//        QString dest = settings.value("destinationFolder").toString();

//        addTrashTask(fileName, dest);
//    }
//    settings.endArray();
}

void MainWindow::saveSettings()
{
    if (!saveChanges)
        return;
    saveChanges = true;

    // 保存一些相关的设置
    QSettings settings("QtBtProject", "Btx");
    settings.clear();

    // 保存应用程序的安装目录
    QString appInstallPath = QDir::currentPath();
    settings.setValue("appInstallPath", appInstallPath);

    // 保存用户使用窗口的一些默认的值
    settings.setValue("geometry", geometry());
    // 保存用户最后选取文件的目录
    settings.setValue("LastDirectory", lastDirectory);

    // 保存当前客户端正在下载的种子文件的信息
    settings.beginWriteArray("Torrents");
    for (int i = 0; i < jobs.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("sourceFileName", jobs.at(i).torrentFileName);
        settings.setValue("destinationFolder", jobs.at(i).destinationDirectory);
    }
    settings.endArray();

    settings.beginWriteArray("CTorrents");
    for (int i = 0; i < done.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("sourceFileName", done.at(i).torrentFileName);
        settings.setValue("destinationFolder", done.at(i).destinationDirectory);
        settings.setValue("statu", done.at(i).statu);
    }
    settings.endArray();

//    settings.beginWriteArray("TrashTask");
//    for (int i = 0; i < trash.size(); i++) {
//        settings.setArrayIndex(i);
//        settings.setValue("sourceFileName", trash.at(i).torrentFileName);
//        settings.setValue("destinationFolder", trash.at(i).destinationDirectory);
//    }
//    settings.endArray();
    settings.sync();
}

void MainWindow::setActionsEnabled()
{
    // 找到当前下载列表中相应的条目并且更新其状态
    QTreeWidgetItem *item = 0;
    if (!ui->treeOfDownloading->selectedItems().isEmpty())
        item = ui->treeOfDownloading->selectedItems().first();
    Torrent *client = item ? jobs.at(ui->treeOfDownloading->indexOfTopLevelItem(item)).torrent : 0;
    bool pauseEnabled = client && ((client->state() == Torrent::Paused)
                                   ||  (client->state() > Torrent::Preparing));

    removeTorrentAction->setEnabled(item != 0);
    pauseTorrentAction->setEnabled(item != 0 && pauseEnabled);

    if (client && client->state() == Torrent::Paused) {
        pauseTorrentAction->setIcon(QIcon(":/icons/resume.png"));
        pauseTorrentAction->setText(tr("恢复下载"));
    } else {
        pauseTorrentAction->setIcon(QIcon(":/icons/pause.png"));
        pauseTorrentAction->setText(tr("暂停下载"));
    }

    int row = ui->treeOfDownloading->indexOfTopLevelItem(item);
    upActionTool->setEnabled(item && row != 0);
    downActionTool->setEnabled(item && row != jobs.size() - 1);
}

void MainWindow::updateProgress(int percent)
{
    Torrent *client = qobject_cast<Torrent *>(sender());
    int row = rowOfClient(client);

    // 更新下载的进度条
    QTreeWidgetItem *item = ui->treeOfDownloading->topLevelItem(row);
    if (item)
        item->setText(2, QString::number(percent));
}

void MainWindow::updateState(Torrent::State)
{
    Torrent *client = qobject_cast<Torrent *>(sender());
    int row = rowOfClient(client);

    QTreeWidgetItem *item = ui->treeOfDownloading->topLevelItem(row);
    if (item)
        item->setText(5, client->stateString());
}

void MainWindow::updatePeerInfo()
{
    // 更新种子文件在下载过程中连接信息的改变
    Torrent *client = qobject_cast<Torrent *>(sender());
    int row = rowOfClient(client);

    QTreeWidgetItem *item = ui->treeOfDownloading->topLevelItem(row);
    item->setText(1, tr("%1/%2").arg(client->connectedPeerCount())
                  .arg(client->seedCount()));
}

void MainWindow::updateRate()
{
    Torrent *client = qobject_cast<Torrent *>(sender());
    int row = rowOfClient(client);

    ui->treeOfDownloading->topLevelItem(row)->setText(3, AddTorrentDialog::stringNumber((qint64)client->getDownRate()) + "/s");
    ui->treeOfDownloading->topLevelItem(row)->setText(4, AddTorrentDialog::stringNumber((qint64)client->getUpRate()) + "/s");
}

void MainWindow::taskDoneHandler()
{
    Torrent *client = qobject_cast<Torrent *>(sender());
    int row = rowOfClient(client);

    QTreeWidgetItem *item = nullptr;
    if (row >= 0 && row < jobs.size()) {
        delete ui->treeOfDownloading->takeTopLevelItem(row);

        for (Job job: done) {
            if (job.torrentFileName == jobs.at(row).torrentFileName &&
                    job.destinationDirectory == jobs.at(row).destinationDirectory)
            {
                QMessageBox::warning(this, tr("已完成!"),
                                     tr("种子文件 %1 已经在下载完成列表了")
                                     .arg(job.torrentFileName.mid(job.torrentFileName.lastIndexOf("/") + 1)));
                return;
            }
        }

        QString torretFilePath = jobs.at(row).torrentFileName;
        taskDoneNoteDialog(torretFilePath.mid(torretFilePath.lastIndexOf("/") + 1));

        done << jobs.at(row);
        item = new QTreeWidgetItem(ui->treeOfDownloaded);
        item->setText(0, jobs.at(row).torrentFileName);
        jobs.removeAt(row);

        saveChanges = true;
        saveSettings();
         ui->tabWidget->setCurrentIndex(1);
    }
}

void MainWindow::taskDoneNoteDialog(const QString &fileName)
{
    QDialog *dialog = new QDialog();
    int deskW = QApplication::desktop()->screenGeometry().width();
    int deskH = QApplication::desktop()->screenGeometry().height();
    dialog->setGeometry(deskW - 310, deskH- 160, 300, 150);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QGridLayout *gLayout = new QGridLayout;
    QLabel *label = new QLabel(fileName);
    gLayout->addWidget(label);
    dialog->setLayout(gLayout);
    dialog->setWindowTitle("NOTE");

    QTimer::singleShot(3000, dialog, &QDialog::close);
    dialog->show();
}

void MainWindow::openDownloadFileDirHandler()
{
    int row = ui->treeOfDownloaded->indexOfTopLevelItem(ui->treeOfDownloaded->currentItem());
    QString torrentFilePath = done.at(row).destinationDirectory;
    QDesktopServices::openUrl(QUrl("file://" + torrentFilePath, QUrl::TolerantMode));
}

void MainWindow::removeTorrentItemHandler()
{
    int row = ui->treeOfDownloaded->indexOfTopLevelItem(ui->treeOfDownloaded->currentItem());
    Torrent *client = done.at(row).torrent;
    if (client != nullptr) {
        client->removeHandler();
        client->disconnect();
        client->deleteLater();
    }

    // 移除至trash 标签中显示
    trash << done.at(row);
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeOfTrash);
    item->setText(0, done.at(row).torrentFileName);

    delete ui->treeOfDownloaded->takeTopLevelItem(row);
//    done.removeAt(row);
    done[row].statu = false;

    saveChanges = true;
    saveSettings();
}

void MainWindow::removeTorrentItemAndFilesHandler()
{
    QAction* ac = qobject_cast<QAction *>(sender());

    // 删除前的确认消息
    QMessageBox msg;
    msg.setText("你确定要删除下载任务以及文件吗？");
    msg.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    msg.setDefaultButton(QMessageBox::Cancel);

    if (msg.exec() == QMessageBox::Cancel)
        return;

    QString torrentFileName;
    QString destinationFolder;
    if (ac == trashDelete) {
        int row = ui->treeOfTrash->indexOfTopLevelItem(ui->treeOfTrash->currentItem());
        Torrent *client = trash.at(row).torrent;

        torrentFileName = trash.at(row).torrentFileName;
        destinationFolder = trash.at(row).destinationDirectory;

        if (client != nullptr) {
            client->removeHandler();
            client->disconnect();
            client->deleteLater();
        }

        delete ui->treeOfTrash->takeTopLevelItem(row);
        trash.removeAt(row);
    } else {
        // 根据程为序的设计将保存文件下载进度的位图文件保存在程序的安装目录
        int row = ui->treeOfDownloaded->indexOfTopLevelItem(ui->treeOfDownloaded->currentItem());
        Torrent *client = done.at(row).torrent;

        torrentFileName = done.at(row).torrentFileName;
        destinationFolder = done.at(row).destinationDirectory;
        if (client != nullptr) {
            client->removeHandler();
            client->disconnect();
            client->deleteLater();
        }

        delete ui->treeOfDownloaded->takeTopLevelItem(row);
        done.removeAt(row);
    }


    saveChanges = true;
    saveSettings();

    QSettings settings("QtBtProject", "torrent-bitmap");
    settings.beginGroup(torrentFileName);
    QString bitmap = settings.value("bitmap").toString();
    QString name = settings.value("name").toString();
    settings.endGroup();
    settings.remove(torrentFileName);

    if (!bitmap.isEmpty()) {
        QString bitmapFilePath = ".btx/" + bitmap;
        if (!name.isEmpty()) {
            QString downloadFilePath = destinationFolder + "/" + name;
            FileUtils::deleteTarget(bitmapFilePath);
            FileUtils::deleteTarget(downloadFilePath);
        }
    }
}

void MainWindow::tabChangeHandler(int index)
{
    if (0 != index) {
        removeTorrentAction->setEnabled(false);
        pauseTorrentAction->setEnabled(false);
    } else {
        setActionsEnabled();
    }
}

int MainWindow::rowOfClient(Torrent *client) const
{
    int row = 0;
    foreach (Job job, jobs) {
        if (job.torrent == client)
            return row;
        ++row;
    }
    return -1;
}

bool MainWindow::addTorrent(const QString &fileName, const QString &destinationFolder)
{
    foreach (Job job, jobs) {
        if (job.torrentFileName == fileName && job.destinationDirectory == destinationFolder) {
            QMessageBox::warning(this, tr("已经在下载了!"),
                                 tr("种子文件 %1 已经在下载列表了").arg(fileName));
            return false;
        }
    }

    // 创建一个新的种子下载条目，并且解析种子文件中的信息
    Torrent *torrent = new Torrent(this);
    if (!torrent->setTorrent(fileName)) {
        QMessageBox::warning(this, tr("错误"), tr("种子文件 %1 打开失败.").arg(fileName));
        delete torrent;
        return false;
    }
    torrent->setDestinationFolder(destinationFolder);

    // 设置客户端相应的槽
    connect(torrent, &Torrent::progressUpdated,
            this, &MainWindow::updateProgress);
    connect(torrent, &Torrent::stateChanged,
            this, &MainWindow::updateState);
    connect(torrent, &Torrent::updatePeerAndSeed,
            this, &MainWindow::updatePeerInfo);
    connect(torrent, &Torrent::updateRate,
            this, &MainWindow::updateRate);
    connect(torrent, &Torrent::taskDone,
            this, &MainWindow::taskDoneHandler);

    // 添加一个下载条目
    Job job;
    job.torrent = torrent;
    job.torrentFileName = fileName;
    job.destinationDirectory = destinationFolder;
    job.statu = true;
    jobs << job;

    // 在客户端更新下载的条目
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeOfDownloading);

    QString baseFileName = QFileInfo(fileName).fileName();
    if (baseFileName.toLower().endsWith(".torrent"))
        baseFileName.remove(baseFileName.size() - 8);

    item->setText(0, baseFileName);
    item->setToolTip(0, tr("Torrent: %1<br>Destination: %2")
                     .arg(baseFileName).arg(destinationFolder));
    item->setText(1, tr("0/0"));
    item->setText(2, "0");
    item->setText(3, "0.0 KB/s");
    item->setText(4, "0.0 KB/s");
    item->setText(5, tr("Idle"));
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(1, Qt::AlignHCenter);

    torrent->start();

    return true;
}

void MainWindow::addCompleteTorrent(const QString &fileName, const QString &destinationFolder, const bool statu)
{
    Job job;
    job.torrent = nullptr;
    job.torrentFileName = fileName;
    job.destinationDirectory = destinationFolder;
    job.statu = statu;
    done << job;

    QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeOfDownloaded);
    item->setText(0, job.torrentFileName);
}

void MainWindow::addTrashTask(const QString &fileName, const QString &destinationFolder, const bool statu)
{
    Job job;
    job.torrent = nullptr;
    job.torrentFileName = fileName;
    job.destinationDirectory = destinationFolder;
    job.statu = statu;
    trash << job;

    QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeOfTrash);
    item->setText(0, fileName);
}

#include "mainwindow.moc"
