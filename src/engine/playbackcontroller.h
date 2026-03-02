// SPDX-License-Identifier: MIT
// Thrive Video Suite – Playback controller (play/pause/seek/scrub)

#pragma once

#include <QObject>
#include <memory>

namespace Mlt {
class Consumer;
class Producer;
class Profile;
} // namespace Mlt

namespace Thrive {

class MltEngine;
class Timeline;

/// Controls playback of the MLT pipeline: play, pause, seek, step, scrub audio.
class PlaybackController : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Playing, Paused };
    Q_ENUM(State)

    explicit PlaybackController(MltEngine *engine, QObject *parent = nullptr);
    ~PlaybackController() override;

    /// Attach the top-level MLT producer (Tractor) that represents the timeline.
    void setProducer(Mlt::Producer *producer);

    /// Open and start the consumer. Call after setProducer().
    bool open();

    /// Close the consumer.
    void close();

    // Transport
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    /// Step forward/backward by N frames. Positive = forward.
    void stepFrames(int frames);

    /// Seek to an absolute frame position.
    void seek(int frame);

    /// Seek to the beginning / end of the timeline.
    void seekToStart();
    void seekToEnd(int totalFrames);

    // Scrub audio
    [[nodiscard]] bool scrubAudioEnabled() const { return m_scrubAudio; }
    void setScrubAudioEnabled(bool enabled);

    // Speed (J/K/L playback)
    void playForward();   ///< L key – increase forward speed
    void playReverse();   ///< J key – increase reverse speed
    void stopTransport(); ///< K key – stop

    [[nodiscard]] State state()    const { return m_state; }
    [[nodiscard]] int   position() const;
    [[nodiscard]] double speed()   const { return m_speed; }

    /// Apply preview scale change to consumer without restart.
    void applyPreviewScale(int width, int height);

signals:
    void stateChanged(State state);
    void positionChanged(int frame);
    void speedChanged(double speed);

private:
    void refreshConsumer();
    void updateState(State newState);

    MltEngine *m_engine = nullptr;
    std::unique_ptr<Mlt::Consumer> m_consumer;
    Mlt::Producer *m_producer = nullptr; // not owned – the Tractor lives in the engine

    State  m_state = State::Stopped;
    double m_speed = 0.0;
    bool   m_scrubAudio = true;
};

} // namespace Thrive
