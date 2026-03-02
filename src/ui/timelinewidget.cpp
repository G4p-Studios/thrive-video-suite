// SPDX-License-Identifier: MIT
// Thrive Video Suite – Timeline widget implementation

#include "timelinewidget.h"
#include "../accessibility/accessibletimeline.h"
#include "../accessibility/announcer.h"
#include "../accessibility/audiocuemanager.h"
#include "../core/timeline.h"
#include "../core/track.h"
#include "../core/clip.h"
#include "../core/timecode.h"

#include <QKeyEvent>
#include <QAccessible>

namespace Thrive {

TimelineWidget::TimelineWidget(Timeline *timeline,
                               Announcer *announcer,
                               AudioCueManager *cues,
                               QWidget *parent)
    : QWidget(parent)
    , m_timeline(timeline)
    , m_layout(new QVBoxLayout(this))
    , m_statusLabel(new QLabel(this))
{
    setObjectName(QStringLiteral("TimelineWidget"));
    setAccessibleName(tr("Timeline"));
    setAccessibleDescription(
        tr("Use Up/Down arrows to navigate tracks, "
           "Left/Right arrows to navigate clips."));
    setFocusPolicy(Qt::StrongFocus);

    m_statusLabel->setWordWrap(true);
    m_layout->addWidget(m_statusLabel);
    m_layout->addStretch();

    // Bridge: model → screen reader
    m_accessibleTimeline = new AccessibleTimeline(
        m_timeline, announcer, cues, this);

    connect(m_timeline, &Timeline::currentTrackChanged,
            this, &TimelineWidget::refresh);
    connect(m_timeline, &Timeline::currentClipChanged,
            this, [this](int, int) { refresh(); });
    connect(m_timeline, &Timeline::playheadChanged,
            this, [this](const TimeCode &) { refresh(); });

    updateStatusLabel();
}

void TimelineWidget::refresh()
{
    updateStatusLabel();
}

void TimelineWidget::setTimeline(Timeline *timeline)
{
    if (m_timeline)
        m_timeline->disconnect(this);

    m_timeline = timeline;

    connect(m_timeline, &Timeline::currentTrackChanged,
            this, &TimelineWidget::refresh);
    connect(m_timeline, &Timeline::currentClipChanged,
            this, [this](int, int) { refresh(); });
    connect(m_timeline, &Timeline::playheadChanged,
            this, [this](const TimeCode &) { refresh(); });

    m_accessibleTimeline->setTimeline(m_timeline);
    updateStatusLabel();
}

// ── keyboard navigation ─────────────────────────────────────────────

void TimelineWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        m_timeline->navigatePreviousTrack();
        emit focusedClipChanged();
        break;

    case Qt::Key_Down:
        m_timeline->navigateNextTrack();
        emit focusedClipChanged();
        break;

    case Qt::Key_Left:
        m_timeline->navigatePreviousClip();
        emit focusedClipChanged();
        break;

    case Qt::Key_Right:
        m_timeline->navigateNextClip();
        emit focusedClipChanged();
        break;

    case Qt::Key_M:
        m_timeline->navigateNextMarker();
        break;

    case Qt::Key_N:
        m_timeline->navigatePreviousMarker();
        break;

    case Qt::Key_Home:
        m_timeline->setPlayheadPosition(TimeCode(0, 25.0));
        break;

    case Qt::Key_End: {
        auto dur = m_timeline->totalDuration();
        if (dur.frame() > 0)
            m_timeline->setPlayheadPosition(dur);
        break;
    }

    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void TimelineWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    updateStatusLabel();
}

// ── status label (visual fallback) ──────────────────────────────────

void TimelineWidget::updateStatusLabel()
{
    int trackIdx = m_timeline->currentTrackIndex();
    const Track *trk = m_timeline->trackAt(trackIdx);
    if (!trk) {
        m_statusLabel->setText(tr("No tracks."));
        return;
    }
    int clipIdx  = m_timeline->currentClipIndex();
    const auto &clips = trk->clips();

    QString text = tr("Track %1 (%2)")
                       .arg(trackIdx + 1)
                       .arg(trk->type() == Track::Type::Video
                                ? tr("Video") : tr("Audio"));

    if (clipIdx >= 0 && clipIdx < clips.size()) {
        const Clip *c = clips.at(clipIdx);
        text += QStringLiteral(" — ") + c->name();
    } else {
        text += QStringLiteral(" — ") + tr("(empty)");
    }

    m_statusLabel->setText(text);
}

} // namespace Thrive
