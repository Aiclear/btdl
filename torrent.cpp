#include "torrent.h"

#include <cmath>
#include <QCryptographicHash>
#include <QSettings>

static const int RE_CONN_TRACKER = 1000 * 60 * 3;
static const int COMPUTE_PEER_RATE = 1000;
static const int COMPUTE_TOTAL_RATE = 1500;

// 将一个字节数据转换为一个字符串，用此值主要是为了保存下载状态文件的不重名
// @byte 是一个sha1sum的字节码，并非字面的数据
static QString convertToChar(const QByteArray &byte)
{
    const char* sha1sum = byte.data();
    char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char* chSha1sum = new char[41];
    memset(chSha1sum, 0, 41);
    for (int i = 0, j = 0; j < byte.length(); i += 2, j++) {
        chSha1sum[i] = hex[((sha1sum[j] >> 4) & (unsigned char) 0x0F)];
        chSha1sum[i + 1] = hex[(sha1sum[j] & (unsigned char) 0x0F)];
    }
    chSha1sum[41] = '\0';

    return QString(chSha1sum);
}

Torrent::Torrent(QObject *parent) : QObject(parent)
{}

bool Torrent::setTorrent(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly) || !setTorrent(file.readAll()))
        return false;

    file.close();

    this->torrentFileName = fileName;
    return true;
}

bool Torrent::setTorrent(const QByteArray &torrentData)
{
    if (torrentData.isEmpty())
        return false;
    if (!metaInfo.parse(torrentData))
        return false;

    // Calculate SHA1 hash of the "info" section in the torrent
    QByteArray infoValue = metaInfo.infoValue();
    infoHash = QCryptographicHash::hash(infoValue, QCryptographicHash::Sha1);
    return true;
}

void Torrent::producePeerId()
{
    qsrand(static_cast<uint>(time(NULL)));

    char *peer_id = new char[21]; // TODO 判断分配失败
    snprintf(peer_id, 21, "-TT1000-%12d", qrand());
    peerId = QByteArray(peer_id);

    delete []peer_id;
}

void Torrent::start()
{
    // 如果此条件成立，说明已经下载完成
    if (!resumeDownloaded())
        return;

    connect(&tracker, &Tracker::updatePeerList,
            &peerSock, &PeerSock::createPeerSocket);
    connect(&tracker, &Tracker::trackerState,
            this, &Torrent::handleState);

    connect(&peerSock, &PeerSock::peerEmpty,
            &tracker, &Tracker::initialTrackerConnect);
    connect(&peerSock, &PeerSock::peerSockState,
            this, &Torrent::handleState);
    connect(&peerSock, &PeerSock::peerAndSeed,
            this, &Torrent::handlePeerAndSeed);
    connect(&peerSock, &PeerSock::upDownRate,
            this, &Torrent::updateUpDownRate);

    connect(&messageHandler, &MessageHandler::createRequest,
            &policy, &Policy::createReqSliceMsg);
    connect(&messageHandler, &MessageHandler::writeData,
            &processData, &ProcessData::writeSliceToPieceBlock);
    connect(&messageHandler, &MessageHandler::readData,
            &processData, &ProcessData::readSliceFromPieceBlock);

    connect(&policy, &Policy::requestData,
            &messageHandler, &MessageHandler::createRequestMessage);
    connect(&policy, &Policy::checkOutReqSlice,
            &peerSock, &PeerSock::checkPeerReq);

    connect(&processData, &ProcessData::haveOnePiece,
            &peerSock, &PeerSock::prepareHaveMsg);
    connect(&processData, &ProcessData::haveNewPiece,
            this, &Torrent::updateProgress);
    connect(&processData, &ProcessData::processDataState,
            this, &Torrent::handleState);
    connect(&processData, &ProcessData::downloadComplete,
            this, &Torrent::complete);

    connect(&trackerTimer, &QTimer::timeout,
            &tracker, &Tracker::initialTrackerConnect);
    connect(&peerRateTimer, &QTimer::timeout,
            &peerSock, &PeerSock::computePeerRate);
    connect(&totalRateTimer, &QTimer::timeout,
            &peerSock, &PeerSock::computeTotalRate);

    trackerTimer.setInterval(RE_CONN_TRACKER);
    peerRateTimer.setInterval(COMPUTE_PEER_RATE);
    peerRateTimer.setInterval(COMPUTE_TOTAL_RATE);

    // 生成客户端握手需要的peer_id
    if (peerId.isEmpty())
        producePeerId();

    // 初始化连接Tracker需要的信息
    tracker.setPeerId(peerId);
    tracker.setInfoHash(infoHash);
    tracker.setMetaInfo(&metaInfo);
    tracker.setBitmap(bitmap);

    // peersock initial
    peerSock.setMessageHandler(&messageHandler);

    // messagehandler initiald
    messageHandler.initial(infoHash, peerId);
    messageHandler.setBitmap(bitmap);

    // policy initial
    policy.setBitmap(bitmap);
    policy.setMetaInfo(&metaInfo);

    // processdata initial
    processData.setDestinationPath(destinationFolder);
    processData.setMetaInfo(metaInfo);
    processData.setBitmap(bitmap);
    processData.createFiles();

    tracker.initialTrackerConnect();
    trackerTimer.start();
    peerRateTimer.start();
    totalRateTimer.start();
}

