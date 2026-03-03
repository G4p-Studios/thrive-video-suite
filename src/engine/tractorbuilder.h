// SPDX-License-Identifier: MIT
// Thrive Video Suite – TractorBuilder: converts Timeline model → MLT Tractor

#pragma once

#include <QObject>
#include <QByteArray>
#include <memory>
#include <vector>

namespace Mlt {
class Filter;
class Playlist;
class Producer;
class Profile;
class Tractor;
class Transition;
} // namespace Mlt

namespace Thrive {

class MltEngine;
class Timeline;
class Track;
class Clip;
class Effect;

/// Builds and maintains an Mlt::Tractor that mirrors the Timeline data model.
///
/// The mapping is:
///   Timeline  → Mlt::Tractor  (top-level container)
///   Track     → Mlt::Playlist (one per track, added to the Tractor's multitrack)
///   Clip      → Mlt::Producer (one per clip, appended to its track's Playlist)
///   Effect    → Mlt::Filter   (attached to the Producer or Playlist)
///
/// Call rebuild() whenever the Timeline changes. The resulting Tractor
/// is owned by TractorBuilder and remains valid until the next rebuild()
/// or destruction.
class TractorBuilder : public QObject
{
    Q_OBJECT

public:
    explicit TractorBuilder(MltEngine *engine, QObject *parent = nullptr);
    ~TractorBuilder() override;

    /// (Re)build the Mlt::Tractor from the current state of \a timeline.
    /// Returns true on success.  After a successful build the tractor()
    /// pointer is valid and can be passed to PlaybackController / RenderEngine.
    bool rebuild(Timeline *timeline);

    /// The most recently built tractor.  May be nullptr before the first build.
    [[nodiscard]] Mlt::Tractor *tractor() const { return m_tractor.get(); }

    /// Serialize the current Mlt::Tractor to MLT XML.  Returns an empty
    /// QByteArray if no tractor has been built yet.
    [[nodiscard]] QByteArray serializeToXml() const;

signals:
    /// Emitted after a successful rebuild().
    void tractorReady(Mlt::Tractor *tractor);

private:
    /// Build an Mlt::Playlist for a single Track.
    std::unique_ptr<Mlt::Playlist> buildPlaylist(Track *track);

    /// Create an Mlt::Producer for a single Clip.
    std::unique_ptr<Mlt::Producer> buildProducer(Clip *clip);

    /// Attach an Effect as an Mlt::Filter on a producer.
    void attachEffect(Mlt::Producer &producer, Effect *effect);

    MltEngine *m_engine = nullptr;

    std::unique_ptr<Mlt::Tractor> m_tractor;

    // Keep producers alive for the lifetime of the tractor
    std::vector<std::unique_ptr<Mlt::Producer>>    m_producers;
    std::vector<std::unique_ptr<Mlt::Playlist>>    m_playlists;
    std::vector<std::unique_ptr<Mlt::Filter>>      m_filters;
    std::vector<std::unique_ptr<Mlt::Transition>>  m_transitions;
};

} // namespace Thrive
