// SPDX-License-Identifier: MIT
// Thrive Video Suite – Accessible timeline bridge (model → screen reader)

#pragma once

#include <QObject>

namespace Thrive {

class Timeline;
class Announcer;
class AudioCueManager;

/// Bridges the Timeline model to the screen reader: whenever the user
/// navigates tracks / clips / markers, this class formats a spoken
/// description and queues it through the Announcer, and optionally
/// plays an audio cue via AudioCueManager.
class AccessibleTimeline : public QObject
{
    Q_OBJECT

public:
    /// \a timeline   – the data model to observe
    /// \a announcer  – speech queue (Normal priority used for navigation)
    /// \a cues       – audio cue player (may be nullptr to skip cues)
    explicit AccessibleTimeline(Timeline *timeline,
                                Announcer *announcer,
                                AudioCueManager *cues = nullptr,
                                QObject *parent = nullptr);

    /// If false, the bridge does nothing (useful while loading a project).
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

private slots:
    void onCurrentTrackChanged(int index);
    void onCurrentClipChanged(int index);
    void onPlayheadChanged();
    void onMarkerReached();

private:
    void announceCurrentClip();

    Timeline         *m_timeline  = nullptr;
    Announcer        *m_announcer = nullptr;
    AudioCueManager  *m_cues      = nullptr;
    bool              m_enabled   = true;
    int               m_lastAnnouncedMarker = -1;
};

} // namespace Thrive
