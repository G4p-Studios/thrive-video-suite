// SPDX-License-Identifier: MIT
// Thrive Video Suite – Application entry point

#include "mainwindow.h"
#include "constants.h"

#include "../core/project.h"
#include "../engine/mltengine.h"
#include "../accessibility/screenreader.h"
#include "../accessibility/announcer.h"
#include "../accessibility/audiocuemanager.h"
#include "../ui/accessibletimelineview.h"

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QProcess>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

// Write Qt debug/warning messages to a log file next to the exe
static QFile *g_logFile = nullptr;
static void logMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (!g_logFile) return;
    const char *tag = "DEBUG";
    switch (type) {
    case QtWarningMsg:  tag = "WARN "; break;
    case QtCriticalMsg: tag = "CRIT "; break;
    case QtFatalMsg:    tag = "FATAL"; break;
    default: break;
    }
    g_logFile->write(QStringLiteral("[%1] %2\n")
                         .arg(QLatin1String(tag), msg).toUtf8());
    g_logFile->flush();
}

int main(int argc, char *argv[])
{
    // Install log file handler before anything else.
    // QCoreApplication::applicationDirPath() is empty before QApplication
    // construction, so derive the exe directory from argv[0].
    const QFileInfo exeInfo(QString::fromLocal8Bit(argv[0]));
    const QString exeDir = exeInfo.absolutePath();
    const QString logPath = exeDir + QStringLiteral("/thrive_debug.log");
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // Fallback: try a hardcoded path
        logFile.setFileName(QStringLiteral("C:/Users/alex/thrive_debug.log"));
        logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    }
    g_logFile = &logFile;
    qInstallMessageHandler(logMessageHandler);
    qDebug() << "Thrive Video Suite starting, log at:" << logFile.fileName();

    QApplication app(argc, argv);
    app.setApplicationName(QLatin1String(Thrive::kAppName));
    app.setOrganizationName(QLatin1String(Thrive::kOrgName));
    app.setOrganizationDomain(QLatin1String(Thrive::kOrgDomain));
    app.setApplicationVersion(QLatin1String(Thrive::kAppVersion));

    // ── Translations ────────────────────────────────────────────────
    QTranslator translator;
    if (translator.load(QLocale(),
                        QStringLiteral("thrive"),
                        QStringLiteral("_"),
                        QStringLiteral(":/translations"))) {
        app.installTranslator(&translator);
    }

    // ── Screen reader ───────────────────────────────────────────────
    Thrive::ScreenReader::instance().initialize();

    // ── Accessible timeline table factory ────────────────────────────
    // Must be called before any TimelineWidget is created so that
    // QAccessible knows how to wrap it as a table.
    Thrive::registerAccessibleTimelineFactory();

    // ── MLT Framework ───────────────────────────────────────────────
    Thrive::MltEngine engine;
    engine.initialize();
    engine.setCompositionProfile(Thrive::kDefaultWidth,
                                Thrive::kDefaultHeight,
                                Thrive::kDefaultFps);
    engine.setPreviewScale(Thrive::kDefaultPreviewScale);

    // ── Core project ────────────────────────────────────────────────
    Thrive::Project project;
    project.setFps(Thrive::kDefaultFps);
    project.setResolution(Thrive::kDefaultWidth, Thrive::kDefaultHeight);
    project.setPreviewScale(Thrive::kDefaultPreviewScale);
    project.reset();

    // ── Accessibility ───────────────────────────────────────────────
    auto *announcer = new Thrive::Announcer(&app);
    auto *cues      = new Thrive::AudioCueManager(&app);
    cues->loadCues();

    // Restore saved preferences
    {
        QSettings s;
        cues->setEnabled(
            s.value(QLatin1String(Thrive::kSettingsAudioCuesOn), true)
                .toBool());
        cues->setVolume(
            s.value(QLatin1String(Thrive::kSettingsAudioCueVolume), 0.5)
                .toFloat());
    }

    // ── Main window ─────────────────────────────────────────────────
    Thrive::MainWindow window(&project, &engine, announcer, cues);
    announcer->setTarget(&window);
    window.show();

    announcer->announce(
        QObject::tr("Thrive Video Suite is ready."),
        Thrive::Announcer::Priority::High);

    // ── Event loop ──────────────────────────────────────────────────
    const int exitCode = app.exec();

    // ── Restart handling ────────────────────────────────────────────
    if (exitCode == Thrive::EXIT_RESTART) {
        QProcess::startDetached(
            QApplication::applicationFilePath(),
            QApplication::arguments().mid(1));
    }

    return exitCode;
}
