#include "fileutils.h"

FileUtils::FileUtils()
{

}

void FileUtils::deleteTarget(const QString &target)
{
    QFileInfo fileInfo(target);
    if (fileInfo.isDir())
        deleteDir(target);
    else
        deleteFile(target);
}

void FileUtils::deleteFile(const QString &filePath)
{
    QFile file(filePath);
    file.remove();
}

void FileUtils::deleteDir(const QString &dirPath)
{
    QDir dir(dirPath);

    dir.setFilter(QDir::AllDirs | QDir::Files
                  | QDir::NoDotAndDotDot);
    QFileInfoList fileInfoList = dir.entryInfoList();

    foreach (QFileInfo info, fileInfoList) {
        if (info.isDir()) {
            deleteFile(info.absoluteFilePath());
        } else {
            QFile file(info.absoluteFilePath());
            file.remove();
        }
    }

    dir.rmpath(dirPath);
}
