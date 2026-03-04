// SPDX-License-Identifier: MIT
// Thrive Video Suite – Playback controller implementation

#include "playbackcontroller.h"
#include "mltengine.h"

#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

#include <framework/mlt_events.h>
#include <framework/mlt_frame.h>
#include <framework/mlt_properties.h>

#include <QTimer>
#include <cmath>

// ── File-local consumer-frame-show callback ─────────────────────────
// Called on the MLT consumer thread for every frame "shown".
// We increment the frame's refcount, then post to the GUI thread for
// image decoding (mlt_frame_get_image is heavy and must not run on the
// consumer's audio thread).
static void onFrameShow(mlt_properties /*owner*/, void *self,
                        mlt_event_data data)
{
    auto *pc = static_cast<Thrive::PlaybackController *>(self);

    // Drop if the GUI is still processing the previous frame
    if (pc->frameProcessingFlag().load(std::memory_order_acquire))
        return;

    mlt_frame frame = mlt_event_data_to_frame(data);
    if (!frame)
        return;

    mlt_properties_inc_ref(MLT_FRAME_PROPERTIES(frame));
    pc->frameProcessingFlag().store(true, std::memory_order_release);

    QMetaObject::invokeMethod(pc, [pc, frame]() {
        uint8_t *buf = nullptr;
        mlt_image_format fmt = mlt_image_rgba;
        int w = 0, h = 0;
        int err = mlt_frame_get_image(frame, &buf, &fmt, &w, &h, 0);
        if (!err && buf && w > 0 && h > 0) {
            QImage img(buf, w, h, w * 4, QImage::Format_RGBA8888);
            emit pc->frameRendered(img.copy());
        }
        mlt_frame_close(frame);
        pc->frameProcessingFlag().store(false, std::memory_order_release);
    }, Qt::QueuedConnection);
}

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

bool PlaybackController::open()
{
    if (!m_engine || !m_engine->isInitialized() || !m_producer)
        return false;

    auto *previewProfile = m_engine->previewProfile();

    // Helper lambda: create & configure a consumer (but do NOT start yet).
    auto tryConsumer = [&](const char *name) -> bool {
        m_consumer = std::make_unique<Mlt::Consumer>(*previewProfile, name);
        if (!m_consumer->is_valid()) {
            qWarning("PlaybackController: consumer '%s' is not valid", name);
            m_consumer.reset();
            return false;
        }
        // Core properties (following Shotcut's approach)
        m_consumer->set("scrub_audio", m_scrubAudio ? 1 : 0);
        m_consumer->set("real_time", 1);
        m_consumer->set("channels", 2);
        m_consumer->set("buffer", 25);   // frame buffer
        m_consumer->set("prefill", 8);
        m_consumer->set("drop_max", 8);
        m_consumer->connect(*m_producer);

        // Install frame-show listener BEFORE start (Shotcut's pattern)
        m_consumer->listen("consumer-frame-show", this,
                           reinterpret_cast<mlt_listener>(onFrameShow));

        if (m_consumer->start() == 0) {
            qWarning("PlaybackController: consumer '%s' started OK", name);
            return true;
        }
        qWarning("PlaybackController: consumer '%s' failed to start", name);
        m_consumer.reset();
        return false;
    };

    // ── Use sdl2_audio for audio playback (Shotcut's approach) ─────
    // Video frames are obtained via consumer-frame-show and rendered by Qt.
    if (tryConsumer("sdl2_audio") || tryConsumer("rtaudio")) {
        updateState(State::Paused);
        return true;
    }

    qWarning("PlaybackController: no usable consumer found");
    return false;
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
    if (!m_producer || !m_consumer) return;

    m_speed = 1.0;
    m_producer->set_speed(m_speed);
    // consumer->start() is idempotent — safe to call even if already running
    m_consumer->start();
    refreshConsumer();
    updateState(State::Playing);
    emit speedChanged(m_speed);
}

void PlaybackController::pause()
{
    if (!m_producer || !m_consumer) return;

    m_speed = 0.0;
    m_producer->set_speed(0.0);
    // Shotcut's pattern: seek to current+1, purge stale frames, restart
    m_producer->seek(m_consumer->position() + 1);
    m_consumer->purge();
    m_consumer->start();
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
    if (m_consumer) {
        m_consumer->purge();
        refreshConsumer();
    }
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
    if (m_consumer) {
        m_consumer->purge();
        m_consumer->start();
    }
    refreshConsumer();
    emit positionChanged(m_producer->position());
}

void PlaybackController::seek(int frame)
{
    if (!m_producer) return;

    m_producer->seek(qMax(0, frame));
    if (m_consumer) {
        if (m_consumer->is_stopped())
            m_consumer->start();
        else
            m_consumer->purge();
    }
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
    if (!m_producer || !m_consumer) return;

    if (m_speed < 0.0)
        m_speed = 1.0;
    else if (m_speed < 1.0)
        m_speed = 1.0;
    else
        m_speed = std::min(m_speed * 2.0, 32.0);

    m_producer->set_speed(m_speed);
    m_consumer->start();
    refreshConsumer();
    updateState(State::Playing);
    emit speedChanged(m_speed);
}

void PlaybackController::playReverse()
{
    if (!m_producer || !m_consumer) return;

    if (m_speed > 0.0)
        m_speed = -1.0;
    else if (m_speed > -1.0)
        m_speed = -1.0;
    else
        m_speed = std::max(m_speed * 2.0, -32.0);

    m_producer->set_speed(m_speed);
    m_consumer->start();
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
