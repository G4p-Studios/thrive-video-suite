// SPDX-License-Identifier: MIT
// Thrive Video Suite – Transport bar (play / pause / stop / scrub)

#pragma once

#include <QWidget>

QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QHBoxLayout)

namespace Thrive {

class PlaybackController;
class Timeline;
class Announcer;

/// Accessible toolbar for playback transport controls.
/// All buttons have accessibleName / accessibleDescription set.
class TransportBar : public QWidget
{
    Q_OBJECT

public:
    explicit TransportBar(PlaybackController *playback,
                          Timeline *timeline,
                          Announcer *announcer,
                          QWidget *parent = nullptr);

public slots:
    void updateTimecodeDisplay();
    void setTimeline(Timeline *timeline);

private slots:
    void onPlayPause();
    void onStop();
    void onStepForward();
    void onStepBackward();
    void onFastForward();
    void onRewind();

private:
    QPushButton *makeButton(const QString &text,
                            const QString &accessibleName,
                            const QString &tooltip);

    PlaybackController *m_playback  = nullptr;
    Timeline           *m_timeline  = nullptr;
    Announcer          *m_announcer = nullptr;

    QPushButton *m_btnPlayPause  = nullptr;
    QPushButton *m_btnStop       = nullptr;
    QPushButton *m_btnStepBack   = nullptr;
    QPushButton *m_btnStepFwd    = nullptr;
    QPushButton *m_btnRewind     = nullptr;
    QPushButton *m_btnFastFwd    = nullptr;
    QLabel      *m_timecodeLabel = nullptr;
    QHBoxLayout *m_layout        = nullptr;
};

} // namespace Thrive
