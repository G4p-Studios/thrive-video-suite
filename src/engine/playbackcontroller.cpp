// SPDX-License-Identifier: MIT
// Thrive Video Suite – Playback controller implementation

#include "playbackcontroller.h"
#include "mltengine.h"

#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

#include <QTimer>
#include <cmath>

namespace Thrive {

PlaybackController::PlaybackController(MltEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_positionTimer(new QTimer(this))
{
    m_positionTimer->setInterval(40); // ~25 fps polling
    connect(m_positionTimer, &QTimer::timeout, this, [this]() {
        if (m_producer && m_state == State::Playing) {
            emit positionChanged(m_producer->position());
        }
    });
}

PlaybackController::~PlaybackController()
{
    close();
}

void PlaybackController::setProducer(Mlt::Producer *producer)
{
    m_producer = producer;
}

void PlaybackController::setWindowId(quintptr id)
{
    m_windowId = id;
}

bool PlaybackController::open()
{
    if (!m_engine || !m_engine->isInitialized() || !m_producer)
        return false;

    auto *previewProfile = m_engine->previewProfile();

    if (m_windowId != 0) {
        // Preferred path: use the SDL2 consumer with an embedded window
        // so video is rendered inside the application's preview widget.
        m_consumer = std::make_unique<Mlt::Consumer>(*previewProfile, "sdl2");
        if (m_consumer->is_valid()) {
            m_consumer->set("window_id",
                           static_cast<int64_t>(m_windowId));
        } else {
            m_consumer.reset();
        }
    }

    // Fallback chain if SDL2 with window_id failed or no window was set
    if (!m_consumer) {
        static const char *fallbacks[] = { "sdl2_audio", "sdl2", "rtaudio" };
        for (const char *name : fallbacks) {
            m_consumer = std::make_unique<Mlt::Consumer>(*previewProfile, name);
            if (m_consumer->is_valid())
                break;
            m_consumer.reset();
        }
    }
    if (!m_consumer)
        return false;

    m_consumer->set("scrub_audio", m_scrubAudio ? 1 : 0);
    m_consumer->set("real_time", 1);
    m_consumer->connect(*m_producer);
    m_consumer->start();

    updateState(State::Paused);
    return true;
}

void PlaybackController::close()
{
    if (m_consumer) {
        m_consumer->stop();
        m_consumer.reset();
    }
    updateState(State::Stopped);
}

void PlaybackController::play()
{
    if (!m_producer) return;

    m_speed = 1.0;
    m_producer->set_speed(m_speed);
    refreshConsumer();
    updateState(State::Playing);
    emit speedChanged(m_speed);
}

void PlaybackController::pause()
{
    if (!m_producer) return;

    m_speed = 0.0;
    m_producer->set_speed(0.0);
    m_producer->pause();
    refreshConsumer();
    updateState(State::Paused);
    emit speedChanged(m_speed);
}

void PlaybackController::stop()
{
    if (!m_producer) return;

    m_speed = 0.0;
    m_producer->set_speed(0.0);
    m_producer->seek(0);
    refreshConsumer();
    updateState(State::Stopped);
    emit positionChanged(0);
    emit speedChanged(m_speed);
}

void PlaybackController::togglePlayPause()
{
    if (m_state == State::Playing)
        pause();
    else
        play();
}

void PlaybackController::stepFrames(int frames)
{
    if (!m_producer) return;

    if (m_state == State::Playing)
        pause();

    const int newPos = m_producer->position() + frames;
    m_producer->seek(qMax(0, newPos));
    refreshConsumer();
    emit positionChanged(m_producer->position());
}

void PlaybackController::seek(int frame)
{
    if (!m_producer) return;

    m_producer->seek(qMax(0, frame));
    refreshConsumer();
    emit positionChanged(m_producer->position());
}

void PlaybackController::seekToStart()
{
    seek(0);
}

void PlaybackController::seekToEnd(int totalFrames)
{
    seek(totalFrames > 0 ? totalFrames - 1 : 0);
}

void PlaybackController::setScrubAudioEnabled(bool enabled)
{
    m_scrubAudio = enabled;
    if (m_consumer)
        m_consumer->set("scrub_audio", enabled ? 1 : 0);
}

void PlaybackController::playForward()
{
    if (!m_producer) return;

    if (m_speed < 0.0)
        m_speed = 1.0;
    else if (m_speed < 1.0)
        m_speed = 1.0;
    else
        m_speed = std::min(m_speed * 2.0, 32.0);

    m_producer->set_speed(m_speed);
    refreshConsumer();
    updateState(State::Playing);
    emit speedChanged(m_speed);
}

void PlaybackController::playReverse()
{
    if (!m_producer) return;

    if (m_speed > 0.0)
        m_speed = -1.0;
    else if (m_speed > -1.0)
        m_speed = -1.0;
    else
        m_speed = std::max(m_speed * 2.0, -32.0);

    m_producer->set_speed(m_speed);
    refreshConsumer();
    updateState(State::Playing);
    emit speedChanged(m_speed);
}

void PlaybackController::stopTransport()
{
    pause();
}

int PlaybackController::position() const
{
    return m_producer ? m_producer->position() : 0;
}

void PlaybackController::applyPreviewScale(int width, int height)
{
    if (!m_consumer) return;

    m_consumer->set("width", width);
    m_consumer->set("height", height);
    refreshConsumer();
}

void PlaybackController::refreshConsumer()
{
    if (m_consumer) {
        m_consumer->set("scrub_audio", m_scrubAudio ? 1 : 0);
        m_consumer->set("refresh", 1);
    }
}

void PlaybackController::updateState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        if (newState == State::Playing)
            startPositionTimer();
        else
            stopPositionTimer();
        emit stateChanged(m_state);
    }
}

void PlaybackController::startPositionTimer()
{
    if (m_positionTimer && !m_positionTimer->isActive())
        m_positionTimer->start();
}

void PlaybackController::stopPositionTimer()
{
    if (m_positionTimer && m_positionTimer->isActive())
        m_positionTimer->stop();
}

} // namespace Thrive
