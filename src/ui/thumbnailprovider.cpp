// SPDX-License-Identifier: MIT
// Thrive Video Suite – Thumbnail provider implementation

#include "thumbnailprovider.h"
#include "../engine/mltengine.h"

#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltFrame.h>

#include <QImage>
#include <QThreadPool>
#include <QRunnable>
#include <QDebug>

namespace Thrive {

// ── worker runnable ─────────────────────────────────────────────────

class ThumbnailTask : public QRunnable
{
public:
    ThumbnailTask(ThumbnailProvider *provider,
                  MltEngine *engine,
                  const QString &sourcePath,
                  int frame,
                  const QSize &size)
        : m_provider(provider)
        , m_engine(engine)
        , m_sourcePath(sourcePath)
        , m_frame(frame)
        , m_size(size)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        auto *profile = m_engine->compositionProfile();
        if (!profile) return;

        // Create a lightweight producer just for thumbnail extraction.
        // Try avformat: prefix directly since that's what works on Windows.
        const QByteArray avfPath = "avformat:" + m_sourcePath.toUtf8();
        Mlt::Producer producer(*profile, avfPath.constData());

        if (!producer.is_valid()) {
            // Try bare path as fallback
            Mlt::Producer bare(*profile, m_sourcePath.toUtf8().constData());
            if (!bare.is_valid()) {
                qDebug() << "ThumbnailProvider: cannot open" << m_sourcePath;
                return;
            }
            extractFrame(bare);
            return;
        }

        extractFrame(producer);
    }

private:
    void extractFrame(Mlt::Producer &producer)
    {
        producer.seek(m_frame);
        std::unique_ptr<Mlt::Frame> frame(producer.get_frame());
        if (!frame || !frame->is_valid())
            return;

        // Request image at thumbnail resolution
        mlt_image_format fmt = mlt_image_rgba;
        int w = m_size.width();
        int h = m_size.height();
        const uint8_t *data = frame->get_image(fmt, w, h);
        if (!data)
            return;

        QImage img(data, w, h, w * 4, QImage::Format_RGBA8888);
        QPixmap px = QPixmap::fromImage(img.copy()); // deep copy before frame dies

        // Store in cache
        ThumbnailProvider::CacheKey key{m_sourcePath, m_frame};
        {
            QMutexLocker lock(&m_provider->m_mutex);
            m_provider->m_cache.insert(key, px);
            m_provider->m_pending.remove(key);
        }

        // Signal on main thread
        QMetaObject::invokeMethod(m_provider, [this]() {
            emit m_provider->thumbnailReady(m_sourcePath, m_frame);
        }, Qt::QueuedConnection);
    }

    ThumbnailProvider *m_provider;
    MltEngine         *m_engine;
    QString            m_sourcePath;
    int                m_frame;
    QSize              m_size;
};

// ── ThumbnailProvider ───────────────────────────────────────────────

ThumbnailProvider::ThumbnailProvider(MltEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

ThumbnailProvider::~ThumbnailProvider() = default;

QPixmap ThumbnailProvider::thumbnail(const QString &sourcePath, int frame,
                                     const QSize &size)
{
    CacheKey key{sourcePath, frame};

    QMutexLocker lock(&m_mutex);

    // Check cache first
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it.value();

    // Already pending?
    if (m_pending.contains(key))
        return {};

    // Schedule generation
    m_pending.insert(key);
    lock.unlock();

    generateInThread(sourcePath, frame, size);
    return {};
}

void ThumbnailProvider::setThumbnailHeight(int h)
{
    if (h != m_thumbHeight) {
        m_thumbHeight = h;
        clearCache();
    }
}

void ThumbnailProvider::clearCache()
{
    QMutexLocker lock(&m_mutex);
    m_cache.clear();
    // Don't clear pending — let in-flight tasks finish
}

void ThumbnailProvider::generateInThread(const QString &sourcePath,
                                         int frame, const QSize &size)
{
    auto *task = new ThumbnailTask(this, m_engine, sourcePath, frame, size);
    QThreadPool::globalInstance()->start(task);
}

} // namespace Thrive
