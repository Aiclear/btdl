#include "messagehandler.h"

#include <QDebug>

inline static void convertIntToChar(qint32 val, unsigned char c[])
{
    memset(c, 0, 4);
    c[3] |= val;
    c[2] |= (val >> 8);
    c[1] |= (val >> 16);
    c[0] |= (val >> 24);
}

inline static qint32 convertCharToInt(unsigned char *c)
{
    int val = 0;
    val |= c[0];
    val = (val << 8);
    val |= c[1];
    val = (val << 8);
    val |= c[2];
    val = (val << 8);
    val |= c[3];

    return val;
}

void MessageHandler::initial(const QByteArray &infoHash, const QByteArray &peerId)
{
    this->infoHash = infoHash;
    this->peerId = peerId;
}

bool MessageHandler::createHandshakeMessage(Peer &peer)
/*
 * <pstrlen><pstr><reserved><info_hash><peer_id>
 * pstrlen: pstr的长度，固定为19
 * pstr: BitTorrent协议的关键字，为"BitTorrent protocol"
 * reserved: 为8字节的保留字节，一般初始化为全零
 * info_hash: tracker连接时使用的info_hash
 * peer_id: tracker连接时使用的peer_id
 * 握手消息的固定长度为68字节
*/
{
    QByteArray temp;
    temp.append(19);
    temp.append("BitTorrent protocol");
    for (int i = 0; i < 8; i++)
        temp.append((char)0);
    temp.append(infoHash);
    temp.append(peerId);

    if (temp.size() < 68)
        return false;
    peer.sendBuff.append(temp);

    return true;
}

/*
 * length prefix: 消息的固定长度
 * 消息的固定格式
 * <length prefix><message id><payload>
 * message id: 消息的id号
 * payload: 长度未定，消息的内容
*/
bool MessageHandler::createKeepAliveMessage(Peer &peer)
{ // keep_alive 消息长度固定位4个字节
    QByteArray temp; temp.clear();
    for (int i = 0; i < 4; i++)
        temp.append((char)0);

    if (temp.size() < 4)
        return false;

    peer.sendBuff.append(temp);
    return true;
}

bool MessageHandler::createChockInterestedMessage(Peer &peer, int type)
/*
 * am_chocking, am_interested, peer_chocking, peer_interested全部为5个字节的固定长度
 * 各个消息的ID分别为：choke 0，unchoke1，interested2，not interested3
 * 前四个字节为消息的长度，最后一个字节为消息的id
*/
{
    QByteArray temp; temp.clear();
    for (int i = 0; i < 3; i++)
        temp.append((char)0);
    temp.append((int8_t)1); // 长度
    temp.append((int8_t)type); // 消息ID
    if (temp.size() < 5)
        return false;

    peer.sendBuff.append(temp);
    return true;
}

bool MessageHandler::createHaveMessage(Peer &peer, qint32 index)
/*
 * have 消息的固定长度为9字节
 * 4字节的长度 + 1字节的消息ID<4> + payload 4个字节的下载的piece的在位图的下标
*/
{
    QByteArray temp; temp.clear();
    temp.append((char)0);
    temp.append((char)0);
    temp.append((char)0);
    temp.append((int8_t)5); // have消息的长度

    temp.append((int8_t)4); // 消息ID
    unsigned char c[4] = { 0 };
    convertIntToChar(index, c);
    for (int i = 0; i < 4; i++)
        temp.append(c[i]);

    if (temp.size() < 9)
        return false;

    peer.sendBuff.append(temp);
    return true;
}

bool MessageHandler::createBitfiledMessage(Peer &peer)
/*
 * bitmap 消息长度不固定
 * 前四位字节表示长度
 * 第五位字节代表消息ID = 5
 * 后面则是bitmap的内容
*/
{
    QByteArray temp;
    unsigned char c[4] = { 0 };
    convertIntToChar(static_cast<qint32>(bitmap->getBitFileLength() + 1), c);
    for (int i = 0; i < 4; i++)
        temp.append(c[i]);
    temp.append((int8_t)5); // 设置消息ID
    unsigned char *bitfield = bitmap->getBitmap();
    if (bitfield == nullptr)
        return false;
    for (int i = 0; i < bitmap->getBitFileLength(); i++)
        temp.append(bitfield[i]);

    if (temp.size() < bitmap->getBitFileLength())
        return false;

    peer.sendBuff.append(temp);
    return true;
}

