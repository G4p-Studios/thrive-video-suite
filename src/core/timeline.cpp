// SPDX-License-Identifier: MIT
// Thrive Video Suite – Timeline implementation

#include "timeline.h"
#include "track.h"
#include "clip.h"
#include "marker.h"

namespace Thrive {

Timeline::Timeline(QObject *parent)
    : QObject(parent)
{
}

Track *Timeline::trackAt(int index) const
{
    if (index >= 0 && index < m_tracks.size())
        return m_tracks.at(index);
    return nullptr;
}

void Timeline::addTrack(Track *track)
{
    track->setParent(this);
    m_tracks.append(track);
    emit tracksChanged();
}

void Timeline::insertTrack(int index, Track *track)
{
    track->setParent(this);
    m_tracks.insert(index, track);
    emit tracksChanged();
}

void Timeline::removeTrack(int index)
{
    if (index >= 0 && index < m_tracks.size()) {
        m_tracks.takeAt(index);
        if (m_currentTrackIndex >= m_tracks.size())
            m_currentTrackIndex = qMax(0, m_tracks.size() - 1);
        emit tracksChanged();
    }
}

void Timeline::moveTrack(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < m_tracks.size()
        && toIndex >= 0 && toIndex < m_tracks.size()
        && fromIndex != toIndex)
    {
        m_tracks.move(fromIndex, toIndex);
        emit tracksChanged();
    }
}

void Timeline::addMarker(Marker *marker)
{
    marker->setParent(this);
    m_markers.append(marker);
    emit markersChanged();
}

void Timeline::insertMarker(int index, Marker *marker)
{
    marker->setParent(this);
    if (index >= 0 && index <= m_markers.size())
        m_markers.insert(index, marker);
    else
        m_markers.append(marker);
    emit markersChanged();
}

void Timeline::removeMarker(int index)
{
    if (index >= 0 && index < m_markers.size()) {
        m_markers.takeAt(index);
        emit markersChanged();
    }
}

void Timeline::setPlayheadPosition(const TimeCode &pos)
{
    m_playhead = pos;
    emit playheadChanged(m_playhead);
}

TimeCode Timeline::totalDuration() const
{
    TimeCode maxEnd;
    for (const auto *track : m_tracks) {
        for (const auto *clip : track->clips()) {
            const auto clipEnd = clip->timelinePosition() + clip->duration();
            if (maxEnd < clipEnd)
                maxEnd = clipEnd;
        }
    }
    return maxEnd;
}

void Timeline::setCurrentTrackIndex(int index)
{
    if (index >= 0 && index < m_tracks.size() && index != m_currentTrackIndex) {
        m_currentTrackIndex = index;
        m_currentClipIndex = qMin(m_currentClipIndex,
                                   qMax(0, m_tracks[m_currentTrackIndex]->clipCount() - 1));
        emit currentTrackChanged(m_currentTrackIndex);
    }
}

void Timeline::setCurrentClipIndex(int index)
{
    auto *track = trackAt(m_currentTrackIndex);
    if (!track) return;

    if (index >= 0 && index < track->clipCount() && index != m_currentClipIndex) {
        m_currentClipIndex = index;
        emit currentClipChanged(m_currentTrackIndex, m_currentClipIndex);
    }
}

void Timeline::navigateNextClip()
{
    auto *track = trackAt(m_currentTrackIndex);
    if (!track) return;

    if (m_currentClipIndex < track->clipCount() - 1) {
        m_currentClipIndex++;
        emit currentClipChanged(m_currentTrackIndex, m_currentClipIndex);
    }
    // At end: accessibility layer will announce "End of track"
}

void Timeline::navigatePreviousClip()
{
    if (m_currentClipIndex > 0) {
        m_currentClipIndex--;
        emit currentClipChanged(m_currentTrackIndex, m_currentClipIndex);
    }
    // At beginning: accessibility layer will announce "Beginning of track"
}

void Timeline::navigateNextTrack()
{
    if (m_currentTrackIndex < m_tracks.size() - 1) {
        m_currentTrackIndex++;
        auto *track = trackAt(m_currentTrackIndex);
        m_currentClipIndex = qMin(m_currentClipIndex,
                                   qMax(0, track ? track->clipCount() - 1 : 0));
        emit currentTrackChanged(m_currentTrackIndex);
    }
    // At bottom: accessibility layer will announce "Last track"
}

void Timeline::navigatePreviousTrack()
{
    if (m_currentTrackIndex > 0) {
        m_currentTrackIndex--;
        auto *track = trackAt(m_currentTrackIndex);
        m_currentClipIndex = qMin(m_currentClipIndex,
                                   qMax(0, track ? track->clipCount() - 1 : 0));
        emit currentTrackChanged(m_currentTrackIndex);
    }
    // At top: accessibility layer will announce "First track"
}

void Timeline::navigateNextMarker()
{
    // Find next marker after playhead
    Marker *nextMarker = nullptr;
    for (auto *m : m_markers) {
        if (m_playhead < m->position()) {
            if (!nextMarker || m->position() < nextMarker->position())
                nextMarker = m;
        }
    }
    if (nextMarker)
        setPlayheadPosition(nextMarker->position());
}

void Timeline::navigatePreviousMarker()
{
    Marker *prevMarker = nullptr;
    for (auto *m : m_markers) {
        if (m->position() < m_playhead) {
            if (!prevMarker || prevMarker->position() < m->position())
                prevMarker = m;
        }
    }
    if (prevMarker)
        setPlayheadPosition(prevMarker->position());
}

} // namespace Thrive