void Torrent::setState(Torrent::State state)
{
    this->pstate = state;
    switch (state) {
    case Torrent::Idle:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Idle");
        break;
    case Torrent::Paused:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Paused");
        break;
    case Torrent::Stopping:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Stopping");
        break;
    case Torrent::Preparing:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Preparing");
        break;
    case Torrent::Searching:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Searching");
        break;
    case Torrent::Connecting:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Connecting");
        break;
    case Torrent::WarmingUp:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Warming up");
        break;
    case Torrent::Downloading:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Downloading");
        break;
    case Torrent::Endgame:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Finishing");
        break;
    case Torrent::Seeding:
        pstateString = QT_TRANSLATE_NOOP(Torrent, "Seeding");
        break;
    }
    emit stateChanged(state);
}

Torrent::State Torrent::state() const
{
    return pstate;
}

QString Torrent::stateString() const
{
    return pstateString;
}

void Torrent::setPaused(bool paused)
{
    if (paused) {
        setState(Paused);
        QHashIterator<QTcpSocket *, Peer *> iter(peerSock.getPeers());
        while (iter.hasNext()) {
            iter.next().key()->abort();
        }
        peerSock.getPeers().clear();
    } else {
        setState(bitmap->getDownloadPieces() == metaInfo.sha1Sums().size() ? Seeding : Searching);
//        tracker.initialTrackerConnect();
        restorm();
    }
}

int Torrent::connectedPeerCount()
{
    return peers;
}

int Torrent::seedCount()
{
    return seeds;
}

bool Torrent::resumeDownloaded()
{
    if (bitmap == nullptr) { // 如果位图为空就创建位图
        bitmap = new BitMapOfPieces(metaInfo.sha1Sums().size(),
                                    (int) ceil((float) metaInfo.sha1Sums().size() / 8));
        bitmap->createBitmap(convertToChar(infoHash));
        if (bitmap->getDownloadPieces() == metaInfo.sha1Sums().size()) {
            updateProgress(100);
            setState(State::Seeding);
            emit taskDone();
            return false;
        }
        // 到此处说明这是一个重新恢复的下载
        int downed = 0;
        if ( (downed = bitmap->getDownloadPieces()) != 0) {
            updateProgress(downed * 100 / metaInfo.sha1Sums().size());
        } else {
            updateProgress(0);
        }
    }

    QSettings settings("QtBtProject", "torrent-bitmap");
    settings.beginGroup(torrentFileName);
    settings.setValue("bitmap", bitmap->getBitmapFileName());
    settings.setValue("name", metaInfo.fileForm() == MetaInfo::SingleFileForm ? metaInfo.singleFile().name
                                                                                : metaInfo.name());
    settings.endGroup();
    settings.sync();

    return true;
}

void Torrent::setDestinationFolder(const QString &value)
{
    destinationFolder = value;
}

int Torrent::progress() const
{
    return lastProgressValue;
}

void Torrent::updateProgress(int progress)
{
    // 更新下载的进度
    lastProgressValue = progress;
    emit progressUpdated(progress);
}

void Torrent::handleState(bool &flag)
{

    if (dynamic_cast<Tracker *>(sender()) == (&tracker)) {
        flag = false;
        setState(State::Searching);
        return;
    }

    if (dynamic_cast<PeerSock *>(sender()) == (&peerSock)) {
        flag = false;
        setState(State::Connecting);
        return;
    }

    if (dynamic_cast<ProcessData *>(sender()) == (&processData)) {
        flag = false;
        setState(State::Downloading);
        return;
    }
}

void Torrent::handlePeerAndSeed(int peer, int seed)
{
    peers = peer;
    seeds = seed;
    emit updatePeerAndSeed();
}

void Torrent::updateUpDownRate(float upRate, float downRate)
{
    this->upRate = upRate;
    this->downRate = downRate;
    emit updateRate();
}

void Torrent::pause()
{
    if (trackerTimer.isActive()) {
        trackerTimer.stop();
//        trackerTimer.deleteLater();
    }

    if (peerRateTimer.isActive()) {
        peerRateTimer.stop();
//        peerRateTimer.deleteLater();
    }

    if (totalRateTimer.isActive()) {
        totalRateTimer.stop();
//        totalRateTimer.deleteLater();
    }

    if (bitmap != nullptr)
        bitmap->restoreBitMapFile();
}

void Torrent::restorm()
{
    trackerTimer.start(1000);
    peerRateTimer.start();
    totalRateTimer.start();
}

void Torrent::complete()
{
    pause();

    updateUpDownRate(0, 0);
    emit taskDone();
}

float Torrent::getDownRate() const
{
    return downRate;
}

void Torrent::removeHandler()
{
    // 删除位图文件
    bitmap->deleteBitmap();
    // 删除下载的文件
    processData.deleteFiles();
}

float Torrent::getUpRate() const
{
    return upRate;
}
