#-------------------------------------------------
#
# Project created by QtCreator 2016-10-25T11:08:06
#
#-------------------------------------------------

QT       += core gui
QT       += multimedia  \
            multimediawidgets \

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = NxGstVideoPlayer
TEMPLATE = app

equals (TEMPLATE, lib) {
    CONFIG += plugin
} else {
    DEFINES += CONFIG_APPLICATION
}

CONFIG += CONFIG_NXP4330
CONFIG += debug

socname = $$getenv(OECORE_SOCNAME)
equals(socname, "") {
    message("OECORE_SOCNAME is empty")
} else {
    message($$socname)

    equals(socname, nxp3220) {
        CONFIG += CONFIG_NXP3220
        CONFIG -= CONFIG_NXP4330
    }
}

contains(CONFIG, CONFIG_NXP3220) {
    DEFINES += CONFIG_NXP3220
} else {
    DEFINES += CONFIG_NXP4330
}
    # Add SQL library
    LIBS += -L$$PWD/../../library/lib -lnxdaudioutils

    # Add xml config library
    LIBS += -L$$PWD/../../library/lib -lnx_config -lxml2 -lsqlite3 -ludev -lcrypto

    # Add Common UI Module
    LIBS += -L$$PWD/../../library/lib -lnxbaseui

    # Add icu libraries
    LIBS += -licuuc -licui18n

    LIBS += -L$$PWD/../../library/prebuilt/lib -lnxgstvplayer -lnx_renderer

    INCLUDEPATH += $$PWD/../../library/include
    INCLUDEPATH += $$PWD/../../library/prebuilt/include
exists($(SDKTARGETSYSROOT)) {
    INCLUDEPATH += -I$(SDKTARGETSYSROOT)/usr/include/drm
    INCLUDEPATH += -I/$(SDKTARGETSYSROOT)/usr/include
}
CONFIG += link_pkgconfig
PKGCONFIG += glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0 libdrm

SOURCES += \
    CNX_FileList.cpp \
    CNX_SubtitleParser.cpp \
    MainFrame.cpp \
    DAudioIface_Impl.cpp \
    PlayerVideoFrame.cpp \
    PlayListVideoFrame.cpp \
    CNX_GstMoviePlayer.cpp \
    CNX_DrmInfo.cpp

HEADERS  += \
    CNX_Util.h \
    CNX_FileList.h \
    CNX_SubtitleParser.h \
    MainFrame.h \
    NxEvent.h \
    PlayListVideoFrame.h \
    PlayerVideoFrame.h \
    CNX_GstMoviePlayer.h \
    CNX_DrmInfo.h
equals (TEMPLATE, app) {
    SOURCES -= DAudioIface_Impl.cpp
    SOURCES += \
        main.cpp \
        media/CNX_UeventManager.cpp \
        media/CNX_MediaScanner.cpp \
        media/CNX_MediaDatabase.cpp \
        media/CNX_File.cpp \
        media/MediaScanner.cpp \
        media/CNX_VolumeManager.cpp \
        media/CNX_DiskManager.cpp

    HEADERS += \
        media/CNX_UeventManager.h \
        media/CNX_MediaScanner.h \
        media/CNX_MediaDatabase.h \
        media/CNX_Base.h \
        media/CNX_File.h \
        media/MediaScanner.h \
        media/CNX_VolumeManager.h \
        media/MediaConf.h \
        media/CNX_DiskManager.h
}

FORMS    += \
    MainFrame.ui \
    PlayerVideoFrame.ui \
    PlayListVideoFrame.ui
