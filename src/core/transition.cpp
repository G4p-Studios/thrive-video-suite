// SPDX-License-Identifier: MIT
// Thrive Video Suite – Transition implementation

#include "transition.h"

namespace Thrive {

Transition::Transition(QObject *parent)
    : QObject(parent)
{
}

Transition::Transition(const QString &serviceId,
                       const QString &displayName,
                       const QString &description,
                       const TimeCode &duration,
                       QObject *parent)
    : QObject(parent)
    , m_serviceId(serviceId)
    , m_displayName(displayName)
    , m_description(description)
    , m_duration(duration)
{
}

void Transition::setDisplayName(const QString &name)
{
    m_displayName = name;
}

void Transition::setDescription(const QString &desc)
{
    m_description = desc;
}

void Transition::setDuration(const TimeCode &duration)
{
    if (!(m_duration == duration)) {
        m_duration = duration;
        emit durationChanged(m_duration);
    }
}

QString Transition::accessibleSummary() const
{
    //: Screen reader summary for a transition. %1=name, %2=duration, %3=description
    return tr("%1, %2. %3")
        .arg(m_displayName, m_duration.toSpokenString(), m_description);
}

} // namespace Thrive
