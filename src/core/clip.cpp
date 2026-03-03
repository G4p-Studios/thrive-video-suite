// SPDX-License-Identifier: MIT
// Thrive Video Suite – Clip implementation

#include "clip.h"
#include "effect.h"
#include "transition.h"

namespace Thrive {

Clip::Clip(QObject *parent)
    : QObject(parent)
{
}

Clip::Clip(const QString &name,
           const QString &sourcePath,
           const TimeCode &inPoint,
           const TimeCode &outPoint,
           QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_sourcePath(sourcePath)
    , m_inPoint(inPoint)
    , m_outPoint(outPoint)
{
}

void Clip::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

void Clip::setDescription(const QString &desc)
{
    m_description = desc;
}

void Clip::setSourcePath(const QString &path)
{
    m_sourcePath = path;
}

void Clip::setInPoint(const TimeCode &tc)
{
    m_inPoint = tc;
    emit timingChanged();
}

void Clip::setOutPoint(const TimeCode &tc)
{
    m_outPoint = tc;
    emit timingChanged();
}

TimeCode Clip::duration() const
{
    return m_outPoint - m_inPoint;
}

void Clip::setTimelinePosition(const TimeCode &pos)
{
    m_timelinePos = pos;
    emit timingChanged();
}

void Clip::addEffect(Effect *effect)
{
    effect->setParent(this);
    m_effects.append(effect);
    emit effectsChanged();
}

void Clip::removeEffect(int index)
{
    if (index >= 0 && index < m_effects.size()) {
        auto *fx = m_effects.takeAt(index);
        fx->setParent(nullptr);   // transfer ownership to caller / undo cmd
        emit effectsChanged();
    }
}

void Clip::insertEffect(int index, Effect *effect)
{
    effect->setParent(this);
    if (index >= 0 && index <= m_effects.size())
        m_effects.insert(index, effect);
    else
        m_effects.append(effect);
    emit effectsChanged();
}

void Clip::moveEffect(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < m_effects.size()
        && toIndex >= 0 && toIndex < m_effects.size()
        && fromIndex != toIndex)
    {
        m_effects.move(fromIndex, toIndex);
        emit effectsChanged();
    }
}

void Clip::setInTransition(Transition *t)
{
    m_inTransition = t;
    if (t) t->setParent(this);
}

void Clip::setOutTransition(Transition *t)
{
    m_outTransition = t;
    if (t) t->setParent(this);
}

QString Clip::accessibleSummary() const
{
    const auto dur = duration();

    //: Screen reader summary for a clip. %1=name, %2=in timecode, %3=out timecode, %4=duration, %5=timeline position
    QString summary = tr("Clip: %1, %2 to %3, duration %4, at %5")
        .arg(m_name,
             m_inPoint.toString(),
             m_outPoint.toString(),
             dur.toSpokenString(),
             m_timelinePos.toSpokenString());

    if (!m_effects.isEmpty()) {
        //: Number of effects applied to a clip
        summary += QStringLiteral(". ") + tr("%n effect(s)", nullptr, m_effects.size());
    }

    if (m_inTransition) {
        //: Transition at the start of a clip
        summary += QStringLiteral(". ") + tr("In transition: %1").arg(m_inTransition->displayName());
    }

    if (m_outTransition) {
        //: Transition at the end of a clip
        summary += QStringLiteral(". ") + tr("Out transition: %1").arg(m_outTransition->displayName());
    }

    return summary;
}

Clip *Clip::deepCopy(const Clip *source, QObject *parent)
{
    if (!source)
        return nullptr;

    auto *copy = new Clip(source->name(), source->sourcePath(),
                          source->inPoint(), source->outPoint(), parent);
    copy->setTimelinePosition(source->timelinePosition());
    copy->setDescription(source->description());

    // Deep-copy effects
    for (auto *fx : source->effects()) {
        auto *fxCopy = new Effect(fx->serviceId(), fx->displayName(),
                                  fx->description(), copy);
        fxCopy->setEnabled(fx->isEnabled());
        for (const auto &p : fx->parameters())
            fxCopy->addParameter(p);
        copy->addEffect(fxCopy);
    }

    // Deep-copy transitions
    if (auto *t = source->outTransition())
        copy->setOutTransition(
            new Transition(t->serviceId(), t->displayName(),
                           t->description(), t->duration(), copy));
    if (auto *t = source->inTransition())
        copy->setInTransition(
            new Transition(t->serviceId(), t->displayName(),
                           t->description(), t->duration(), copy));

    return copy;
}

} // namespace Thrive
