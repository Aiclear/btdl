#include "processdata.h"

#include <QCryptographicHash>

bool ProcessData::createFiles()
{ // 创建文件夹和文件
    numPieces = -1;

    // create file
    if (metaInfo.fileForm() == MetaInfo::SingleFileForm) {
        MetaInfoSingleFile singleFile = metaInfo.singleFile();

        QString prefix;
        if (!destinationPath.isEmpty()) {
            prefix = destinationPath;
            if (!prefix.endsWith("/"))
                prefix += "/";
            QDir dir;
            if (!dir.mkpath(prefix)) {
                return false;
            }
        }
        QFile *file = new QFile(prefix + singleFile.name);
//  NOTE:      printf("[%s:%d]: %s\n", __FILE__, __LINE__, prefix.toStdString().data());
        if (!file->open(QFile::ReadWrite)) {
            delete file;
            return false;
        }

        if (file->size() != singleFile.length) {
            newFile = true;
            if (!file->resize(singleFile.length)) {
                delete file;
                return false;
            }
        }
        fileSizes << file->size();
        fds << file;

        pieceLength = metaInfo.pieceLength();
        totalLength = singleFile.length;
    } else {
        QDir dir;
        QString prefix;

        if (!destinationPath.isEmpty()) {
            prefix = destinationPath;
            if (!prefix.endsWith("/"))
                prefix += "/";
        }
        if (!metaInfo.name().isEmpty()) {
            prefix += metaInfo.name();
            if (!prefix.endsWith("/"))
                prefix += "/";
        }
        if (!dir.mkpath(prefix)) {
            return false;
        }

        foreach (const MetaInfoMultiFile &entry, metaInfo.multiFiles()) {
            QString filePath = QFileInfo(prefix + entry.path).path();
            if (!QFile::exists(filePath)) {
                if (!dir.mkpath(filePath)) {
                    return false;
                }
            }

            QFile *file = new QFile(prefix + entry.path);
            if (!file->open(QFile::ReadWrite)) {
                delete file;
                return false;
            }

            if (file->size() != entry.length) {
                newFile = true;
                if (!file->resize(entry.length)) {
                    delete file;
                    return false;
                }
            }
            fileSizes << file->size();
            fds << file;

            totalLength += entry.length; // 获取文件的总长度
        }

        pieceLength = metaInfo.pieceLength(); // 每一个piece的长度
    }

    sha1s = metaInfo.sha1Sums(); // 保存piece校验码的容器
    numPieces = sha1s.size(); // 总的下载piece的数量
    return true;
}

bool ProcessData::deleteFiles()
{

    if (fds.isEmpty())
        return false;

    if (metaInfo.fileForm() == MetaInfo::SingleFileForm) {
        fds.at(0)->remove();
//        printf("[%s:%d]\n", __FILE__, __LINE__);
    } else {

    }

    return true;
}

inline static void relaseQList(QList<Pieces *> *piecesList)
{
    QListIterator<Pieces *> listIterator(*piecesList);
    while (listIterator.hasNext()) {
        delete listIterator.next();
    }

    delete piecesList;
}

inline bool ProcessData::isCompletePiece(int count, int index)
{
    if (index == metaInfo.getPieceValue().lastPieceIndex) {
        if (count == metaInfo.getPieceValue().lastPieceCount) {
            return true;
        }
    } else {
        if (count == metaInfo.getPieceValue().onePieceSliceNum)
            return true;
    }

    return false;
}

bool ProcessData::writeSliceToPieceBlock(int index, int begin, int length, Peer &peer)
{
    if (peer.messageBuff.isEmpty())
        return false;

    QByteArray block;
    QList<Pieces *> *pieces = nullptr;

    Pieces *piece = new Pieces(index, begin, length, peer.messageBuff.mid(13));
    if (piece == nullptr)
        return false;
    pieces = pieceBlock.value(index);
    if (pieces == nullptr) {
        pieces = new QList<Pieces *>;
        pieces->insert(0, piece);
    } else {
        int i = 0;
        for (i = 0; i < pieces->size(); i++) {
            if ((*piece) == (*pieces->value(i))) {
                delete piece;
                return false;
            }
            if ((*piece) < pieces->value(i)) {
                pieces->insert(i, piece);
                break;
            }
        }
        if (i == pieces->size())
            pieces->append(piece);
    }

    pieceBlock.insert(index, pieces);
    if (isCompletePiece(pieces->size(), index)) {
        for (int i = 0; i < pieces->size(); i++)
            block.append(pieces->value(i)->slice);

        if (QCryptographicHash::hash(block, QCryptographicHash::Sha1) == sha1s.at(index)) {

            for (int i = 0; i < pieces->size(); i++) { // 数据写入文件中
                writeSliceToFiles(pieces->value(i)->index,
                                  pieces->value(i)->begin,
                                  pieces->value(i)->length,
                                  pieces->value(i)->slice);
            }
            bitmap->setBitValue(index, 1);
            // 保存下载状态
            int down = bitmap->getDownloadPieces();
            if (down % 10 == 0)
                bitmap->restoreBitMapFile();
            relaseQList(pieceBlock.value(index));
            // 清除已下载的piece
            pieceBlock.remove(index);

            // 清除请求连表中的piece信息
            RequestPiece *req;
            QListIterator<RequestPiece *> iter(peer.requestPiece);
            while (iter.hasNext()) {
                req = iter.next();
                if (req->index == index) {
                    peer.requestPiece.removeAll(req);
                    delete req;
                }
            }

            emit haveOnePiece(index);
            emit haveNewPiece(down * 100 / numPieces);
        }
    } else
        return false;

    if (stateFlag)
        emit processDataState(stateFlag);

    if (bitmap->getDownloadPieces() == numPieces)
        emit downloadComplete();

    return true;
}

