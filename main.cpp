#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // High-DPI is always enabled in Qt6 — no extra setup needed
#endif

    QApplication app(argc, argv);
    app.setApplicationName("Point-N-Click");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("PointNClick");
    app.setOrganizationDomain("polital.com");

    // Don't quit when last window is hidden (keep tray alive)
    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::warning(nullptr, "Point-N-Click",
            "No system tray detected. The application will still run,\n"
            "but you won't be able to hide it to the tray.");
    }

    MainWindow w;
    w.show();

    return app.exec();
}