bool MessageHandler::createRequestMessage(Peer &peer, int index, int begin, int length)
/*
 * request 消息的固定长度为17字节
 * 消息ID：6
 * index: piece的索引
 * begin: piece内的偏移
 * length: 请求peer发送数据的长度一般为16K
*/
{
    QByteArray temp;
    temp.append((char)0);
    temp.append((char)0);
    temp.append((char)0);
    temp.append((int8_t)13); // 消息长度前缀

    temp.append((int8_t)6); // 消息ID
    unsigned char c[4] = { 0 };
    convertIntToChar(index, c);
    for (int i = 0; i < 4; i++)
        temp.append(c[i]);
    memset(c, 0, 4);
    convertIntToChar(begin, c);
    for (int i = 0; i < 4; i++)
        temp.append(c[i]);
    memset(c, 0, 4);
    convertIntToChar(length, c);
    for (int i = 0; i < 4; i++)
        temp.append(c[i]);

    if (temp.size() < 17)
        return false;

    peer.sendBuff.append(temp);
    return true;
}

bool MessageHandler::createPieceMessage(Peer &peer, int index, int begin, int length,
                                        const QByteArray &buff /* 响应peer的数据 */ )
/* 此消息为向请求用户发送数据
 * piece消息的长度不固定
 * ID = 7
 * index 和 begin长度固定为八个字节
 * length一般为请求的数据块的长度16KB
*/
{
    QByteArray temp;
    unsigned char c[4] = { 0 };
    convertIntToChar(length + 9, c);
    for (int i = 0; i < 4; i++)
        temp.append(c[i]);
    temp.append((int8_t)7); // 消息ID
    memset(c, 0, 4);
    convertIntToChar(index, c);
    for (int i = 0; i < 4; i++)
        temp.append(c[i]);
    memset(c, 0, 4);
    convertIntToChar(begin, c);
    for (int i = 0; i < 4; i++)
        temp.append(c[i]);
    temp.append(buff);

    if (temp.size() < (length + 13))
        return false;

    peer.sendBuff.append(temp);
    return true;
}

bool MessageHandler::createCancelMessage(Peer &peer, int index, int begin, int length)
/*
 * 和request消息相同但作用相反
 * ID = 8， 取消客户端的数据请求
*/
{
    unsigned char c[4] = { 0 };
    QByteArray temp; temp.clear();
    temp.append((char)0);
    temp.append((char)0);
    temp.append((char)0);
    temp.append((int8_t)8); // 消息ID
    convertIntToChar(index, c);
    for (int i = 0; i < 4; i++) // index
        temp.append((int8_t)c[i]);
    convertIntToChar(begin, c);
    for (int i = 0; i < 4; i++)
        temp.append((int8_t)c[i]);
    convertIntToChar(length, c);
    for (int i = 0; i < 4; i++)
        temp.append((int8_t)c[i]);

    if (temp.size() < 17)
        return false;

    peer.sendBuff.append(temp);
    return true;
}

// 处理消息
bool MessageHandler::parseHandshakeMessage(Peer &peer)
{// 处理握手消息
    if (peer.messageBuff.isEmpty())
        return false;

    if (peer.messageBuff.right(40).left(20) != infoHash) { // info hash不匹配
        peer.state = Peer::CLOSING;
        return false;
    }

    peer.peerId = peer.messageBuff.right(20);
    if (peer.state == Peer::INITIAL) {
        peer.state = Peer::HANDSHAKED;
        createHandshakeMessage(peer);
    }
    if (peer.state == Peer::HALFSHAKED)
        peer.state = Peer::HANDSHAKED;

    return true;
}

bool MessageHandler::parseKeepAliveMessage(Peer &peer)
{ // 处理 keep_alive 消息
    if (peer.messageBuff.isEmpty())
        return false;

    peer.startTimestamp = time(NULL);
    return true;
}

bool MessageHandler::parseChockMessage(Peer &peer)
{ // 处理chock消息
    if (peer.messageBuff.isEmpty())
        return false;

    if (peer.state != Peer::CLOSING && peer.peerChocking == 0) {
        peer.peerChocking = 1;
        peer.lastDownTimestamp = 0;
        peer.downCount = 0;
        peer.downRate = 0;
    }

    peer.startTimestamp  = time(NULL);
    return true;
}

