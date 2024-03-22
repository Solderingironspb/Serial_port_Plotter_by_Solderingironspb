#include "mainwindow.hpp"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName("Solderingironspb");
    QApplication::setApplicationName("Serial port Plotter");
    QApplication::setApplicationVersion("1.0");
    MainWindow w;
    w.show();

    return a.exec();
}
