// SPDX-License-Identifier: MIT
// Thrive Video Suite – Clip definition

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include "timecode.h"

namespace Thrive {

class Effect;
class Transition;

/// A single clip on a track (a segment of source media with in/out points).
class Clip : public QObject
{
    Q_OBJECT

public:
    explicit Clip(QObject *parent = nullptr);
    Clip(const QString &name,
         const QString &sourcePath,
         const TimeCode &inPoint,
         const TimeCode &outPoint,
         QObject *parent = nullptr);

    // Identity & metadata
    [[nodiscard]] QString name()        const { return m_name; }
    [[nodiscard]] QString description() const { return m_description; }
    [[nodiscard]] QString sourcePath()  const { return m_sourcePath; }

    void setName(const QString &name);
    void setDescription(const QString &desc);
    void setSourcePath(const QString &path);

    // Timing
    [[nodiscard]] TimeCode inPoint()  const { return m_inPoint; }
    [[nodiscard]] TimeCode outPoint() const { return m_outPoint; }
    [[nodiscard]] TimeCode duration() const;

    void setInPoint(const TimeCode &tc);
    void setOutPoint(const TimeCode &tc);

    // Position on the timeline (absolute frame where this clip starts)
    [[nodiscard]] TimeCode timelinePosition() const { return m_timelinePos; }
    void setTimelinePosition(const TimeCode &pos);

    // Effects
    [[nodiscard]] const QVector<Effect *> &effects() const { return m_effects; }
    void addEffect(Effect *effect);
    void removeEffect(int index);
    void moveEffect(int fromIndex, int toIndex);

    // Transitions
    [[nodiscard]] Transition *inTransition()  const { return m_inTransition; }
    [[nodiscard]] Transition *outTransition() const { return m_outTransition; }
    void setInTransition(Transition *t);
    void setOutTransition(Transition *t);

    /// Screen reader: "Clip: Intro.mp4, 00:00:05 to 00:00:12, duration 7 seconds. 2 effects."
    [[nodiscard]] QString accessibleSummary() const;

signals:
    void nameChanged(const QString &name);
    void timingChanged();
    void effectsChanged();

private:
    QString  m_name;
    QString  m_description;
    QString  m_sourcePath;
    TimeCode m_inPoint;
    TimeCode m_outPoint;
    TimeCode m_timelinePos;

    QVector<Effect *> m_effects;
    Transition *m_inTransition  = nullptr;
    Transition *m_outTransition = nullptr;
};

} // namespace Thrive
