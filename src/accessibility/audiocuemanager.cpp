// SPDX-License-Identifier: MIT
// Thrive Video Suite – Audio cue manager implementation

#include "audiocuemanager.h"

#include <QUrl>

namespace Thrive {

AudioCueManager::AudioCueManager(QObject *parent)
    : QObject(parent)
{
}

void AudioCueManager::loadCues()
{
    static const QHash<Cue, QString> paths = {
        { Cue::ClipStart,      QStringLiteral("qrc:/sounds/clip_start.wav")      },
        { Cue::ClipEnd,        QStringLiteral("qrc:/sounds/clip_end.wav")        },
        { Cue::Gap,            QStringLiteral("qrc:/sounds/gap.wav")             },
        { Cue::TrackBoundary,  QStringLiteral("qrc:/sounds/track_boundary.wav")  },
        { Cue::Selection,      QStringLiteral("qrc:/sounds/selection.wav")       },
        { Cue::Error,          QStringLiteral("qrc:/sounds/error.wav")           },
        { Cue::ClipAdded,      QStringLiteral("qrc:/sounds/selection.wav")       },
        { Cue::ClipRemoved,    QStringLiteral("qrc:/sounds/clip_end.wav")        },
    };

    for (auto it = paths.cbegin(); it != paths.cend(); ++it) {
        auto *effect = new QSoundEffect(this);
        effect->setSource(QUrl(it.value()));
        effect->setVolume(m_volume);
        effect->setLoopCount(1);
        m_effects.insert(it.key(), effect);
    }
}

void AudioCueManager::play(Cue cue)
{
    if (!m_enabled) {
        return;
    }
    if (auto *fx = cueEffect(cue)) {
        fx->play();
    }
}

void AudioCueManager::setVolume(float volume)
{
    m_volume = qBound(0.0f, volume, 1.0f);
    for (auto *fx : m_effects) {
        fx->setVolume(static_cast<double>(m_volume));
    }
}

void AudioCueManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!m_enabled) {
        for (auto *fx : m_effects) {
            fx->stop();
        }
    }
}

QSoundEffect *AudioCueManager::cueEffect(Cue cue)
{
    auto it = m_effects.find(cue);
    if (it != m_effects.end()) {
        return it.value();
    }
    return nullptr;
}

} // namespace Thrive