bool ProcessData::writeSliceToFiles(int index, int begin, int length, const QByteArray &buff)
{
    if (buff.isEmpty())
        return false;

    // 文件中数据位置的定位
    qint64 linePos = index * pieceLength + begin;

    QFile *file = fds.first();
    if (metaInfo.fileForm() == MetaInfo::SingleFileForm) { // 单文件
        file->seek(linePos);
        file->write(buff);

        return true;
    }

    // 多文件
    int i = 0;
    for (i = 0; i < fds.size(); i++) {
        file = fds.at(i);
        if ((linePos < file->size()) && (linePos + length <= file->size())) { // 内容属于同一个文件
            file->seek(linePos);
            file->write(buff);
            break;
        } else if ((linePos < file->size()) && (linePos + length >= file->size())) { // 跨越了文件
            int offset = 0; // 已经写入文件的字节数据
            int left = length; // 还剩余的字节

            file->seek(linePos);
            file->write(buff.left(static_cast<int>(file->size() - linePos)));

            offset = static_cast<int>(file->size() - linePos); // 已经写入文件中的字节
            left = static_cast<int>(left - (file->size() - linePos)); // 还剩余的字节

            i++;
            file = fds.at(i);

            while (left > 0) {
                if (file->size() >= left) { // 文件大小大于剩余的字节
                    file->seek(0);
                    file->write(buff.mid(offset));
                    left = 0;
                } else {
                    file->seek(0);
                    file->write(buff.mid(offset, static_cast<int>(file->size())));
                    offset = static_cast<int>(offset + file->size());
                    left = static_cast<int>(left - file->size());
                    i++;
                    file = fds.at(i);
                }
            }

            break;
        } else { // 不是对应的文件
            linePos = linePos - file->size();
        }
    }

    return true;
}

bool ProcessData::readSliceFromPieceBlock(int index, int begin, int length, QByteArray &buff)
{
    readSliceFromFiles(index, begin, length, buff);

    return true;
}

bool ProcessData::readSliceFromFiles(int index, int begin, int length, QByteArray &buff)
{
    // 文件中数据位置的定位
    qint64 linePos = index * pieceLength + begin;

    QFile *file = fds.first();
    if (metaInfo.fileForm() == MetaInfo::SingleFileForm) { // 单文件
        file->seek(linePos);
        buff.append(file->read(length));

        return true;
    }

    // 多文件
    for (int i = 0; i < fds.size(); i++) {
        file = fds.at(i);
        if ((linePos < file->size()) && (linePos + length <= file->size())) { // 内容属于同一个文件
            file->seek(linePos);
            buff.append(file->read(length));
            break;
        } else if ((linePos < file->size()) && (linePos + length >= file->size())) { // 跨越了文件
            int offset = 0; // 读取的字节
            int left = length; // 还剩余的字节

            file->seek(linePos);
            buff.append(file->read(file->size() - linePos));

            offset = static_cast<int>(file->size() - linePos);
            left = static_cast<int>(left - (file->size() - linePos));

            i++;
            file = fds.at(i);

            while (left > 0) {
                if (file->size() >= left) { // 文件大小大于剩余的字节
                    file->seek(0);
                    buff.append(file->read(left));
                    left = 0;
                } else {
                    file->seek(0);
                    buff.append(file->read(left));
                    offset = static_cast<int>(offset + file->size());
                    left = static_cast<int>(left - file->size());
                    i++;
                    file = fds.at(i);
                }
            }

            break;
        } else { // 不是对应的文件
            linePos = linePos - file->size();
        }
    }

    return true;
}

ProcessData::~ProcessData()
{
    QFile *file = nullptr;
    QListIterator<QFile *> listIterator(fds);
    while (listIterator.hasNext()) {
        file = listIterator.next();
        file->flush();
        file->close();

        delete file;
    }
}
