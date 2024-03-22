QT       += core gui
QT       += serialport
CONFIG += c++11


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

VERSION = 1.0.0.0
QMAKE_TARGET_COMPANY = Solderingironspb
QMAKE_TARGET_PRODUCT = Serial port Plotter
QMAKE_TARGET_DESCRIPTION = Application for Debug
QMAKE_TARGET_COPYRIGHT = Oleg Volkov

TARGET = Serial_port_Plotter
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
