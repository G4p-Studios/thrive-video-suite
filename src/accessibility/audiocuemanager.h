// SPDX-License-Identifier: MIT
// Thrive Video Suite – Audio cue manager (boundary tones, navigation feedback)

#pragma once

#include <QObject>
#include <QHash>
#include <QSoundEffect>

namespace Thrive {

/// Pre-loads and plays short WAV audio cues for timeline navigation feedback.
/// These cues are played alongside (not instead of) screen reader speech.
class AudioCueManager : public QObject
{
    Q_OBJECT

public:
    enum class Cue {
        ClipStart,       ///< Navigated to the beginning of a clip
        ClipEnd,         ///< Navigated to the end of a clip
        Gap,             ///< Playhead is over an empty gap between clips
        TrackBoundary,   ///< Navigated past the first or last track
        Selection,       ///< Item selected / deselected
        Error            ///< Invalid operation attempted
    };
    Q_ENUM(Cue)

    explicit AudioCueManager(QObject *parent = nullptr);

    /// Load all WAV cues from the embedded Qt resources.
    void loadCues();

    /// Play a specific cue.
    void play(Cue cue);

    /// Master volume for all cues (0.0 – 1.0).
    [[nodiscard]] float volume() const { return m_volume; }
    void setVolume(float volume);

    /// Enable / disable audio cues globally.
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

private:
    QSoundEffect *cueEffect(Cue cue);

    QHash<Cue, QSoundEffect *> m_effects;
    float m_volume  = 0.5f;
    bool  m_enabled = true;
};

} // namespace Thrive
