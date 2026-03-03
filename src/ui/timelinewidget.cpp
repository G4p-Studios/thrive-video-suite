// SPDX-License-Identifier: MIT
// Thrive Video Suite – Timeline widget implementation

#include "timelinewidget.h"
#include "../accessibility/accessibletimeline.h"
#include "../accessibility/announcer.h"
#include "../accessibility/audiocuemanager.h"
#include "../core/timeline.h"
#include "../core/track.h"
#include "../core/clip.h"
#include "../core/marker.h"
#include "../core/timecode.h"

#include <QKeyEvent>
#include <QAccessible>
#include <QAccessibleTableInterface>

namespace Thrive {

TimelineWidget::TimelineWidget(Timeline *timeline,
                               Announcer *announcer,
                               AudioCueManager *cues,
                               QWidget *parent)
    : QWidget(parent)
    , m_timeline(timeline)
    , m_announcer(announcer)
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

    // Notify screen readers that the table model changed so NVDA
    // picks up newly added clips / tracks.
    if (auto *iface = QAccessible::queryAccessibleInterface(this)) {
        QAccessibleTableModelChangeEvent ev(this,
            QAccessibleTableModelChangeEvent::ModelReset);
        QAccessible::updateAccessibility(&ev);
    }
}

void TimelineWidget::connectTrackSignals()
{
    // Disconnect any previous per-track connections
    for (const auto &conn : m_trackConnections)
        disconnect(conn);
    m_trackConnections.clear();

    // Connect every track's clipsChanged signal so the widget
    // refreshes when clips are added, removed, or reordered.
    if (!m_timeline) return;
    for (auto *trk : m_timeline->tracks()) {
        m_trackConnections.append(
            connect(trk, &Track::clipsChanged, this, &TimelineWidget::refresh));
    }
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
    connect(m_timeline, &Timeline::tracksChanged,
            this, [this]() {
                connectTrackSignals();
                refresh();
            });

    connectTrackSignals();
    m_accessibleTimeline->setTimeline(m_timeline);
    updateStatusLabel();
}

// ── keyboard navigation ─────────────────────────────────────────────

void TimelineWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up: {
        int before = m_timeline->currentTrackIndex();
        m_timeline->navigatePreviousTrack();
        if (m_timeline->currentTrackIndex() == before && before == 0)
            m_announcer->announce(
                tr("First track"), Announcer::Priority::Normal);
        emit focusedClipChanged();
        break;
    }

    case Qt::Key_Down: {
        int before = m_timeline->currentTrackIndex();
        m_timeline->navigateNextTrack();
        if (m_timeline->currentTrackIndex() == before
            && before == m_timeline->trackCount() - 1)
            m_announcer->announce(
                tr("Last track"), Announcer::Priority::Normal);
        emit focusedClipChanged();
        break;
    }

    case Qt::Key_Left: {
        int before = m_timeline->currentClipIndex();
        m_timeline->navigatePreviousClip();
        if (m_timeline->currentClipIndex() == before && before == 0)
            m_announcer->announce(
                tr("Beginning of track"), Announcer::Priority::Normal);
        emit focusedClipChanged();
        break;
    }

    case Qt::Key_Right: {
        int before = m_timeline->currentClipIndex();
        m_timeline->navigateNextClip();
        auto *trk = m_timeline->trackAt(m_timeline->currentTrackIndex());
        if (m_timeline->currentClipIndex() == before
            && trk && before == trk->clipCount() - 1)
            m_announcer->announce(
                tr("End of track"), Announcer::Priority::Normal);
        emit focusedClipChanged();
        break;
    }

    case Qt::Key_M: {
        auto posBefore = m_timeline->playheadPosition();
        m_timeline->navigateNextMarker();
        if (m_timeline->playheadPosition().frame() != posBefore.frame()) {
            // Find the marker at the new position
            for (auto *mk : m_timeline->markers()) {
                if (mk->position().frame() == m_timeline->playheadPosition().frame()) {
                    m_announcer->announce(
                        tr("Marker: %1, %2")
                            .arg(mk->name(),
                                 m_timeline->playheadPosition().toString()),
                        Announcer::Priority::Normal);
                    break;
                }
            }
        } else {
            m_announcer->announce(
                tr("No next marker."), Announcer::Priority::Normal);
        }
        break;
    }

    case Qt::Key_N: {
        auto posBefore = m_timeline->playheadPosition();
        m_timeline->navigatePreviousMarker();
        if (m_timeline->playheadPosition().frame() != posBefore.frame()) {
            for (auto *mk : m_timeline->markers()) {
                if (mk->position().frame() == m_timeline->playheadPosition().frame()) {
                    m_announcer->announce(
                        tr("Marker: %1, %2")
                            .arg(mk->name(),
                                 m_timeline->playheadPosition().toString()),
                        Announcer::Priority::Normal);
                    break;
                }
            }
        } else {
            m_announcer->announce(
                tr("No previous marker."), Announcer::Priority::Normal);
        }
        break;
    }

    case Qt::Key_Home:
        m_timeline->setPlayheadPosition(
            TimeCode(0, m_timeline->playheadPosition().fps()));
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

    // Announce current position for screen reader users
    int trackIdx = m_timeline->currentTrackIndex();
    const Track *trk = m_timeline->trackAt(trackIdx);
    if (trk) {
        QString summary = tr("Timeline. Track %1 of %2, %3.")
                              .arg(trackIdx + 1)
                              .arg(m_timeline->trackCount())
                              .arg(trk->name());
        int clipIdx = m_timeline->currentClipIndex();
        if (clipIdx >= 0 && clipIdx < trk->clipCount()) {
            summary += QLatin1Char(' ')
                       + tr("Clip %1 of %2, %3.")
                             .arg(clipIdx + 1)
                             .arg(trk->clipCount())
                             .arg(trk->clipAt(clipIdx)->name());
        } else {
            summary += QLatin1Char(' ') + tr("No clips.");
        }
        m_announcer->announce(summary, Announcer::Priority::Normal);
    }
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

    QString text = tr("Track %1 of %2 (%3, %n clip(s))", nullptr, clips.size())
                       .arg(trackIdx + 1)
                       .arg(m_timeline->trackCount())
                       .arg(trk->type() == Track::Type::Video
                                ? tr("Video") : tr("Audio"));

    if (clipIdx >= 0 && clipIdx < clips.size()) {
        const Clip *c = clips.at(clipIdx);
        text += QStringLiteral(" — ") + c->name()
              + QStringLiteral(" [") + c->timelinePosition().toString()
              + QStringLiteral(" – ") + (c->timelinePosition() + c->duration()).toString()
              + QStringLiteral("]");
    } else if (!clips.isEmpty()) {
        // Clips exist but index is out of range — show first
        text += QStringLiteral(" — ") + clips.first()->name();
    } else {
        text += QStringLiteral(" — ") + tr("(empty)");
    }

    m_statusLabel->setText(text);
}

} // namespace Thrive
