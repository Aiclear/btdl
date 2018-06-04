#include "peersock.h"
#include "policy.h"

static const qint64 RECVMSGBUFF = 2 * 1024 + 16 * 1024; // 接收消息的缓冲区大小
static const qint64 RECVMSGLIMIT = 18 * 1024 - 1500; // 接收缓冲区的最大数据限度

inline static void printfPeerInfo(QTcpSocket &socket, const QString &msg)
{
    printf("\x1b[31m-[Peer %s:%d] %s\x1b[0m\n",
           socket.peerName().toUtf8().data(),
           socket.peerPort(),
           msg.toUtf8().data());
}

PeerSock::PeerSock()
{
}

void PeerSock::createPeerSocket(QHash<QString, quint16> &peerInfo)
{ // 创建peer和对应的socket
    Peer *peer = nullptr;
    QTcpSocket *sock = nullptr;

    if (!peerInfo.isEmpty()) { // 创建到peer的连接
        // 找到新的peer的连接
        QHashIterator<QString, quint16> iterOfPeerInfo(peerInfo);
        if (!peers.isEmpty()) {
            QHashIterator<QTcpSocket *, Peer *> iterator(peers);
            while (iterOfPeerInfo.hasNext()) {
                iterOfPeerInfo.next();
                while (iterator.hasNext()) {
                    iterator.next();
                    if (iterator.key()->peerName() == iterOfPeerInfo.key() &&
                            iterator.key()->peerPort() == iterOfPeerInfo.value())
                    {
                        peerInfo.remove(iterOfPeerInfo.key());
                        break;
                    }
                }
                iterator.toFront();
            }
        }

        if (peerInfo.isEmpty())
            return;
        iterOfPeerInfo.toFront();
        if (peers.size() < 50)
            while (iterOfPeerInfo.hasNext()) {
                iterOfPeerInfo.next();
                try {
                    sock = new QTcpSocket;
                    peer = new Peer;
                } catch (std::bad_alloc &e) {
                    printf("[%d:%s]\n", __LINE__, e.what());
                }
                connect(sock, &QTcpSocket::connected,
                        this, &PeerSock::connetSuccess);
                connect(sock, &QTcpSocket::readyRead,
                        this, &PeerSock::readMsg, Qt::QueuedConnection);
                connect(sock,
                        static_cast<void (QTcpSocket:: *)(QAbstractSocket::SocketError)>(&QTcpSocket::error),
                        this,
                        &PeerSock::connectionError);
                connect(sock, &QTcpSocket::disconnected,
                        sock, &QTcpSocket::deleteLater);
                peer->ip = iterOfPeerInfo.key();
                peer->port = iterOfPeerInfo.value();
                sock->connectToHost(peer->ip, peer->port);
                peers.insert(sock, peer);

                if (peers.size() >= 50)
                    break;
            }
    }

    peerInfo.clear();

    if (stateFlag)
        emit peerSockState(stateFlag);
}

void PeerSock::connetSuccess()
{ // 连接peer成功后的处理方法
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    Peer *peer = peers.value(socket);

    messageHandler->createResponseMessage(*peer);
    socket->write(peer->sendBuff);
    peer->startTimestamp = time(NULL);
    peer->sendBuff.clear();
}

void PeerSock::readMsg()
{ // 处理peer发送的消息
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    Peer *peer = peers.value(sock);

    peer->recvBuff.append(sock->read(RECVMSGBUFF - (peer->recvBuff.size())));
    processPeerMsg(sock, peer);
}

void PeerSock::processPeerMsg(QTcpSocket *socket, Peer *peer)
{
    int completeLen;
    bool completed = messageHandler->isCompleteMessage(*peer, completeLen);
    if (completed) {
        messageHandler->parseResponseMessage(*peer);
    } else if (peer->recvBuff.size() /*>= RECVMSGLIMIT*/) {
        messageHandler->parseResponseUncompleteMessage(*peer, completeLen);
    } else {
        peer->startTimestamp = time(NULL);
    }

    if (peer->state != Peer::CLOSING) {
        if (peer->sendBuffCopy.isEmpty()) {
            messageHandler->createResponseMessage(*peer);
            if (!peer->sendBuff.isEmpty()) {
                peer->sendBuffCopy = peer->sendBuff;
                peer->sendBuff.clear();
            }
        }
        if (peer->sendBuffCopy.size() > 1024) {
            socket->write(peer->sendBuffCopy.mid(0, 1023));
            peer->sendBuffCopy = peer->sendBuffCopy.mid(1024);
            peer->recetTimestamp = time(NULL);
        } else if (peer->sendBuffCopy.size() <= 1024 &&
                   !peer->sendBuffCopy.isEmpty())
        {
            socket->write(peer->sendBuffCopy);
            peer->sendBuffCopy.clear();
            peer->recetTimestamp = time(NULL);
        }
    } else
        closePeerConnect(socket, peer);
}

