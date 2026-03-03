// SPDX-License-Identifier: MIT
// Thrive Video Suite – Render engine implementation

#include "renderengine.h"
#include "mltengine.h"

#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>

#include <QTimer>
#include <QTemporaryFile>
#include <QFile>

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

    m_outputPath = outputPath;

    // Clone the producer via XML so that edits to the live timeline
    // during export don't corrupt the render pipeline.
    auto *profile = m_engine->compositionProfile();

    // Serialize via the xml consumer into a temp file, then reload
    {
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(true);
        if (tmpFile.open()) {
            const QByteArray tmpPath = tmpFile.fileName().toUtf8();
            tmpFile.close();

            Mlt::Consumer xmlCons(*profile, "xml", tmpPath.constData());
            if (xmlCons.is_valid()) {
                xmlCons.set("no_meta", 1);
                xmlCons.connect(*producer);
                xmlCons.run();

                m_clonedProducer = std::make_unique<Mlt::Producer>(
                    *profile, tmpPath.constData());
            }
        }
    }

    if (m_clonedProducer && m_clonedProducer->is_valid()) {
        m_producer = m_clonedProducer.get();
    } else {
        // Fallback: use the original pointer directly
        m_clonedProducer.reset();
        m_producer = producer;
    }

    m_totalFrames = m_producer->get_length();

    // Create consumer at full composition resolution
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
        // Remove partial output file
        if (!m_outputPath.isEmpty())
            QFile::remove(m_outputPath);
        emit renderFinished(false);
    }
    m_renderConsumer.reset();
    m_clonedProducer.reset();
    m_outputPath.clear();
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
