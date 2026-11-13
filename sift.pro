QT += core gui qml quick quickcontrols2 dbus concurrent

CONFIG += c++17 release
TARGET = sift
TEMPLATE = app
LIBS += -lsqlite3

HEADERS += \
    src/backend.h \
    src/database.h \
    src/filepicker.h \
    src/highlighter.h \
    src/portalfilepicker.h \
    src/resultmodel.h \
    src/sqltext.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/database.cpp \
    src/highlighter.cpp \
    src/portalfilepicker.cpp \
    src/resultmodel.cpp \
    src/sqltext.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