bool MessageHandler::parseUnchockMessage(Peer &peer)
{ // 处理unchock消息
    if (peer.messageBuff.isEmpty())
        return false;

    if (peer.state != Peer::CLOSING && peer.peerChocking == 1) {
        peer.peerChocking = 0;
        if (peer.amInterested == 1) {
            emit createRequest(peer);
        } else {
            // this = 1 dst = 0 则有兴趣
            peer.amInterested = BitMapOfPieces::isInterested(*peer.peerBitmap, *bitmap);
            if (peer.amInterested == 1) {
                emit createRequest(peer);
            } else {
                qDebug() << "not chock but no interested!";
            }
        }
        peer.lastDownTimestamp = 0;
        peer.downCount = 0;
        peer.downRate = 0;
    }

    peer.startTimestamp = time(NULL);
    return true;
}

bool MessageHandler::parseInterestedMessage(Peer &peer)
{ // interested 消息
    if (peer.messageBuff.isEmpty())
        return false;

    if (peer.state != Peer::CLOSING && peer.state == Peer::DATA) {
        peer.peerInterested = BitMapOfPieces::isInterested(*bitmap, *peer.peerBitmap); // 可能是无效消息
        if (peer.peerInterested == 0)
            return false;
        if (peer.amChocking == 0)
            createChockInterestedMessage(peer, 1);
    }

    peer.startTimestamp = time(NULL);
    return true;
}

bool MessageHandler::parseUnInterestedMessage(Peer &peer)
{ // un interested 消息
    if (peer.messageBuff.isEmpty())
        return false;

    if (peer.state != Peer::CLOSING && peer.state == Peer::DATA) {
        peer.peerInterested = 0;
        peer.requestedPiece.clear(); // 取消此peer的请求队列
    }

    peer.startTimestamp = time(NULL);
    return true;
}

bool MessageHandler::parseBitfiledMessage(Peer &peer)
{ // 处理位图消息
    if (peer.messageBuff.isEmpty())
        return false;

    unsigned char c[4] = { 0 };
    if (peer.state == Peer::HANDSHAKED || peer.state == Peer::SENDBITFILED) {

        for (int i = 0; i < 4; i++)
            c[i] = static_cast<unsigned char>(peer.messageBuff.at(i));

        if ((convertCharToInt(c) - 1) != bitmap->getBitFileLength()) {
            peer.state = Peer::CLOSING;
            return false;
        }

        peer.peerBitmap->setValidLength(bitmap->getValidLength());
        peer.peerBitmap->setBitFileLength(convertCharToInt(c) - 1);
        unsigned char *bit = new unsigned char[convertCharToInt(c) -1];
        if (bit == nullptr)
            return false;
        peer.peerBitmap->setBitmap(bit);
        // 将位图的消息复制到peer的位图区域中去
        memcpy(peer.peerBitmap->getBitmap(),
               (unsigned char *)&(peer.messageBuff.data()[5]),
                static_cast<size_t>(convertCharToInt(c) - 1));

        // 判断当前peer的状态
        if (peer.state == Peer::HANDSHAKED) {
            createBitfiledMessage(peer);
            peer.state = Peer::DATA;
        }

        if (peer.state == Peer::SENDBITFILED) {
            peer.state = Peer::DATA;
        }

        // peer是否对自己感兴趣
        peer.peerInterested = BitMapOfPieces::isInterested(*bitmap, *(peer.peerBitmap));

        // 是否对peer感兴趣
        peer.amInterested = BitMapOfPieces::isInterested(*(peer.peerBitmap), *bitmap);
        if (peer.amInterested == 1)
            createChockInterestedMessage(peer, MessageType::INTERESTED);
    }

    peer.startTimestamp=time(NULL);
    return true;
}

