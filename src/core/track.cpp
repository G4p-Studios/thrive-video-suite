// SPDX-License-Identifier: MIT
// Thrive Video Suite – Track implementation

#include "track.h"
#include "clip.h"
#include "effect.h"

namespace Thrive {

Track::Track(QObject *parent)
    : QObject(parent)
{
}

Track::Track(Type type, QObject *parent)
    : QObject(parent)
    , m_type(type)
{
}

Track::Track(const QString &name, Type type, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_type(type)
{
}

void Track::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

void Track::setMuted(bool muted)
{
    if (m_muted != muted) {
        m_muted = muted;
        emit mutedChanged(m_muted);
    }
}

void Track::setLocked(bool locked)
{
    if (m_locked != locked) {
        m_locked = locked;
        emit lockedChanged(m_locked);
    }
}

Clip *Track::clipAt(int index) const
{
    if (index >= 0 && index < m_clips.size())
        return m_clips.at(index);
    return nullptr;
}

void Track::addClip(Clip *clip)
{
    clip->setParent(this);
    m_clips.append(clip);
    emit clipsChanged();
}

void Track::insertClip(int index, Clip *clip)
{
    clip->setParent(this);
    m_clips.insert(index, clip);
    emit clipsChanged();
}

void Track::removeClip(int index)
{
    if (index >= 0 && index < m_clips.size()) {
        auto *clip = m_clips.takeAt(index);
        clip->setParent(nullptr);   // transfer ownership to caller / undo cmd
        emit clipsChanged();
    }
}

void Track::moveClip(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < m_clips.size()
        && toIndex >= 0 && toIndex < m_clips.size()
        && fromIndex != toIndex)
    {
        m_clips.move(fromIndex, toIndex);
        emit clipsChanged();
    }
}

void Track::addTrackEffect(Effect *effect)
{
    effect->setParent(this);
    m_trackEffects.append(effect);
    emit trackEffectsChanged();
}

void Track::removeTrackEffect(int index)
{
    if (index >= 0 && index < m_trackEffects.size()) {
        auto *fx = m_trackEffects.takeAt(index);
        fx->setParent(nullptr);
        emit trackEffectsChanged();
    }
}

QString Track::typeString() const
{
    switch (m_type) {
    case Type::Video: return tr("Video");
    case Type::Audio: return tr("Audio");
    }
    return {};
}

QString Track::accessibleSummary() const
{
    //: Screen reader summary for a track. %1=name, %2=type, %3=clip count
    QString summary = tr("%1, %2. %n clip(s)", nullptr, m_clips.size())
        .arg(m_name, typeString());

    if (m_muted)
        summary += QStringLiteral(". ") + tr("Muted");
    if (m_locked)
        summary += QStringLiteral(". ") + tr("Locked");
    if (!m_trackEffects.isEmpty())
        summary += QStringLiteral(". ") + tr("%n track effect(s)", nullptr, m_trackEffects.size());

    return summary;
}

} // namespace Thrive
