// SPDX-License-Identifier: MIT
// Thrive Video Suite – Marker implementation

#include "marker.h"

namespace Thrive {

Marker::Marker(QObject *parent)
    : QObject(parent)
{
}

Marker::Marker(const QString &name, const TimeCode &position,
               const QString &comment, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_position(position)
    , m_comment(comment)
{
}

void Marker::setName(const QString &name)
{
    m_name = name;
    emit changed();
}

void Marker::setPosition(const TimeCode &pos)
{
    m_position = pos;
    emit changed();
}

void Marker::setComment(const QString &comment)
{
    m_comment = comment;
    emit changed();
}

QString Marker::accessibleSummary() const
{
    //: Screen reader summary for a timeline marker. %1=name, %2=timecode
    QString summary = tr("Marker: %1, at %2").arg(m_name, m_position.toString());
    if (!m_comment.isEmpty())
        summary += QStringLiteral(". ") + m_comment;
    return summary;
}

} // namespace Thrive