bool MessageHandler::parseRequestMessage(Peer &peer)
{ // 处理请求消息
    if (peer.messageBuff.isEmpty())
        return false;

    unsigned char c[4];
    RequestPiece *requestedPiece = new RequestPiece;
    if (peer.amChocking == 0 && peer.peerInterested == 1) {
        for (int i = 0; i < 4; i++)
            c[i] = static_cast<unsigned char>(peer.messageBuff.at(i + 5));
        requestedPiece->index = convertCharToInt(c);

        for (int i = 0; i < 4; i++)
            c[i] = static_cast<unsigned char>(peer.messageBuff.at(i + 9));
        requestedPiece->begin = convertCharToInt(c);

        for (int i = 0; i < 4; i++)
            c[i] = static_cast<unsigned char>(peer.recvBuff.at(i + 13));
        requestedPiece->length = convertCharToInt(c);

        if (requestedPiece->begin % (16 * 1024) != 0)
            return true;
    }

    for (int k = 0; k < peer.requestedPiece.size(); k++) {
        if (*(peer.requestedPiece.at(k)) == *requestedPiece)
            return true;
    }

    // 存放peer请求的队列
    peer.requestedPiece.append(requestedPiece);

    peer.startTimestamp = time(NULL);
    return true;
}

bool MessageHandler::parsePieceMessage(Peer &peer)
{ // piece消息是用户响应自己请求发送的piece的数据
    if (peer.messageBuff.isEmpty())
        return false;

    int length, index, begin;
    unsigned char c[4] = { 0 };
    if (peer.peerChocking == 0) {
        for (int i = 0; i < 4; i++)
            c[i] = static_cast<unsigned char>(peer.messageBuff.at(i));
        length = convertCharToInt(c) - 9;

        for (int i = 0; i < 4; i++)
            c[i] = static_cast<unsigned char>(peer.messageBuff.at(i + 5));
        index = convertCharToInt(c);

        for (int i = 0; i < 4; i++)
            c[i] = static_cast<unsigned char>(peer.messageBuff.at(i + 9));
        begin = convertCharToInt(c);

        bool find = false;

        for (int k = 0; k < peer.requestPiece.size(); ++k) {
            if (peer.requestPiece.at(k)->index == index &&
                    peer.requestPiece.at(k)->begin == begin &&
                    peer.requestPiece.at(k)->length == length)
            {
                find = true;
                break;
            }
        }

        if (!find)
            return false;

        if (peer.lastDownTimestamp == 0)
            peer.lastDownTimestamp = time(NULL);
        peer.downCount += length;
        peer.downTotal += length;

        emit writeData(index, begin, length, peer);
        emit createRequest(peer);
    }

    peer.startTimestamp = time(NULL);
    return true;
}

bool MessageHandler::parseCancelMessage(Peer &peer)
{ // 取消消息的处理
    if (peer.messageBuff.isEmpty())
        return false;

    unsigned char c[4] = { 0 };
    int index, begin, length;
    if (peer.state != Peer::CLOSING && peer.state == Peer::DATA) {
        for (int i = 0; i < 4; i++)
            c[i] = (unsigned char)peer.messageBuff.at(i + 5);
        index = convertCharToInt(c);
        for (int i = 0; i < 4; i++)
            c[i] = (unsigned char)peer.messageBuff.at(i + 9);
        begin = convertCharToInt(c);
        for (int i = 0; i < 4; i++)
            c[i] = (unsigned char)peer.messageBuff.at(i + 13);
        length = convertCharToInt(c);

        int k = 0;
        bool find = false;
        for (k = 0; k < peer.requestedPiece.size(); ++k) {
            if (index == peer.requestedPiece.at(k)->index &&
                    begin == peer.requestedPiece.at(k)->begin &&
                    length == peer.requestedPiece.at(k)->length)
            {
                find = true;
                break;
            }
        }
        if (find)
            peer.requestedPiece.removeAll(peer.requestedPiece.at(k));
    }

    peer.startTimestamp = time(NULL);
    return true;
}

bool MessageHandler::parseHaveMessage(Peer &peer)
{ // peer发送的have消息表明自己有某个piece
    if (peer.messageBuff.isEmpty())
        return false;

    if (peer.peerBitmap->getBitmap() == nullptr)
        return false;

    unsigned char c[4] = { 0 };
    // 判断当前的状态
    if (peer.state != Peer::CLOSING && peer.state == Peer::DATA) {
        c[0] = static_cast<unsigned char>(peer.messageBuff.at(5));
        c[1] = static_cast<unsigned char>(peer.messageBuff.at(6));
        c[2] = static_cast<unsigned char>(peer.messageBuff.at(7));
        c[3] = static_cast<unsigned char>(peer.messageBuff.at(8));
        peer.peerBitmap->setBitValue(convertCharToInt(c), 1);
        peer.have++;

        if (peer.amInterested == 0) {
            peer.amInterested = BitMapOfPieces::isInterested(*(peer.peerBitmap), *bitmap);

            // 如果此时对peer发生兴趣，则发送interested消息
            if (peer.amInterested == 1) {
                createChockInterestedMessage(peer, MessageType::INTERESTED);
                emit createRequest(peer);
            }
        } else {
            if (peer.have >= 3) {
                createChockInterestedMessage(peer, MessageType::INTERESTED);
                emit createRequest(peer);
                peer.have = 0;
            }
        }
    }

    peer.startTimestamp = time(NULL);
    return true;
}


