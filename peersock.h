#ifndef PEERSOCK_H
#define PEERSOCK_H

#include "bitmapofpieces.h"
#include "peerunit.h"
#include "messagehandler.h"

#include <QTcpSocket>

#include <iostream>

class PeerSock : public QObject
{
    Q_OBJECT

public:
    PeerSock();

    void setMessageHandler(MessageHandler *value) { messageHandler = value; }
    void createPeerSocket(QHash<QString, quint16> &peerInfo); // 根据tracker返回的peer信息然后构造peer的对象

    // slots
    void readMsg(); // 处理peer返回的消息
    void connetSuccess(); // peer 的tcp连接成功
    void connectionError(QAbstractSocket::SocketError); // 处理连接发生错误的peer的socket
    void processPeerMsg(QTcpSocket *socket, Peer *peer);
    void closePeerConnect(QTcpSocket *sock, Peer *peer);
    void closeAllPeerConnect();

    void computePeerRate();
    void computeTotalRate();

    QHash<QTcpSocket *, Peer *> & getPeers();

public slots:
    void prepareHaveMsg(qint32 index);
    void checkPeerReq(bool &find, int index);

signals:
    void peerEmpty();
    void peerSockState(bool &flag);
    void peerAndSeed(int peers, int seed);
    void upDownRate(float upRate, float downRate);

private:
    MessageHandler *messageHandler; // 与peer进行交互，分析消息的发送
    QHash<QTcpSocket *, Peer *> peers; // 连接peer的集合

    bool stateFlag = true;
    int seeds = 0;
};

#endif // PEERSOCK_H
