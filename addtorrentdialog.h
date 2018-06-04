#ifndef ADDTORRENTDIALOG_H
#define ADDTORRENTDIALOG_H

#include <QDialog>

namespace Ui {
class AddTorrentDialog;
}

class AddTorrentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddTorrentDialog(QWidget *parent = 0);
    ~AddTorrentDialog();

    QString torrentFileName() const;
    QString destinationFolder() const;

    static QString stringNumber(qint64 number);

public slots:
    void setTorrent(const QString &torrentFile);

private slots:
    void selectTorrent();
    void selectDestination();
    void enableOkButton();

private:
    Ui::AddTorrentDialog *ui;
    QString destinationDirectory;
    QString lastDirectory;
    QString lastDestinationDirectory;
};

#endif // ADDTORRENTDIALOG_H