bool MessageHandler::createResponseMessage(Peer &peer)
{ // 主动创建发送的消息
    if (peer.state == Peer::INITIAL) {
        createHandshakeMessage(peer);
        peer.state=Peer::HALFSHAKED;
        return true;
    }

    if (peer.state == Peer::HANDSHAKED) {
        createBitfiledMessage(peer);
        peer.state=Peer::SENDBITFILED;
        return true;
    }

    // 此处发送piece消息，对方请求的数据
    if (peer.amChocking == 0 && !peer.requestedPiece.isEmpty()) {
        QByteArray buff;
        RequestPiece *requestPiece = peer.requestedPiece.first();
        emit readData(requestPiece->index, requestPiece->begin, requestPiece->length, buff);
        if (!buff.isEmpty())
            createPieceMessage(peer, requestPiece->index,
                               requestPiece->begin, requestPiece->length, buff);
        else
            return false;
        if (peer.lastUpTimestamp == 0)
            peer.lastUpTimestamp = time(NULL);
        peer.upCount += requestPiece->length;
        peer.upTotal += requestPiece->length;
        peer.requestedPiece.removeFirst();

        if (requestPiece != nullptr)
            delete requestPiece;
        requestPiece = nullptr;
        return true;
    }

    // 三分钟没有收到任何消息，关闭连接
    time_t now = time(NULL);
    long interval1 = now - peer.startTimestamp;
    if (interval1 > 180) {
        peer.state = Peer::CLOSING;
        peer.sendBuff.clear();
    }

    // 如果45秒没有发送和接收消息则发送keep_alive 消息
    long interval2 = now - peer.startTimestamp;
    if (interval1 > 45 && interval2 > 45 && peer.recvBuff.isEmpty())
        createKeepAliveMessage(peer);

    return true;
}

