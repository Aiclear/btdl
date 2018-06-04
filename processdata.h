#ifndef PROCESSDATA_H
#define PROCESSDATA_H

#include "peerunit.h"
#include "metainfo.h"

#include <QDir>
#include <QList>

#include <cmath>

/*
 * 处理从peer下载的数据
 * 以及上传peer请求的数据
*/

struct Pieces {

    Pieces(int index, int begin, int length, const QByteArray &slice)
        : index(index), begin(begin), length(length), slice(slice)
    {
    }

    bool operator ==(const Pieces &piece)
    {
        return index == piece.index
                && begin == piece.begin
                && length == piece.length;
    }

    bool operator <(const Pieces &piece)
    {
        return begin < piece.begin;
    }

    bool operator <(const Pieces *piece)
    {
        return begin < piece->begin;
    }

    int index;
    int begin;
    int length;
    QByteArray slice;
};

class ProcessData : public QObject
{
    Q_OBJECT

public:
    ~ProcessData();

    void setMetaInfo(const MetaInfo &value) { metaInfo = value; }
    void setDestinationPath(const QString &value) { destinationPath = value; }
    void setBitmap(BitMapOfPieces *value) { bitmap = value; }
    // 创建需要下载的文件
    bool createFiles();

    // 删除下载的文件
    bool deleteFiles();

    // 处理从peer下载到的数据
    bool writeSliceToFiles(int index, int begin, int length, const QByteArray &buff);

    // 读取peer请求的slice
    bool readSliceFromFiles(int index, int begin, int length, QByteArray &buff);

    // 关闭所有的文件描述符
    void closeAllFileDescriptor();
private:
    bool isCompletePiece(int size, int index);

public slots:
    bool writeSliceToPieceBlock(int index, int begin, int length, Peer &peer);
    bool readSliceFromPieceBlock(int index, int begin, int length, QByteArray &buff);

signals:
    void haveOnePiece(qint32 pieceIndex);
    void downloadComplete();
    void haveNewPiece(int progress);
    void processDataState(bool &flag);

private:
    QList<QFile *> fds;
    QList<qint64> fileSizes;
    QVector<bool> havePieces;

    // 为新创建文件
    bool newFile;
    // 一个piece的长度
    int pieceLength;
    // 多少个piece
    int numPieces;
     // 文件总长度
    qint64 totalLength;

    QString destinationPath;
    MetaInfo metaInfo;
    QList<QByteArray> sha1s;
    QMap<qint64, QList<Pieces *> *> pieceBlock; // 存放下载的piece

    BitMapOfPieces *bitmap;

    bool stateFlag = true;
};

#endif // PROCESSDATA_H
