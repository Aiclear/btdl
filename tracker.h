#ifndef TRACKER_H
#define TRACKER_H

#include "bitmapofpieces.h"
#include "metainfo.h"

#include <QTcpSocket>
#include <QObject>
#include <QList>
#include <QAbstractSocket>
#include <QTimer>

#include <iostream>

class Tracker : public QObject
{
    Q_OBJECT

public:
    Tracker();
    ~Tracker();

    void setInfoHash(const QByteArray &value) { infoHash = value; }
    void setPeerId(const QByteArray &value) { peerId = value; }
    void setBitmap(BitMapOfPieces *value) { bitmap = value; }
    void setMetaInfo(MetaInfo *value) { metaInfo = value; }
    void initialTrackerConnect(); // 初始化连接tracker的socket

    void httpRequestEncode(QByteArray &httpUrl); // 对http的请求的url进行编码(UTF-8编码格式)
    // 创建和tracker交互的http的 GET 请求
    QString createHttpRequest(QString url, quint16 port, QByteArray infoHash,
                              QByteArray peerId, qint64 upload, qint64 downloaded,
                              qint64 left, qint64 numwant);

    // slots
    void tcpConnectError(QAbstractSocket::SocketError sockError); // 处理socket连接错误
    void processTrackerConnect(); // 处理连接之后接收到的消息
    bool parseTrackerMessage(const QByteArray &msg);
    void sendHttpRequest(); // 构建http请求并且发送
    void disconnectSocket(); // 当socket断开之后进行处理
    void closeAllTrackerConnect(); // 当peersock处理的piece下载完成之后处理tracker的tcp连接

    void connectTracker(QTcpSocket *socket, const QString url, const quint16 port);
    void pareseBaseEncoding(int &i, QByteArray &temp, const QByteArray &msg);
    quint16 parseTrackerUrlPort(const QString &url) const;

    QList<QTcpSocket *> & getTcpSocketList() { return tcpSocketList; }
signals:
    void updatePeerList(QHash<QString, quint16> &peers);
    void trackerState(bool &flag);

private:
    QByteArray infoHash;
    QByteArray peerId;

    BitMapOfPieces *bitmap;
    MetaInfo *metaInfo;

    QList<QTcpSocket *> tcpSocketList;
    QHash<QString, quint16> peers;

    bool stateFlag = true;
};

#endif // TRACKER_H
