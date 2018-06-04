#include "addtorrentdialog.h"
#include "ui_addtorrentdialog.h"

#include "metainfo.h"

#include <QDir>
#include <QFileDialog>

QString AddTorrentDialog::stringNumber(qint64 number)
{
    QString tmp;
    if (number > (1024 * 1024 * 1024))
        tmp.sprintf("%.2fGB", number / (1024.0 * 1024.0 * 1024.0));
    else if (number > (1024 * 1024))
        tmp.sprintf("%.2fMB", number / (1024.0 * 1024.0));
    else if (number > (1024))
        tmp.sprintf("%.2fKB", number / (1024.0));
    else
        tmp.sprintf("%d bytes", int(number));
    return tmp;
}

AddTorrentDialog::AddTorrentDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddTorrentDialog)
{
    ui->setupUi(this);

    connect(ui->browseTorrents, &QPushButton::clicked,
            this, &AddTorrentDialog::selectTorrent);
    connect(ui->browseDestination, &QPushButton::clicked,
            this, &AddTorrentDialog::selectDestination);
    connect(ui->torrentFile, &QLineEdit::textChanged,
            this, &AddTorrentDialog::setTorrent);

    ui->destinationFolder->setText(destinationDirectory = QDir::current().path());
    ui->torrentFile->setFocus();
}

AddTorrentDialog::~AddTorrentDialog()
{
    delete ui;
}

QString AddTorrentDialog::torrentFileName() const
{
    return ui->torrentFile->text();
}

QString AddTorrentDialog::destinationFolder() const
{
    return ui->destinationFolder->text();
}

void AddTorrentDialog::setTorrent(const QString &torrentFile)
{
    if (torrentFile.isEmpty()) {
        enableOkButton();
        return;
    }

    ui->torrentFile->setText(torrentFile);
    if (!torrentFile.isEmpty())
        lastDirectory = QFileInfo(torrentFile).absolutePath();

    if (lastDestinationDirectory.isEmpty())
        lastDestinationDirectory = lastDirectory;

    MetaInfo metaInfo;
    QFile torrent(torrentFile);
    if (!torrent.open(QFile::ReadOnly) || !metaInfo.parse(torrent.readAll())) {
        enableOkButton();
        return;
    }

    ui->torrentFile->setText(torrentFile);
    ui->announceUrl->setText(metaInfo.announceUrl());
    if (metaInfo.comment().isEmpty())
        ui->commentLabel->setText("<unknown>");
    else
        ui->commentLabel->setText(metaInfo.comment());
    if (metaInfo.createdBy().isEmpty())
        ui->creatorLabel->setText("<unknown>");
    else
        ui->creatorLabel->setText(metaInfo.createdBy());
    ui->sizeLabel->setText(stringNumber(metaInfo.totalSize()));
    if (metaInfo.fileForm() == MetaInfo::SingleFileForm) {
        ui->torrentContents->setHtml(metaInfo.singleFile().name);
    } else {
        QString html;
        foreach (MetaInfoMultiFile file, metaInfo.multiFiles()) {
            QString name = metaInfo.name();
            if (!name.isEmpty()) {
                html += name;
                if (!name.endsWith('/'))
                    html += '/';
            }
            html += file.path + "<br>";
        }
        ui->torrentContents->setHtml(html);
    }

    QFileInfo info(torrentFile);
    ui->destinationFolder->setText(info.absolutePath());

    enableOkButton();
}

void AddTorrentDialog::selectTorrent()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose a torrent file"),
                                                    lastDirectory,
                                                    tr("Torrents (*.torrent);; All files (*.*)"));
    if (fileName.isEmpty())
        return;
    lastDirectory = QFileInfo(fileName).absolutePath();
    setTorrent(fileName);
}

void AddTorrentDialog::selectDestination()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a destination directory"),
                                                    lastDestinationDirectory);
    if (dir.isEmpty())
        return;
    lastDestinationDirectory = destinationDirectory = dir;
    ui->destinationFolder->setText(destinationDirectory);
    enableOkButton();
}

void AddTorrentDialog::enableOkButton()
{
    ui->okButton->setEnabled(!ui->destinationFolder->text().isEmpty()
                             && !ui->torrentFile->text().isEmpty());
}
