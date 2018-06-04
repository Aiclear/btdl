/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef METAINFO_H
#define METAINFO_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

struct PieceValue
{
    qint64 lastPieceCount = 0;
    qint64 lastPieceIndex = 0;
    qint64 lastSliceLength = 0;
    int onePieceSliceNum = 0;
};

struct MetaInfoSingleFile
{
    qint64 length;
    QByteArray md5sum;
    QString name;
    // int pieceLength;
    // QList<QByteArray> sha1Sums;
};

struct MetaInfoMultiFile
{
    qint64 length;
    QByteArray md5sum;
    QString path;
};

/*
 * MetaInfo 内保存了从种子文件中获取到的相关的信息
 */
class MetaInfo
{
public:
    enum FileForm {
        SingleFileForm,
        MultiFileForm
    };

    MetaInfo();
    void clear();

    bool parse(const QByteArray &data);
    QString errorString() const;

    QByteArray infoValue() const;

    FileForm fileForm() const;
    QString announceUrl() const;
    QStringList announceList() const;
    QDateTime creationDate() const;
    QString comment() const;
    QString createdBy() const;

    // For single file form
    MetaInfoSingleFile singleFile() const;

    // For multifile form
    QList<MetaInfoMultiFile> multiFiles() const;
    QString name() const;
    int pieceLength() const;
    QList<QByteArray> sha1Sums() const;

    // Total size
    qint64 totalSize() const;

    // 与piece相关的一些变量
    PieceValue getPieceValue() const;
    int pieceNum();

private:
    void initialPieceValue();

private:
    PieceValue  pieceValue;

    QString errString;
    QByteArray content;
    QByteArray infoData;

    // 表示下载单个文件还是多个文件
    FileForm metaInfoFileForm;
    // 保存单个文件的相关下载信息
    MetaInfoSingleFile metaInfoSingleFile;
    // 多个下载文件的相关信息
    QList<MetaInfoMultiFile> metaInfoMultiFiles;
    // tracker服务器的url
    QString metaInfoAnnounce;
    // announce-list 字段的所有的备用tracker服务器的url地址
    QStringList metaInfoAnnounceList;
    // 种子文件的创建日期
    QDateTime metaInfoCreationDate;
    // 种子创建者的注释
    QString metaInfoComment;
    // 种子文件的作者
    QString metaInfoCreatedBy;
    // 保存的文件名或者目录名
    QString metaInfoName;
    // 每个piece的长度
    int metaInfoPieceLength;
    // 每个piece的sha1校验码
    QList<QByteArray> metaInfoSha1Sums;
    // piece的总的数量
    int metaInfoPieceNum;
};

#endif
