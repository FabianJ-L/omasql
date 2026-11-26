QT += core gui qml quick quickcontrols2 testlib dbus concurrent
CONFIG += c++17 release
TARGET = omasql_demo
TEMPLATE = app
LIBS += -lsqlite3

INCLUDEPATH += ../../src

HEADERS += ../../src/aifixer.h ../../src/backend.h ../../src/database.h ../../src/filepicker.h ../../src/highlighter.h \
           ../../src/resultmodel.h ../../src/sqltext.h ../../src/theme.h ../../src/translator.h
SOURCES += demo.cpp ../../src/aifixer.cpp ../../src/backend.cpp ../../src/database.cpp ../../src/highlighter.cpp \
           ../../src/resultmodel.cpp ../../src/sqltext.cpp ../../src/theme.cpp ../../src/translator.cpp
RESOURCES += ../../src/resources.qrc
