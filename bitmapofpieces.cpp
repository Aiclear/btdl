#include "bitmapofpieces.h"
#include "messagehandler.h"

#include <iostream>
#include <QDebug>
#include <QDir>

bool BitMapOfPieces::createBitmap(QString fileName)
{ // 创建位图

    QDir dir;
    dir.mkdir(".btx");
    this->downloadBitmapFileName = fileName;
    try {
        bitmap = new unsigned char[bitFileLength]; // TODO: 判断内存分配失败的情况
    } catch (std::bad_alloc &e) {
        std::cout << "new error" << e.what() << std::endl;
    }

    QFile file(".btx/" + this->downloadBitmapFileName);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            while (file.read((char *) bitmap, bitFileLength) != 0);
            file.close();
            downloadPieces = getDownloadPieces();
        } else {
            qDebug() << "file open error";
            return false;
        }
    } else { // 位图文件不存在，为全新的下载
        memset(bitmap, 0, bitFileLength); // 全部的位初始化位零
    }

    return true;
}

bool BitMapOfPieces::deleteBitmap()
{
    QFile file(this->downloadBitmapFileName);
    if (file.exists()) {
        file.remove();

        return true;
    }

    return false;
}

bool BitMapOfPieces::allZero()
{ // 全部设置为零
    if (bitmap == nullptr)
        return false;
    memset(bitmap, 0, static_cast<size_t>(bitFileLength));
    return true;
}

bool BitMapOfPieces::allSet()
{ // 全部设置为一
    if (bitmap == nullptr)
        return false;
    memset(bitmap, 0xff, static_cast<size_t>(bitFileLength));
    return true;
}

qint64 BitMapOfPieces::getDownloadPieces() const
{ // 获取位图中所有的值为 1 的位数
    if (bitmap == nullptr)
        return 0;

    const unsigned char mask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

    qint64 dpieces = 0;
    int i, j;
    for (i = 0; i < bitFileLength - 1; i++) {
        for (j = 0; j < 8; j++) {
            if ((mask[j] & bitmap[i]) != 0)
                dpieces++;
        }
    }

    const char c = bitmap[i];
    j = static_cast<int>(validLength - ((bitFileLength - 1) * 8));
    for (i = 0; i < j; i++) {
        if ((c & mask[i]) != 0)
            dpieces++;
    }

    return dpieces;
}

bool BitMapOfPieces::setBitValue(qint64 index, unsigned char v)
{ // 设置某一位的值
    if (index > validLength || index < 0)
        return false;
    if ((v != 1) && (v != 0))
        return false;

    if (bitmap == nullptr)
        return false;

    unsigned char mask = 0x01;
    int i = static_cast<int>(index / 8);
    int j = static_cast<int>(index % 8);

    if (v == 1) {
        v = mask << (7 - j);
        bitmap[i] = bitmap[i] | v;
    } else {
        v = ~(mask << (7 - j));
        bitmap[i] = bitmap[i] & v;
    }

    return true;
}

int BitMapOfPieces::getBitValue(qint64 index) const
{ // 获取某一位的值
    if (index > validLength || index < 0)
        return -1;

    if (bitmap == nullptr)
        return -1;

    int i = static_cast<int>(index / 8); // 定位到具体字节
    int j = static_cast<int>(index % 8); // 定位到具体字节的具体位

    unsigned char c = bitmap[i];
    c = c >> (7 - j);
    if (c % 2 == 0)
        return 0;
    else
        return 1;
}

bool BitMapOfPieces::restoreBitMapFile()
{ // 保存位图文件，断点续传
    if (bitmap != nullptr) {
        if (!this->downloadBitmapFileName.isEmpty()) {
            QFile file(".btx/" + this->downloadBitmapFileName);
            file.open(QIODevice::WriteOnly | QIODevice::Truncate);
            try {
                file.write((const char *) bitmap, bitFileLength);
                file.flush();
                file.close();
            } catch (...) { // TODO 捕作打开文件的异常
                qDebug() << "file open error";
            }
        }
    }

    return false;
}

int BitMapOfPieces::isInterested(const BitMapOfPieces &src, const BitMapOfPieces &dst)
{ // 比较自己和peer的位图信息
    unsigned char bitVal[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    unsigned char c1, c2;
    int i, j;

    if (src.getBitFileLength() != dst.getBitFileLength() ||
        src.getValidLength() != dst.getValidLength())
        return -1;

    if (src.getBitmap() == nullptr || dst.getBitmap() == nullptr)
        return -1;

    for (i = 0; i < src.getBitFileLength() - 1; i++) {
        for (j = 0; j < 8; j++) {
            c1 = (src.getBitmap()[i]) & bitVal[j];
            c2 = (dst.getBitmap()[i]) & bitVal[j];
            if (c1 > 0 && c2 == 0)
                return 1;
        }
    }

    j = static_cast<int>(src.getValidLength() % 8);
    c1 = src.getBitmap()[src.getBitFileLength() - 1];
    c2 = dst.getBitmap()[dst.getBitFileLength() - 1];
    for (i = 0; i < j; i++) {
        if (((c1 & bitVal[i]) > 0) && ((c2 & bitVal[i]) == 0))
            return 1;
    }

    return 0;
}

BitMapOfPieces::~BitMapOfPieces()
{
    delete[]bitmap;
}

void BitMapOfPieces::setValidLength(const qint64 &value)
{
    validLength = value;
}

void BitMapOfPieces::setBitmap(unsigned char *value)
{
    bitmap = value;
}

void BitMapOfPieces::setBitFileLength(const qint64 &value)
{
    bitFileLength = value;
}

QString &BitMapOfPieces::getBitmapFileName()
{
    return this->downloadBitmapFileName;
}

qint64 BitMapOfPieces::getValidLength() const
{
    return validLength;
}

qint64 BitMapOfPieces::getBitFileLength() const
{
    return bitFileLength;
}

unsigned char *BitMapOfPieces::getBitmap() const
{
    return bitmap;
}

// 测试模块
void BitMapOfPieces::printBitmap()
{ // 打印位图的信息
    const unsigned char mask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

    int temp = 0;
    for (int i = 0; i < bitFileLength; i++) {
        for (int j = 0; j < 8; j++) {
            if ((mask[j] & bitmap[i]) != 0) {
                printf("%c", '1');
            } else {
                printf("%c", '0');
            }
            temp++;
            if ((temp % 8) == 0 && temp != 0)
                printf(" ");
            if ((temp % 64) == 0 && temp != 0)
                printf("\n");
        }
    }
}

// 测试模块
void BitMapOfPieces::testMode()
{
    allZero();
    setBitValue(0, 1);
    printBitmap();
    printf("\n");
}
