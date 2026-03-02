// SPDX-License-Identifier: MIT
// Thrive Video Suite – Track definition

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace Thrive {

class Clip;
class Effect;

/// A single track (video or audio) in the timeline.
class Track : public QObject
{
    Q_OBJECT

public:
    enum class Type { Video, Audio };
    Q_ENUM(Type)

    explicit Track(QObject *parent = nullptr);
    Track(Type type, QObject *parent = nullptr);
    Track(const QString &name, Type type, QObject *parent = nullptr);

    // Identity
    [[nodiscard]] QString name() const { return m_name; }
    [[nodiscard]] Type    type() const { return m_type; }
    [[nodiscard]] bool    isMuted()  const { return m_muted; }
    [[nodiscard]] bool    isLocked() const { return m_locked; }

    void setName(const QString &name);
    void setMuted(bool muted);
    void setLocked(bool locked);

    // Clips (ordered by timeline position)
    [[nodiscard]] const QVector<Clip *> &clips() const { return m_clips; }
    [[nodiscard]] int clipCount() const { return m_clips.size(); }
    [[nodiscard]] Clip *clipAt(int index) const;

    void addClip(Clip *clip);
    void insertClip(int index, Clip *clip);
    void removeClip(int index);
    void moveClip(int fromIndex, int toIndex);

    // Track-level effects (applied to all clips on this track)
    [[nodiscard]] const QVector<Effect *> &trackEffects() const { return m_trackEffects; }
    void addTrackEffect(Effect *effect);
    void removeTrackEffect(int index);

    /// "Track 1, Video. 5 clips. Unmuted."
    [[nodiscard]] QString accessibleSummary() const;

    /// Type as readable text
    [[nodiscard]] QString typeString() const;

signals:
    void nameChanged(const QString &name);
    void mutedChanged(bool muted);
    void lockedChanged(bool locked);
    void clipsChanged();
    void trackEffectsChanged();

private:
    QString m_name;
    Type    m_type = Type::Video;
    bool    m_muted  = false;
    bool    m_locked = false;

    QVector<Clip *>   m_clips;
    QVector<Effect *> m_trackEffects;
};

} // namespace Thrive
