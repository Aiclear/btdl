#include "policy.h"

#include "peersock.h"

Policy::Policy()
{
}

bool Policy::createReqSliceMsg(Peer &peer)
{
    // 如果peer将自己阻塞或者自己对对方不感兴趣则不要生成请求消息
    if (peer.peerChocking == 1 || peer.amInterested == 0)
        return false;

    /* 每个slice的长度固定为16KB
     * index: 代表每个piece的下标
     * begin: 代表偏移量
     * length = 16 * 1024(16KB)
     * 如果列表不为空则说明当前已经请求了piece的slice
     */
    int index = -1, begin, length = 16 * 1024;
    int count = 0;
    RequestPiece *req = nullptr;
    if (!peer.requestPiece.isEmpty()) {
        req = peer.requestPiece.last();

        if (req == nullptr)
            return false;
        qint64 lastSliceBegin = metaInfo->pieceLength() - (16 * 1024);
        if (req->index == metaInfo->getPieceValue().lastPieceIndex)
            lastSliceBegin = (metaInfo->getPieceValue().lastPieceCount - 1) * 16 * 1024;

        // 当前的piece还有未请求的slice
        if (req->begin < lastSliceBegin) {
            index = req->index;
            begin = req->begin + 16 * 1024;
            count = 0;

            while (begin != metaInfo->pieceLength() && count < 1) {
                if (req->index == metaInfo->getPieceValue().lastPieceIndex) {
                    if (begin == (metaInfo->getPieceValue().lastPieceCount - 1) * 16 * 1024) {
                        length = static_cast<int>(metaInfo->getPieceValue().lastSliceLength);
                    }
                }

                emit requestData(peer, index, begin, length);

                req = new RequestPiece(index, begin, length);
                peer.requestPiece.append(req);
                begin += 16 * 1024;
                count++;
            }

            return true;
        }
    }

    if (peer.peerBitmap == nullptr)
        return false;
    // 随机选择
    if (!randPieces(metaInfo->pieceNum()))
        return false;

    int i = 0;
    for (i = 0; i < metaInfo->pieceNum(); i++) {
        index = randPiecesSelectVec.at(i);

        if (peer.peerBitmap->getBitValue(index) != 1)
            continue;

        if (bitmap->getBitValue(index) == 1)
            continue;

        bool find = false;
        emit checkOutReqSlice(find, index);
        if (find)
            continue;

        break;
    }

    if (i == metaInfo->pieceNum()) {
        for (i = 0; i < metaInfo->pieceNum(); i++) {
            if (bitmap->getBitValue(i) == 0) {
                index = i;
                break;
            }
        }

        if (i == metaInfo->pieceNum()) {
            return false;
        }
    }

    // 构造piece请求
    begin = 0;
    count = 0;
    while (count < 4) {
        if (index == metaInfo->getPieceValue().lastPieceIndex) {
            if (count + 1 >= metaInfo->getPieceValue().lastPieceCount)
                break;
            if (begin == (metaInfo->getPieceValue().lastPieceCount - 1) * 16 * 1024)
                length = static_cast<int>(metaInfo->getPieceValue().lastSliceLength);
        }

        emit requestData(peer, index, begin, length);
        req = new RequestPiece(index, begin, length);
        peer.requestPiece.append(req);

        begin += 16 * 1024;
        count++;
    }

    return true;
}

bool Policy::isSeed(Peer &peer)
{
    qint64 validLength = peer.peerBitmap->getValidLength();
    qint64 bitmapByteNum = peer.peerBitmap->getBitFileLength();
    const unsigned char *bitmap = peer.peerBitmap->getBitmap();

    if (bitmap == nullptr || validLength <= 0 || bitmapByteNum <= 0)
        return false;

    const unsigned char bit[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    int i = 0, j = 0;
    for (i = 0; i < bitmapByteNum - 1; i++) {
        for (j = 0; j < 8; j++) {
            if ((bitmap[i] & bit[j]) == 0)
                return false; // 不是种子
        }
    }

    int lastByte = static_cast<int>(validLength - (8 * (bitmapByteNum - 1)));
    for (j = 0; j < lastByte; j++) {
        if ((bitmap[i] & bit[j]) == 0)
            return false;
    }

    return true; // 是种子
}

bool Policy::randPieces(int length)
{
    int index;
    QVector<int> temp;

    if (length == 0)
        return false;

    int pieceCount = length;
    for (int i = 0; i < pieceCount; i++)
        temp.insert(i, i);

    randPiecesSelectVec.resize(length);
    srand(static_cast<unsigned int>(time(NULL)));
    for (int i = 0; i < pieceCount; i++) {
        index = (int)((float)(pieceCount - i) * rand() / (RAND_MAX + 1.0));
        randPiecesSelectVec[i] = temp.at(index);
        temp[index] = temp.at(pieceCount - 1 - i);
    }

    return true;
}
