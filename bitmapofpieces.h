#ifndef BITMAPOFPIECES_H
#define BITMAPOFPIECES_H

#include <QFile>
#include <QString>

class BitMapOfPieces
{
public:
    BitMapOfPieces(qint64 validLength, qint64 bitFileLength)
        :validLength(validLength), bitFileLength(bitFileLength) {}
    BitMapOfPieces() {}
    ~BitMapOfPieces();

    bool       createBitmap(QString fileName); // 创建保存下载信息的位图
    bool       deleteBitmap(); // 删除位图文件
    bool       allZero(); // 全部的位初始化为零
    bool       allSet(); // 全部的位设置为 1
    bool       setBitValue(qint64 index, unsigned char v);
    int        getBitValue(qint64 index) const;
    bool       restoreBitMapFile(); // 下载被中断，保存下载的位图为文文件
    static int isInterested(const BitMapOfPieces &src, const BitMapOfPieces &dst);

    qint64          getDownloadPieces() const; // 统计位图中所有的至一的位，获取总的下载的pieces的数量
    qint64          getValidLength() const;
    qint64          getBitFileLength() const;
    void            setValidLength(const qint64 &value);
    unsigned char * getBitmap() const;
    void            setBitmap(unsigned char *value);
    void            setBitFileLength(const qint64 &value);

    QString & getBitmapFileName();
    // 调试函数
    void testMode();
    void printBitmap();

private:
    unsigned char *bitmap   = nullptr; // 位图缓冲区的首地址

    qint64 validLength      = 0; // 缓冲区中有效的位数，可用的bit值
    qint64 bitFileLength    = 0; // 位图缓冲区的长度, 字节数组的长度
    qint64 downloadPieces   = 0; // 已经下载的pieces
    QString downloadBitmapFileName; // 保存下载数据位图的名字
};

#endif // BITMAPOFPIECES_H
