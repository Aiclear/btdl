#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "torrent.h"

#include <QMainWindow>
#include <QTreeWidget>
#include <QMessageBox>
#include <QSettings>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    const Torrent *clientForRow(int row) const;

protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

private slots:
    // 实现鼠标右键菜单功能
    void treeWidgetItemPressed(QTreeWidgetItem *pressedItem, int colum);
    // trash tab item presed
    void trashTreeWidgetItemPressed(QTreeWidgetItem *pressedItem, int colum);

// 各个按键的绑定函数
private slots:
    // widget 1 action slots
    bool addTorrent();
    void pauseTorrent();
    void removeTorrent();
    // 软件的相关信息展示
    void about();

    // 保存用户的数据
    void loadSettings();
    void saveSettings();

    // 初始化
    void setActionsEnabled();

// 更新下载中种子的状态
private slots:
    void updateProgress(int percent);
    void updateState(Torrent::State);
    void updatePeerInfo();
    void updateRate();

    // 下载完成后的工作
    void taskDoneHandler();
    void taskDoneNoteDialog(const QString &fileName);

// 鼠标右键菜单的action的实现
private slots:
    void openDownloadFileDirHandler();
    void removeTorrentItemHandler();
    void removeTorrentItemAndFilesHandler();

    // tabwidget 事件的响应函数
    void tabChangeHandler(int index);

// 仅在内部使用
private:
    int rowOfClient(Torrent *client) const;
    bool addTorrent(const QString &fileName, const QString &destinationFolder);
    void addCompleteTorrent(const QString &fileName, const QString &destinationFolder, const bool statu);
    void addTrashTask(const QString &fileName, const QString &destinationFolder, const bool statu);

private:
    Ui::MainWindow *ui;

    // 整个wideget功能按键对应的action
    QAction *pauseTorrentAction;
    QAction *removeTorrentAction;
    QAction *upActionTool;
    QAction *downActionTool;

    // 菜单动作仅仅属于treewidget2中的鼠标右键菜单的动作
    QMenu *mouseRightMenu;
    QAction *openDownloadFileDir;
    QAction *removeTorrentItem;
    QAction *removeTorrentItemAndFiles;

    // trash menu
    QMenu *trashMenu;
    QAction *trashDelete;

    struct Job {
        Torrent *torrent;
        QString torrentFileName;
        QString destinationDirectory;
        bool statu;
    };
    // 正在进行的工作
    QList<Job> jobs;
    // 已经完成的工作
    QList<Job> done;
    // 任务被删除但是没有删除下载文件的任务
    QList<Job> trash;

    // 保存用户的每次选择种子文件的目录
    QString lastDirectory;
    QString appInstallPath;

    // 保存用户数据的开关
    bool saveChanges;

    int rightClickItemRow;
};

#endif // MAINWINDOW_H
