// SPDX-License-Identifier: MIT
// Thrive Video Suite – Accessible timeline bridge implementation

#include "accessibletimeline.h"
#include "announcer.h"
#include "audiocuemanager.h"
#include "../core/timeline.h"
#include "../core/track.h"
#include "../core/clip.h"
#include "../core/marker.h"
#include "../core/timecode.h"

namespace Thrive {

AccessibleTimeline::AccessibleTimeline(Timeline *timeline,
                                       Announcer *announcer,
                                       AudioCueManager *cues,
                                       QObject *parent)
    : QObject(parent)
    , m_timeline(timeline)
    , m_announcer(announcer)
    , m_cues(cues)
{
    Q_ASSERT(m_timeline);
    Q_ASSERT(m_announcer);

    connect(m_timeline, &Timeline::currentTrackChanged,
            this, &AccessibleTimeline::onCurrentTrackChanged);
    connect(m_timeline, &Timeline::currentClipChanged,
            this, [this](int /*trackIndex*/, int clipIndex) {
                onCurrentClipChanged(clipIndex);
            });
    connect(m_timeline, &Timeline::playheadChanged,
            this, [this](const TimeCode &) { onPlayheadChanged(); });
}

void AccessibleTimeline::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void AccessibleTimeline::setTimeline(Timeline *timeline)
{
    if (m_timeline)
        m_timeline->disconnect(this);

    m_timeline = timeline;
    m_lastAnnouncedMarker = -1;

    connect(m_timeline, &Timeline::currentTrackChanged,
            this, &AccessibleTimeline::onCurrentTrackChanged);
    connect(m_timeline, &Timeline::currentClipChanged,
            this, [this](int /*trackIndex*/, int clipIndex) {
                onCurrentClipChanged(clipIndex);
            });
    connect(m_timeline, &Timeline::playheadChanged,
            this, [this](const TimeCode &) { onPlayheadChanged(); });
}

// ── slot: track focus changed ───────────────────────────────────────

void AccessibleTimeline::onCurrentTrackChanged(int index)
{
    if (!m_enabled) return;

    const auto &tracks = m_timeline->tracks();

    // Boundary detection
    if (index < 0 || index >= tracks.size()) {
        m_announcer->announce(tr("No more tracks."), Announcer::Priority::Normal);
        if (m_cues) m_cues->play(AudioCueManager::Cue::TrackBoundary);
        return;
    }

    const Track *trk = tracks.at(index);
    const QString typeStr = (trk->type() == Track::Type::Video)
                                ? tr("Video") : tr("Audio");

    //: e.g. "Track 1, Video, 3 clips"
    const QString msg = tr("Track %1, %2, %n clip(s).", nullptr, trk->clips().size())
                            .arg(index + 1)
                            .arg(typeStr);

    m_announcer->announce(msg, Announcer::Priority::Normal);
}

// ── slot: clip focus changed ────────────────────────────────────────

void AccessibleTimeline::onCurrentClipChanged(int index)
{
    if (!m_enabled) return;

    const auto *trk = m_timeline->trackAt(m_timeline->currentTrackIndex());
    if (!trk) return;

    const auto &clips = trk->clips();
    if (index < 0 || index >= clips.size()) {
        // User navigated past all clips
        m_announcer->announce(tr("No more clips on this track."),
                              Announcer::Priority::Normal);
        if (m_cues) m_cues->play(AudioCueManager::Cue::Gap);
        return;
    }

    announceCurrentClip();
}

// ── slot: playhead moved (scrub / seek) ─────────────────────────────

void AccessibleTimeline::onPlayheadChanged()
{
    if (!m_enabled) return;
    onMarkerReached();
}

// ── marker proximity ────────────────────────────────────────────────

void AccessibleTimeline::onMarkerReached()
{
    if (!m_enabled) return;

    const auto &markers = m_timeline->markers();
    const TimeCode &pos = m_timeline->playheadPosition();

    for (int i = 0; i < markers.size(); ++i) {
        if (markers.at(i)->position() == pos && i != m_lastAnnouncedMarker) {
            m_lastAnnouncedMarker = i;
            const QString msg = tr("Marker: %1").arg(markers.at(i)->name());
            m_announcer->announce(msg, Announcer::Priority::Normal);
            return;
        }
    }
}

// ── helpers ─────────────────────────────────────────────────────────

void AccessibleTimeline::announceCurrentClip()
{
    const auto *trk = m_timeline->trackAt(m_timeline->currentTrackIndex());
    if (!trk) return;

    int idx = m_timeline->currentClipIndex();
    const auto &clips = trk->clips();
    if (idx < 0 || idx >= clips.size()) return;

    const Clip *clip = clips.at(idx);

    // Determine if we're at start or end of a clip for cue purposes
    const TimeCode &playhead = m_timeline->playheadPosition();
    if (m_cues) {
        if (playhead == clip->timelinePosition()) {
            m_cues->play(AudioCueManager::Cue::ClipStart);
        } else if (playhead == clip->timelinePosition() + clip->outPoint() - clip->inPoint()) {
            m_cues->play(AudioCueManager::Cue::ClipEnd);
        }
    }

    m_announcer->announce(clip->accessibleSummary(), Announcer::Priority::Normal);
}

} // namespace Thrive
