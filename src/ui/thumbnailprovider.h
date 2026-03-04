// SPDX-License-Identifier: MIT
// Thrive Video Suite – Thumbnail provider for timeline clip strips

#pragma once

#include <QObject>
#include <QPixmap>
#include <QHash>
#include <QMutex>
#include <QSize>
#include <QSet>
#include <QString>

namespace Mlt {
class Profile;
}

namespace Thrive {

class MltEngine;

/// Asynchronously extracts video frames via MLT producers and caches
/// them as QPixmaps.  Each thumbnail is identified by (sourcePath, frame).
///
/// Typical usage from a paint routine:
///     QPixmap px = provider->thumbnail(path, frameNo, thumbSize);
///     if (px.isNull())   // not cached yet — will arrive via thumbnailReady()
///         drawPlaceholder();
class ThumbnailProvider : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailProvider(MltEngine *engine, QObject *parent = nullptr);
    ~ThumbnailProvider() override;

    /// Returns a cached thumbnail or a null pixmap if it hasn't been
    /// generated yet.  When the thumbnail becomes available the
    /// thumbnailReady() signal is emitted so the caller can repaint.
    QPixmap thumbnail(const QString &sourcePath, int frame,
                      const QSize &size);

    /// Height in pixels for generated thumbnails (default 60).
    void setThumbnailHeight(int h);
    [[nodiscard]] int thumbnailHeight() const { return m_thumbHeight; }

    /// Discard all cached thumbnails.
    void clearCache();

signals:
    /// Emitted (on the main thread) when a new thumbnail is ready.
    void thumbnailReady(const QString &sourcePath, int frame);

private:
    friend class ThumbnailTask;

    struct CacheKey {
        QString path;
        int     frame;
        bool operator==(const CacheKey &o) const {
            return path == o.path && frame == o.frame;
        }
    };
    friend size_t qHash(const CacheKey &k, size_t seed) {
        return qHash(k.path, seed) ^ qHash(k.frame, seed);
    }

    void generateInThread(const QString &sourcePath, int frame,
                          const QSize &size);

    MltEngine *m_engine = nullptr;
    int        m_thumbHeight = 60;

    QMutex                    m_mutex;
    QHash<CacheKey, QPixmap>  m_cache;
    QSet<CacheKey>            m_pending;  // currently being generated
};

} // namespace Thrive
