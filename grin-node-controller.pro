QT = core network

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
        src \
        src/nodes \
        src/http

SOURCES += \
        main.cpp \
        src/http/httpserver.cpp \
        src/nodes/grinppnode.cpp \
        src/nodes/grinrustnode.cpp \
        src/nodes/nodeproc.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    src/http/httpserver.h \
    src/nodes/grinppnode.h \
    src/nodes/grinrustnode.h \
    src/nodes/inodecontroller.h \
    src/nodes/nodeproc.h
