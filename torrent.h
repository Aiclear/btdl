#ifndef TORRENT_H
#define TORRENT_H

#include "metainfo.h"
#include "tracker.h"
#include "peersock.h"
#include "messagehandler.h"
#include "policy.h"
#include "processdata.h"

#include <QTimer>
#include <QObject>

/*
 * 一个Torrent类代表一个种子的下载任务，以及
 * 种子下载过程中所使用到的资源的控制类
 */
class Torrent : public QObject
{
    Q_OBJECT

public:
    enum State { // 表示当前种子的下载状态
        Idle,        // 空闲态
        Paused,      // 暂停
        Stopping,    // 停止下载
        Preparing,   // 准备下载
        Searching,   // 搜索资源
        Connecting,  // 连接服务器
        WarmingUp,   // 连接peer
        Downloading, // 开始数据下载
        Endgame,     // endgame 模式
        Seeding      // 下载完成，成为作种者
    };

    explicit Torrent(QObject *parent = nullptr);

    // 设置种子文件信息的来源
    bool setTorrent(const QString &torrentFile);
    bool setTorrent(const QByteArray &torrentData);

    // 生成种子客户端的随机ID
    void producePeerId();
    // 设置种子下载文件的保存位置
    void setDestinationFolder(const QString &value);

    // 更新种子下载进度条的信息
    int progress() const;

    // 获取以及更新当前种子的状态
    void setState(Torrent::State state);
    State state() const;
    QString stateString() const;
    void setPaused(bool paused);

    // 统计种子当前的客户端连接数和作种者的数量
    int connectedPeerCount();
    int seedCount();

    // 判断文件下载的状态，也就是如果这是一个断点续传的下载
    // 文件的话，可能已经下载了部分的数据
    bool resumeDownloaded();
    // 开启一个种子文件的下载
    void start();

    // 获取当前种子文件下载的速率
    float getUpRate() const;
    float getDownRate() const;

    // 当一个任务被一处时，作出相应的处理
    void removeHandler();

    // ...
    void pause();
    void restorm();

    // 处理各个模块的信号
public slots:
    void updateProgress(int progress);
    void handleState(bool &flag);
    void handlePeerAndSeed(int peer, int seed);
    void updateUpDownRate(float upRate, float downRate);
    void complete();

    // 种子下载过程中的状态变化更新的信号
signals:
    void stateChanged(Torrent::State state);
    void peerInfoUpdated();
    void progressUpdated(int percentProgress);
    void updatePeerAndSeed();
    void updateRate();
    void taskDone();

private:
    // 下载数据保存的位置
    QString destinationFolder;

    // 单独一个piece的信息
    PieceValue pieceValue;
    // download progress
    float upRate;
    float downRate;
    // peer的数量
    int peers;
    // peer中的种子数量
    int seeds;
    // 进度条的值
    int lastProgressValue;
    State pstate = State::Idle;
    QString pstateString;

    // 种子文件的名字
    QString torrentFileName;

    // 保存握手时需要的数据
    QByteArray infoHash;
    QByteArray peerId;

    // 保存种子文件中的信息
    MetaInfo metaInfo;

    // 位图
    BitMapOfPieces *bitmap = nullptr;
    // 负责与Tracker服务器进行交互
    Tracker tracker;
    // 与peer进行连接
    PeerSock peerSock;
    // 与peer进行信息的交互
    MessageHandler messageHandler;
    // 生成下载数据的策率
    Policy policy;
    // 处理下载到的文件数据
    ProcessData processData;

    // 定时器
    QTimer trackerTimer;
    QTimer peerRateTimer;
    QTimer totalRateTimer;
};

#endif // TORRENT_H
