// SPDX-License-Identifier: MIT
// Thrive Video Suite – Render engine implementation
//
// The actual encoding runs in a **child process** (our own exe with
// --render) so that any crash in MLT / FFmpeg cannot bring down the
// editor.  The child writes "RENDER:PROGRESS:xx" lines to stdout.

#include "renderengine.h"
#include "mltengine.h"

#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>

#include <QTemporaryFile>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>

namespace Thrive {

// ─── RenderEngine ────────────────────────────────────────────────

RenderEngine::RenderEngine(MltEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

RenderEngine::~RenderEngine()
{
    cancelRender();
}

bool RenderEngine::startRender(Mlt::Producer *producer,
                                const QString &outputPath,
                                const QString &format,
                                const QString &vcodec,
                                const QString &acodec,
                                int vBitrateKbps,
                                int aBitrateKbps)
{
    if (m_rendering || !m_engine || !m_engine->isInitialized() || !producer)
        return false;

    m_outputPath = outputPath;

    // ── 1. Serialize the tractor to MLT XML ──────────────────────────
    auto *compProfile = m_engine->compositionProfile();

    {
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);   // we remove manually later
        if (!tmpFile.open()) {
            qWarning() << "RenderEngine: could not create temp XML file";
            return false;
        }
        m_xmlTempPath = tmpFile.fileName();
        tmpFile.close();

        Mlt::Consumer xmlCons(*compProfile, "xml",
                              m_xmlTempPath.toUtf8().constData());
        if (!xmlCons.is_valid()) {
            qWarning() << "RenderEngine: xml consumer invalid";
            QFile::remove(m_xmlTempPath);
            m_xmlTempPath.clear();
            return false;
        }
        xmlCons.set("no_meta", 1);
        xmlCons.connect(*producer);
        xmlCons.run();
    }

    // ── 2. Save a debug copy that survives crashes ──────────────────
    {
        const QString debugXml = QDir(QCoreApplication::applicationDirPath())
                                     .filePath(QStringLiteral("render_debug.mlt"));
        QFile::remove(debugXml);
        QFile::copy(m_xmlTempPath, debugXml);
        qDebug() << "RenderEngine: debug XML saved to" << debugXml;
    }

    // ── 3. Compute effective bitrates ────────────────────────────────
    int effectiveVb = vBitrateKbps;
    int effectiveAb = aBitrateKbps;
    if (effectiveVb <= 0) {
        const int64_t px = static_cast<int64_t>(compProfile->width())
                         * compProfile->height();
        effectiveVb = qMax(1000, static_cast<int>(px * 5000 / (1920 * 1080)));
    }
    if (effectiveAb <= 0)
        effectiveAb = 192;

    // ── 4. Build subprocess command line ─────────────────────────────
    const QString exe = QCoreApplication::applicationFilePath();
    QStringList args;
    args << QStringLiteral("--render")
         << QStringLiteral("--xml")     << m_xmlTempPath
         << QStringLiteral("--output")  << outputPath
         << QStringLiteral("--format")  << (format.isEmpty()
                                                ? QStringLiteral("mp4") : format)
         << QStringLiteral("--vcodec")  << (vcodec.isEmpty()
                                                ? QStringLiteral("mpeg4") : vcodec)
         << QStringLiteral("--acodec")  << (acodec.isEmpty()
                                                ? QStringLiteral("aac") : acodec)
         << QStringLiteral("--vb")      << QString::number(effectiveVb)
         << QStringLiteral("--ab")      << QString::number(effectiveAb);

    qDebug() << "RenderEngine: spawning render subprocess"
             << exe << args.join(QLatin1Char(' '));

    // ── 5. Launch the child process ──────────────────────────────────
    m_rendering   = true;
    m_lastPercent = 0;
    m_gotSuccess  = false;

    m_renderLogPath = QDir(QCoreApplication::applicationDirPath())
                          .filePath(QStringLiteral("render_log.txt"));

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &RenderEngine::onProcessReadyRead);
    connect(m_process, &QProcess::finished,
            this, &RenderEngine::onProcessFinished);

    m_process->start(exe, args);
    if (!m_process->waitForStarted(5000)) {
        qWarning() << "RenderEngine: failed to start render subprocess";
        delete m_process;
        m_process = nullptr;
        m_rendering = false;
        QFile::remove(m_xmlTempPath);
        m_xmlTempPath.clear();
        return false;
    }

    emit renderStarted();
    return true;
}

void RenderEngine::onProcessReadyRead()
{
    while (m_process && m_process->canReadLine()) {
        const QByteArray line = m_process->readLine().trimmed();
        const QString str = QString::fromUtf8(line);

        if (str.startsWith(QStringLiteral("RENDER:PROGRESS:"))) {
            bool ok = false;
            const int pct = QStringView(str).mid(16).toInt(&ok);
            if (ok && pct != m_lastPercent) {
                m_lastPercent = pct;
                emit renderProgress(pct);
            }
        } else if (str == QStringLiteral("RENDER:SUCCESS")) {
            m_gotSuccess = true;
        }
        // All other lines (RENDER:LOG:... etc) are informational
    }
}

void RenderEngine::onProcessFinished(int exitCode,
                                      QProcess::ExitStatus status)
{
    // Drain any remaining output
    if (m_process) {
        const QByteArray remaining = m_process->readAll();
        if (remaining.contains("RENDER:SUCCESS"))
            m_gotSuccess = true;
    }

    const bool crashed = (status == QProcess::CrashExit);
    const bool success = !crashed && exitCode == 0 && m_gotSuccess;

    qDebug() << "RenderEngine: subprocess finished  exitCode=" << exitCode
             << "crashed=" << crashed << "success=" << success
             << "log=" << m_renderLogPath;

    if (!success) {
        qWarning() << "RenderEngine: render failed.  Check"
                   << m_renderLogPath << "and crash_log.txt";
    } else {
        emit renderProgress(100);
    }

    // Clean up temp XML (debug copy was already saved)
    if (!m_xmlTempPath.isEmpty()) {
        QFile::remove(m_xmlTempPath);
        m_xmlTempPath.clear();
    }

    // Clean up partial output on failure
    if (!success && !m_outputPath.isEmpty())
        QFile::remove(m_outputPath);

    m_rendering = false;
    m_process->deleteLater();
    m_process = nullptr;

    emit renderFinished(success);
}

void RenderEngine::cancelRender()
{
    if (!m_process)
        return;

    disconnect(m_process, nullptr, this, nullptr);

    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }

    delete m_process;
    m_process = nullptr;

    const bool wasRendering = m_rendering;
    m_rendering = false;

    // Remove partial output and temp XML
    if (!m_outputPath.isEmpty())
        QFile::remove(m_outputPath);
    m_outputPath.clear();

    if (!m_xmlTempPath.isEmpty()) {
        QFile::remove(m_xmlTempPath);
        m_xmlTempPath.clear();
    }

    if (wasRendering)
        emit renderFinished(false);
}

} // namespace Thrive
