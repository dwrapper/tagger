QT       += core gui widgets opengl sql

greaterThan(QT_MAJOR_VERSION, 5) {
    QT += openglwidgets
}

CONFIG += c++17 link_pkgconfig

PKGCONFIG += mpv

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    filedetailstab.cpp \
    filehasher.cpp \
    filterproxy.cpp \
    imageview.cpp \
    main.cpp \
    mainwindow.cpp \
    mpvopenglwidget.cpp \
    pagedproxy.cpp \
    paginationbar.cpp \
    picturedetailstab.cpp \
    taggerstore.cpp \
    thumbnaildelegate.cpp \
    thumbnailmanager.cpp \
    thumbnailmodel.cpp \
    videodetailstab.cpp \
    workspacelistmodel.cpp

HEADERS += \
    filedetailstab.h \
    filehasher.h \
    fileitem.h \
    filterproxy.h \
    imageview.h \
    mainwindow.h \
    mpvopenglwidget.h \
    pagedproxy.h \
    paginationbar.h \
    picturedetailstab.h \
    taggerstore.h \
    thumbnaildelegate.h \
    thumbnailmanager.h \
    thumbnailmodel.h \
    videodetailstab.h \
    workspacelistmodel.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
