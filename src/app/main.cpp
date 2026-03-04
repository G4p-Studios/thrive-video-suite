// SPDX-License-Identifier: MIT
// Thrive Video Suite – Application entry point
//
// When invoked with --render, this executable acts as a headless
// render subprocess.  The GUI parent process monitors progress via
// stdout lines like "RENDER:PROGRESS:42".

#include "mainwindow.h"
#include "constants.h"

#include "../core/project.h"
#include "../engine/mltengine.h"
#include "../accessibility/screenreader.h"
#include "../accessibility/announcer.h"
#include "../accessibility/audiocuemanager.h"
#include "../ui/accessibletimelineview.h"

#include <mlt++/MltFactory.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <framework/mlt_factory.h>

#include <QApplication>
#include <QCoreApplication>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QProcess>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QThread>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

// ── Global crash handler ────────────────────────────────────────────
// Catches unhandled exceptions on ANY thread and writes crash info
// to crash_log.txt using only Win32 API calls (safe in corrupted state).
#ifdef Q_OS_WIN
static char g_crashLogPath[MAX_PATH] = {0};

static LONG WINAPI globalCrashHandler(EXCEPTION_POINTERS *info)
{
    HANDLE hFile = CreateFileA(g_crashLogPath, GENERIC_WRITE, 0, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        SetFilePointer(hFile, 0, nullptr, FILE_END);

        SYSTEMTIME st;
        GetLocalTime(&st);

        char buf[1024];
        int len = wsprintfA(buf,
            "=== UNHANDLED EXCEPTION ===\r\n"
            "Time       : %04d-%02d-%02d %02d:%02d:%02d\r\n"
            "Code       : 0x%08lX\r\n"
            "Address    : 0x%p\r\n"
            "Thread ID  : %lu\r\n"
            "Description: %s\r\n\r\n",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond,
            info->ExceptionRecord->ExceptionCode,
            info->ExceptionRecord->ExceptionAddress,
            GetCurrentThreadId(),
            (info->ExceptionRecord->ExceptionCode == 0xC0000005)
                ? "ACCESS_VIOLATION"
            : (info->ExceptionRecord->ExceptionCode == 0xC00000FD)
                ? "STACK_OVERFLOW"
            : (info->ExceptionRecord->ExceptionCode == 0xC0000094)
                ? "INTEGER_DIVIDE_BY_ZERO"
                : "OTHER");

        DWORD written = 0;
        WriteFile(hFile, buf, static_cast<DWORD>(len), &written, nullptr);
        CloseHandle(hFile);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif // Q_OS_WIN

// =====================================================================
// Render subprocess mode  (--render)
// =====================================================================
// This function runs in a headless child process.  It initialises MLT,
// loads the timeline XML, creates an avformat consumer, renders, and
// writes progress to stdout so the GUI parent can update its progress
// bar.  If the process crashes, the parent stays alive and reports it.

static int renderMain(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QStringList args = app.arguments();

    // Parse arguments: --xml <path> --output <path> --format <f>
    //                  --vcodec <v> --acodec <a> --vb <n> --ab <n>
    auto argValue = [&](const QString &key) -> QString {
        const int idx = args.indexOf(key);
        return (idx >= 0 && idx + 1 < args.size()) ? args.at(idx + 1)
                                                    : QString();
    };

    const QString xmlPath  = argValue(QStringLiteral("--xml"));
    const QString output   = argValue(QStringLiteral("--output"));
    const QString format   = argValue(QStringLiteral("--format"));
    const QString vcodec   = argValue(QStringLiteral("--vcodec"));
    const QString acodec   = argValue(QStringLiteral("--acodec"));
    const int vb           = argValue(QStringLiteral("--vb")).toInt();
    const int ab           = argValue(QStringLiteral("--ab")).toInt();

    // Open a render log next to the executable
    const QDir appDir(app.applicationDirPath());
    const QString logPath = appDir.filePath(QStringLiteral("render_log.txt"));
    QFile logFile(logPath);
    (void)logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    QTextStream log(&logFile);

    auto writeLog = [&](const QString &msg) {
        const QString ts = QDateTime::currentDateTime()
                               .toString(QStringLiteral("hh:mm:ss.zzz"));
        log << ts << QStringLiteral("  ") << msg << QStringLiteral("\n");
        log.flush();
    };

    writeLog(QStringLiteral("=== Thrive Video Suite — Render Subprocess ==="));
    writeLog(QStringLiteral("Started: %1").arg(
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
    writeLog(QStringLiteral("XML     : %1").arg(xmlPath));
    writeLog(QStringLiteral("Output  : %1").arg(output));
    writeLog(QStringLiteral("Format  : %1").arg(format));
    writeLog(QStringLiteral("VCodec  : %1").arg(vcodec));
    writeLog(QStringLiteral("ACodec  : %1").arg(acodec));
    writeLog(QStringLiteral("VBitrate: %1k").arg(vb));
    writeLog(QStringLiteral("ABitrate: %1k").arg(ab));

    if (xmlPath.isEmpty() || output.isEmpty()) {
        writeLog(QStringLiteral("ERROR: --xml and --output are required"));
        fprintf(stdout, "RENDER:ERROR:missing arguments\n");
        fflush(stdout);
        return 1;
    }

    // ── Initialise MLT (same env setup as MltEngine::initialize) ────
    const QString repoPath = appDir.absoluteFilePath(QStringLiteral("lib/mlt-7"));
    const QString dataPath = appDir.absoluteFilePath(QStringLiteral("share/mlt-7"));
    const QString profilesPath = dataPath + QStringLiteral("/profiles");

    qputenv("MLT_REPOSITORY",     repoPath.toUtf8());
    qputenv("MLT_DATA",           dataPath.toUtf8());
    qputenv("MLT_PROFILES_PATH",  profilesPath.toUtf8());
    qputenv("MLT_REPOSITORY_DENY",
            "libmltglaxnimate-qt6:libmltqt6:"
            "libmltrtaudio:libmltsox:libmltrubberband:"
            "libmltdecklink:libmltjackrack:"
            "libmltfrei0r:libmltladspa:"
            "libmltmovit:libmltopencv:"
            "libmltoldfilm:libmltkdenlive:"
            "libmltspatialaudio:libmltvidstab:"
            "libmltxine");

    {
        const QByteArray exeDir = appDir.absolutePath().toUtf8();
        QByteArray p = qgetenv("PATH");
        if (!p.contains(exeDir))
            qputenv("PATH", exeDir + ";" + p);
    }

#ifdef Q_OS_WIN
    const UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS
                                      | SEM_NOOPENFILEERRORBOX);
    AddDllDirectory(reinterpret_cast<PCWSTR>(
        appDir.absolutePath()
              .replace(QLatin1Char('/'), QLatin1Char('\\'))
              .utf16()));
#endif

    Mlt::Repository *repo = Mlt::Factory::init(repoPath.toUtf8().constData());

#ifdef Q_OS_WIN
    SetErrorMode(oldMode);
#endif

    if (!repo) {
        writeLog(QStringLiteral("FATAL: Mlt::Factory::init failed"));
        fprintf(stdout, "RENDER:ERROR:MLT init failed\n");
        fflush(stdout);
        return 1;
    }

    writeLog(QStringLiteral("MLT initialised OK"));

    // All MLT objects must be destroyed BEFORE Mlt::Factory::close().
    // Use a scope block so the stack-allocated profile, producer,
    // and consumer are destructed in the right order.
    bool success = false;

    {   // ── MLT object scope ─────────────────────────────────────────

    // ── Load the XML ─────────────────────────────────────────────────
    Mlt::Profile profile;    // will be overwritten by XML content
    const QByteArray xmlArg =
        QStringLiteral("xml:%1").arg(xmlPath).toUtf8();
    Mlt::Producer producer(profile, xmlArg.constData());

    if (!producer.is_valid()) {
        writeLog(QStringLiteral("FATAL: could not load XML as producer"));
        fprintf(stdout, "RENDER:ERROR:XML load failed\n");
        fflush(stdout);
        Mlt::Factory::close();
        return 1;
    }

    const int totalFrames = producer.get_length();
    writeLog(QStringLiteral("Producer loaded: %1 frames, profile %2x%3 @ %4/%5 fps")
                 .arg(totalFrames)
                 .arg(profile.width()).arg(profile.height())
                 .arg(profile.frame_rate_num()).arg(profile.frame_rate_den()));

    if (totalFrames <= 0) {
        writeLog(QStringLiteral("FATAL: producer has no frames"));
        fprintf(stdout, "RENDER:ERROR:no frames\n");
        fflush(stdout);
        Mlt::Factory::close();
        return 1;
    }

    // ── Create avformat consumer ─────────────────────────────────────
    Mlt::Consumer consumer(profile, "avformat");
    if (!consumer.is_valid()) {
        writeLog(QStringLiteral("FATAL: avformat consumer invalid"));
        fprintf(stdout, "RENDER:ERROR:avformat consumer invalid\n");
        fflush(stdout);
        Mlt::Factory::close();
        return 1;
    }

    consumer.set("target",              output.toUtf8().constData());
    consumer.set("real_time",           -1);
    consumer.set("terminate_on_pause",  1);
    consumer.set("rescale",             "bicubic");
    consumer.set("progressive",         1);
    consumer.set("pix_fmt",            "yuv420p");

    if (!format.isEmpty())
        consumer.set("f",      format.toUtf8().constData());
    if (!vcodec.isEmpty())
        consumer.set("vcodec", vcodec.toUtf8().constData());
    if (!acodec.isEmpty())
        consumer.set("acodec", acodec.toUtf8().constData());
    if (vb > 0)
        consumer.set("vb",    QStringLiteral("%1k").arg(vb).toUtf8().constData());
    if (ab > 0)
        consumer.set("ab",    QStringLiteral("%1k").arg(ab).toUtf8().constData());

    consumer.connect(producer);
    producer.set_in_and_out(0, totalFrames - 1);
    producer.seek(0);

    writeLog(QStringLiteral("Consumer configured, starting..."));

    // ── Start encoding ───────────────────────────────────────────────
    const int err = consumer.start();
    if (err != 0) {
        writeLog(QStringLiteral("FATAL: consumer.start() returned %1").arg(err));
        fprintf(stdout, "RENDER:ERROR:consumer start failed (%d)\n", err);
        fflush(stdout);
        Mlt::Factory::close();
        return 1;
    }

    writeLog(QStringLiteral("consumer.start() returned 0 — polling..."));

    int iteration = 0;
    int lastPos   = -1;
    int lastPct   = -1;

    while (!consumer.is_stopped()) {
        QThread::msleep(200);
        ++iteration;

        const int pos = producer.position();
        const int pct = qBound(0,
            static_cast<int>(100.0 * pos / totalFrames), 100);

        // Log every iteration for first 20, then every 10th, or if
        // stalled or past 50%
        const bool stalled = (pos == lastPos && pos > 0);
        if (iteration <= 20 || (iteration % 10) == 0
            || stalled || pct >= 50) {
            writeLog(QStringLiteral("  [iter %1] pos=%2/%3 (%4%)  stalled=%5")
                         .arg(iteration).arg(pos).arg(totalFrames)
                         .arg(pct).arg(stalled ? 1 : 0));
        }
        lastPos = pos;

        // Send progress to parent process
        if (pct != lastPct) {
            lastPct = pct;
            fprintf(stdout, "RENDER:PROGRESS:%d\n", pct);
            fflush(stdout);
        }
    }

    writeLog(QStringLiteral("Poll loop exited after %1 iterations").arg(iteration));

    consumer.stop();
    consumer.purge();

    const int finalPos = producer.position();
    success = (finalPos >= totalFrames - 2);

    writeLog(QStringLiteral("Final position: %1/%2  success=%3")
                 .arg(finalPos).arg(totalFrames).arg(success));

    if (success) {
        fprintf(stdout, "RENDER:PROGRESS:100\n");
        fprintf(stdout, "RENDER:SUCCESS\n");
    } else {
        fprintf(stdout, "RENDER:ERROR:stopped at frame %d/%d\n",
                finalPos, totalFrames);
    }
    fflush(stdout);

    }   // ── End MLT object scope (consumer, producer, profile destroyed) ──

    writeLog(QStringLiteral("Ended: %1").arg(
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
    logFile.close();

    // Safe to close the factory now — all MLT objects are already gone.
    Mlt::Factory::close();
    return success ? 0 : 1;
}

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
    // ── Check for --render subprocess mode ───────────────────────────
    // Must be done BEFORE creating QApplication (which needs a display).
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--render") == 0)
            return renderMain(argc, argv);
    }

    // ── Normal GUI mode ──────────────────────────────────────────────
    // Install log file handler before anything else.
    // QCoreApplication::applicationDirPath() is empty before QApplication
    // construction, so derive the exe directory from argv[0].
    const QFileInfo exeInfo(QString::fromLocal8Bit(argv[0]));
    const QString exeDir = exeInfo.absolutePath();

#ifdef Q_OS_WIN
    // Set up crash log path and install the global exception handler
    // BEFORE anything else — this catches crashes on ANY thread.
    {
        const QString crashPath = exeDir + QStringLiteral("/crash_log.txt");
        const QByteArray pathUtf8 = crashPath.toLocal8Bit();
        strncpy_s(g_crashLogPath, pathUtf8.constData(), _TRUNCATE);
        SetUnhandledExceptionFilter(globalCrashHandler);
    }
#endif

    const QString logPath = exeDir + QStringLiteral("/thrive_debug.log");
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // Fallback: try a hardcoded path
        logFile.setFileName(QStringLiteral("C:/Users/alex/thrive_debug.log"));
        (void)logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
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
