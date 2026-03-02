// SPDX-License-Identifier: MIT
// Thrive Video Suite – Transition definition

#pragma once

#include <QObject>
#include <QString>
#include "timecode.h"

namespace Thrive {

/// A transition between two adjacent clips (dissolve, wipe, etc.).
class Transition : public QObject
{
    Q_OBJECT

public:
    explicit Transition(QObject *parent = nullptr);
    Transition(const QString &serviceId,
               const QString &displayName,
               const QString &description,
               const TimeCode &duration,
               QObject *parent = nullptr);

    [[nodiscard]] QString  serviceId()   const { return m_serviceId; }
    [[nodiscard]] QString  displayName() const { return m_displayName; }
    [[nodiscard]] QString  description() const { return m_description; }
    [[nodiscard]] TimeCode duration()    const { return m_duration; }

    void setDisplayName(const QString &name);
    void setDescription(const QString &desc);
    void setDuration(const TimeCode &duration);

    /// Screen reader: "Cross Dissolve, 1 second. Gradually blends the end of one clip…"
    [[nodiscard]] QString accessibleSummary() const;

signals:
    void durationChanged(const TimeCode &duration);

private:
    QString  m_serviceId;
    QString  m_displayName;
    QString  m_description;
    TimeCode m_duration;
};

} // namespace Thrive
