TARGET = qqnx

QT += platformsupport-private core-private gui-private

# Uncomment this to build with support for IMF once it becomes available in the BBNDK
#CONFIG += qqnx_imf

# Uncomment this to build with support for PPS based platform integration
#CONFIG += qqnx_pps

CONFIG(blackberry) {
    CONFIG += qqnx_pps

    # Uncomment following line to enable screen event
    # handling through a dedicated thread.
    # CONFIG += qqnx_screeneventthread
} else {
    CONFIG += qqnx_screeneventthread
}

# Uncomment these to enable debugging output for various aspects of the plugin
#DEFINES += QQNXBPSEVENTFILTER_DEBUG
#DEFINES += QQNXBUFFER_DEBUG
#DEFINES += QQNXBUTTON_DEBUG
#DEFINES += QQNXCLIPBOARD_DEBUG
#DEFINES += QQNXFILEDIALOGHELPER_DEBUG
#DEFINES += QQNXGLBACKINGSTORE_DEBUG
#DEFINES += QQNXGLCONTEXT_DEBUG
#DEFINES += QQNXINPUTCONTEXT_DEBUG
#DEFINES += QQNXINPUTCONTEXT_IMF_EVENT_DEBUG
#DEFINES += QQNXINTEGRATION_DEBUG
#DEFINES += QQNXNAVIGATOREVENTHANDLER_DEBUG
#DEFINES += QQNXNAVIGATOREVENTNOTIFIER_DEBUG
#DEFINES += QQNXNAVIGATOR_DEBUG
#DEFINES += QQNXRASTERBACKINGSTORE_DEBUG
#DEFINES += QQNXROOTWINDOW_DEBUG
#DEFINES += QQNXSCREENEVENTTHREAD_DEBUG
#DEFINES += QQNXSCREENEVENT_DEBUG
#DEFINES += QQNXSCREEN_DEBUG
#DEFINES += QQNXVIRTUALKEYBOARD_DEBUG
#DEFINES += QQNXWINDOW_DEBUG
#DEFINES += QQNXCURSOR_DEBUG
#DEFINES += QQNXFILEPICKER_DEBUG


SOURCES =   main.cpp \
            qqnxbuffer.cpp \
            qqnxintegration.cpp \
            qqnxscreen.cpp \
            qqnxwindow.cpp \
            qqnxrasterbackingstore.cpp \
            qqnxrootwindow.cpp \
            qqnxscreeneventhandler.cpp \
            qqnxnativeinterface.cpp \
            qqnxnavigatoreventhandler.cpp \
            qqnxabstractnavigator.cpp \
            qqnxabstractvirtualkeyboard.cpp \
            qqnxservices.cpp \
            qqnxcursor.cpp

HEADERS =   main.h \
            qqnxbuffer.h \
            qqnxkeytranslator.h \
            qqnxintegration.h \
            qqnxscreen.h \
            qqnxwindow.h \
            qqnxrasterbackingstore.h \
            qqnxrootwindow.h \
            qqnxscreeneventhandler.h \
            qqnxnativeinterface.h \
            qqnxnavigatoreventhandler.h \
            qqnxabstractnavigator.h \
            qqnxabstractvirtualkeyboard.h \
            qqnxservices.h \
            qqnxcursor.h

CONFIG(qqnx_screeneventthread) {
    DEFINES += QQNX_SCREENEVENTTHREAD
    SOURCES += qqnxscreeneventthread.cpp
    HEADERS += qqnxscreeneventthread.h
}

LIBS += -lscreen

contains(QT_CONFIG, opengles2) {
    SOURCES += qqnxglcontext.cpp

    HEADERS += qqnxglcontext.h

    LIBS += -lEGL
}

CONFIG(blackberry) {
    SOURCES += qqnxnavigatorbps.cpp \
               qqnxeventdispatcher_blackberry.cpp \
               qqnxbpseventfilter.cpp \
               qqnxvirtualkeyboardbps.cpp \
               qqnxtheme.cpp \
               qqnxsystemsettings.cpp

    HEADERS += qqnxnavigatorbps.h \
               qqnxeventdispatcher_blackberry.h \
               qqnxbpseventfilter.h \
               qqnxvirtualkeyboardbps.h  \
               qqnxtheme.h \
               qqnxsystemsettings.h \
               qqnxfiledialoghelper.h

    LIBS += -lbps
}

CONFIG(blackberry-playbook) {
    SOURCES += qqnxfiledialoghelper_playbook.cpp
} else {
    CONFIG(blackberry) {
        SOURCES += qqnxfiledialoghelper_bb10.cpp \
                   qqnxfilepicker.cpp

        HEADERS += qqnxfilepicker.h
    }
}

CONFIG(qqnx_pps) {
    DEFINES += QQNX_PPS

    SOURCES += qqnxnavigatorpps.cpp \
               qqnxnavigatoreventnotifier.cpp \
               qqnxvirtualkeyboardpps.cpp \
               qqnxclipboard.cpp \
               qqnxbuttoneventnotifier.cpp

    HEADERS += qqnxnavigatorpps.h \
               qqnxnavigatoreventnotifier.h \
               qqnxvirtualkeyboardpps.h \
               qqnxclipboard.h \
               qqnxbuttoneventnotifier.h

    LIBS += -lpps -lclipboard

    CONFIG(qqnx_imf) {
        DEFINES += QQNX_IMF
        HEADERS += qqnxinputcontext_imf.h
        SOURCES += qqnxinputcontext_imf.cpp
    } else {
        HEADERS += qqnxinputcontext_noimf.h
        SOURCES += qqnxinputcontext_noimf.cpp
    }
}

OTHER_FILES += qnx.json

QMAKE_CXXFLAGS += -I./private

include (../../../platformsupport/eglconvenience/eglconvenience.pri)
include (../../../platformsupport/fontdatabases/fontdatabases.pri)

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QQnxIntegrationPlugin
load(qt_plugin)
