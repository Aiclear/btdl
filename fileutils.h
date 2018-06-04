#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QDir>
#include <QString>

class FileUtils
{
public:
    FileUtils();

    static void deleteTarget(const QString &target);

private:
    static void deleteFile(const QString & filePath);
    static void deleteDir(const QString & dirPath);
};

#endif // FILEUTILS_H
