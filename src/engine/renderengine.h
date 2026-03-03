// SPDX-License-Identifier: MIT
// Thrive Video Suite – Render engine (export to file)

#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace Mlt {
class Consumer;
class Producer;
} // namespace Mlt

namespace Thrive {

class MltEngine;

/// Exports the timeline to a video file at full composition resolution.
class RenderEngine : public QObject
{
    Q_OBJECT

public:
    explicit RenderEngine(MltEngine *engine, QObject *parent = nullptr);
    ~RenderEngine() override;

    /// Start rendering to the given output file.
    /// @param producer  The top-level Tractor representing the timeline.
    /// @param outputPath  Destination file path (e.g. "output.mp4").
    /// @param format  Container format (e.g. "mp4", "mkv"). Empty = auto-detect.
    /// @param vcodec  Video codec (e.g. "libx264"). Empty = default.
    /// @param acodec  Audio codec (e.g. "aac"). Empty = default.
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
    [[nodiscard]] int  progressPercent() const;

signals:
    void renderStarted();
    void renderProgress(int percent);
    void renderFinished(bool success);

private:
    void pollProgress();

    MltEngine *m_engine = nullptr;
    std::unique_ptr<Mlt::Consumer> m_renderConsumer;
    Mlt::Producer *m_producer = nullptr;
    bool m_rendering = false;
    int  m_totalFrames = 0;
};

} // namespace Thrive
