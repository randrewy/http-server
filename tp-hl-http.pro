TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++11 \
                  -O3

LIBS += -levent \
        -lpthread

SOURCES += main.cpp \
    server.cpp \
    http.cpp

HEADERS += \
    server.h \
    http.h