bool MessageHandler::isCompleteMessage(Peer &peer, int &completeLen)
{
    completeLen = 0;
    if (peer.recvBuff.isEmpty())
        return false;

    int len = peer.recvBuff.size();

    int length;
    char btkeyword[20];
    int i = 0;

    unsigned char keep_alive[4] = {0x0, 0x0, 0x0, 0x0};
    unsigned char chocke[5] = {0x0, 0x0, 0x0, 0x1, 0x0};
    unsigned char unchocke[5] = {0x0, 0x0, 0x0, 0x1, 0x1};
    unsigned char interested[5] = {0x0, 0x0, 0x0, 0x1, 0x2};
    unsigned char uninterested[5] = {0x0, 0x0, 0x0, 0x1, 0x3};
    unsigned char have[5] = {0x0, 0x0, 0x0, 0x5, 0x4};
    unsigned char request[5] = {0x0, 0x0, 0x0, 0xd, 0x6};
    unsigned char cancel[5] = {0x0, 0x0, 0x0, 0xd, 0x8};
    unsigned char port[5] = {0x0, 0x0, 0x0, 0x3, 0x9};

    unsigned char c[4];

    btkeyword[0] = 19;
    memcpy(&btkeyword[1], "BitTorrent protocol", 19); // BitTorrent协议关键字

    for (i = 0; i < len;) {
        if (i + 68 <= len && memcmp(peer.recvBuff.mid(i, 20).data(), btkeyword, 20) == 0)
            i += 68;
        else if (i + 4 <= len && memcmp(peer.recvBuff.mid(i, 4).data(), keep_alive, 4) == 0)
            i += 4;
        else if (i + 5 <= len && memcmp(peer.recvBuff.mid(i, 5).data(), chocke, 5) == 0)
            i += 5;
        else if (i + 5 <= len && memcmp(peer.recvBuff.mid(i, 5).data(), unchocke, 5) == 0)
            i += 5;
        else if (i + 5 <= len && memcmp(peer.recvBuff.mid(i, 5).data(), interested, 5) == 0)
            i += 5;
        else if (i + 5 <= len && memcmp(peer.recvBuff.mid(i, 5).data(), uninterested, 5) == 0)
            i += 5;
        else if (i + 9 <= len && memcmp(peer.recvBuff.mid(i, 5).data(), have, 5) == 0)
            i += 9;
        else if (i + 17 <= len && memcmp(peer.recvBuff.mid(i, 5).data(), request, 5) == 0)
            i += 17;
        else if (i + 17 <= len && memcmp(peer.recvBuff.mid(i, 5).data(), cancel, 5) == 0)
            i += 17;
        else if (i + 7 <= len && memcmp(peer.recvBuff.mid(i, 5).data(), port, 5) == 0)
            i += 7;
        else if (i + 5 <= len && peer.recvBuff.at(i + 4) == MessageType::BITFIELD) {
            c[0] = static_cast<unsigned char>(peer.recvBuff.at(i));
            c[1] = static_cast<unsigned char>(peer.recvBuff.at(i + 1));
            c[2] = static_cast<unsigned char>(peer.recvBuff.at(i + 2));
            c[3] = static_cast<unsigned char>(peer.recvBuff.at(i + 3));
            length = convertCharToInt(c);
            if (i + 4 + length <= len)
                i += 4 + length;
            else {
                completeLen = i;
                return false;
            }
        } else if (i + 5 <= len && peer.recvBuff.at(i + 4) == MessageType::PIECE) {
            c[0] = static_cast<unsigned char>(peer.recvBuff.at(i));
            c[1] = static_cast<unsigned char>(peer.recvBuff.at(i + 1));
            c[2] = static_cast<unsigned char>(peer.recvBuff.at(i + 2));
            c[3] = static_cast<unsigned char>(peer.recvBuff.at(i + 3));
            length = convertCharToInt(c);
            if (i + 4 + length <= len)
                i += 4 + length;
            else {
                completeLen = i;
                return false;
            }
        } else {
            if (i + 4 <= len) {
                c[0] = static_cast<unsigned char>(peer.recvBuff.at(i));
                c[1] = static_cast<unsigned char>(peer.recvBuff.at(i + 1));
                c[2] = static_cast<unsigned char>(peer.recvBuff.at(i + 2));
                c[3] = static_cast<unsigned char>(peer.recvBuff.at(i + 3));
                length = convertCharToInt(c);
                if (i + 4 + length <= len)
                    i += 4 + length;
                else {
                    completeLen = i;
                    return false;
                }
            }

            completeLen = i;
            return false;
        }
    }

    completeLen = i;
    return true;
}

static bool ByteArrayCompare(const QByteArray &src, const QByteArray &dst, int len)
{
    if (len == 0)
        return 0;
    if ((src.size() == dst.size()) && len > src.size())
        len = src.size();
    if ((src.size() != dst.size()) && (len > src.size() ||len > dst.size()))
        return false;

    for (int i = 0; i < len; i++) {
        if (src.at(i) != dst.at(i))
            return false; // 不相等
    }

    return true; // 字符串相等
}

