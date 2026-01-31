#include "mainwindow.h"

#include <QApplication>
#include <QStandardPaths>
#include <QDir>


int main(int argc, char** argv) {
    QCoreApplication::setOrganizationName("Tagger");
    QCoreApplication::setApplicationName("Tagger");

    // Typically ~/.config/Tagger on Linux
    const QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(cfgDir);



    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1200, 800);
    w.show();
    return app.exec();
}

