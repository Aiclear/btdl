#include "tracker.h"

Tracker::Tracker()
{
}

Tracker::~Tracker()
{
}

inline static QString getHostNameByUrl(const QString &url)
{ // 获取host
    return url.section(":", 1, 1).section("/", 2, 2);
}

quint16 Tracker::parseTrackerUrlPort(const QString &url) const
{
    QStringList strList = url.split(":");
    QString str = strList[strList.length() - 1];
    str = str.section("/", 0, 0);
    if (url.indexOf("https") != -1)
        return static_cast<quint16>(!str.isEmpty() ? str.toInt() : 443);
    else
        return static_cast<quint16>(!str.isEmpty() ? str.toInt() : 80);
}

inline void Tracker::connectTracker(QTcpSocket *socket, const QString url, const quint16 port)
{
    connect(socket, &QTcpSocket::connected,
            this, &Tracker::sendHttpRequest, Qt::QueuedConnection);
    connect(socket, &QTcpSocket::readyRead,
            this, &Tracker::processTrackerConnect, Qt::QueuedConnection);
    connect(socket, static_cast<void (QTcpSocket:: *)(QAbstractSocket::SocketError)>(&QTcpSocket::error),
            this, &Tracker::tcpConnectError, Qt::QueuedConnection);
    connect(socket, &QTcpSocket::disconnected,
            this, &Tracker::disconnectSocket);
    socket->connectToHost(getHostNameByUrl(url), port);
}

void Tracker::initialTrackerConnect()
{ // 创建连接tracker的socket（基于http协议）
    QTcpSocket *tcpSocket = nullptr;
    QListIterator<QTcpSocket *> tcpIterator(tcpSocketList);

    if (metaInfo == nullptr || metaInfo->announceList().isEmpty())
        return;

    if (tcpSocketList.isEmpty()) { // 列表为空时直接集进行创建
        foreach (QString host, metaInfo->announceList()) {
            if (!host.startsWith("http://"))
                continue;
            try {
                tcpSocket = new QTcpSocket;
            } catch (std::bad_alloc &e) {
                printf("%d: %s\n", __LINE__, e.what());
            }
            connectTracker(tcpSocket, host, parseTrackerUrlPort(host));
            tcpSocketList.append(tcpSocket);
        }
    } else { // 否则如果到该主机的连接已经存在的话就跳过该peer
        bool find = false;
        foreach (QString host, metaInfo->announceList()) {
            while (tcpIterator.hasNext()) {
                tcpSocket = tcpIterator.next();
                if (host == tcpSocket->peerName()) {
                    find = true;
                    break;
                }
            }
            tcpIterator.toFront();
            if (find) {
                find = false;
                continue;
            }
            if (host.startsWith("http://")) {
                try {
                    tcpSocket = new QTcpSocket;
                } catch (std::bad_alloc &e) {
                    printf("%d: %s\n", __LINE__, e.what());
                }
                connectTracker(tcpSocket, host, parseTrackerUrlPort(host));
                tcpSocketList.append(tcpSocket);
            }
        }
    }

    if (stateFlag)
        emit trackerState(stateFlag);
}

void Tracker::sendHttpRequest()
{ // 发送http请求
    QTcpSocket *tcp   =  qobject_cast<QTcpSocket *>(sender()); // 获取信号的发送者

    qint64  left        =  (metaInfo->sha1Sums().size() - bitmap->getDownloadPieces()) * metaInfo->pieceLength();
    qint64  downloaded  =  bitmap->getDownloadPieces() * metaInfo->pieceLength();

    quint16 listenPort = 33550;
    /*
     * createHttpRequest(QString url, quint16 port, QString infoHash,
     *                   QString peerId, qint64 upload, qint64 downloaded,
     *                   qint64 left, qint64 numwant)
     * url: tracker 服务器的地址
     * listenPort: 自己监听的端口号
     * infoHash: .torrent 文件中info字段的sha1值
     * peerId: 20字节的固定的随机值标识bt客户端
     * upload: 上传的字节数
     * downloaded: 下载的字节数
     * left: 还剩余的数量
     * numwant: 想要得到的peer端的数量
     */
    QString httpReq = createHttpRequest(tcp->peerName(),
                                        listenPort,
                                        infoHash,
                                        peerId,
                                        0,
                                        downloaded,
                                        left,
                                        200);
    tcp->write(httpReq.toUtf8().data());
}

