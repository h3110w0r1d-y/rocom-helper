#include "ui/main_window.h"

#include <QApplication>
#include <QImageReader>

int main(int argc, char *argv[])
{
    QImageReader::setAllocationLimit(1024);

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/app.png")));
    app::MainWindow window;
    window.show();
    return app.exec();
}
