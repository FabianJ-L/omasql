QT += core gui quick quickcontrols2 testlib dbus concurrent
CONFIG += c++17 testcase
TARGET = sift_tests
TEMPLATE = app
LIBS += -lsqlite3

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/database.h ../src/filepicker.h ../src/resultmodel.h \
           ../src/sqltext.h ../src/theme.h ../src/translator.h
SOURCES += sift_tests.cpp ../src/backend.cpp ../src/database.cpp ../src/resultmodel.cpp \
           ../src/sqltext.cpp ../src/theme.cpp ../src/translator.cpp
