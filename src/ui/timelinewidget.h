// SPDX-License-Identifier: MIT
// Thrive Video Suite – Timeline widget (accessible table of tracks × clips)

#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

QT_FORWARD_DECLARE_CLASS(QAccessibleInterface)

namespace Thrive {

class Timeline;
class AccessibleTimeline;
class Announcer;
class AudioCueManager;

/// The primary editing surface.  Logically it is a table:
///   – rows   = tracks
///   – columns = clips in the current track
///
/// All interaction is keyboard-driven.  The visual canvas is
/// intentionally minimal (a status label); the real "view" is the
/// screen reader output delivered by AccessibleTimeline.
class TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(Timeline *timeline,
                            Announcer *announcer,
                            AudioCueManager *cues,
                            QWidget *parent = nullptr);

    /// Re-read model and update the status label.
    void refresh();

    /// Access the underlying Timeline model (used by AccessibleTimelineView).
    [[nodiscard]] Timeline *timeline() const { return m_timeline; }

signals:
    /// Emitted after every keyboard navigation so that the property
    /// panel (or other listeners) can update.
    void focusedClipChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    void updateStatusLabel();

    Timeline           *m_timeline           = nullptr;
    AccessibleTimeline *m_accessibleTimeline = nullptr;
    QLabel             *m_statusLabel        = nullptr;
    QVBoxLayout        *m_layout             = nullptr;
};

} // namespace Thrive
