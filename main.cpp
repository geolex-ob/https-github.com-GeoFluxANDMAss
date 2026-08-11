#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GeoFluxMass");
    app.setOrganizationName("GeoFluxMass");

    MainWindow window;
    window.setWindowTitle("Калькулятор бурового инструмента");
    window.resize(1200, 800);
    window.show();

    return app.exec();
}
