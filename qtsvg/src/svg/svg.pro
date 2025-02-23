TARGET     = QtSvg
QT         = core-private gui-private
!contains(QT_CONFIG, no-widgets): QT += widgets-private

DEFINES   += QT_NO_USING_NAMESPACE
win32-msvc*|win32-icc:QMAKE_LFLAGS += /BASE:0x66000000
solaris-cc*:QMAKE_CXXFLAGS_RELEASE -= -O2

QMAKE_DOCS = $$PWD/doc/qtsvg.qdocconf
load(qt_module)

HEADERS += \
    qsvggraphics_p.h        \
    qsvghandler_p.h         \
    qsvgnode_p.h            \
    qsvgstructure_p.h       \
    qsvgstyle_p.h           \
    qsvgfont_p.h            \
    qsvgtinydocument_p.h    \
    qsvgrenderer.h          \
    qsvgwidget.h            \
    qgraphicssvgitem.h      \
    qsvggenerator.h \
    qtsvgglobal.h


SOURCES += \
    qsvggraphics.cpp        \
    qsvghandler.cpp         \
    qsvgnode.cpp            \
    qsvgstructure.cpp       \
    qsvgstyle.cpp           \
    qsvgfont.cpp            \
    qsvgtinydocument.cpp    \
    qsvgrenderer.cpp        \
    qsvgwidget.cpp          \
    qgraphicssvgitem.cpp    \
    qsvggenerator.cpp

wince*: {
    SOURCES += \
        qsvgfunctions_wince.cpp
    HEADERS += \
        qsvgfunctions_wince.h
}

contains(QT_CONFIG, system-zlib) {
    if(unix|win32-g++*):     LIBS_PRIVATE += -lz
    else:                    LIBS += zdll.lib
} else {
    INCLUDEPATH += $$[QT_INSTALL_HEADERS/get]/QtZlib
}
