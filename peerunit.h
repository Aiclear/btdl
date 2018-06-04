#ifndef PEERDATA_H
#define PEERDATA_H

#include "bitmapofpieces.h"

#include <QString>
#include <QByteArray>
#include <QList>
#include <QBitArray>

struct RequestPiece
{
    RequestPiece() {}
    RequestPiece(int index, int begin, int length)
            :index(index), begin(begin), length(length)
    { }

    bool operator ==(const RequestPiece &other)
    {
        return index == other.index &&
                begin == other.begin;
    }

    int index = 0;
    int begin = 0;
    int length = 0;
};

struct Peer
{
    Peer() = default;
    ~Peer()
    {
        if (peerBitmap != nullptr)
            delete peerBitmap;

        peerBitmap = nullptr;
    }

    enum PeerState {
        INITIAL = -1, // 表明处于初始化状态
        HALFSHAKED = 0, // 处于半握手状态
        HANDSHAKED = 1, // 握手完成
        SENDBITFILED = 2, // 发送位图的状态
        RECVBITFILED = 3, // 接受对方发送位图的状态
        DATA = 4, // 处于传送数据的状态
        CLOSING = 5  // peer已经关闭
    };

    QString ip; // peer的ip
    quint16 port; // peer的端口
    QString peerId; // peer的id

    int state = PeerState::INITIAL; // 当前的状态

    int amChocking = 1; // 该值为1时表明将peer阻塞，为0则相反
    int amInterested = 0; // 为1时表明对peer感兴趣
    int peerChocking = 1; // 为1时表明peer将客户端阻塞
    int peerInterested = 0; // 为1时peer对客户段感兴趣

    BitMapOfPieces *peerBitmap = new BitMapOfPieces; // peer的位图

    QList<RequestPiece *> requestPiece; // 请求peer数据的队列
    QList<RequestPiece *> requestedPiece; // 被peer请求的数据的队列

    QByteArray recvBuff; // 存放peer发送给自的消息的缓冲区
    QByteArray messageBuff; // 分析接收缓冲区后将一条完整的消息法如此缓冲区
    QByteArray sendBuff; // 发送给peer的消息的缓冲区
    QByteArray sendBuffCopy; // 发送消息的缓冲区

    quint64 downTotal = 0; // 从该peer接收的总的字节数
    quint64 upTotal = 0;  // 向该peer上传的总的字节数

    time_t startTimestamp = 0;       // 最近一次接收到peer消息的时间
    time_t recetTimestamp = 0;       // 最近一次发送消息给peer的时间

    time_t lastDownTimestamp = 0;   // 最近下载数据的开始时间
    time_t lastUpTimestamp = 0;     // 最近上传数据的开始时间
    qint64 downCount = 0;            // 本计时周期从peer下载的数据的字节数
    qint64 upCount = 0;              // 本计时周期向peer上传的数据的字节数
    float  downRate = 0.0;             // 本计时周期从peer处下载数据的速度
    float  upRate = 0.0;               // 本计时周期向peer处上传数据的速度
    int have = 0;
};

#endif // PEERDATA_H
