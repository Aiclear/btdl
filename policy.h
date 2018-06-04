#ifndef POLICY_H
#define POLICY_H

#include "peerunit.h"
#include "metainfo.h"

#include <QVector>

class Policy : public QObject
{
    Q_OBJECT

public:
    Policy();

    void setMetaInfo(MetaInfo *value) { metaInfo = value; }
    void setBitmap(BitMapOfPieces *value) { bitmap = value; }
    bool randPieces(int length);

    static bool isSeed(Peer &peer);

public slots:
    bool createReqSliceMsg(Peer &peer);

signals:
    void requestData(Peer &peer, int index, int begin, int length);
    void checkOutReqSlice(bool &find, int index);

private:
    MetaInfo *metaInfo;
    BitMapOfPieces *bitmap;
    QVector<int> randPiecesSelectVec;
};

#endif // POLICY_H
