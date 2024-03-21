QT       += core gui
QT       += serialport
CONFIG += c++11


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = serial_port_plotter
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QCUSTOMPLOT_USE_OPENGL


LIBS += -lOpengl32

SOURCES += main.cpp\
        about.cpp \
        mainwindow.cpp \
        qcustomplot/qcustomplot.cpp

HEADERS  += mainwindow.hpp \
        about.h \
        qcustomplot/qcustomplot.h


FORMS    += mainwindow.ui \
    about.ui

win32:RC_FILE = file.rc