void Tracker::processTrackerConnect()
{ // 处理tracker返回的信息
    QTcpSocket* tcpSocket = qobject_cast<QTcpSocket *>(sender());

    // 获取tracker返回的消息
    QByteArray recv = tcpSocket->readAll();
    if (parseTrackerMessage(recv)) {
        if (!peers.isEmpty()) { // 和peer建立连接
            emit updatePeerList(peers);
        }
    }
    recv.clear();
}

void Tracker::pareseBaseEncoding(int &i, QByteArray &temp, const QByteArray &msg)
{
    int len = 0;
    for (; msg[i] != ':'; ++i) {
        temp.append(msg[i]);
    }
    i++; // 跳过":"
    len = temp.toInt();
    temp.clear();
    for (; --len >= 0; ++i) {
        temp.append(msg[i]);
    }
}

bool Tracker::parseTrackerMessage(const QByteArray &msg)
{
    /*
     * 分析返回的消息的类型
     * 1：含有Location关键字，是http消息返回的关键字，对访问文件的重定位
     * 2：含有peers列表
     *    例子：d8:intervali3600e5:peersld2:ip13:192.168.24.527:peer id20:{peer_id}4:porti2001eeeee
     * 3: 紧密类型的消息
     *    例子：192.168.24.52:2001 => 0xC0 0xA8 0x18 0x34 0x07 0xD1
     */
    int i = 0; // 全部文件字符的总的坐标
    int index = 0; // 关键字坐标
    QByteArray type1 = "Location: ";
    QByteArray type2 = "2:ip";
    QString redirection;
    QByteArray temp;
    if ( (index = msg.indexOf(type1)) != -1) { // http 重定位消息
        i = index + type1.length();
        int start = msg.indexOf("http://");
        int end = msg.indexOf("announce");
        redirection = msg.mid(start, end + 8 - start);
        // 将新的tracker的地址加入到列表中
        metaInfo->announceList().append(redirection); // TODO 验证
    } else if ( (index = msg.indexOf(type2)) != -1) { // 第二种类型的消息
        QByteArray peersKey = "5:peers";

        int len = 0;
        QString peerIp; // peer ip
        quint16 port; // peer端口
        if ( (index = msg.indexOf(peersKey)) != -1) { // 含有peers字段，如果含有failure resason字段则peer请求失败
            i = index + peersKey.length();
            int stack = -1;

            i++;
            stack++;
            while (stack != -1) {
                switch (msg[i]) {
                case 'l':
                case 'i':
                case 'd':
                    i++;
                    stack++;
                    break;
                case 'e':
                    i++;
                    stack--;
                    break;
                default:
                    pareseBaseEncoding(i, temp, msg);
                    if (temp == "ip") {
                        temp.clear();
                        for (; msg[i] != ':'; i++) {
                            temp.append(msg[i]);
                        }
                        i++; // 跳过 ：
                        len = temp.toInt();
                        temp.clear();
                        for (; len-- > 0; i++) {
                            temp.append(msg[i]);
                        }
                        peerIp = temp;
                    } else if (temp == "port") {
                        temp.clear();
                        i++; // 跳过 i 字符
                        stack++;
                        for (; isdigit(msg[i]); i++) {
                            temp.append(msg[i]);
                        }
                        port = static_cast<quint16>(temp.toUInt());
                        i++; // 跳过 e 字符
                        stack--;
                        peers.insert(peerIp, port); // 获取peer的ip地址和端口号
                        peerIp.clear();
                    } else {
                        temp.clear();
                    }
                    break;
                } // end switch
            } // end while
        } // end if
    } else { // 第三种类型的消息
        QByteArray peersKey = "5:peers";
        unsigned char c[4] = { 0 };
        QString ip;
        quint16 port = 0;
        int len = 0;
        if ( (index = msg.indexOf(peersKey)) != -1) {
            i = index + peersKey.length();
            for (; isdigit(msg[i]) != 0; i++) {
                temp.append(msg[i]);
            }
            i++;
            len = temp.toInt();
            if (len <= 0)
                return false;
            temp.clear();
            len = len / 6;
            for (; len != 0 && (i + 6) < msg.size(); len--) {
                c[0] = static_cast<unsigned char>(msg[i]);
                c[1] = static_cast<unsigned char>(msg[i + 1]);
                c[2] = static_cast<unsigned char>(msg[i + 2]);
                c[3] = static_cast<unsigned char>(msg[i + 3]);
                ip = QString("%1.%2.%3.%4").arg(c[0]).arg(c[1]).arg(c[2]).arg(c[3]);
                i += 4;
                port |= (unsigned char)msg[i];
                port = port << 8;
                port |= (unsigned char)msg[i + 1];
                i += 2;
                peers.insert(ip, port); // 获取peer的ip地址和端口
                ip.clear(); port = 0;
            } // end for
        } else {
            return false;
        } // end if
    } // end else

    return true;
}

