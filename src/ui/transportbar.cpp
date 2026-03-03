// SPDX-License-Identifier: MIT
// Thrive Video Suite – Transport bar implementation

#include "transportbar.h"
#include "../engine/playbackcontroller.h"
#include "../core/timeline.h"
#include "../core/timecode.h"
#include "../accessibility/announcer.h"

#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QKeyEvent>

namespace Thrive {

TransportBar::TransportBar(PlaybackController *playback,
                           Timeline *timeline,
                           Announcer *announcer,
                           QWidget *parent)
    : QWidget(parent)
    , m_playback(playback)
    , m_timeline(timeline)
    , m_announcer(announcer)
    , m_layout(new QHBoxLayout(this))
{
    setObjectName(QStringLiteral("TransportBar"));
    setAccessibleName(tr("Transport controls"));

    m_btnRewind    = makeButton(tr("<<"),  tr("Rewind"),       tr("Rewind playback (J)"));
    m_btnStepBack  = makeButton(tr("<"),   tr("Step back"),    tr("Step one frame back (,)"));
    m_btnPlayPause = makeButton(tr("▶"),  tr("Play / Pause"), tr("Toggle playback (K / Space)"));
    m_btnStepFwd   = makeButton(tr(">"),   tr("Step forward"), tr("Step one frame forward (.)"));
    m_btnFastFwd   = makeButton(tr(">>"),  tr("Fast forward"), tr("Fast forward playback (L)"));
    m_btnStop      = makeButton(tr("■"),  tr("Stop"),         tr("Stop and return to start"));

    m_timecodeLabel = new QLabel(QStringLiteral("00:00:00:00"), this);
    m_timecodeLabel->setAccessibleName(tr("Current timecode"));
    m_timecodeLabel->setMinimumWidth(120);

    m_layout->addWidget(m_btnRewind);
    m_layout->addWidget(m_btnStepBack);
    m_layout->addWidget(m_btnPlayPause);
    m_layout->addWidget(m_btnStepFwd);
    m_layout->addWidget(m_btnFastFwd);
    m_layout->addWidget(m_btnStop);
    m_layout->addStretch();
    m_layout->addWidget(m_timecodeLabel);

    connect(m_btnPlayPause, &QPushButton::clicked,
            this, &TransportBar::onPlayPause);
    connect(m_btnStop, &QPushButton::clicked,
            this, &TransportBar::onStop);
    connect(m_btnStepFwd, &QPushButton::clicked,
            this, &TransportBar::onStepForward);
    connect(m_btnStepBack, &QPushButton::clicked,
            this, &TransportBar::onStepBackward);
    connect(m_btnFastFwd, &QPushButton::clicked,
            this, &TransportBar::onFastForward);
    connect(m_btnRewind, &QPushButton::clicked,
            this, &TransportBar::onRewind);

    connect(m_timeline, &Timeline::playheadChanged,
            this, [this](const TimeCode &) { updateTimecodeDisplay(); });

    // Update Play/Pause button text when state changes
    connect(m_playback, &PlaybackController::stateChanged,
            this, [this](PlaybackController::State state) {
                if (state == PlaybackController::State::Playing) {
                    m_btnPlayPause->setText(tr("❚❚"));
                    m_btnPlayPause->setAccessibleName(tr("Pause"));
                } else {
                    m_btnPlayPause->setText(tr("▶"));
                    m_btnPlayPause->setAccessibleName(tr("Play"));
                }
            });

    // Show shuttle speed (J/K/L)
    connect(m_playback, &PlaybackController::speedChanged,
            this, [this](double speed) {
                if (speed != 0.0 && speed != 1.0) {
                    m_announcer->announce(
                        tr("Speed: %1x").arg(speed, 0, 'g', 2),
                        Announcer::Priority::Low);
                }
            });
}

// ── slots ───────────────────────────────────────────────────────────

void TransportBar::onPlayPause()
{
    m_playback->togglePlayPause();
    const bool playing = (m_playback->state() == PlaybackController::State::Playing);
    m_announcer->announce(playing ? tr("Playing") : tr("Paused"),
                          Announcer::Priority::High);
}

void TransportBar::onStop()
{
    m_playback->stop();
    m_announcer->announce(tr("Stopped"), Announcer::Priority::High);
}

void TransportBar::onStepForward()
{
    m_playback->stepFrames(1);
}

void TransportBar::onStepBackward()
{
    m_playback->stepFrames(-1);
}

void TransportBar::onFastForward()
{
    m_playback->playForward();
}

void TransportBar::onRewind()
{
    m_playback->playReverse();
}

void TransportBar::updateTimecodeDisplay()
{
    const TimeCode &pos = m_timeline->playheadPosition();
    m_timecodeLabel->setText(pos.toString());
}

void TransportBar::setTimeline(Timeline *timeline)
{
    if (m_timeline)
        m_timeline->disconnect(this);

    m_timeline = timeline;

    connect(m_timeline, &Timeline::playheadChanged,
            this, [this](const TimeCode &) { updateTimecodeDisplay(); });
    updateTimecodeDisplay();
}

// ── helpers ─────────────────────────────────────────────────────────

QPushButton *TransportBar::makeButton(const QString &text,
                                      const QString &accessibleName,
                                      const QString &tooltip)
{
    auto *btn = new QPushButton(text, this);
    btn->setAccessibleName(accessibleName);
    btn->setToolTip(tooltip);
    btn->setFocusPolicy(Qt::TabFocus);
    return btn;
}

} // namespace Thrive
