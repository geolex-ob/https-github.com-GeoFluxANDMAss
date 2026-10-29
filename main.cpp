#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Расчеты ГТИ");
    app.setOrganizationName("GTI");
    
    MainWindow w;
    w.show();
    return app.exec();
}