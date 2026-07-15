#include "tsparser.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QLibraryInfo>
#include <QStandardPaths>
#include <QTranslator>
#include <QXmlStreamReader>

// Set TRACKCLICK_I18N_DEBUG=1 to log where each language's translator is loaded
// from (or why it failed) — useful when "translations stopped working".
static bool i18nDebug()
{
    static const bool on = !qEnvironmentVariableIsEmpty("TRACKCLICK_I18N_DEBUG");
    return on;
}

TsTranslator::TsTranslator(QObject* parent) : QTranslator(parent) {}

bool TsTranslator::loadTs(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QXmlStreamReader xml(&f);
    QString ctx, source;

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNextStartElement();
        if (!xml.isStartElement()) continue;

        const auto tag = xml.name();
        // Use IncludeChildElements so an unexpected child element never raises a
        // parse error that would discard the entire file (and thus the whole
        // language).  Our entries are plain text, but this keeps one odd entry
        // from taking down every translation.
        if (tag == u"name") {
            ctx = xml.readElementText(QXmlStreamReader::IncludeChildElements);
        } else if (tag == u"source") {
            source = xml.readElementText(QXmlStreamReader::IncludeChildElements);
        } else if (tag == u"translation") {
            // Skip placeholder entries ("unfinished" or "obsolete")
            const auto type = xml.attributes().value("type");
            if (type != u"unfinished" && type != u"obsolete") {
                const QString t = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                if (!t.isEmpty() && !ctx.isEmpty() && !source.isEmpty())
                    m_data[ctx][source] = t;
            }
        }
    }
    return !xml.hasError();
}

QString TsTranslator::translate(const char* context, const char* sourceText,
                                 const char* /*disambiguation*/, int /*n*/) const
{
    const auto ci = m_data.constFind(QString::fromLatin1(context));
    if (ci == m_data.constEnd()) return {};
    const auto si = ci->constFind(QString::fromUtf8(sourceText));
    if (si == ci->constEnd()) return {};
    return *si;
}

bool TsTranslator::isEmpty() const
{
    return m_data.isEmpty();
}

// ─── loadBestTranslator ───────────────────────────────────────────────────────
QTranslator* loadBestTranslator(const QString& lang, QObject* parent)
{
    if (lang == "en") return nullptr;

    const QString stem = "trackclick_" + lang;

    // Ordered list of directories to search. The user-data directory comes first
    // so that dropping a new .ts file there overrides the bundled copy immediately,
    // without rebuilding the application.
    const QStringList dirs = {
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + "/translations",
        QCoreApplication::applicationDirPath() + "/translations",
        QCoreApplication::applicationDirPath() + "/../translations",  // macOS bundle
    };

    // For each external directory: prefer a compiled .qm (faster when lrelease is
    // available), otherwise accept a plain .ts XML file (always editable).
    for (const QString& dir : dirs) {
        auto* qt = new QTranslator(parent);
        if (qt->load(dir + "/" + stem + ".qm")) {
            if (i18nDebug()) qWarning() << "TrackClick i18n:" << lang
                                        << "loaded .qm from" << dir;
            return qt;
        }
        delete qt;

        const QString tsPath = dir + "/" + stem + ".ts";
        auto* ts = new TsTranslator(parent);
        if (ts->loadTs(tsPath)) {
            if (i18nDebug()) qWarning() << "TrackClick i18n:" << lang
                                        << "loaded .ts from" << dir;
            return ts;
        }
        delete ts;
        if (i18nDebug() && QFile::exists(tsPath))
            qWarning() << "TrackClick i18n:" << lang
                       << "FAILED to parse existing" << tsPath;
    }

    // Embedded resource: guaranteed fallback, never missing.
    const QString embedded = ":/translations/" + stem + ".ts";
    auto* ts = new TsTranslator(parent);
    if (ts->loadTs(embedded)) {
        if (i18nDebug()) qWarning() << "TrackClick i18n:" << lang
                                    << "loaded embedded" << embedded;
        return ts;
    }
    delete ts;

    if (i18nDebug())
        qWarning() << "TrackClick i18n:" << lang
                   << "NO translator found (embedded parse failed or resource missing)";
    return nullptr;
}

// ─── installQtBaseTranslator ──────────────────────────────────────────────────
void installQtBaseTranslator(const QString& lang)
{
    // One process-wide Qt catalog translator; swap it out on each call so this is
    // safe to invoke on every language change.
    static QTranslator* qtCatalog = nullptr;
    if (qtCatalog) {
        QCoreApplication::removeTranslator(qtCatalog);
        delete qtCatalog;
        qtCatalog = nullptr;
    }
    if (lang == "en") return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QString dir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const QString dir = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    auto* t = new QTranslator(QCoreApplication::instance());
    if (t->load("qtbase_" + lang, dir)) {
        QCoreApplication::installTranslator(t);
        qtCatalog = t;
        if (i18nDebug())
            qWarning() << "TrackClick i18n: Qt catalog for" << lang << "loaded from" << dir;
    } else {
        if (i18nDebug())
            qWarning() << "TrackClick i18n: no Qt catalog for" << lang << "in" << dir
                       << "(Qt-provided strings stay English)";
        delete t;
    }
}
