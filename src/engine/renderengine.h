// SPDX-License-Identifier: MIT
// Thrive Video Suite – Render engine (export to file)
//
// Rendering is done in a **separate process** to isolate the main
// application from any crash inside MLT / FFmpeg / codec libraries.
// The subprocess is our own executable invoked with "--render" which
// writes progress lines to stdout.  This is the same pattern used by
// Shotcut (spawns melt) and Kdenlive (spawns kdenlive_render).

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>

namespace Mlt {
class Consumer;
class Producer;
class Profile;
} // namespace Mlt

namespace Thrive {

class MltEngine;

/// Exports the timeline to a video file by spawning a child process.
/// The child process loads the MLT XML and drives the avformat consumer.
/// If the child crashes, the main application stays alive and can
/// report the error.
class RenderEngine : public QObject
{
    Q_OBJECT

public:
    explicit RenderEngine(MltEngine *engine, QObject *parent = nullptr);
    ~RenderEngine() override;

    /// Start rendering to the given output file.
    bool startRender(Mlt::Producer *producer,
                     const QString &outputPath,
                     const QString &format = {},
                     const QString &vcodec = {},
                     const QString &acodec = {},
                     int vBitrateKbps = 0,
                     int aBitrateKbps = 0);

    /// Cancel an in-progress render.
    void cancelRender();

    [[nodiscard]] bool isRendering() const { return m_rendering; }
    [[nodiscard]] int  progressPercent() const { return m_lastPercent; }

signals:
    void renderStarted();
    void renderProgress(int percent);
    void renderFinished(bool success);

private:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

    MltEngine  *m_engine  = nullptr;
    QProcess   *m_process = nullptr;
    QString     m_outputPath;
    QString     m_xmlTempPath;
    QString     m_renderLogPath;
    bool        m_rendering    = false;
    int         m_lastPercent  = 0;
    bool        m_gotSuccess   = false;
};

} // namespace Thrive
