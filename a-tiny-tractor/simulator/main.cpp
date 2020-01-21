#include <QApplication>

#include "simulator.h"
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app{argc, argv};
    QApplication::setStyle(QStyleFactory::create("fusion"));

    Simulator simulator;
    simulator.show();

    return app.exec();
}
