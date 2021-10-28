#include "mainwindow.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //ustawienie ścieżki do aplikacji
    QDir::setCurrent(a.applicationDirPath());

    MainWindow w;
    w.show();
    return a.exec();
}

