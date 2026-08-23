#include "mainwindow.hpp"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Allex Notes");
    app.setOrganizationName("Allex");

    MainWindow window;
    window.show();

    return app.exec();
}