void Tracker::tcpConnectError(QAbstractSocket::SocketError sockError)
{ // 处理socket连接错误
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());

    tcpSocketList.removeAll(socket);
    socket->disconnectFromHost();
    socket->close();
    socket->deleteLater();
}

void Tracker::httpRequestEncode(QByteArray &plain)
// 转为带%号的十六进制的编码
{
    QByteArray temp; temp.clear();
    temp = plain;
    plain.clear();

    char hex_table[17] = "0123456789abcdef";
    for (int i = 0; i < temp.size(); i++) {
        if ( isdigit(temp[i]) || isalpha(temp[i])) {
            plain.append((unsigned char)temp[i]);
        } else {
            plain.append("%");
            plain.append(hex_table[((unsigned char)temp[i] >> 4)]);
            plain.append(hex_table[((unsigned char)temp[i] & 0xf)]);
        }
    }
}

QString Tracker::createHttpRequest(QString url, quint16 port, QByteArray infoHash,
                                   QByteArray peerId, qint64 upload, qint64 downloaded,
                                   qint64 left, qint64 numwant)
{ // 构造http请求
    httpRequestEncode(infoHash);
    httpRequestEncode(peerId); // 进行编码
    // 构造http消息的get请求
    char request[1024] = { 0 };
    snprintf(request, sizeof(request),
             "GET /announce?info_hash=%s&peer_id=%s&port=%u"
             "&uploaded=%lld&downloaded=%lld&left=%lld&numwant=%lld"
             "&compact=1&event=started HTTP/1.1\r\n"
             "Accept: */*\r\n"
             "Host: %s\r\n"
             "Accept-Encoding: gzip\r\n"
             "User-Agent: Bittorrent\r\n"
             "Connection: close\r\n"
             "\r\n",
             infoHash.data(), peerId.data(),
             port, upload, downloaded, left, numwant, url.toUtf8().data());

    return QString((const char *)request);
}

void Tracker::disconnectSocket()
{
    QTcpSocket *tcpSocket = qobject_cast<QTcpSocket *>(sender());
    if (tcpSocket != nullptr) {
        tcpSocketList.removeAll(tcpSocket);
        tcpSocket->disconnectFromHost();
        tcpSocket->close();

        tcpSocket->deleteLater();
    }
}

void Tracker::closeAllTrackerConnect()
{
    if (tcpSocketList.isEmpty())
        return;
    QTcpSocket *tcpSocket = nullptr;
    QListIterator<QTcpSocket *> iterator(tcpSocketList);
    while (iterator.hasNext()) {
        tcpSocket = iterator.next();
        tcpSocket->disconnectFromHost();
        tcpSocket->close();

        tcpSocket->deleteLater();
    }
}
