#include <QApplication>
#include <QMessageBox>
#include <QSettings>
#include <QSystemTrayIcon>
#include "mainwindow.h"
#include "translations/tsparser.h"

int main(int argc, char* argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // High-DPI is always enabled in Qt6 — no extra setup needed
#endif

    QApplication app(argc, argv);
    app.setApplicationName("TrackClick");
    app.setApplicationVersion("0.9.0");
    app.setOrganizationName("TrackClick");
    app.setOrganizationDomain("trackclick.app");

    // Install translator for the saved language before any UI is created.
    // The pointer is passed to MainWindow so it can remove it when the user
    // switches languages (e.g. back to English); MainWindow takes ownership.
    QTranslator* startupTranslator = nullptr;
    {
        QSettings s("TrackClick", "TrackClick");
        const QString lang = s.value("language", "en").toString();
        startupTranslator = loadBestTranslator(lang, &app);
        if (startupTranslator)
            app.installTranslator(startupTranslator);
    }

    // Don't quit when last window is hidden (keep tray alive)
    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::warning(nullptr,
            QCoreApplication::translate("main", "TrackClick"),
            QCoreApplication::translate("main",
                "No system tray detected. The application will still run,\n"
                "but you won't be able to hide it to the tray."));
    }

    MainWindow w(startupTranslator);

    // Honour "start minimized to tray": read the persisted setting before
    // showing the window so we never flash it on screen then hide it.
    {
        QSettings s("TrackClick", "TrackClick");
        if (!s.value("window/startMin", false).toBool())
            w.show();
    }

    return app.exec();
}
