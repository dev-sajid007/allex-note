#include "mainwindow.hpp"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Allex Notes");
    app.setOrganizationName("Allex");
    app.setWindowIcon(QIcon(":/allex-notes-128.png"));

    MainWindow window;
    window.show();

    return app.exec();
}