/* 解析十二种类型的消息
 * handshark
 * keep_alive
 * 0 - choke
 * 1 - unchoke
 * 2 - interested
 * 3 - not interested
 * 4 - have (length: 9 bytes)
 * 5 - bitfield
 * 6 - request
 * 7 - piece
 * 8 - cancel
 * 9 - port
 * uncomplete message
*/
bool MessageHandler::parseResponseMessage(Peer &peer)
{
    if (peer.recvBuff.isEmpty())
        return false;

    int len = peer.recvBuff.size(); // 消息缓冲区的长度
    int index = 0; // 消息字节的下标

    QByteArray btKeyword;
    btKeyword.append((int8_t)19);
    btKeyword.append("BitTorrent protocol");

    QByteArray keepAliveKeyword;
    for (int i = 0; i < 4; i++)
        keepAliveKeyword.append((char)0);

    // 假设当前缓冲区放置了多个消息，则需要根据长度不断的进行判断和比较
    for (index = 0; index < len;) {
        if ((len - index) >= 68 && ByteArrayCompare(peer.recvBuff.mid(index, 19), btKeyword, 19)) {
            peer.messageBuff = peer.recvBuff.mid(index, 68);
            parseHandshakeMessage(peer);
            index += 68;
        } else if ((len - index) >= 4 && ByteArrayCompare(peer.recvBuff.mid(index), keepAliveKeyword, 4)) {
            peer.messageBuff = peer.recvBuff.mid(index, 4);
            parseKeepAliveMessage(peer);
            index += 4;
        } else if ((len - index) >= 5 && peer.recvBuff.right(len - index).at(4) == MessageType::CHOKE) {
            peer.messageBuff = peer.recvBuff.mid(index, 5);
            parseChockMessage(peer);
            index += 5;
        } else if ((len - index) >= 5 && peer.recvBuff.right(len - index).at(4) == MessageType::UNCHOKE) {
            peer.messageBuff = peer.recvBuff.mid(index, 5);
            parseUnchockMessage(peer);
            index += 5;
        } else if ((len - index) >= 5 && peer.recvBuff.right(len - index).at(4) == MessageType::INTERESTED) {
            peer.messageBuff = peer.recvBuff.mid(index, 5);
            parseInterestedMessage(peer);
            index += 5;
        } else if ((len - index) >= 5 && peer.recvBuff.right(len - index).at(4) == MessageType::NOTINTERESTED) {
            peer.messageBuff = peer.recvBuff.mid(index, 5);
            parseUnInterestedMessage(peer);
            index += 5;
        } else if ((len - index) >= 9 && peer.recvBuff.right(len - index).at(4) == MessageType::HAVE) {
            peer.messageBuff = peer.recvBuff.mid(index, 9);
            parseHaveMessage(peer);
            index += 9;
        } else if ((len - index) >= 5 && peer.recvBuff.right(len - index).at(4) == MessageType::BITFIELD) {
            peer.messageBuff = peer.recvBuff.mid(index, static_cast<int>(bitmap->getBitFileLength() + 5));
            parseBitfiledMessage(peer);
            index += bitmap->getBitFileLength() + 5;
        } else if ((len - index) >= 17 && peer.recvBuff.right(len - index).at(4) == MessageType::REQUEST) {
            peer.messageBuff = peer.recvBuff.mid(index, 17);
            parseRequestMessage(peer);
            index += 17;
        } else if ((len - index) >= 13 && peer.recvBuff.right(len - index).at(4) == MessageType::PIECE) {
            unsigned char c[4] = { 0 };
            c[0] = static_cast<unsigned char>(peer.recvBuff.right(len - index).at(0));
            c[1] = static_cast<unsigned char>(peer.recvBuff.right(len - index).at(1));
            c[2] = static_cast<unsigned char>(peer.recvBuff.right(len - index).at(2));
            c[3] = static_cast<unsigned char>(peer.recvBuff.right(len - index).at(3));
            peer.messageBuff = peer.recvBuff.mid(index, convertCharToInt(c) + 4);
            parsePieceMessage(peer);
            index += convertCharToInt(c) + 4;
        } else if ((len - index) >= 17 && peer.recvBuff.right(len - index).at(4) == MessageType::CANCEL) {
            peer.messageBuff = peer.recvBuff.mid(index, 17);
            parseCancelMessage(peer);
            index += 17;
        } else if ((len - index) >= 7 && peer.recvBuff.right(len - index).at(4) == MessageType::PORT) {
            index += 7;
        } else { // 未知类型的消息
            unsigned char c[4] = { 0 };
            int length = 0;
            if (index + 4 <= len) {
                c[0] = static_cast<unsigned char>(peer.recvBuff.right(len - index).at(0));
                c[1] = static_cast<unsigned char>(peer.recvBuff.right(len - index).at(1));
                c[2] = static_cast<unsigned char>(peer.recvBuff.right(len - index).at(2));
                c[3] = static_cast<unsigned char>(peer.recvBuff.right(len - index).at(3));
                length = convertCharToInt(c);
                if (index + 4 + length <= len) {
                    index += 4 + length;
                    continue;
                }
            }
            peer.recvBuff.clear();
            return false;
        }
    }

    // 处理完消息后清空接受消息的缓冲区
    peer.recvBuff.clear();
    return true;
}

bool MessageHandler::parseResponseUncompleteMessage(Peer &peer, int &completeMsgLen)
{
    QByteArray tempMsgBuff;
    tempMsgBuff = peer.recvBuff.mid(completeMsgLen);
    peer.recvBuff = peer.recvBuff.left(completeMsgLen);
    parseResponseMessage(peer);

    peer.recvBuff = tempMsgBuff;

    return true;
}
