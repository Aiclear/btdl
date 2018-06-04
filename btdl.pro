QT += widgets network

TEMPLATE = app
CONFIG += c++11

TARGET = btdl

SOURCES += main.cpp \
    bitmapofpieces.cpp \
    messagehandler.cpp \
    tracker.cpp \
    peersock.cpp \
    processdata.cpp \
    policy.cpp \
    mainwindow.cpp \
    addtorrentdialog.cpp \
    torrent.cpp \
    bencodeparser.cpp \
    metainfo.cpp \
    fileutils.cpp

HEADERS += \
    bitmapofpieces.h \
    messagehandler.h \
    tracker.h \
    peersock.h \
    processdata.h \
    policy.h \
    mainwindow.h \
    addtorrentdialog.h \
    torrent.h \
    bencodeparser.h \
    peerunit.h \
    metainfo.h \
    fileutils.h

FORMS += \
    addtorrentdialog.ui \
    mainwindow.ui

RESOURCES += \
    icons.qrc
