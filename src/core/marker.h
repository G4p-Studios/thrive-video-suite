// SPDX-License-Identifier: MIT
// Thrive Video Suite – Marker definition

#pragma once

#include <QObject>
#include <QString>
#include "timecode.h"

namespace Thrive {

/// A named marker on the timeline (used for navigation landmarks).
class Marker : public QObject
{
    Q_OBJECT

public:
    explicit Marker(QObject *parent = nullptr);
    Marker(const QString &name, const TimeCode &position,
           const QString &comment = {}, QObject *parent = nullptr);

    [[nodiscard]] QString  name()     const { return m_name; }
    [[nodiscard]] TimeCode position() const { return m_position; }
    [[nodiscard]] QString  comment()  const { return m_comment; }

    void setName(const QString &name);
    void setPosition(const TimeCode &pos);
    void setComment(const QString &comment);

    /// "Marker: Intro, at 00:00:05:00"
    [[nodiscard]] QString accessibleSummary() const;

signals:
    void changed();

private:
    QString  m_name;
    TimeCode m_position;
    QString  m_comment;
};

} // namespace Thrive
