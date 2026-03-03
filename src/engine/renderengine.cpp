// SPDX-License-Identifier: MIT
// Thrive Video Suite – Render engine implementation

#include "renderengine.h"
#include "mltengine.h"

#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

#include <QTimer>

namespace Thrive {

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

    m_producer = producer;
    m_totalFrames = producer->get_length();

    // Create consumer at full composition resolution
    auto *profile = m_engine->compositionProfile();
    m_renderConsumer = std::make_unique<Mlt::Consumer>(*profile, "avformat");

    if (!m_renderConsumer->is_valid())
        return false;

    m_renderConsumer->set("target", outputPath.toUtf8().constData());
    m_renderConsumer->set("real_time", -1); // render as fast as possible

    if (!format.isEmpty())
        m_renderConsumer->set("f", format.toUtf8().constData());
    if (!vcodec.isEmpty())
        m_renderConsumer->set("vcodec", vcodec.toUtf8().constData());
    if (!acodec.isEmpty())
        m_renderConsumer->set("acodec", acodec.toUtf8().constData());
    if (vBitrateKbps > 0)
        m_renderConsumer->set("vb",
            QStringLiteral("%1k").arg(vBitrateKbps).toUtf8().constData());
    if (aBitrateKbps > 0)
        m_renderConsumer->set("ab",
            QStringLiteral("%1k").arg(aBitrateKbps).toUtf8().constData());

    // Sensible defaults
    m_renderConsumer->set("rescale", "bicubic");
    m_renderConsumer->set("progressive", 1);

    m_renderConsumer->connect(*m_producer);
    m_producer->set_speed(0.0);
    m_producer->seek(0);

    m_renderConsumer->start();
    m_rendering = true;

    emit renderStarted();

    // Poll progress
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, timer]() {
        if (!m_rendering) {
            timer->stop();
            timer->deleteLater();
            return;
        }

        pollProgress();

        if (m_renderConsumer->is_stopped()) {
            m_rendering = false;
            timer->stop();
            timer->deleteLater();
            emit renderProgress(100);
            emit renderFinished(true);
        }
    });
    timer->start(250);

    return true;
}

void RenderEngine::cancelRender()
{
    if (m_renderConsumer && m_rendering) {
        m_renderConsumer->stop();
        m_rendering = false;
        emit renderFinished(false);
    }
    m_renderConsumer.reset();
}

int RenderEngine::progressPercent() const
{
    if (!m_rendering || m_totalFrames <= 0 || !m_producer)
        return 0;

    return qBound(0,
        static_cast<int>(100.0 * m_producer->position() / m_totalFrames),
        100);
}

void RenderEngine::pollProgress()
{
    emit renderProgress(progressPercent());
}

} // namespace Thrive