void PeerSock::connectionError(QAbstractSocket::SocketError /*sockError*/)
{ // peer连接出错
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    Peer *peer = peers.value(socket);

    closePeerConnect(socket, peer);

    if (peers.isEmpty()) {
        emit peerEmpty();
    }
}

void PeerSock::computePeerRate()
{
    time_t tt;
    time_t timeNow = time(NULL);
    Peer *peer = nullptr;
    QHashIterator<QTcpSocket *, Peer *> peersIter(peers);
    while (peersIter.hasNext()) {
        peersIter.next();
        peer = peersIter.value();
        // 判断是否为种子
        if (Policy::isSeed(*peer))
            seeds++;

        if (peer->lastDownTimestamp == 0) { // 说明还未下载
            peer->downRate = 0.0f;
            peer->downCount = 0;
        } else {
            // 最近下载数据的时间到现在的时间差，如果时间差为零，则
            // 说明当前没有收到任何的新的数据
            tt = timeNow - peer->lastDownTimestamp;
            if (tt != 0)
                peer->downRate = (float) (peer->downCount / tt);
            // 从新开始统计下载的数据
            peer->downCount = 0;
            peer->lastDownTimestamp = 0;
        }

        if (peer->lastUpTimestamp == 0) { // 未有数据上传
            peer->upRate = 0.0f;
            peer->upCount = 0;
        } else {
            tt = timeNow - peer->lastUpTimestamp;
            if (tt != 0)
                peer->upRate = (float) (peer->upCount / tt);

            peer->upCount = 0;
            peer->lastUpTimestamp = 0;
        }
    }

    // 更新peer和seed的数值
    emit peerAndSeed(peers.size(), seeds);
    seeds = 0;
}

void PeerSock::computeTotalRate()
{
    float upRate = 0.0f;
    float downRate = 0.0f;

    Peer *peer = nullptr;
    QHashIterator<QTcpSocket *, Peer *> iter(peers);
    while (iter.hasNext()) {
        iter.next();
        peer = iter.value();

        upRate += peer->upRate;
        downRate += peer->downRate;
    }

    emit upDownRate(upRate, downRate);
}

QHash<QTcpSocket *, Peer *> &PeerSock::getPeers()
{
    return peers;
}

void PeerSock::closePeerConnect(QTcpSocket *tcpSocket, Peer *peer)
{
    peers.remove(tcpSocket);

    tcpSocket->disconnectFromHost();
    tcpSocket->close();
    tcpSocket->deleteLater();

    if (peer != nullptr)
        delete peer;
}

void PeerSock::prepareHaveMsg(qint32 index)
{
    QHashIterator<QTcpSocket *, Peer *> hashIterator(peers);
    while (hashIterator.hasNext()) {
        hashIterator.next();
        if (hashIterator.value()->state != Peer::CLOSING) {
            messageHandler->createHaveMessage(*hashIterator.value(), index);
        }
    }
}

void PeerSock::checkPeerReq(bool &find, int index)
{
    QHashIterator<QTcpSocket *, Peer *> iter(peers);
    while (iter.hasNext()) {
        iter.next();
        for (int i = 0; i < iter.value()->requestPiece.size(); ++i) {
            if (iter.value()->requestPiece.at(i)->index == index) {
                find = true;
                break;
            }
        }
        if (find)
            break;
    }
}

void PeerSock::closeAllPeerConnect()
{
    if (peers.isEmpty())
        return;
    QHashIterator<QTcpSocket *, Peer *> iter(peers);
    while (iter.hasNext()) {
        iter.next();
        iter.key()->abort();
        iter.key()->close();
        iter.key()->deleteLater();

        if (iter.value() != nullptr)
            delete iter.value();

        peers.remove(iter.key());
    }
}
