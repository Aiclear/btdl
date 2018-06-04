#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include "peerunit.h"
#include "bitmapofpieces.h"

class MessageHandler : public QObject
{
    Q_OBJECT

public:
    /* 0 - choke
    * 1 - unchoke
    * 2 - interested
    * 3 - not interested
    * 4 - have
    * 5 - bitfield
    * 6 - request
    * 7 - piece
    * 8 - cancel
    * 9 - port
    */
    enum MessageType {
        CHOKE = 0,
        UNCHOKE = 1,
        INTERESTED = 2,
        NOTINTERESTED = 3,
        HAVE = 4,
        BITFIELD = 5,
        REQUEST = 6,
        PIECE = 7,
        CANCEL = 8,
        PORT = 9
    };

public:
    MessageHandler() {}

    void setBitmap(BitMapOfPieces *bitFile) { bitmap = bitFile; }
    void initial(const QByteArray &infoHash, const QByteArray &peerId);

    // 创建发送的消息
    bool createHandshakeMessage(Peer &peer);
    bool createKeepAliveMessage(Peer &peer);
    bool createChockInterestedMessage(Peer &peer, int type);
    bool createHaveMessage(Peer &peer, qint32 index);
    bool createBitfiledMessage(Peer &peer);
    bool createRequestMessage(Peer &peer, int index, int begin, int length);
    bool createPieceMessage(Peer &peer, int index, int begin, int length, const QByteArray &buff);
    bool createCancelMessage(Peer &peer, int index, int begin, int length);

    // 主动创建发送的消息
    bool createResponseMessage(Peer &peer);
    // 解析收到的消息的类型
    bool isCompleteMessage(Peer &peer, int &completeMsgLen);
    bool parseResponseMessage(Peer &peer);
    bool parseResponseUncompleteMessage(Peer &peer, int &completeMsgLen);

private:
    // 解析发送的消息
    bool parseHandshakeMessage(Peer &peer);
    bool parseKeepAliveMessage(Peer &peer);
    bool parseChockMessage(Peer &peer);
    bool parseUnchockMessage(Peer &peer);
    bool parseInterestedMessage(Peer &peer);
    bool parseUnInterestedMessage(Peer &peer);
    bool parseHaveMessage(Peer &peer);
    bool parseBitfiledMessage(Peer &peer);
    bool parseRequestMessage(Peer &peer);
    bool parsePieceMessage(Peer &peer);
    bool parseCancelMessage(Peer &peer);

signals:
    void createRequest(Peer &peer); // policy handler
    void readData(int index, int begin, int length, QByteArray &buff);
    void writeData(int index, int begin, int length, Peer &peer);

private:
    BitMapOfPieces *bitmap;
    QByteArray infoHash;
    QByteArray peerId;
};

#endif // MESSAGEHANDLER_H
