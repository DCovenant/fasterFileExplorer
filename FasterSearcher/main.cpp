#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>

#ifdef Q_OS_WIN
#include <windows.h>
#include <iostream>
#endif



int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Allocate console for debugging output but hide it initially
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);
#endif

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/appicon.ico"));

    qDebug() << "Application starting...";
    qDebug() << "Current working directory:" << QDir::currentPath();
    qDebug() << "Application directory:" << QCoreApplication::applicationDirPath();
    
    MainWindow w;
    w.setWindowTitle("GreedSearcher");
    w.show();
    return a.exec();
}
