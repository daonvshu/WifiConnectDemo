#include "WifiConnectTest.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    WifiConnectTest w;
    w.show();
    return a.exec();
}
