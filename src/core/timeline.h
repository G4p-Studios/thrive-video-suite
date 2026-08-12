// SPDX-License-Identifier: GPL-3.0-or-later
// Thrive Video Suite – Timeline definition

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include "timecode.h"

namespace Thrive {

class Track;
class Marker;

/// The central timeline model: an ordered collection of tracks and markers.
/// Modelled as a table (tracks = rows, clips within each track = cells).
class Timeline : public QObject
{
    Q_OBJECT

public:
    explicit Timeline(QObject *parent = nullptr);

    // Tracks
    [[nodiscard]] const QVector<Track *> &tracks() const { return m_tracks; }
    [[nodiscard]] int trackCount() const { return static_cast<int>(m_tracks.size()); }
    [[nodiscard]] Track *trackAt(int index) const;

    void addTrack(Track *track);
    void insertTrack(int index, Track *track);
    void removeTrack(int index);
    void moveTrack(int fromIndex, int toIndex);

    // Markers
    [[nodiscard]] const QVector<Marker *> &markers() const { return m_markers; }
    void addMarker(Marker *marker);
    void insertMarker(int index, Marker *marker);
    void removeMarker(int index);

    // Playhead position
    [[nodiscard]] TimeCode playheadPosition() const { return m_playhead; }
    void setPlayheadPosition(const TimeCode &pos);

    // Optional project-range selection used by keyboard editing commands.
    [[nodiscard]] bool hasSelectionStart() const { return m_selectionStartFrame >= 0; }
    [[nodiscard]] bool hasSelection() const {
        return m_selectionStartFrame >= 0
            && m_selectionEndFrame > m_selectionStartFrame;
    }
    [[nodiscard]] TimeCode selectionStart() const;
    [[nodiscard]] TimeCode selectionEnd() const;
    void setSelectionStart(const TimeCode &pos);
    bool setSelectionEnd(const TimeCode &pos);
    void clearSelection();

    // Total duration (end of the last clip across all tracks)
    [[nodiscard]] TimeCode totalDuration() const;

    // Navigation helpers for keyboard navigation
    [[nodiscard]] int currentTrackIndex() const { return m_currentTrackIndex; }
    [[nodiscard]] int currentClipIndex()  const { return m_currentClipIndex; }

    void setCurrentTrackIndex(int index);
    void setCurrentClipIndex(int index);

    /// Navigate to the next/previous clip on the current track
    void navigateNextClip();
    void navigatePreviousClip();

    /// Navigate to the next/previous track
    void navigateNextTrack();
    void navigatePreviousTrack();

    /// Navigate to the next/previous marker
    void navigateNextMarker();
    void navigatePreviousMarker();

signals:
    void tracksChanged();
    void markersChanged();
    void playheadChanged(const TimeCode &position);
    void selectionChanged();
    void currentTrackChanged(int trackIndex);
    void currentClipChanged(int trackIndex, int clipIndex);

private:
    QVector<Track *>  m_tracks;
    QVector<Marker *> m_markers;
    TimeCode m_playhead;
    int64_t m_selectionStartFrame = -1;
    int64_t m_selectionEndFrame = -1;
    int m_currentTrackIndex = 0;
    int m_currentClipIndex  = 0;
};

} // namespace Thrive
