QT += core gui quick quickcontrols2 testlib dbus concurrent
CONFIG += c++17 testcase
TARGET = omasql_tests
TEMPLATE = app
LIBS += -lsqlite3

INCLUDEPATH += ../src

HEADERS += ../src/aifixer.h ../src/backend.h ../src/database.h ../src/filepicker.h ../src/highlighter.h ../src/resultmodel.h \
           ../src/sqltext.h ../src/theme.h ../src/translator.h
SOURCES += omasql_tests.cpp ../src/aifixer.cpp ../src/backend.cpp ../src/database.cpp ../src/highlighter.cpp ../src/resultmodel.cpp \
           ../src/sqltext.cpp ../src/theme.cpp ../src/translator.cpp
