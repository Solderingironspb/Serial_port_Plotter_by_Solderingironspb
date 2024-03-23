QT       += core gui
QT       += serialport
CONFIG += c++11


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

VERSION = 1.1.0.0
QMAKE_TARGET_COMPANY = Solderingironspb
QMAKE_TARGET_PRODUCT = Serial port Plotter
QMAKE_TARGET_DESCRIPTION = Application for Debug
QMAKE_TARGET_COPYRIGHT = Oleg Volkov

TARGET = Serial_port_Plotter
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QCUSTOMPLOT_USE_OPENGL


LIBS += -lOpengl32

SOURCES += src/main.cpp\
        src/about.cpp \
        src/mainwindow.cpp \
        qcustomplot/qcustomplot.cpp \
        src/serialthreaded.cpp

HEADERS  += src/mainwindow.hpp \
        src/about.h \
        qcustomplot/qcustomplot.h \
        src/serialthreaded.h


FORMS    += src/mainwindow.ui \
    src/about.ui


win32:RC_FILE = src/file.rc
