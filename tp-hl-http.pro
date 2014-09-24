TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -levent

SOURCES += main.cpp \
    server.cpp \
    http.cpp

HEADERS += \
    server.h \
    http.h

